#pragma once
/* Generic published foreground alarm service fallback. The scheduler owns
 * schedules, time, output mode and persistence. Status is copied before every
 * cooperative phase; owners settle before an exposed output reservation. */
#include "RiscRuntimeV1.h"
#define ALARM_SERVICE_CLOCK_EDIT_V2
#include "AlarmServiceV2.h"
#include <string.h>
typedef struct {
 const risc_runtime_api_v1 *rt;bool(*alive)(void*);void(*retain)(void*);void*context;
 risc_runtime_capability_v1 grant;const alarm_service_v1 *api;
 alarm_status_v1 status;bool checked,terminal,busy,owned;
} scanner_alarm;
static inline bool scanner_alarm_live(scanner_alarm*s){return !s->terminal&&s->rt&&risc_runtime_get_api(1)==s->rt&&s->alive(s->context);}
static inline bool scanner_alarm_fail(scanner_alarm*s){if(!s->terminal){s->terminal=true;s->retain(s->context);}return false;}
static inline bool scanner_alarm_check(scanner_alarm*s){return scanner_alarm_live(s)||scanner_alarm_fail(s);}
static inline bool scanner_alarm_result(scanner_alarm*s,int rc){return scanner_alarm_check(s)&&((rc>=ALARM_FOREGROUND&&rc<=ALARM_PENDING&&rc!=ALARM_OUTPUT)||scanner_alarm_fail(s));}
static inline bool scanner_alarm_release(scanner_alarm*s){
 risc_runtime_capability_v1 g=s->grant;bool ok=s->rt->release(&g);
 if(!scanner_alarm_check(s)||!ok||g.struct_size!=sizeof(g)||g.api||g.slot||g.generation)return scanner_alarm_fail(s);
 s->grant=g;s->api=NULL;s->owned=false;return true;
}
static inline void scanner_alarm_init(scanner_alarm*s,const risc_runtime_api_v1*r,bool(*alive)(void*),void(*retain)(void*),void*c){memset(s,0,sizeof(*s));s->rt=r;s->alive=alive;s->retain=retain;s->context=c;}
static inline bool scanner_alarm_open(scanner_alarm*s){
 if(!scanner_alarm_check(s))return false;
 if(s->busy)return scanner_alarm_fail(s);
 if(s->checked)return true;
 s->busy=true;
 for(unsigned version=2;version;version--){
  s->grant=(risc_runtime_capability_v1){.struct_size=sizeof(s->grant)};
  bool ok=s->rt->acquire(ALARM_SERVICE_CAPABILITY,version,0,&s->grant);
  if(!scanner_alarm_check(s))return false;
  if(s->grant.struct_size!=sizeof(s->grant)||(ok?(!s->grant.api||!s->grant.slot||!s->grant.generation):(s->grant.api||s->grant.slot||s->grant.generation)))return scanner_alarm_fail(s);
  if(!ok)continue;
  s->api=s->grant.api;
  if(s->api->api_version!=version||s->api->struct_size<sizeof(*s->api)||!s->api->status||!s->api->step||!s->api->refresh||!s->api->acknowledge||!s->api->prepare_sleep||!s->api->stop_only||(version==2&&!alarm_service_descriptor(s->api)))return scanner_alarm_fail(s);
  break;
 }
 s->checked=true;s->busy=false;return true;
}
static inline bool scanner_alarm_snapshot(scanner_alarm*s){
 alarm_status_v1 q={.struct_size=sizeof(q)};int rc=s->api->status(s->api->context,&q);
 if(!scanner_alarm_result(s,rc)||rc!=ALARM_OK||q.api_version!=1||q.struct_size!=sizeof(q)||q.state>ALARM_STATE_CUE||q.mode>ALARM_MODE_BOTH||q.output_uncertain||!memchr(q.label,0,sizeof(q.label)))return scanner_alarm_fail(s);
 if(!scanner_alarm_result(s,q.error))return false;
 s->status=q;s->owned=q.occurrence.generation||q.state==ALARM_STATE_ALERT||q.state==ALARM_STATE_DISMISSING||q.state==ALARM_STATE_BLOCKED||q.state==ALARM_STATE_CUE;return true;
}
static inline bool scanner_alarm_step(scanner_alarm*s,bool settled,bool(*stop)(void)){
 if(!scanner_alarm_check(s))return false;
 if(!s->api)return true;
 if(s->busy)return scanner_alarm_fail(s);
 s->busy=true;
 if(!scanner_alarm_snapshot(s))return false;
 if(s->owned&&!stop()){s->busy=false;return scanner_alarm_check(s);}
 if(settled){int rc=s->api->step(s->api->context);if(!scanner_alarm_result(s,rc)||!scanner_alarm_snapshot(s))return false;
  if(s->owned&&!stop()){s->busy=false;return scanner_alarm_check(s);}}
 s->busy=false;return true;
}
static inline bool scanner_alarm_ack(scanner_alarm*s,const alarm_token_v1*t){
 if(!scanner_alarm_check(s)||!s->api)return false;
 if(s->busy)return scanner_alarm_fail(s);
 s->busy=true;if(!scanner_alarm_snapshot(s))return false;
 if(s->status.state!=ALARM_STATE_ALERT||memcmp(t,&s->status.occurrence,sizeof(*t))){s->busy=false;return true;}
 int rc=s->api->acknowledge(s->api->context,t);if(!scanner_alarm_result(s,rc))return false;s->busy=false;return true;
}
static inline bool scanner_alarm_refresh(scanner_alarm*s){
 if(!scanner_alarm_check(s)||!s->api)return false;
 if(s->busy)return scanner_alarm_fail(s);
 s->busy=true;int rc=s->api->refresh(s->api->context);
 if(!scanner_alarm_result(s,rc))return false;
 s->busy=false;return true;
}
static inline bool scanner_alarm_close(scanner_alarm*s){
 if(!scanner_alarm_check(s))return false;
 if(!s->api)return true;
 if(s->busy)return scanner_alarm_fail(s);
 s->busy=true;int rc=s->api->stop_only(s->api->context);if(!scanner_alarm_result(s,rc))return false;
 if(rc==ALARM_PENDING||rc==ALARM_BUSY){s->busy=false;return false;}
 if(rc!=ALARM_OK)return scanner_alarm_fail(s);
 if(!scanner_alarm_release(s))return false;
 s->busy=false;return true;
}
