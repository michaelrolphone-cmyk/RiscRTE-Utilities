#pragma once
/* Checked, copied passive sensor session. Provider owns radio and 15s bound. */
#include "RiscRuntimeV1.h"
#include "RiscBluetoothSensorsV1.h"
#include <string.h>
typedef struct {
 const risc_runtime_api_v1 *rt; bool (*alive)(void*);void (*retain)(void*);void *context;
 risc_runtime_capability_v1 grant;const risc_bluetooth_sensors_v1 *api;
 uint64_t token;bool terminal,busy,available,restore_failed,cleanup_pending;
 risc_ble_sensor_status_v1 status;risc_ble_sensor_device_v1 devices[RISC_BLE_MAX_DEVICES];
 uint32_t copied_at;
} scanner_session;
static inline bool scanner_session_live(scanner_session*s){
 if(s->terminal)return false;
 if(risc_runtime_get_api(1)!=s->rt||(s->alive&&!s->alive(s->context))){s->terminal=true;if(s->retain)s->retain(s->context);return false;}return true;
}
static inline bool scanner_session_fault(scanner_session*s){if(!s->terminal){s->terminal=true;if(s->retain)s->retain(s->context);}return false;}
static inline bool scanner_session_enter(scanner_session*s){if(!scanner_session_live(s))return false;if(s->busy)return scanner_session_fault(s);s->busy=true;return true;}
static inline bool scanner_session_leave(scanner_session*s){s->busy=false;return scanner_session_live(s);}
static inline bool scanner_session_empty(const risc_runtime_capability_v1*g){return g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation;}
static inline void scanner_session_init(scanner_session*s,const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*context){
 memset(s,0,sizeof(*s));s->rt=rt;s->alive=alive;s->retain=retain;s->context=context;s->grant.struct_size=sizeof(s->grant);s->status.struct_size=sizeof(s->status);
}
static inline bool scanner_session_status_valid(const risc_ble_sensor_status_v1*v){return v->struct_size==sizeof(*v)&&v->state<=RISC_BLE_SENSOR_ERROR&&v->count<=RISC_BLE_MAX_DEVICES&&memchr(v->error,0,sizeof(v->error));}
static inline bool scanner_session_device_valid(const risc_ble_sensor_device_v1*v){return v->address_type<=3&&v->reading_count<=RISC_BLE_MAX_READINGS&&v->service_count<=8&&v->payload_size<=31&&memchr(v->name,0,sizeof(v->name));}
static inline bool scanner_session_stop(scanner_session*s){
 if(!scanner_session_enter(s))return false;
 if(s->token){bool ok=s->api->close(s->api->context,s->token);if(!scanner_session_live(s))return false;if(!ok){s->cleanup_pending=true;s->busy=false;return false;}s->token=0;s->cleanup_pending=false;
  risc_ble_sensor_status_v1 v={.struct_size=sizeof(v)};ok=s->api->status(s->api->context,&v);if(!scanner_session_live(s))return false;
  if(!ok||!scanner_session_status_valid(&v)||v.cleanup_pending)return scanner_session_fault(s);
  s->restore_failed|=v.restore_failed;
 }
 if(s->grant.api){risc_runtime_capability_v1 g=s->grant;bool ok=s->rt->release(&g);if(!scanner_session_live(s))return false;if(!ok||!scanner_session_empty(&g))return scanner_session_fault(s);s->grant=g;s->api=NULL;}
 else if(!scanner_session_empty(&s->grant))return scanner_session_fault(s);
 if(s->status.state==RISC_BLE_SENSOR_STARTING||s->status.state==RISC_BLE_SENSOR_SCANNING)s->status.state=RISC_BLE_SENSOR_COMPLETE;
 return scanner_session_leave(s);
}
static inline bool scanner_session_start(scanner_session*s){
 if(!scanner_session_live(s))return false;
 if(s->token||s->grant.api){if(!scanner_session_stop(s))return false;}
 if(!scanner_session_enter(s))return false;
 if(!scanner_session_empty(&s->grant))return scanner_session_fault(s);
 bool ok=s->rt->acquire(RISC_BLUETOOTH_SENSORS_CAPABILITY,1,0,&s->grant);
 if(!scanner_session_live(s))return false;
 if(!ok){if(!scanner_session_empty(&s->grant))return scanner_session_fault(s);s->available=false;scanner_session_leave(s);return false;}
 if(s->grant.struct_size!=sizeof(s->grant)||!s->grant.api||!s->grant.slot||!s->grant.generation)return scanner_session_fault(s);
 s->api=s->grant.api;s->available=true;const risc_bluetooth_sensors_v1*a=s->api;
 if(a->api_version!=1||a->struct_size<sizeof(*a)||!a->open||!a->poll||!a->status||!a->device||!a->close){s->busy=false;scanner_session_stop(s);return false;}
 s->token=0;ok=a->open(a->context,&s->token);if(!scanner_session_live(s))return false;
 if(!ok||!s->token){s->busy=false;if(ok)return scanner_session_fault(s);scanner_session_stop(s);return false;}
 s->restore_failed=false;s->status=(risc_ble_sensor_status_v1){.struct_size=sizeof(s->status),.state=RISC_BLE_SENSOR_STARTING};memset(s->devices,0,sizeof(s->devices));return scanner_session_leave(s);
}
static inline bool scanner_session_step(scanner_session*s,uint32_t now){
 if(!scanner_session_live(s))return false;
 if(s->cleanup_pending)return scanner_session_stop(s);
 if(!s->token)return true;
 if(!scanner_session_enter(s))return false;
 bool ok=s->api->poll(s->api->context,s->token,6);if(!scanner_session_live(s))return false;
 risc_ble_sensor_status_v1 v={.struct_size=sizeof(v)};
 bool got=s->api->status(s->api->context,&v);if(!scanner_session_live(s))return false;
 if(!got||!scanner_session_status_valid(&v)||v.cleanup_pending)return scanner_session_fault(s);
 for(uint32_t i=0;i<v.count;i++){risc_ble_sensor_device_v1 d={0};got=s->api->device(s->api->context,i,&d);if(!scanner_session_live(s))return false;if(!got||!scanner_session_device_valid(&d))return scanner_session_fault(s);s->devices[i]=d;}
 s->status=v;s->copied_at=now;s->restore_failed|=v.restore_failed;if(!ok){s->status.state=RISC_BLE_SENSOR_ERROR;memcpy(s->status.error,"Sensor scan failed",19);}
 if(!scanner_session_leave(s))return false;
 if(s->status.state==RISC_BLE_SENSOR_COMPLETE||s->status.state==RISC_BLE_SENSOR_ERROR)return scanner_session_stop(s);
 return true;
}
