#pragma once
/* Files controller. Checked acquisition, UTC and CFP persistence mechanics
 * derive from scanner_background.h at f057c6c; Files policy, BLE progression,
 * automatic actions and root focus boundary are independent below. */
#include <stdbool.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscRealtimeV1.h"
#include "RiscRtcUtcV1.h"
static bool files_background_live(void);
#define PORTABLE_CONTEXTS_LIVE() files_background_live()
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() files_background_live()
/* The published client's legacy RTC helper is not called. Our public clock
 * uses the generic reader below and does not select a physical clock backend. */
#define portable_fingerprint_clock files_background_unused_legacy_clock
#include "PortableContextsClient.h"
#undef portable_fingerprint_clock
#include "battery_broadcast.h"
#include "files_background_api.h"
#include "RiscResidentSettingsEffectsV1.h"
#include "PortablePreferenceAutomatic.h"
#include "PortableContextActions.h"
static struct {
 const risc_runtime_api_v1*rt;bool(*alive)(void*);void(*retain)(void*);void*context;
 bool initialized,retained,busy,paused,clock_busy,capture_permitted,radio_permitted;portable_contexts_client contexts;
 battery_broadcast broadcast;contexts_fingerprint_service_v1 proxy;
 files_background_status status;
 files_background_hooks hooks;uint32_t action_checked;bool action_checked_valid;
 risc_runtime_capability_v1 automatic_grant;portable_preference_automatic automatic;
 const risc_key_value_atomic_v1*automatic_source;risc_key_value_atomic_v1 automatic_view;
} files_bg;
static bool files_background_live(void){return !files_bg.retained&&files_bg.rt&&files_bg.alive&&risc_runtime_get_api(1)==files_bg.rt&&files_bg.alive(files_bg.context);}
static bool fbg_fail(void){if(!files_bg.retained){files_bg.retained=true;files_bg.status.retained=true;if(files_bg.retain)files_bg.retain(files_bg.context);}return false;}
static bool fbg_check(void){return files_background_live()||fbg_fail();}
static bool fbg_alive_hook(void*c){(void)c;return files_background_live();}
static void fbg_retain_hook(void*c){(void)c;(void)fbg_fail();}
static bool fbg_empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation;}
/* 0 admitted, 1 clean absent, -1 terminal. */
static int fbg_acquire_version(const char*name,uint32_t version,uint32_t id,risc_runtime_capability_v1*g){
 if(!fbg_check())return -1;
 if((!strcmp(name,RISC_KEY_VALUE_CAPABILITY)||!strcmp(name,RISC_SHARED_DATA_CAPABILITY)||!strcmp(name,RISC_KEY_VALUE_ATOMIC_CAPABILITY))&&files_bg.broadcast.api){
  bool loaded=files_bg.broadcast.loaded;int rc=battery_broadcast_pause(&files_bg.broadcast);
  if(rc!=BATTERY_BROADCAST_OK&&rc!=BATTERY_BROADCAST_UNAVAILABLE){fbg_fail();return -1;}
  files_bg.broadcast.loaded=loaded;files_bg.status.broadcast_valid=false;
 }
 if(!fbg_empty(g)||(g->struct_size&&g->struct_size!=sizeof(*g))){fbg_fail();return -1;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=files_bg.rt->acquire(name,version,id,g);if(!fbg_check())return -1;
 if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):!fbg_empty(g))){fbg_fail();return -1;}
 return ok?0:1;
}
static int fbg_acquire(const char*name,uint32_t id,risc_runtime_capability_v1*g){return fbg_acquire_version(name,1,id,g);}
static bool fbg_release(risc_runtime_capability_v1*g){
 if(!fbg_check())return false;
 if(fbg_empty(g))return (g->struct_size==0||g->struct_size==sizeof(*g))||fbg_fail();
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation)return fbg_fail();
 risc_runtime_capability_v1 copy=*g;bool ok=files_bg.rt->release(&copy);
 if(!fbg_check()||!ok||copy.struct_size!=sizeof(copy)||!fbg_empty(&copy))return fbg_fail();
 memset(g,0,sizeof(*g));return true;
}
static bool fbg_data_result(int rc){if(!fbg_check())return false;return (rc<=0&&rc>=RISC_APP_DATA_BUSY&&rc!=RISC_APP_DATA_CONTEXT&&rc!=RISC_APP_DATA_RETAINED)||fbg_fail();}
/* RTC provider UTC is authoritative, including clean failure. Calendar bounds
 * 2000..2099 shifted by any supported +/-24h basis span this envelope. It
 * includes the selected UTC+8 lower edge 946656000 without device inference.
 * The native realtime fallback retains its original narrower UTC range. */
static bool fbg_clock_read(uint64_t*out){
 risc_runtime_capability_v1 g={0};int ac=fbg_acquire_version("rtc.clock",2,0,&g);
 if(ac<0)return false;
 if(!ac){
  int status=risc_rtc_utc_status_v1(g.api);if(status<0)return fbg_fail();
  if(status){
   risc_rtc_utc_client_v1 client={0};if(!risc_rtc_utc_bind_v1(g.api,&client))return fbg_fail();
   risc_rtc_utc_snapshot_v1 q={.struct_size=sizeof(q)},before=q;
   bool ok=client.read(client.context,&q);if(!fbg_check())return false;
   if((!ok&&memcmp(&q,&before,sizeof(q)))||(ok&&(q.struct_size!=sizeof(q)||q.reserved)))return fbg_fail();
   if(ok&&q.epoch_seconds>=INT64_C(946598400)&&q.epoch_seconds<=INT64_C(4102531199))*out=(uint64_t)q.epoch_seconds;
   return fbg_release(&g);
  }
  if(!fbg_release(&g))return false;
 }
 ac=fbg_acquire(RISC_REALTIME_CAPABILITY,0,&g);if(ac)return ac>0;
 const risc_realtime_api_v1*p=g.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->read)return fbg_fail();
 risc_realtime_snapshot_v1 q={.struct_size=sizeof(q)},before=q;int rc=p->read(p->context,&q);
 if(!fbg_check())return false;
 if(rc!=RISC_REALTIME_OK&&rc!=RISC_REALTIME_INVALID&&rc!=RISC_REALTIME_IO)return fbg_fail();
 if(rc&&memcmp(&q,&before,sizeof(q)))return fbg_fail();
 if(!rc&&(q.struct_size!=sizeof(q)||q.reserved||q.validity>RISC_REALTIME_VALID||q.nanoseconds>=1000000000||q.nanoseconds%1000||q.monotonic_before_us>q.monotonic_after_us||(q.validity==RISC_REALTIME_UNSET&&(q.epoch_seconds||q.nanoseconds))))return fbg_fail();
 if(!rc&&q.validity==RISC_REALTIME_VALID&&q.epoch_seconds>=946684800&&q.epoch_seconds<=4102444799LL)*out=(uint64_t)q.epoch_seconds;
 return fbg_release(&g);
}
static bool files_background_clock(const risc_runtime_api_v1*rt,uint64_t*out){
 if(!out||rt!=files_bg.rt||!fbg_check())return false;
 *out=0;if(files_bg.clock_busy)return fbg_fail();
 bool previous_busy=files_bg.busy;files_bg.busy=files_bg.clock_busy=true;
 uint64_t value=0;bool ok=fbg_clock_read(&value);
 files_bg.clock_busy=false;files_bg.busy=previous_busy;
 if(ok)*out=value;
 return ok;
}
static bool fbg_fp(uint32_t op,void*q){
 if(!fbg_check())return false;
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(files_bg.contexts.api);if(!f)return false;
 bool ok=f->fingerprint(f->base.context,op,q);if(!fbg_check())return false;
 /* False also means a rejected domain request. The published status method
  * explicitly permits copied cleanup_pending inspection; do not turn a normal
  * rejected profile edit into invocation retention. */
 contexts_status_v1 st={.struct_size=sizeof(st)};
 bool inspected=files_bg.contexts.api->status(files_bg.contexts.api->context,&st);
 if(!fbg_check()||!inspected||st.struct_size!=sizeof(st)||st.state>=CONTEXTS_RETAINED||st.cleanup_pending)return fbg_fail();
 return ok;
}
static bool fbg_fingerprint_proxy(void*c,uint32_t op,void*q){(void)c;if(files_bg.busy)return fbg_fail();files_bg.busy=true;bool ok=fbg_fp(op,q);files_bg.busy=false;return ok;}
static bool fbg_pause_context(void){if(!fbg_check())return false;
 files_bg.capture_permitted=files_bg.radio_permitted=false;
 if(!files_bg.contexts.api)return true;
 bool ok=files_bg.contexts.api->pause(files_bg.contexts.api->context);return fbg_check()&&(ok||fbg_fail());}
/* Original shared namespace checkpoints, with callback fences and copied bytes.
 * An unread bank is never replaced. Dirty service state remains authoritative
 * after unknown/failed persistence; only confirmed writes receive FP_SAVED. */
static bool fbg_record(unsigned source,bool load){
 portable_contexts_client*c=&files_bg.contexts;
 contexts_fingerprint_status_v1 st={.struct_size=sizeof(st)};if(!fbg_fp(CONTEXTS_FP_STATUS,&st))return false;
 if(st.struct_size!=sizeof(st)||(st.sources&~CONTEXTS_ALL)||(st.dirty_sources&~CONTEXTS_ALL))return fbg_fail();
 if(load&&(!(st.sources&source)||(st.dirty_sources&source)))return true;
 if(!load&&!(st.dirty_sources&source))return true;
 if(!load&&(c->fingerprint_unread&source)){c->fingerprint_error=RISC_APP_DATA_INVALID;return true;}
 int ac=fbg_acquire(RISC_SHARED_DATA_CAPABILITY,source==CONTEXTS_AUDIO?2:3,&c->fingerprint_store);if(ac){if(ac>0){c->fingerprint_error=RISC_APP_DATA_UNAVAILABLE;if(load)c->fingerprint_unread|=source;}return ac>0;}
 const risc_app_data_v1*f=c->fingerprint_store.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return fbg_fail();
 uint32_t n=0;uint64_t rev=0;int rc=f->stat(f->context,"context-fingerprints.cfp",&n,&rev);if(!fbg_data_result(rc))return false;
 contexts_fingerprint_record_v1 q={.struct_size=sizeof(q),.source=source,.bytes=portable_fp_bytes,.capacity=sizeof(portable_fp_bytes)};
 if(load){
  if(!rc&&n<=sizeof(portable_fp_bytes)&&rev){uint64_t actual=0;rc=f->read(f->context,"context-fingerprints.cfp",rev,portable_fp_bytes,sizeof(portable_fp_bytes),&q.size,&actual);if(!fbg_data_result(rc))return false;
 if(!rc){if(actual!=rev||q.size!=n)rc=RISC_APP_DATA_INVALID;else if(!fbg_fp(CONTEXTS_FP_IMPORT,&q)){if(!fbg_check())return false;rc=RISC_APP_DATA_INVALID;}}}
  else if(!rc)rc=RISC_APP_DATA_INVALID;
  if(rc==RISC_APP_DATA_NOT_FOUND)rc=0;
  if(rc)c->fingerprint_unread|=source;else c->fingerprint_unread&=~source;
 }else{
  if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}
  if(!rc){if(!fbg_fp(CONTEXTS_FP_EXPORT,&q)){if(!fbg_check())return false;rc=RISC_APP_DATA_INVALID;}
 if(!rc&&q.size>sizeof(portable_fp_bytes))return fbg_fail();
 if(!rc)rc=f->replace(f->context,"context-fingerprints.cfp",rev,portable_fp_bytes,q.size);
 if(!fbg_data_result(rc))return false;
 if(!rc&&!fbg_fp(CONTEXTS_FP_SAVED,&q)){if(!fbg_check())return false;rc=RISC_APP_DATA_STALE;}}
 }
 if(rc)c->fingerprint_error=rc;
 return fbg_release(&c->fingerprint_store);
}
static bool fbg_checkpoint(bool load){
 portable_contexts_client*c=&files_bg.contexts;if(!contexts_fingerprint_api(c->api))return true;
 c->fingerprint_error=0;if(!fbg_pause_context())return false;uint64_t utc=0;if(!files_background_clock(files_bg.rt,&utc))return false;
 contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only,.utc_seconds=utc};
 return fbg_fp(CONTEXTS_FP_CONFIG,&cfg)&&fbg_record(CONTEXTS_AUDIO,load)&&fbg_record(CONTEXTS_RADIO,load);
}
static bool fbg_rules_load(void){
 portable_contexts_client*c=&files_bg.contexts;if(!contexts_fingerprint_api(c->api))return true;
 int ac=fbg_acquire(RISC_SHARED_DATA_CAPABILITY,4,&c->fingerprint_store);if(ac)return ac>0;
 const risc_app_data_v1*f=c->fingerprint_store.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return fbg_fail();
 portable_rules_request=(contexts_rules_v1){.struct_size=sizeof(portable_rules_request),.set=true};cr_init(&portable_rules_request.store);
 uint32_t n=0,got=0;uint64_t rev=0,actual=0;int rc=f->stat(f->context,"context-rules.ctx",&n,&rev);if(!fbg_data_result(rc))return false;
 if(!rc&&n==CR_FILE_BYTES&&rev){rc=f->read(f->context,"context-rules.ctx",rev,portable_rules_bytes,sizeof(portable_rules_bytes),&got,&actual);if(!fbg_data_result(rc))return false;
 if(!rc&&(actual!=rev||got!=n||!cr_decode(&portable_rules_request.store,portable_rules_bytes,got)))rc=RISC_APP_DATA_INVALID;}
 else if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}else if(!rc)rc=RISC_APP_DATA_INVALID;
 if(!fbg_release(&c->fingerprint_store))return false;
 c->rules_available=!rc;c->rules_revision=rev;portable_rules_request.available=!rc;
 return rc?true:fbg_fp(CONTEXTS_FP_RULES,&portable_rules_request);
}
static int32_t fbg_kv_get(void*context,const char*key,void*out,uint32_t cap,uint32_t*n){
 if(!fbg_check())return RISC_KEY_VALUE_CONTEXT;
 const risc_key_value_v1*p=context;int rc=p->get(p->context,key,out,cap,n);
 if(!fbg_check())return RISC_KEY_VALUE_CONTEXT;
 if(rc>0||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT){fbg_fail();return RISC_KEY_VALUE_CONTEXT;}return rc;
}
static int32_t fbg_deny_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;fbg_fail();return RISC_KEY_VALUE_CONTEXT;}
static inline int fbg_broadcast_step(battery_broadcast*s,bool allow){
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
   const risc_key_value_v1*raw=s->storage.api;if(!pqa_preferences_valid(raw))return battery_broadcast_retain(s);
   risc_key_value_v1 checked={1,sizeof(checked),(void*)raw,fbg_kv_get,fbg_deny_put};const risc_key_value_v1*kv=&checked;bool enabled=false;uint8_t bytes[4],flags=0;uint32_t n=0;
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
static bool fbg_policy(void){
 portable_contexts_client*c=&files_bg.contexts;if(!fbg_pause_context())return false;
 c->policy=(contexts_policy_v1){.struct_size=sizeof(c->policy),.sources=CONTEXTS_ALL};c->settings_valid=false;c->low_battery=true;
 int ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&c->storage);if(ac)return ac>0;
 const risc_key_value_v1*p=c->storage.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->get)return fbg_fail();
 risc_key_value_v1 kv={1,sizeof(kv),(void*)p,fbg_kv_get,fbg_deny_put};bool enabled=false,low=true;uint8_t flags=0;
 bool valid=portable_context_enabled_load(&kv,&enabled)&&portable_radio_load(&kv,&flags)&&portable_low_battery_load(&kv,&low);if(!fbg_check())return false;
 c->settings_valid=valid;c->low_battery=low;c->policy.enabled=valid&&enabled;c->policy.radio_allowed=(flags&7u)==PORTABLE_RADIO_WIFI&&!low;c->policy.audio_allowed=!low;
 if(contexts_fingerprint_api(c->api)){uint8_t sources=CONTEXTS_ALL,timing=0;uint32_t n=0;int rc=fbg_kv_get((void*)p,"context_sources",&sources,1,&n);if(!fbg_check())return false;
 if(rc!=RISC_KEY_VALUE_NOT_FOUND&&(rc||n!=1||(sources&~CONTEXTS_ALL)))sources=0;
 c->policy.sources=c->fingerprint_sources=sources;n=0;rc=fbg_kv_get((void*)p,"context_timing",&timing,1,&n);if(!fbg_check())return false;
 c->fingerprint_temporal_only=!rc&&n==1&&timing==1;}
 c->loaded=true;return fbg_release(&c->storage);
}
static bool files_background_pause(void){
 if(!files_bg.initialized)return true;
 if(!fbg_check())return false;
 if(files_bg.busy)return fbg_fail();
 files_bg.busy=true;
 bool ok=fbg_pause_context();
 if(ok){int rc=battery_broadcast_cleanup(&files_bg.broadcast);
  if(rc==BATTERY_BROADCAST_PENDING){files_bg.status.cleanup_pending=true;files_bg.busy=false;return false;}
  ok=rc==BATTERY_BROADCAST_OK||rc==BATTERY_BROADCAST_UNAVAILABLE;
 }
 files_bg.status.cleanup_pending=false;
 if(ok)ok=fbg_checkpoint(false);
 if(ok&&files_bg.broadcast.storage.api)ok=battery_broadcast_release(&files_bg.broadcast,&files_bg.broadcast.storage)==BATTERY_BROADCAST_OK;
 files_bg.status.contexts_valid=files_bg.status.broadcast_valid=false;files_bg.contexts.loaded=false;files_bg.paused=ok;files_bg.busy=false;return ok?fbg_check():fbg_fail();
}
static inline bool files_background_models_save(void){if(!files_background_pause())return false;return !files_bg.contexts.fingerprint_error;}
static inline const contexts_service_v1*files_background_service(void){if(!fbg_check()||!files_bg.contexts.api)return NULL;return contexts_fingerprint_api(files_bg.contexts.api)?&files_bg.proxy.base:files_bg.contexts.api;}
static bool files_background_init(const risc_runtime_api_v1*rt,const files_background_hooks*hooks){
 if(!hooks)return false;
 bool(*alive)(void*)=hooks->alive;void(*retain)(void*)=hooks->retain;void*ctx=hooks->context;
 if(files_bg.initialized||files_bg.retained)return fbg_fail();
 memset(&files_bg,0,sizeof(files_bg));files_bg.rt=rt;files_bg.alive=alive;files_bg.retain=retain;files_bg.context=ctx;files_bg.initialized=true;files_bg.hooks=*hooks;
 if(!fbg_check()||rt->api_version!=1||rt->struct_size<sizeof(*rt)||!rt->acquire||!rt->release||!rt->health||!rt->monotonic_ms)return fbg_fail();
 portable_contexts_client*c=&files_bg.contexts;c->runtime=rt;c->fingerprint_sources=CONTEXTS_ALL;
 int ac=fbg_acquire(CONTEXTS_SERVICE_CAPABILITY,0,&c->grant);if(ac<0)return false;
 if(!ac){if(!files_bg.hooks.observe)return fbg_fail();const contexts_service_v1*p=c->grant.api;if(p->api_version!=1||p->struct_size<CONTEXTS_SERVICE_V1_SIZE||!p->step||!p->pause||!p->status||!p->capture_audio||!p->claim_preset||!p->preset_result)return fbg_fail();c->api=p;files_bg.status.contexts_available=true;
  if(contexts_fingerprint_api(p)){files_bg.proxy=(contexts_fingerprint_service_v1){.base={.api_version=1,.struct_size=sizeof(files_bg.proxy)},.fingerprint_abi=0x31504643u,.fingerprint=fbg_fingerprint_proxy};if(!fbg_checkpoint(true)||!fbg_rules_load())return false;}}
 int rc=battery_broadcast_init(&files_bg.broadcast,rt,fbg_alive_hook,fbg_retain_hook,NULL);if(rc!=BATTERY_BROADCAST_OK&&rc!=BATTERY_BROADCAST_UNAVAILABLE)return fbg_fail();files_bg.status.broadcast_available=rc==BATTERY_BROADCAST_OK;return fbg_check();
}
static bool fbg_actions(uint32_t now,bool workflow_ready);
static bool files_background_step(const files_background_boundary*boundary){
 if(!fbg_check())return false;
 if(!boundary)return false;
 if(boundary->sharing)return files_bg.paused||fbg_fail();
 if(!boundary->settled)return true; /* PRESENTING preserves policy/capture */
 if(files_bg.busy)return fbg_fail();
 if(files_bg.status.cleanup_pending)return files_background_pause();
 files_bg.busy=true;
 portable_contexts_client*c=&files_bg.contexts;bool ok=true;uint32_t now=0;
 if(!files_bg.rt->monotonic_ms(&now)||!fbg_check())ok=false;
 if(ok&&(!files_bg.broadcast.loaded||(uint32_t)(now-files_bg.broadcast.loaded_at)>=1000u)&&!fbg_pause_context())ok=false;
 if(ok){int rc=fbg_broadcast_step(&files_bg.broadcast,boundary->broadcast_allowed);ok=rc==BATTERY_BROADCAST_OK||rc==BATTERY_BROADCAST_UNAVAILABLE;if(ok&&rc==0){rc=battery_broadcast_read(&files_bg.broadcast,&files_bg.status.broadcast);ok=rc==0;files_bg.status.broadcast_valid=ok;files_bg.status.broadcast_available=true;}}
 if(ok&&c->api){if(!c->loaded){ok=fbg_policy();c->loaded_at=now;}if(ok&&contexts_fingerprint_api(c->api)){contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only};ok=fbg_fp(CONTEXTS_FP_CONFIG,&cfg);if(ok&&(uint32_t)(now-c->fingerprint_checkpoint_at)>=60000){ok=fbg_checkpoint(false);c->fingerprint_checkpoint_at=now;}}
  if(ok){contexts_policy_v1 p=c->policy;p.awake=true;p.audio_allowed=p.audio_allowed&&boundary->audio_allowed;p.radio_allowed=p.radio_allowed&&boundary->radio_allowed;files_bg.capture_permitted=p.enabled&&p.audio_allowed;files_bg.radio_permitted=p.enabled&&p.radio_allowed;ok=c->api->step(c->api->context,&p)&&fbg_check();}
  if(ok){files_bg.status.contexts=(contexts_status_v1){.struct_size=sizeof(contexts_status_v1)};ok=c->api->status(c->api->context,&files_bg.status.contexts)&&fbg_check();if(ok&&(files_bg.status.contexts.struct_size!=sizeof(contexts_status_v1)||files_bg.status.contexts.state>=CONTEXTS_RETAINED||files_bg.status.contexts.cleanup_pending))ok=false;files_bg.status.contexts_valid=ok;}}
 if(ok)ok=fbg_actions(now,boundary->workflow_ready);
 files_bg.busy=false;files_bg.paused=false;return ok?fbg_check():fbg_fail();
}
/* Call at least every 16 ms while the root permits background work, including
 * display/text/radio loops. Capture only: no policy, persistence or RF work. */
static bool files_background_capture(void){
 if(!fbg_check())return false;
 if(files_bg.busy)return fbg_fail();
 if(!files_bg.initialized||files_bg.paused||!files_bg.contexts.api||!files_bg.contexts.loaded||!files_bg.contexts.policy.enabled||!files_bg.capture_permitted)return true;
 files_bg.busy=true;
 bool ok=files_bg.contexts.api->capture_audio(files_bg.contexts.api->context);
 if(!fbg_check())return false;
 files_bg.busy=false;
 return ok||fbg_fail();
}
static bool files_background_close(void){if(!files_background_pause())return false;
 if(!fbg_release(&files_bg.contexts.grant))return false;
 files_bg.contexts.api=NULL;files_bg.initialized=false;return fbg_check();}

/* Same conservative foreground-owner bound as the published resident adapter.
 * Pure copied state: no service call, no claim about a fresh RF sample. */
static inline bool files_background_capture_active(void){
 const portable_contexts_client*c=&files_bg.contexts;
 return !files_bg.paused&&c->loaded&&c->policy.enabled&&(files_bg.capture_permitted||files_bg.radio_permitted);
}
/* Checked write view, preserving portable encoders and namespace bytes. */
static int32_t fbg_kv_put(void*context,const char*key,const void*bytes,uint32_t n){
 if(!fbg_check())return RISC_KEY_VALUE_CONTEXT;
 const risc_key_value_v1*p=context;int rc=p->put(p->context,key,bytes,n);
 if(!fbg_check())return RISC_KEY_VALUE_CONTEXT;
 if(rc>0||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT){fbg_fail();return RISC_KEY_VALUE_CONTEXT;}return rc;
}
static bool fbg_automatic_wait(void*c){(void)c;if(!fbg_check())return true;if(!files_bg.rt->yield_ms){fbg_fail();return true;}files_bg.rt->yield_ms(1);return !fbg_check();}
static bool fbg_automatic_close(void){
 if(!fbg_check())return false;
 if(files_bg.automatic.running||files_bg.automatic.operation.token||files_bg.automatic.retained)return fbg_fail();
 return fbg_release(&files_bg.automatic_grant);
}
/* Prefer the explicit automatic writer when advertised. Only clean absence
 * permits the existing legacy namespace writer; a failed begin never falls
 * back, and neither path claims USER intent. */
/* Fence every native atomic callback before an encoder can make another call. */
static int fbg_atomic_result(int rc){
 if(!fbg_check())return RISC_ATOMIC_RETAINED;
 if(rc<RISC_ATOMIC_RETAINED||rc>RISC_ATOMIC_CANCELLED){fbg_fail();return RISC_ATOMIC_RETAINED;}
 if(rc==RISC_ATOMIC_RETAINED)fbg_fail();
 return rc;
}
static int32_t fbg_atomic_state(void*ctx,risc_key_value_atomic_state_v1*out){
 (void)ctx;if(!fbg_check())return RISC_ATOMIC_RETAINED;
 const risc_key_value_atomic_v1*p=files_bg.automatic_source;int rc=fbg_atomic_result(p->state(p->context,out));
 if(!rc){if(out->struct_size<sizeof(*out)||out->reserved||(out->flags&3u)!=3u){fbg_fail();return RISC_ATOMIC_RETAINED;}out->flags|=RISC_PREFERENCE_STORE_COPIED_INPUT|RISC_PREFERENCE_STORE_ATOMIC_GROUP;}
 return rc;
}
static int32_t fbg_atomic_get(void*ctx,uint64_t rev,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)ctx;if(!fbg_check())return RISC_ATOMIC_RETAINED;
 const risc_key_value_atomic_v1*p=files_bg.automatic_source;int rc=fbg_atomic_result(p->get(p->context,rev,k,b,cap,n));
 if((!rc&&*n>cap)||(rc==RISC_ATOMIC_NOT_FOUND&&*n)){fbg_fail();return RISC_ATOMIC_RETAINED;}return rc;
}
static int32_t fbg_atomic_submit(void*ctx,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*i,uint32_t n,uint64_t*t){
 (void)ctx;if(!fbg_check())return RISC_ATOMIC_RETAINED;
 const risc_key_value_atomic_v1*p=files_bg.automatic_source;return fbg_atomic_result(p->submit(p->context,rev,m,count,i,n,t));
}
static int32_t fbg_atomic_collect(void*ctx,uint64_t token,risc_key_value_atomic_receipt_v1*out){
 (void)ctx;if(!fbg_check())return RISC_ATOMIC_RETAINED;
 const risc_key_value_atomic_v1*p=files_bg.automatic_source;return fbg_atomic_result(p->collect(p->context,token,out));
}
static int32_t fbg_atomic_cancel(void*ctx,uint64_t token){
 (void)ctx;if(!fbg_check())return RISC_ATOMIC_RETAINED;
 const risc_key_value_atomic_v1*p=files_bg.automatic_source;return fbg_atomic_result(p->cancel(p->context,token));
}
static bool fbg_settings_open(unsigned boundary,risc_key_value_v1*view){
 memset(view,0,sizeof(*view));if(!fbg_pause_context())return false;
 int ac=fbg_acquire(RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,&files_bg.automatic_grant);
 if(ac<0)return false;
 if(!ac){
  files_bg.automatic_source=files_bg.automatic_grant.api;
  if(!portable_preference_atomic_api_valid(files_bg.automatic_source))return fbg_fail();
  files_bg.automatic_view=(risc_key_value_atomic_v1){1,sizeof(files_bg.automatic_view),NULL,fbg_atomic_state,fbg_atomic_get,fbg_atomic_submit,fbg_atomic_collect,fbg_atomic_cancel};
  int rc=portable_preference_automatic_begin(&files_bg.automatic,&files_bg.automatic_view,boundary,fbg_alive_hook,NULL,fbg_automatic_wait,NULL);
  if(!fbg_check()||rc==RISC_ATOMIC_RETAINED)return fbg_fail();
  if(rc!=PORTABLE_PREFERENCE_READY)return fbg_automatic_close();
  *view=files_bg.automatic.view;return true;
 }
 ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&files_bg.contexts.storage);if(ac)return ac>0;
 const risc_key_value_v1*p=files_bg.contexts.storage.api;
 if(!pqa_preferences_valid(p))return fbg_fail();
 *view=(risc_key_value_v1){1,sizeof(*view),(void*)p,fbg_kv_get,fbg_kv_put};return true;
}
static bool fbg_settings_close(void){return fbg_automatic_close()&&fbg_release(&files_bg.contexts.storage);}
static void fbg_effect(unsigned effect){files_bg.status.pending_effects|=1u<<effect;}
static void fbg_setting_effect(unsigned field){
 if(field==CR_BRIGHTNESS)fbg_effect(RISC_SETTINGS_BRIGHTNESS);
 if(field==CR_WIFI)fbg_effect(RISC_SETTINGS_WIFI);
 if(field==CR_BLUETOOTH)fbg_effect(RISC_SETTINGS_BLUETOOTH);
 if(field==CR_VOLUME||field==CR_DND||field==CR_NOTIFY)fbg_effect(RISC_SETTINGS_ALARM);
}
static bool fbg_room(void){
 portable_contexts_client*c=&files_bg.contexts;
 if(!c->settings_valid||!c->policy.enabled)return true;
 if(c->preset_checked&&c->preset_checked_at==c->loaded_at)return true;
 c->preset_checked=true;c->preset_checked_at=c->loaded_at;
 const contexts_status_v1*status=&files_bg.status.contexts;
 const contexts_source_status_v1*room=NULL;
 if(status->audio.current&&status->audio.room_valid&&!status->audio.room_ambiguous)room=&status->audio;
 if(status->radio.current&&status->radio.room_valid&&!status->radio.room_ambiguous){
  if(room&&strncmp(room->room_name,status->radio.room_name,CONTEXTS_NAME_SIZE))return true;
  if(!room)room=&status->radio;
 }
 if(!room||room->room_slot<0||room->room_slot>=8||!memchr(room->room_name,0,CONTEXTS_NAME_SIZE))return true;
 if(room->room_entry&&room->room_entry==room->preset_entry)return true;
 contexts_source_status_v1 selected_room=*room;
 if(!fbg_pause_context())return false;
 int ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&c->storage);if(ac)return ac>0;
 const risc_key_value_v1*p=c->storage.api;if(!pqa_preferences_valid(p))return fbg_fail();
 risc_key_value_v1 view={1,sizeof(view),(void*)p,fbg_kv_get,fbg_kv_put};
 portable_context_preset selected;portable_context_preset_init(&selected);unsigned matches=0;bool readable=true;
 for(unsigned i=0;i<PORTABLE_CONTEXT_PRESETS;i++){
  portable_context_preset preset;int rc=portable_context_preset_load(&view,i,&preset);if(!fbg_check())return false;
  if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND){readable=false;break;}
  if(!rc&&portable_context_preset_matches(&preset,selected_room.source,(unsigned)selected_room.room_slot,selected_room.room_name)){selected=preset;matches++;}
 }
 if(!fbg_release(&c->storage))return false;
 if(!readable||matches!=1)return true;
 bool claimed=c->api->claim_preset(c->api->context,selected_room.source,(uint32_t)selected_room.room_slot,selected_room.room_name,selected_room.model_generation);
 if(!fbg_check())return false;
 if(!claimed)return true;
 if(!fbg_settings_open(PORTABLE_AUTOMATIC_HOME_PRESET,&view))return false;
 uint16_t applied=0;portable_context_preset permitted=selected;permitted.actions&=files_bg.hooks.action_mask;
 if(selected.alert&~files_bg.hooks.alert_modes)permitted.actions&=~PORTABLE_CONTEXT_ALERT;
 bool ok=portable_contexts_apply_preferences(&view,&permitted,c->low_battery,&applied,files_bg.hooks.face_save);
 if(!fbg_check()||!fbg_settings_close())return false;
 bool reported=c->api->preset_result(c->api->context,selected_room.source,selected_room.model_generation,ok&&applied==selected.actions?CONTEXTS_PRESET_APPLIED:CONTEXTS_PRESET_PARTIAL);
 if(!fbg_check()||!reported)return fbg_fail();
 if(applied&PORTABLE_CONTEXT_BRIGHTNESS)fbg_effect(RISC_SETTINGS_BRIGHTNESS);
 if(applied&(PORTABLE_CONTEXT_VOLUME|PORTABLE_CONTEXT_DND|PORTABLE_CONTEXT_ALERT))fbg_effect(RISC_SETTINGS_ALARM);
 c->loaded=false;return true;
}
/* Timer commit uses the native alarm namespace and refuses an existing live
 * countdown. Refresh is queued to Home after the workflow result is reported. */
static bool fbg_timer(uint32_t duration,bool*out){
 *out=false;if(!duration||duration>ALARM_COUNTDOWN_MAX)return true;
 risc_runtime_capability_v1 service={0};int ac=fbg_acquire(ALARM_SERVICE_CAPABILITY,0,&service);if(ac)return ac>0;
 const alarm_service_v1*a=service.api;if(a->api_version!=1||a->struct_size<sizeof(*a)||!a->status)return fbg_fail();
 alarm_status_v1 st={.struct_size=sizeof(st)};int rc=a->status(a->context,&st);if(!fbg_check())return false;
 if(rc<ALARM_FOREGROUND||rc>ALARM_PENDING||rc==ALARM_OUTPUT)return fbg_fail();
 if(!rc&&(st.struct_size!=sizeof(st)||st.api_version!=1||st.state>ALARM_STATE_CUE||st.output_uncertain))return fbg_fail();
 if(!fbg_release(&service))return false;
 if(rc||st.state!=ALARM_STATE_READY||!st.rtc_seconds||st.rtc_seconds>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS-duration)return true;
 risc_runtime_capability_v1*g=&files_bg.contexts.storage;ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,3,g);if(ac)return ac>0;
 const risc_key_value_v1*p=g->api;if(!pqa_preferences_valid(p))return fbg_fail();
 risc_key_value_v1 view={1,sizeof(view),(void*)p,fbg_kv_get,fbg_kv_put};alarm_writer writer={0};
 bool ok=!alarm_writer_load(&writer,&view,2)&&writer.saved.revision<UINT32_MAX;
 if(!fbg_check())return false;
 if(ok&&writer.saved.enabled&&writer.saved.deadline>st.rtc_seconds)ok=false;
 if(ok){alarm_config config={.revision=writer.saved.revision+1,.deadline=st.rtc_seconds+duration,.created=st.rtc_seconds,.duration=duration,.kind=2,.enabled=1};ok=!alarm_writer_commit(&writer,&view,&config);}
 if(!fbg_check()||!fbg_release(g))return false;
 *out=ok;return true;
}
static bool fbg_actions(uint32_t now,bool workflow_ready){
 portable_contexts_client*c=&files_bg.contexts;if(!c->api||files_bg.status.pending_effects)return true;
 if(!contexts_fingerprint_api(c->api)||!c->rules_available)return fbg_room();
 if(files_bg.action_checked_valid&&(uint32_t)(now-files_bg.action_checked)<500u)return true;
 files_bg.action_checked=now;files_bg.action_checked_valid=true;
 contexts_evaluate_v1 q={.struct_size=sizeof(q)};q.observation.enabled=c->policy.enabled&&c->settings_valid;
 if(files_bg.hooks.observe){files_background_observation o={0};bool ok=files_bg.hooks.observe(files_bg.context,&o);if(!fbg_check())return false;if(!ok)return false;
  if((o.battery_valid&&o.battery_percent>100)||(o.local_time_valid&&o.local_hour>=24))return fbg_fail();
  q.observation.battery_valid=o.battery_valid;q.observation.battery=o.battery_percent;q.observation.charging=o.charging;q.observation.time_valid=o.local_time_valid;q.observation.hour=o.local_hour;
 }
 if(!fbg_fp(CONTEXTS_FP_EVALUATE,&q))return false;
 if(q.struct_size!=sizeof(q)||(q.result.changed&~((1u<<CR_SETTINGS)-1u)))return fbg_fail();
 if(q.result.changed){
  risc_key_value_v1 view;uint16_t success=0;if(!fbg_settings_open(PORTABLE_AUTOMATIC_HOME_RULES,&view))return false;
  for(unsigned f=0;f<CR_SETTINGS;f++)if(q.result.changed&(1u<<f)){
   if(q.result.winners[f]<0||(unsigned)q.result.winners[f]>=CR_RULES)return fbg_fail();
   bool ok=portable_context_setting(&view,f,q.result.settings[f],c->low_battery,files_bg.hooks.face_save);
   if(!fbg_check())return false;
   if(ok)success|=1u<<f;
  }
  if(!fbg_settings_close())return false;
  for(unsigned i=0;i<CR_RULES;i++){bool changed=false,complete=true;
   for(unsigned f=0;f<CR_SETTINGS;f++)if(q.result.winners[f]==(int)i&&(q.result.changed&(1u<<f))){changed=true;complete&=(success&(1u<<f))!=0;}
   if(changed){contexts_workflow_v1 report={.struct_size=sizeof(report),.rule=i,.result=complete?CR_APPLIED:CR_PARTIAL};if(!fbg_fp(CONTEXTS_FP_WORKFLOW,&report))return false;}
  }
  for(unsigned f=0;f<CR_SETTINGS;f++)if(success&(1u<<f))fbg_setting_effect(f);
  c->loaded=false;
 }
 if(!workflow_ready)return true;
 contexts_workflow_v1 action={.struct_size=sizeof(action),.claim=true};if(!fbg_fp(CONTEXTS_FP_WORKFLOW,&action))return false;
 if(action.available){
  if(action.struct_size!=sizeof(action)||action.rule>=CR_RULES||action.index>=CR_STEPS||!memchr(action.step.text,0,sizeof(action.step.text)))return fbg_fail();
  if(!fbg_pause_context())return false;
  bool ok=false,timer=false;
  if(action.step.kind==CR_MESSAGE)ok=files_bg.hooks.message&&files_bg.hooks.message(files_bg.context,action.step.text);
  else if(action.step.kind==CR_LAUNCH)ok=files_bg.hooks.launch&&files_bg.hooks.launch(files_bg.context,action.step.text);
  else if(action.step.kind==CR_TIMER){if(!fbg_timer(action.step.value,&ok))return false;timer=ok;}
  if(!fbg_check())return false;
  action.claim=false;action.result=ok?CR_RAN:CR_FAILED;if(!fbg_fp(CONTEXTS_FP_WORKFLOW,&action))return false;
  if(timer)fbg_effect(RISC_SETTINGS_ALARM);
 }
 return true;
}
static inline files_background_status files_background_status_copy(void){
 files_background_status status=files_bg.status;status.paused=files_bg.paused;
 status.capture_active=!status.paused&&files_background_capture_active();
 status.automatic_work=status.cleanup_pending||!!status.pending_effects||files_bg.automatic.running||!!files_bg.automatic.operation.token;
 return status;
}
static inline bool files_background_before_storage(bool write){
 bool loaded=files_bg.contexts.loaded,bloaded=files_bg.broadcast.loaded;
 if(!files_background_pause())return false;
 if(!write){files_bg.contexts.loaded=loaded;files_bg.broadcast.loaded=bloaded;}
 return true;
}
static inline bool files_background_effect_done(unsigned effect,unsigned outcome){
 if(!fbg_check())return false;
 if(files_bg.busy)return fbg_fail();
 if(effect<1||effect>4||outcome<1||outcome>3||!(files_bg.status.pending_effects&(1u<<effect)))return false;
 if(outcome!=RISC_SETTINGS_PENDING)files_bg.status.pending_effects&=~(1u<<effect);
 return true;
}
