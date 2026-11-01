#pragma once
/* Invocation-local BLE-foreground background. Published client data/encodings,
 * native Runtime authority; Context audio only, no RF or automatic presets.
 * BLE advertising remains paused while this foreground app owns the invocation.
 * No Runtime proxy: only copied KV readers and fingerprint API are fenced. */
#include <stdbool.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscRealtimeV1.h"
#include "RiscRtcUtcV1.h"
static bool scanner_background_live(void);
#define PORTABLE_CONTEXTS_LIVE() scanner_background_live()
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() scanner_background_live()
/* The published client's legacy RTC helper is not called. Our public clock
 * uses the generic reader below and does not select a physical clock backend. */
#define portable_fingerprint_clock scanner_background_unused_legacy_clock
#include "PortableContextsClient.h"
#undef portable_fingerprint_clock
#include "battery_broadcast.h"
typedef struct {bool contexts_available,broadcast_available,contexts_valid,broadcast_valid,retained;contexts_status_v1 contexts;telemetry_broadcast_status_v1 broadcast;} scanner_background_status;
static struct {
 const risc_runtime_api_v1*rt;bool(*alive)(void*);void(*retain)(void*);void*context;
 bool initialized,retained,busy,paused,clock_busy;portable_contexts_client contexts;
 battery_broadcast broadcast;contexts_fingerprint_service_v1 proxy;
 scanner_background_status status;
} scanner_bg;
static bool scanner_background_live(void){return !scanner_bg.retained&&scanner_bg.rt&&scanner_bg.alive&&risc_runtime_get_api(1)==scanner_bg.rt&&scanner_bg.alive(scanner_bg.context);}
static bool scb_fail(void){if(!scanner_bg.retained){scanner_bg.retained=true;scanner_bg.status.retained=true;if(scanner_bg.retain)scanner_bg.retain(scanner_bg.context);}return false;}
static bool scb_check(void){return scanner_background_live()||scb_fail();}
static bool scb_alive_hook(void*c){(void)c;return scanner_background_live();}
static void scb_retain_hook(void*c){(void)c;(void)scb_fail();}
static bool scb_empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation;}
/* 0 admitted, 1 clean absent, -1 terminal. */
static int scb_acquire_version(const char*name,uint32_t version,uint32_t id,risc_runtime_capability_v1*g){
 if(!scb_check())return -1;
 if(!scb_empty(g)||(g->struct_size&&g->struct_size!=sizeof(*g))){scb_fail();return -1;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=scanner_bg.rt->acquire(name,version,id,g);if(!scb_check())return -1;
 if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):!scb_empty(g))){scb_fail();return -1;}
 return ok?0:1;
}
static int scb_acquire(const char*name,uint32_t id,risc_runtime_capability_v1*g){return scb_acquire_version(name,1,id,g);}
static bool scb_release(risc_runtime_capability_v1*g){
 if(!scb_check())return false;
 if(scb_empty(g))return (g->struct_size==0||g->struct_size==sizeof(*g))||scb_fail();
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation)return scb_fail();
 risc_runtime_capability_v1 copy=*g;bool ok=scanner_bg.rt->release(&copy);
 if(!scb_check()||!ok||copy.struct_size!=sizeof(copy)||!scb_empty(&copy))return scb_fail();
 memset(g,0,sizeof(*g));return true;
}
static bool scb_data_result(int rc){if(!scb_check())return false;return (rc<=0&&rc>=RISC_APP_DATA_BUSY&&rc!=RISC_APP_DATA_CONTEXT&&rc!=RISC_APP_DATA_RETAINED)||scb_fail();}
/* RTC provider UTC is authoritative, including clean failure. Calendar bounds
 * 2000..2099 shifted by any supported +/-24h basis span this envelope. It
 * includes the selected UTC+8 lower edge 946656000 without device inference.
 * The native realtime fallback retains its original narrower UTC range. */
static bool scb_clock_read(uint64_t*out){
 risc_runtime_capability_v1 g={0};int ac=scb_acquire_version("rtc.clock",2,0,&g);
 if(ac<0)return false;
 if(!ac){
  int status=risc_rtc_utc_status_v1(g.api);if(status<0)return scb_fail();
  if(status){
   risc_rtc_utc_client_v1 client={0};if(!risc_rtc_utc_bind_v1(g.api,&client))return scb_fail();
   risc_rtc_utc_snapshot_v1 q={.struct_size=sizeof(q)},before=q;
   bool ok=client.read(client.context,&q);if(!scb_check())return false;
   if((!ok&&memcmp(&q,&before,sizeof(q)))||(ok&&(q.struct_size!=sizeof(q)||q.reserved)))return scb_fail();
   if(ok&&q.epoch_seconds>=INT64_C(946598400)&&q.epoch_seconds<=INT64_C(4102531199))*out=(uint64_t)q.epoch_seconds;
   return scb_release(&g);
  }
  if(!scb_release(&g))return false;
 }
 ac=scb_acquire(RISC_REALTIME_CAPABILITY,0,&g);if(ac)return ac>0;
 const risc_realtime_api_v1*p=g.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->read)return scb_fail();
 risc_realtime_snapshot_v1 q={.struct_size=sizeof(q)},before=q;int rc=p->read(p->context,&q);
 if(!scb_check())return false;
 if(rc!=RISC_REALTIME_OK&&rc!=RISC_REALTIME_INVALID&&rc!=RISC_REALTIME_IO)return scb_fail();
 if(rc&&memcmp(&q,&before,sizeof(q)))return scb_fail();
 if(!rc&&(q.struct_size!=sizeof(q)||q.reserved||q.validity>RISC_REALTIME_VALID||q.nanoseconds>=1000000000||q.nanoseconds%1000||q.monotonic_before_us>q.monotonic_after_us||(q.validity==RISC_REALTIME_UNSET&&(q.epoch_seconds||q.nanoseconds))))return scb_fail();
 if(!rc&&q.validity==RISC_REALTIME_VALID&&q.epoch_seconds>=946684800&&q.epoch_seconds<=4102444799LL)*out=(uint64_t)q.epoch_seconds;
 return scb_release(&g);
}
static bool scanner_background_clock(const risc_runtime_api_v1*rt,uint64_t*out){
 if(!out||rt!=scanner_bg.rt||!scb_check())return false;
 *out=0;if(scanner_bg.clock_busy)return scb_fail();
 bool previous_busy=scanner_bg.busy;scanner_bg.busy=scanner_bg.clock_busy=true;
 uint64_t value=0;bool ok=scb_clock_read(&value);
 scanner_bg.clock_busy=false;scanner_bg.busy=previous_busy;
 if(ok)*out=value;
 return ok;
}
static bool scb_fp(uint32_t op,void*q){
 if(!scb_check())return false;
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(scanner_bg.contexts.api);if(!f)return false;
 bool ok=f->fingerprint(f->base.context,op,q);if(!scb_check())return false;
 /* False also means a rejected domain request. The published status method
  * explicitly permits copied cleanup_pending inspection; do not turn a normal
  * rejected profile edit into invocation retention. */
 contexts_status_v1 st={.struct_size=sizeof(st)};
 bool inspected=scanner_bg.contexts.api->status(scanner_bg.contexts.api->context,&st);
 if(!scb_check()||!inspected||st.struct_size!=sizeof(st)||st.cleanup_pending)return scb_fail();
 return ok;
}
static bool scb_fingerprint_proxy(void*c,uint32_t op,void*q){(void)c;if(scanner_bg.busy)return scb_fail();scanner_bg.busy=true;bool ok=scb_fp(op,q);scanner_bg.busy=false;return ok;}
static bool scb_pause_context(void){if(!scb_check())return false;
 if(!scanner_bg.contexts.api)return true;
 bool ok=scanner_bg.contexts.api->pause(scanner_bg.contexts.api->context);return scb_check()&&(ok||scb_fail());}
/* Original shared namespace checkpoints, with callback fences and copied bytes.
 * An unread bank is never replaced. Dirty service state remains authoritative
 * after unknown/failed persistence; only confirmed writes receive FP_SAVED. */
static bool scb_record(unsigned source,bool load){
 portable_contexts_client*c=&scanner_bg.contexts;
 contexts_fingerprint_status_v1 st={.struct_size=sizeof(st)};if(!scb_fp(CONTEXTS_FP_STATUS,&st))return false;
 if(st.struct_size!=sizeof(st)||(st.sources&~CONTEXTS_ALL)||(st.dirty_sources&~CONTEXTS_ALL))return scb_fail();
 if(load&&(!(st.sources&source)||(st.dirty_sources&source)))return true;
 if(!load&&!(st.dirty_sources&source))return true;
 if(!load&&(c->fingerprint_unread&source)){c->fingerprint_error=RISC_APP_DATA_INVALID;return true;}
 int ac=scb_acquire(RISC_SHARED_DATA_CAPABILITY,source==CONTEXTS_AUDIO?2:3,&c->fingerprint_store);if(ac){if(ac>0){c->fingerprint_error=RISC_APP_DATA_UNAVAILABLE;if(load)c->fingerprint_unread|=source;}return ac>0;}
 const risc_app_data_v1*f=c->fingerprint_store.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return scb_fail();
 uint32_t n=0;uint64_t rev=0;int rc=f->stat(f->context,"context-fingerprints.cfp",&n,&rev);if(!scb_data_result(rc))return false;
 contexts_fingerprint_record_v1 q={.struct_size=sizeof(q),.source=source,.bytes=portable_fp_bytes,.capacity=sizeof(portable_fp_bytes)};
 if(load){
  if(!rc&&n<=sizeof(portable_fp_bytes)&&rev){uint64_t actual=0;rc=f->read(f->context,"context-fingerprints.cfp",rev,portable_fp_bytes,sizeof(portable_fp_bytes),&q.size,&actual);if(!scb_data_result(rc))return false;
 if(!rc){if(actual!=rev||q.size!=n)rc=RISC_APP_DATA_INVALID;else if(!scb_fp(CONTEXTS_FP_IMPORT,&q)){if(!scb_check())return false;rc=RISC_APP_DATA_INVALID;}}}
  else if(!rc)rc=RISC_APP_DATA_INVALID;
  if(rc==RISC_APP_DATA_NOT_FOUND)rc=0;
  if(rc)c->fingerprint_unread|=source;else c->fingerprint_unread&=~source;
 }else{
  if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}
  if(!rc){if(!scb_fp(CONTEXTS_FP_EXPORT,&q)){if(!scb_check())return false;rc=RISC_APP_DATA_INVALID;}
 if(!rc&&q.size>sizeof(portable_fp_bytes))return scb_fail();
 if(!rc)rc=f->replace(f->context,"context-fingerprints.cfp",rev,portable_fp_bytes,q.size);
 if(!scb_data_result(rc))return false;
 if(!rc&&!scb_fp(CONTEXTS_FP_SAVED,&q)){if(!scb_check())return false;rc=RISC_APP_DATA_STALE;}}
 }
 if(rc)c->fingerprint_error=rc;
 return scb_release(&c->fingerprint_store);
}
static bool scb_checkpoint(bool load){
 portable_contexts_client*c=&scanner_bg.contexts;if(!contexts_fingerprint_api(c->api))return true;
 c->fingerprint_error=0;if(!scb_pause_context())return false;uint64_t utc=0;if(!scanner_background_clock(scanner_bg.rt,&utc))return false;
 contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only,.utc_seconds=utc};
 return scb_fp(CONTEXTS_FP_CONFIG,&cfg)&&scb_record(CONTEXTS_AUDIO,load)&&scb_record(CONTEXTS_RADIO,load);
}
static bool scb_rules_load(void){
 portable_contexts_client*c=&scanner_bg.contexts;if(!contexts_fingerprint_api(c->api))return true;
 int ac=scb_acquire(RISC_SHARED_DATA_CAPABILITY,4,&c->fingerprint_store);if(ac)return ac>0;
 const risc_app_data_v1*f=c->fingerprint_store.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return scb_fail();
 portable_rules_request=(contexts_rules_v1){.struct_size=sizeof(portable_rules_request),.set=true};cr_init(&portable_rules_request.store);
 uint32_t n=0,got=0;uint64_t rev=0,actual=0;int rc=f->stat(f->context,"context-rules.ctx",&n,&rev);if(!scb_data_result(rc))return false;
 if(!rc&&n==CR_FILE_BYTES&&rev){rc=f->read(f->context,"context-rules.ctx",rev,portable_rules_bytes,sizeof(portable_rules_bytes),&got,&actual);if(!scb_data_result(rc))return false;
 if(!rc&&(actual!=rev||got!=n||!cr_decode(&portable_rules_request.store,portable_rules_bytes,got)))rc=RISC_APP_DATA_INVALID;}
 else if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}else if(!rc)rc=RISC_APP_DATA_INVALID;
 if(!scb_release(&c->fingerprint_store))return false;
 c->rules_available=!rc;c->rules_revision=rev;portable_rules_request.available=!rc;
 return rc?true:scb_fp(CONTEXTS_FP_RULES,&portable_rules_request);
}
static int32_t scb_kv_get(void*context,const char*key,void*out,uint32_t cap,uint32_t*n){
 if(!scb_check())return RISC_KEY_VALUE_CONTEXT;
 const risc_key_value_v1*p=context;int rc=p->get(p->context,key,out,cap,n);
 if(!scb_check())return RISC_KEY_VALUE_CONTEXT;
 if(rc>0||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT){scb_fail();return RISC_KEY_VALUE_CONTEXT;}return rc;
}
static int32_t scb_deny_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;scb_fail();return RISC_KEY_VALUE_CONTEXT;}
static bool scb_policy(void){
 portable_contexts_client*c=&scanner_bg.contexts;if(!scb_pause_context())return false;
 c->policy=(contexts_policy_v1){.struct_size=sizeof(c->policy),.sources=CONTEXTS_ALL};c->settings_valid=false;c->low_battery=true;
 int ac=scb_acquire(RISC_KEY_VALUE_CAPABILITY,1,&c->storage);if(ac)return ac>0;
 const risc_key_value_v1*p=c->storage.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->get)return scb_fail();
 risc_key_value_v1 kv={1,sizeof(kv),(void*)p,scb_kv_get,scb_deny_put};bool enabled=false,low=true;uint8_t flags=0;
 bool valid=portable_context_enabled_load(&kv,&enabled)&&portable_radio_load(&kv,&flags)&&portable_low_battery_load(&kv,&low);if(!scb_check())return false;
 c->settings_valid=valid;c->low_battery=low;c->policy.enabled=valid&&enabled;c->policy.radio_allowed=false;c->policy.audio_allowed=!low;
 if(contexts_fingerprint_api(c->api)){uint8_t sources=CONTEXTS_ALL,timing=0;uint32_t n=0;int rc=scb_kv_get((void*)p,"context_sources",&sources,1,&n);if(!scb_check())return false;
 if(rc!=RISC_KEY_VALUE_NOT_FOUND&&(rc||n!=1||(sources&~CONTEXTS_ALL)))sources=0;
 c->policy.sources=c->fingerprint_sources=sources;n=0;rc=scb_kv_get((void*)p,"context_timing",&timing,1,&n);if(!scb_check())return false;
 c->fingerprint_temporal_only=!rc&&n==1&&timing==1;}
 c->loaded=true;return scb_release(&c->storage);
}
static bool scanner_background_pause(void){
 if(!scanner_bg.initialized)return true;
 if(!scb_check())return false;
 if(scanner_bg.busy)return scb_fail();
 scanner_bg.busy=true;
 bool ok=scb_pause_context()&&scb_checkpoint(false);if(ok&&scanner_bg.broadcast.api){int rc=battery_broadcast_pause(&scanner_bg.broadcast);ok=rc==BATTERY_BROADCAST_OK||rc==BATTERY_BROADCAST_UNAVAILABLE;}
 if(ok&&scanner_bg.broadcast.storage.api)ok=battery_broadcast_release(&scanner_bg.broadcast,&scanner_bg.broadcast.storage)==BATTERY_BROADCAST_OK;
 scanner_bg.status.contexts_valid=scanner_bg.status.broadcast_valid=false;scanner_bg.contexts.loaded=false;scanner_bg.paused=ok;scanner_bg.busy=false;return ok?scb_check():scb_fail();
}
static inline bool scanner_background_models_save(void){if(!scanner_background_pause())return false;return !scanner_bg.contexts.fingerprint_error;}
static inline const contexts_service_v1*scanner_background_service(void){if(!scb_check()||!scanner_bg.contexts.api)return NULL;return contexts_fingerprint_api(scanner_bg.contexts.api)?&scanner_bg.proxy.base:scanner_bg.contexts.api;}
static bool scanner_background_init(const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*ctx){
 if(scanner_bg.initialized||scanner_bg.retained)return scb_fail();
 memset(&scanner_bg,0,sizeof(scanner_bg));scanner_bg.rt=rt;scanner_bg.alive=alive;scanner_bg.retain=retain;scanner_bg.context=ctx;scanner_bg.initialized=true;
 if(!scb_check()||rt->api_version!=1||rt->struct_size<sizeof(*rt)||!rt->acquire||!rt->release||!rt->health||!rt->monotonic_ms)return scb_fail();
 portable_contexts_client*c=&scanner_bg.contexts;c->runtime=rt;c->fingerprint_sources=CONTEXTS_ALL;
 int ac=scb_acquire(CONTEXTS_SERVICE_CAPABILITY,0,&c->grant);if(ac<0)return false;
 if(!ac){const contexts_service_v1*p=c->grant.api;if(p->api_version!=1||p->struct_size<CONTEXTS_SERVICE_V1_SIZE||!p->step||!p->pause||!p->status||!p->capture_audio)return scb_fail();c->api=p;scanner_bg.status.contexts_available=true;
  if(contexts_fingerprint_api(p)){scanner_bg.proxy=(contexts_fingerprint_service_v1){.base={.api_version=1,.struct_size=sizeof(scanner_bg.proxy)},.fingerprint_abi=0x31504643u,.fingerprint=scb_fingerprint_proxy};if(!scb_checkpoint(true)||!scb_rules_load())return false;}}
 int rc=battery_broadcast_init(&scanner_bg.broadcast,rt,scb_alive_hook,scb_retain_hook,NULL);if(rc!=BATTERY_BROADCAST_OK&&rc!=BATTERY_BROADCAST_UNAVAILABLE)return scb_fail();scanner_bg.status.broadcast_available=rc==BATTERY_BROADCAST_OK;return scb_check();
}
static bool scanner_background_step(bool allow){
 if(!allow)return scanner_background_pause();
 if(!scb_check())return false;
 if(scanner_bg.busy)return scb_fail();
 scanner_bg.busy=true;
 portable_contexts_client*c=&scanner_bg.contexts;bool ok=true;uint32_t now=0;
 if(!scanner_bg.rt->monotonic_ms(&now)||!scb_check())ok=false;
 if(ok&&c->api){if(!c->loaded)ok=scb_policy();if(ok&&contexts_fingerprint_api(c->api)){contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only};ok=scb_fp(CONTEXTS_FP_CONFIG,&cfg);if(ok&&(uint32_t)(now-c->fingerprint_checkpoint_at)>=60000){ok=scb_checkpoint(false);c->fingerprint_checkpoint_at=now;}}
  if(ok){contexts_policy_v1 p=c->policy;p.awake=true;p.radio_allowed=false;ok=c->api->step(c->api->context,&p)&&scb_check();}
  if(ok){scanner_bg.status.contexts=(contexts_status_v1){.struct_size=sizeof(contexts_status_v1)};ok=c->api->status(c->api->context,&scanner_bg.status.contexts)&&scb_check();if(ok&&(scanner_bg.status.contexts.struct_size!=sizeof(contexts_status_v1)||scanner_bg.status.contexts.cleanup_pending))ok=false;scanner_bg.status.contexts_valid=ok;}}
 if(ok){int rc=battery_broadcast_step(&scanner_bg.broadcast,false);ok=rc==BATTERY_BROADCAST_OK||rc==BATTERY_BROADCAST_UNAVAILABLE;if(ok&&rc==0){rc=battery_broadcast_read(&scanner_bg.broadcast,&scanner_bg.status.broadcast);ok=rc==0;scanner_bg.status.broadcast_valid=ok;scanner_bg.status.broadcast_available=true;}}
 scanner_bg.busy=false;scanner_bg.paused=false;return ok?scb_check():scb_fail();
}
/* Call at least every 16 ms while the root permits background work, including
 * display/text/radio loops. Capture only: no policy, persistence or RF work. */
static bool scanner_background_capture(void){
 if(!scb_check())return false;
 if(scanner_bg.busy)return scb_fail();
 if(!scanner_bg.initialized||scanner_bg.paused||!scanner_bg.contexts.api||!scanner_bg.contexts.loaded||!scanner_bg.contexts.policy.enabled||!scanner_bg.contexts.policy.audio_allowed)return true;
 scanner_bg.busy=true;
 bool ok=scanner_bg.contexts.api->capture_audio(scanner_bg.contexts.api->context);
 if(!scb_check())return false;
 scanner_bg.busy=false;
 return ok||scb_fail();
}
static bool scanner_background_close(void){if(!scanner_background_pause())return false;
 if(!scb_release(&scanner_bg.contexts.grant))return false;
 scanner_bg.contexts.api=NULL;scanner_bg.initialized=false;return scb_check();}
static inline scanner_background_status scanner_background_status_copy(void){return scanner_bg.status;}
/* Same conservative foreground-owner bound as the published resident adapter.
 * Pure copied state: no service call, no claim about a fresh RF sample. */
static inline bool scanner_background_capture_active(void){
 const portable_contexts_client*c=&scanner_bg.contexts;
 return c->loaded&&c->policy.enabled&&(c->policy.audio_allowed||c->policy.radio_allowed);
}
