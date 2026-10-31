#ifndef BATTERY_BROADCAST_H
#define BATTERY_BROADCAST_H
/* Optional shared-app controller. Providers own RF and storage; no device or
 * backend selection. Only explicit set_enabled creates an intention. */
#include "RiscRuntimeV1.h"
#include "TelemetryBroadcastPolicyV1.h"
#include "PortableRadioPolicy.h"
#ifndef PORTABLE_PREFERENCE_STORE
#define PORTABLE_PREFERENCE_STORE
#endif
#include "PortablePreferenceAtomic.h"
#include "battery_preference_adapter.h"
enum { BATTERY_BROADCAST_OK=0, BATTERY_BROADCAST_UNAVAILABLE=1,
 BATTERY_BROADCAST_ERROR=2, BATTERY_BROADCAST_PENDING=3,
 BATTERY_BROADCAST_RETAINED=4, BATTERY_BROADCAST_UNCERTAIN=5 };
typedef struct battery_broadcast {
 const risc_runtime_api_v1 *runtime;
 bool (*alive)(void *);void (*retain)(void *);void *context;
 risc_runtime_capability_v1 service,storage,preference;
 const telemetry_broadcast_v1 *api;
 portable_preference_atomic edit;
 battery_preference_adapter atomic_adapter;
 telemetry_broadcast_policy_v1 policy;
 bool retained,save_failed,desired,loaded;
 uint32_t loaded_at;
} battery_broadcast;
static inline bool battery_broadcast_guard(void *context){
 battery_broadcast *s=context;
 return s&&!s->retained&&s->runtime&&s->alive&&s->alive(s->context)&&risc_runtime_get_api(1)==s->runtime;
}
static inline int battery_broadcast_retain(battery_broadcast*s){
 if(!s->retained){s->retained=true;if(s->retain)s->retain(s->context);}
 return BATTERY_BROADCAST_RETAINED;
}
static inline int battery_broadcast_acquire(battery_broadcast*s,const char*name,uint32_t instance,risc_runtime_capability_v1*g){
 if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
 if(g->api)return BATTERY_BROADCAST_OK;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=s->runtime->acquire(name,1,instance,g);
 if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
 if(!ok){if(g->struct_size!=sizeof(*g)||g->api||g->slot||g->generation)return battery_broadcast_retain(s);return BATTERY_BROADCAST_UNAVAILABLE;}
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation)return battery_broadcast_retain(s);
 return BATTERY_BROADCAST_OK;
}
static inline int battery_broadcast_release(battery_broadcast*s,risc_runtime_capability_v1*g){
 if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
 if(!g->api)return BATTERY_BROADCAST_OK;
 risc_runtime_capability_v1 copy=*g;bool ok=s->runtime->release(&copy);
 if(!battery_broadcast_guard(s)||!ok||copy.struct_size!=sizeof(copy)||copy.api||copy.slot||copy.generation)return battery_broadcast_retain(s);
 *g=(risc_runtime_capability_v1){0};return BATTERY_BROADCAST_OK;
}
static inline int battery_broadcast_open(battery_broadcast*s);
static inline int battery_broadcast_close_service(battery_broadcast*s);
static inline int battery_broadcast_init(battery_broadcast*s,const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*context){
 memset(s,0,sizeof(*s));s->runtime=rt;s->alive=alive;s->retain=retain;s->context=context;
 if(!rt||!rt->acquire||!rt->release||!rt->health)return battery_broadcast_retain(s);
 return battery_broadcast_open(s);
}
static inline int battery_broadcast_open(battery_broadcast*s){
 int rc=battery_broadcast_acquire(s,TELEMETRY_BROADCAST_CAPABILITY,0,&s->service);if(rc)return rc;
 s->api=s->service.api;
 if(s->api->api_version!=1||s->api->struct_size<sizeof(*s->api)||!s->api->step||!s->api->pause||!s->api->status||!s->api->enumerate||!s->api->read)return battery_broadcast_retain(s);
 return BATTERY_BROADCAST_OK;
}
static inline int battery_broadcast_close_service(battery_broadcast*s){
 int rc=battery_broadcast_release(s,&s->service);if(!rc)s->api=NULL;return rc;
}
static inline int battery_broadcast_pause(battery_broadcast*s){
 if(s->edit.token)return battery_broadcast_guard(s)?BATTERY_BROADCAST_PENDING:battery_broadcast_retain(s);
 int rc=battery_broadcast_open(s);if(rc)return rc;
 bool ok=s->api->pause(s->api->context);
 if(!battery_broadcast_guard(s)||!ok)return battery_broadcast_retain(s);
 s->loaded=false;return battery_broadcast_close_service(s);
}
static inline int battery_broadcast_read(battery_broadcast*s,telemetry_broadcast_status_v1*out){
 if(!out)return BATTERY_BROADCAST_ERROR;
 *out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out)};
 if(s->edit.token)return battery_broadcast_guard(s)?BATTERY_BROADCAST_PENDING:battery_broadcast_retain(s);
 int rc=battery_broadcast_open(s);if(rc)return rc;
 bool ok=s->api->status(s->api->context,out);
 if(!battery_broadcast_guard(s)||!ok||out->struct_size<sizeof(*out)||out->state>TELEMETRY_BROADCAST_RETAINED||out->cleanup_pending||out->state==TELEMETRY_BROADCAST_RETAINED)return battery_broadcast_retain(s);
 return BATTERY_BROADCAST_OK;
}
static inline int battery_broadcast_terminal(battery_broadcast*s,int result){
 if(result==RISC_ATOMIC_RETAINED||!battery_broadcast_guard(s)||s->edit.token)return battery_broadcast_retain(s);
 if(result!=PORTABLE_PREFERENCE_READY&&!portable_preference_atomic_terminal(result))return battery_broadcast_retain(s);
 bool saved=result==RISC_ATOMIC_OK;s->save_failed=!saved;s->loaded=false;
 int rc=battery_broadcast_release(s,&s->preference);if(rc)return rc;
 s->policy=(telemetry_broadcast_policy_v1){.struct_size=sizeof(s->policy),.enabled=saved&&s->desired,.settings_valid=saved};
 rc=battery_broadcast_open(s);if(rc)return rc;
 bool ok=s->api->step(s->api->context,false,&s->policy);
 if(!battery_broadcast_guard(s)||!ok)return battery_broadcast_retain(s);
 return saved?BATTERY_BROADCAST_OK:result==RISC_ATOMIC_UNCERTAIN?BATTERY_BROADCAST_UNCERTAIN:BATTERY_BROADCAST_ERROR;
}
/* A failed/uncertain terminal transaction remains paused until the next
 * explicit user action. No automatic replay, legacy put, or rebase. */
static inline int battery_broadcast_set_enabled(battery_broadcast*s,bool enabled){
 if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
 if(s->edit.token)return BATTERY_BROADCAST_PENDING;
 int rc=battery_broadcast_pause(s);if(rc)return rc;
 s->desired=enabled;s->save_failed=true;s->loaded=false;
 rc=battery_broadcast_acquire(s,RISC_PREFERENCE_STORE_CAPABILITY,1,&s->preference);
 const risc_preference_store_v1 *preferences=s->preference.api;
 if(rc==BATTERY_BROADCAST_UNAVAILABLE){
  /* Only a clean absent grant permits trying the other existing optional
   * contract. No operation, malformed grant, refusal status or receipt can
   * enter this branch after acquisition. */
  rc=battery_broadcast_acquire(s,RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,&s->preference);
  if(!rc){if(!battery_preference_adapt(&s->atomic_adapter,s->preference.api))return battery_broadcast_retain(s);preferences=&s->atomic_adapter.view;}
 }
 if(rc)return rc;
 rc=portable_preference_atomic_begin(&s->edit,preferences,TELEMETRY_BROADCAST_KEY,PORTABLE_PREFERENCE_USER,battery_broadcast_guard,s);
 if(rc==PORTABLE_PREFERENCE_READY){
  const uint8_t value[]={0x62,1,enabled?1:0,(uint8_t)((enabled?1:0)^0xa5u)};
  rc=portable_preference_atomic_prepare(&s->edit,value,sizeof(value));
  if(rc==PORTABLE_PREFERENCE_READY)rc=portable_preference_atomic_submit(&s->edit);
 }
 if(!battery_broadcast_guard(s)||rc==RISC_ATOMIC_RETAINED)return battery_broadcast_retain(s);
 if(s->edit.token)return BATTERY_BROADCAST_PENDING;
 return battery_broadcast_terminal(s,rc);
}
static inline int battery_broadcast_poll(battery_broadcast*s,bool cancel){
 if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
 if(!s->edit.token)return BATTERY_BROADCAST_OK;
 int rc=cancel?portable_preference_atomic_cancel(&s->edit):portable_preference_atomic_collect(&s->edit);
 if(!battery_broadcast_guard(s)||rc==RISC_ATOMIC_RETAINED)return battery_broadcast_retain(s);
 if(s->edit.token)return BATTERY_BROADCAST_PENDING;
 return battery_broadcast_terminal(s,rc);
}
static inline int battery_broadcast_step(battery_broadcast*s,bool allow){
 if(s->edit.token)return battery_broadcast_poll(s,false);
 if(!allow)return battery_broadcast_pause(s);
 int rc=battery_broadcast_open(s);if(rc)return rc;
 risc_runtime_health_v1 health={.struct_size=sizeof(health)};bool healthy=s->runtime->health(&health);
 if(!battery_broadcast_guard(s)||!healthy)return battery_broadcast_retain(s);
 if(!s->save_failed&&(!s->loaded||(uint32_t)(health.uptime_ms-s->loaded_at)>=1000u)){
  bool paused=s->api->pause(s->api->context);if(!battery_broadcast_guard(s)||!paused)return battery_broadcast_retain(s);
  s->policy=(telemetry_broadcast_policy_v1){.struct_size=sizeof(s->policy)};
  rc=battery_broadcast_acquire(s,RISC_KEY_VALUE_CAPABILITY,1,&s->storage);
  if(rc==BATTERY_BROADCAST_RETAINED)return rc;
  if(!rc){
   const risc_key_value_v1*kv=s->storage.api;bool enabled=false;uint8_t bytes[4],flags=0;uint32_t n=0;
   if(!pqa_preferences_valid(kv))return battery_broadcast_retain(s);
   int got=kv->get(kv->context,TELEMETRY_BROADCAST_KEY,bytes,sizeof(bytes),&n);
   if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
   if(got<RISC_KEY_VALUE_BUSY||got>0)return battery_broadcast_retain(s);
   bool valid=false;
   if(got==RISC_KEY_VALUE_NOT_FOUND&&!n&&s->api->struct_size>=sizeof(telemetry_broadcast_policy_api_v1)){
    const telemetry_broadcast_policy_api_v1 *extended=(const void*)s->api;
    if(extended->tag!=TELEMETRY_BROADCAST_POLICY_TAG||extended->version!=1||!extended->defaults)return battery_broadcast_retain(s);
    telemetry_broadcast_policy_v1 defaults={.struct_size=sizeof(defaults)};
    bool ok=extended->defaults(s->api->context,&defaults);
    if(!battery_broadcast_guard(s)||!ok||defaults.struct_size!=sizeof(defaults)||!defaults.settings_valid||defaults.radios_allowed)return battery_broadcast_retain(s);
    enabled=defaults.enabled;valid=true;
   }
   if(got==RISC_KEY_VALUE_OK&&n==4&&bytes[0]==0x62&&bytes[1]==1&&bytes[2]<=1&&bytes[3]==(uint8_t)(bytes[2]^0xa5u)){enabled=bytes[2]!=0;valid=true;}
   bool radio=portable_radio_load(kv,&flags);
   if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
   s->policy.settings_valid=valid&&radio;s->policy.enabled=enabled;s->policy.radios_allowed=(flags&6u)==PORTABLE_RADIO_BLUETOOTH;
   rc=battery_broadcast_release(s,&s->storage);if(rc)return rc;
  }
  s->loaded=true;s->loaded_at=health.uptime_ms;
 }
 bool ok=s->api->step(s->api->context,allow&&!s->save_failed,&s->policy);
 if(!battery_broadcast_guard(s)||!ok)return battery_broadcast_retain(s);
 return BATTERY_BROADCAST_OK;
}
/* Cleanup may need repeated calls while accepted storage work cancels/collects.
 * RETAINED means no additional ordinary calls or grant release is legal. */
static inline int battery_broadcast_cleanup(battery_broadcast*s){
 if(!battery_broadcast_guard(s))return battery_broadcast_retain(s);
 if(s->edit.token){int rc=battery_broadcast_poll(s,true);if(rc==BATTERY_BROADCAST_PENDING||rc==BATTERY_BROADCAST_RETAINED)return rc;}
 int rc=battery_broadcast_pause(s);if(rc==BATTERY_BROADCAST_UNAVAILABLE)rc=BATTERY_BROADCAST_OK;
 if(rc)return rc;
 rc=battery_broadcast_release(s,&s->storage);if(rc)return rc;
 return battery_broadcast_release(s,&s->preference);
}
#endif
