#pragma once
#include "AlarmServiceV2.h"
#include <string.h>
/* Public scheduler's copied CUE/BLOCKED reservation is checked before its next
 * output phase. Domain stop must prove HID and background owners settled. */
typedef struct {
 const alarm_service_v1 *api;bool (*alive)(void*),(*stop)(void*);void *owner;
 alarm_status_v1 status;bool reserved,terminal,available;
} ble_buttons_alarm;
static inline bool bba_live(ble_buttons_alarm *a){if(a->terminal)return false;if(!a->alive||!a->alive(a->owner)){a->terminal=true;return false;}return true;}
static inline bool bba_rc(ble_buttons_alarm *a,int rc){if(!bba_live(a))return false;if(rc>ALARM_PENDING||rc<ALARM_FOREGROUND||rc==ALARM_OUTPUT){a->terminal=true;return false;}return true;}
static inline bool bba_init(ble_buttons_alarm *a,const alarm_service_v1 *api,bool(*alive)(void*),bool(*stop)(void*),void *owner){
 memset(a,0,sizeof(*a));a->api=api;a->alive=alive;a->stop=stop;a->owner=owner;
 a->available=api&&(api->api_version==1||alarm_service_descriptor(api))&&api->struct_size>=sizeof(*api)&&api->status&&api->step&&api->acknowledge&&api->stop_only;
 return a->available;
}
static inline bool bba_status(ble_buttons_alarm *a){
 alarm_status_v1 s={.struct_size=sizeof(s)};
 if(!bba_live(a))return false;
 int rc=a->api->status(a->api->context,&s);
 if(!bba_rc(a,rc))return false;
 if(rc!=ALARM_OK)return false;
 if(s.api_version!=1||s.struct_size!=sizeof(s)||s.state>ALARM_STATE_CUE||s.output_uncertain){a->terminal=true;return false;}
 a->status=s;a->reserved=s.state==ALARM_STATE_ALERT||s.state==ALARM_STATE_DISMISSING||s.state==ALARM_STATE_BLOCKED||s.state==ALARM_STATE_CUE;
 return true;
}
static inline bool bba_step(ble_buttons_alarm *a,bool settled){
 if(!a->available)return true;
 if(!bba_status(a))return false;
 if(a->reserved){if(!a->stop(a->owner)||!bba_live(a))return false;}
 if(!settled){
  if(!a->reserved)return true;
  int rc=a->api->stop_only(a->api->context);return bba_rc(a,rc)&&rc==ALARM_OK;
 }
 int rc=a->api->step(a->api->context);
 if(!bba_rc(a,rc))return false;
 if(!bba_status(a))return false;
 if(a->reserved&&!a->stop(a->owner))return false;
 return bba_live(a);
}
static inline bool bba_ack(ble_buttons_alarm *a,const alarm_token_v1 *token){
 if(!a->available||!bba_live(a)||a->status.state!=ALARM_STATE_ALERT||memcmp(token,&a->status.occurrence,sizeof(*token)))return false;
 if(!a->stop(a->owner)||!bba_live(a))return false;
 int rc=a->api->acknowledge(a->api->context,token);return bba_rc(a,rc)&&(rc==ALARM_OK||rc==ALARM_PENDING);
}

static inline bool bba_pause(ble_buttons_alarm *a){
 if(!a->available)return true;
 if(!bba_live(a)||!a->stop(a->owner)||!bba_live(a))return false;
 int rc=a->api->stop_only(a->api->context);return bba_rc(a,rc)&&rc==ALARM_OK;
}
