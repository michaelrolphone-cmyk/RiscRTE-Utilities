#pragma once
/* Invocation-local Audio-session background. Published client data/encodings,
 * native Runtime authority; no automatic presets and never background audio.
 * No Runtime proxy: only copied KV readers and fingerprint API are fenced. */
#include <stdbool.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscRealtimeV1.h"
static bool spectrum_scene_background_live(void);
#define PORTABLE_CONTEXTS_LIVE() spectrum_scene_background_live()
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() spectrum_scene_background_live()
/* The published client's legacy RTC helper is not called. Our public clock
 * uses the generic reader below and does not select a physical clock backend. */
#define portable_fingerprint_clock spectrum_background_unused_legacy_clock
#include "PortableContextsClient.h"
#undef portable_fingerprint_clock
#include "battery_broadcast.h"
typedef struct {bool contexts_available,broadcast_available,contexts_valid,broadcast_valid,retained;contexts_status_v1 contexts;telemetry_broadcast_status_v1 broadcast;} spectrum_scene_background_status;
static struct {
 const risc_runtime_api_v1*rt;bool(*alive)(void*);void(*retain)(void*);void*context;
 bool initialized,retained,busy,paused;portable_contexts_client contexts;
 battery_broadcast broadcast;contexts_fingerprint_service_v1 proxy;
 spectrum_scene_background_status status;
} spectrum_bg;
static bool spectrum_scene_background_live(void){return !spectrum_bg.retained&&spectrum_bg.rt&&spectrum_bg.alive&&risc_runtime_get_api(1)==spectrum_bg.rt&&spectrum_bg.alive(spectrum_bg.context);}
static bool sb_fail(void){if(!spectrum_bg.retained){spectrum_bg.retained=true;spectrum_bg.status.retained=true;if(spectrum_bg.retain)spectrum_bg.retain(spectrum_bg.context);}return false;}
static bool sb_check(void){return spectrum_scene_background_live()||sb_fail();}
static bool sb_alive_hook(void*c){(void)c;return spectrum_scene_background_live();}
static void sb_retain_hook(void*c){(void)c;(void)sb_fail();}
static bool sb_empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation;}
/* 0 admitted, 1 clean absent, -1 terminal. */
static int sb_acquire(const char*name,uint32_t id,risc_runtime_capability_v1*g){
 if(!sb_check())return -1;
 if(!sb_empty(g)||(g->struct_size&&g->struct_size!=sizeof(*g))){sb_fail();return -1;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=spectrum_bg.rt->acquire(name,1,id,g);if(!sb_check())return -1;
 if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):!sb_empty(g))){sb_fail();return -1;}
 return ok?0:1;
}
static bool sb_release(risc_runtime_capability_v1*g){
 if(!sb_check())return false;
 if(sb_empty(g))return (g->struct_size==0||g->struct_size==sizeof(*g))||sb_fail();
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation)return sb_fail();
 risc_runtime_capability_v1 copy=*g;bool ok=spectrum_bg.rt->release(&copy);
 if(!sb_check()||!ok||copy.struct_size!=sizeof(copy)||!sb_empty(&copy))return sb_fail();
 memset(g,0,sizeof(*g));return true;
}
static bool sb_data_result(int rc){if(!sb_check())return false;return (rc<=0&&rc>=RISC_APP_DATA_BUSY&&rc!=RISC_APP_DATA_CONTEXT&&rc!=RISC_APP_DATA_RETAINED)||sb_fail();}
static bool spectrum_scene_background_clock(const risc_runtime_api_v1*rt,uint64_t*out){
 if(!out||rt!=spectrum_bg.rt||!sb_check())return false;
 *out=0;
 risc_runtime_capability_v1 g={0};int ac=sb_acquire(RISC_REALTIME_CAPABILITY,0,&g);if(ac)return ac>0;
 const risc_realtime_api_v1*p=g.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->read)return sb_fail();
 risc_realtime_snapshot_v1 q={.struct_size=sizeof(q)};int rc=p->read(p->context,&q);
 if(!sb_check())return false;
 if(rc!=RISC_REALTIME_OK&&rc!=RISC_REALTIME_INVALID&&rc!=RISC_REALTIME_IO)return sb_fail();
 if(!rc&&(q.struct_size!=sizeof(q)||q.reserved||q.validity>RISC_REALTIME_VALID||q.nanoseconds>=1000000000||q.nanoseconds%1000||q.monotonic_before_us>q.monotonic_after_us||(q.validity==RISC_REALTIME_UNSET&&(q.epoch_seconds||q.nanoseconds))))return sb_fail();
 if(!rc&&q.validity==RISC_REALTIME_VALID&&q.epoch_seconds>=946684800&&q.epoch_seconds<=4102444799LL)*out=(uint64_t)q.epoch_seconds;
 return sb_release(&g);
}
static bool sb_fp(uint32_t op,void*q){
 if(!sb_check())return false;
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(spectrum_bg.contexts.api);if(!f)return false;
 bool ok=f->fingerprint(f->base.context,op,q);if(!sb_check())return false;
 /* False also means a rejected domain request. The published status method
  * explicitly permits copied cleanup_pending inspection; do not turn a normal
  * rejected profile edit into invocation retention. */
 contexts_status_v1 st={.struct_size=sizeof(st)};
 bool inspected=spectrum_bg.contexts.api->status(spectrum_bg.contexts.api->context,&st);
 if(!sb_check()||!inspected||st.struct_size!=sizeof(st)||st.cleanup_pending)return sb_fail();
 return ok;
}
static bool sb_fingerprint_proxy(void*c,uint32_t op,void*q){(void)c;if(spectrum_bg.busy)return sb_fail();spectrum_bg.busy=true;bool ok=sb_fp(op,q);spectrum_bg.busy=false;return ok;}
static bool sb_pause_context(void){if(!sb_check())return false;
 if(!spectrum_bg.contexts.api)return true;
 bool ok=spectrum_bg.contexts.api->pause(spectrum_bg.contexts.api->context);return sb_check()&&(ok||sb_fail());}
/* Original shared namespace checkpoints, with callback fences and copied bytes.
 * An unread bank is never replaced. Dirty service state remains authoritative
 * after unknown/failed persistence; only confirmed writes receive FP_SAVED. */
static bool sb_record(unsigned source,bool load){
 portable_contexts_client*c=&spectrum_bg.contexts;
 contexts_fingerprint_status_v1 st={.struct_size=sizeof(st)};if(!sb_fp(CONTEXTS_FP_STATUS,&st))return false;
 if(st.struct_size!=sizeof(st)||(st.sources&~CONTEXTS_ALL)||(st.dirty_sources&~CONTEXTS_ALL))return sb_fail();
 if(load&&(!(st.sources&source)||(st.dirty_sources&source)))return true;
 if(!load&&!(st.dirty_sources&source))return true;
 if(!load&&(c->fingerprint_unread&source)){c->fingerprint_error=RISC_APP_DATA_INVALID;return true;}
 int ac=sb_acquire(RISC_SHARED_DATA_CAPABILITY,source==CONTEXTS_AUDIO?2:3,&c->fingerprint_store);if(ac){if(ac>0){c->fingerprint_error=RISC_APP_DATA_UNAVAILABLE;if(load)c->fingerprint_unread|=source;}return ac>0;}
 const risc_app_data_v1*f=c->fingerprint_store.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return sb_fail();
 uint32_t n=0;uint64_t rev=0;int rc=f->stat(f->context,"context-fingerprints.cfp",&n,&rev);if(!sb_data_result(rc))return false;
 contexts_fingerprint_record_v1 q={.struct_size=sizeof(q),.source=source,.bytes=portable_fp_bytes,.capacity=sizeof(portable_fp_bytes)};
 if(load){
  if(!rc&&n<=sizeof(portable_fp_bytes)&&rev){uint64_t actual=0;rc=f->read(f->context,"context-fingerprints.cfp",rev,portable_fp_bytes,sizeof(portable_fp_bytes),&q.size,&actual);if(!sb_data_result(rc))return false;
 if(!rc){if(actual!=rev||q.size!=n)rc=RISC_APP_DATA_INVALID;else if(!sb_fp(CONTEXTS_FP_IMPORT,&q)){if(!sb_check())return false;rc=RISC_APP_DATA_INVALID;}}}
  else if(!rc)rc=RISC_APP_DATA_INVALID;
  if(rc==RISC_APP_DATA_NOT_FOUND)rc=0;
  if(rc)c->fingerprint_unread|=source;else c->fingerprint_unread&=~source;
 }else{
  if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}
  if(!rc){if(!sb_fp(CONTEXTS_FP_EXPORT,&q)){if(!sb_check())return false;rc=RISC_APP_DATA_INVALID;}
 if(!rc&&q.size>sizeof(portable_fp_bytes))return sb_fail();
 if(!rc)rc=f->replace(f->context,"context-fingerprints.cfp",rev,portable_fp_bytes,q.size);
 if(!sb_data_result(rc))return false;
 if(!rc&&!sb_fp(CONTEXTS_FP_SAVED,&q)){if(!sb_check())return false;rc=RISC_APP_DATA_STALE;}}
 }
 if(rc)c->fingerprint_error=rc;
 return sb_release(&c->fingerprint_store);
}
static bool sb_checkpoint(bool load){
 portable_contexts_client*c=&spectrum_bg.contexts;if(!contexts_fingerprint_api(c->api))return true;
 c->fingerprint_error=0;if(!sb_pause_context())return false;uint64_t utc=0;if(!spectrum_scene_background_clock(spectrum_bg.rt,&utc))return false;
 contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only,.utc_seconds=utc};
 return sb_fp(CONTEXTS_FP_CONFIG,&cfg)&&sb_record(CONTEXTS_AUDIO,load)&&sb_record(CONTEXTS_RADIO,load);
}
static bool sb_rules_load(void){
 portable_contexts_client*c=&spectrum_bg.contexts;if(!contexts_fingerprint_api(c->api))return true;
 int ac=sb_acquire(RISC_SHARED_DATA_CAPABILITY,4,&c->fingerprint_store);if(ac)return ac>0;
 const risc_app_data_v1*f=c->fingerprint_store.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return sb_fail();
 portable_rules_request=(contexts_rules_v1){.struct_size=sizeof(portable_rules_request),.set=true};cr_init(&portable_rules_request.store);
 uint32_t n=0,got=0;uint64_t rev=0,actual=0;int rc=f->stat(f->context,"context-rules.ctx",&n,&rev);if(!sb_data_result(rc))return false;
 if(!rc&&n==CR_FILE_BYTES&&rev){rc=f->read(f->context,"context-rules.ctx",rev,portable_rules_bytes,sizeof(portable_rules_bytes),&got,&actual);if(!sb_data_result(rc))return false;
 if(!rc&&(actual!=rev||got!=n||!cr_decode(&portable_rules_request.store,portable_rules_bytes,got)))rc=RISC_APP_DATA_INVALID;}
 else if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}else if(!rc)rc=RISC_APP_DATA_INVALID;
 if(!sb_release(&c->fingerprint_store))return false;
 c->rules_available=!rc;c->rules_revision=rev;portable_rules_request.available=!rc;
 return rc?true:sb_fp(CONTEXTS_FP_RULES,&portable_rules_request);
}
static int32_t sb_kv_get(void*context,const char*key,void*out,uint32_t cap,uint32_t*n){
 if(!sb_check())return RISC_KEY_VALUE_CONTEXT;
 const risc_key_value_v1*p=context;int rc=p->get(p->context,key,out,cap,n);
 if(!sb_check())return RISC_KEY_VALUE_CONTEXT;
 if(rc>0||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT){sb_fail();return RISC_KEY_VALUE_CONTEXT;}return rc;
}
static int32_t sb_deny_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;sb_fail();return RISC_KEY_VALUE_CONTEXT;}
static bool sb_policy(void){
 portable_contexts_client*c=&spectrum_bg.contexts;if(!sb_pause_context())return false;
 c->policy=(contexts_policy_v1){.struct_size=sizeof(c->policy),.sources=CONTEXTS_ALL};c->settings_valid=false;c->low_battery=true;
 int ac=sb_acquire(RISC_KEY_VALUE_CAPABILITY,1,&c->storage);if(ac)return ac>0;
 const risc_key_value_v1*p=c->storage.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->get)return sb_fail();
 risc_key_value_v1 kv={1,sizeof(kv),(void*)p,sb_kv_get,sb_deny_put};bool enabled=false,low=true;uint8_t flags=0;
 bool valid=portable_context_enabled_load(&kv,&enabled)&&portable_radio_load(&kv,&flags)&&portable_low_battery_load(&kv,&low);if(!sb_check())return false;
 c->settings_valid=valid;c->low_battery=low;c->policy.enabled=valid&&enabled;c->policy.radio_allowed=(flags&7u)==PORTABLE_RADIO_WIFI&&!low;c->policy.audio_allowed=false;
 if(contexts_fingerprint_api(c->api)){uint8_t sources=CONTEXTS_ALL,timing=0;uint32_t n=0;int rc=sb_kv_get((void*)p,"context_sources",&sources,1,&n);if(!sb_check())return false;
 if(rc!=RISC_KEY_VALUE_NOT_FOUND&&(rc||n!=1||(sources&~CONTEXTS_ALL)))sources=0;
 c->policy.sources=c->fingerprint_sources=sources;n=0;rc=sb_kv_get((void*)p,"context_timing",&timing,1,&n);if(!sb_check())return false;
 c->fingerprint_temporal_only=!rc&&n==1&&timing==1;}
 c->loaded=true;return sb_release(&c->storage);
}
static bool spectrum_scene_background_pause(void){
 if(!spectrum_bg.initialized)return true;
 if(!sb_check())return false;
 if(spectrum_bg.busy)return sb_fail();
 spectrum_bg.busy=true;
 bool ok=sb_pause_context()&&sb_checkpoint(false);if(ok&&spectrum_bg.broadcast.api){int rc=battery_broadcast_pause(&spectrum_bg.broadcast);ok=rc==BATTERY_BROADCAST_OK||rc==BATTERY_BROADCAST_UNAVAILABLE;}
 if(ok&&spectrum_bg.broadcast.storage.api)ok=battery_broadcast_release(&spectrum_bg.broadcast,&spectrum_bg.broadcast.storage)==BATTERY_BROADCAST_OK;
 spectrum_bg.status.contexts_valid=spectrum_bg.status.broadcast_valid=false;spectrum_bg.contexts.loaded=false;spectrum_bg.paused=ok;spectrum_bg.busy=false;return ok?sb_check():sb_fail();
}
static bool spectrum_scene_background_models_save(void){if(!spectrum_scene_background_pause())return false;return !spectrum_bg.contexts.fingerprint_error;}
static const contexts_service_v1*spectrum_scene_background_service(void){if(!sb_check()||!spectrum_bg.contexts.api)return NULL;return contexts_fingerprint_api(spectrum_bg.contexts.api)?&spectrum_bg.proxy.base:spectrum_bg.contexts.api;}
static bool spectrum_scene_background_init(const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*ctx){
 if(spectrum_bg.initialized||spectrum_bg.retained)return sb_fail();
 memset(&spectrum_bg,0,sizeof(spectrum_bg));spectrum_bg.rt=rt;spectrum_bg.alive=alive;spectrum_bg.retain=retain;spectrum_bg.context=ctx;spectrum_bg.initialized=true;
 if(!sb_check()||rt->api_version!=1||rt->struct_size<sizeof(*rt)||!rt->acquire||!rt->release||!rt->health||!rt->monotonic_ms)return sb_fail();
 portable_contexts_client*c=&spectrum_bg.contexts;c->runtime=rt;c->fingerprint_sources=CONTEXTS_ALL;
 int ac=sb_acquire(CONTEXTS_SERVICE_CAPABILITY,0,&c->grant);if(ac<0)return false;
 if(!ac){const contexts_service_v1*p=c->grant.api;if(p->api_version!=1||p->struct_size<CONTEXTS_SERVICE_V1_SIZE||!p->step||!p->pause||!p->status||!p->capture_audio)return sb_fail();c->api=p;spectrum_bg.status.contexts_available=true;
  if(contexts_fingerprint_api(p)){spectrum_bg.proxy=(contexts_fingerprint_service_v1){.base={.api_version=1,.struct_size=sizeof(spectrum_bg.proxy)},.fingerprint_abi=0x31504643u,.fingerprint=sb_fingerprint_proxy};if(!sb_checkpoint(true)||!sb_rules_load())return false;}}
 int rc=battery_broadcast_init(&spectrum_bg.broadcast,rt,sb_alive_hook,sb_retain_hook,NULL);if(rc!=BATTERY_BROADCAST_OK&&rc!=BATTERY_BROADCAST_UNAVAILABLE)return sb_fail();spectrum_bg.status.broadcast_available=rc==BATTERY_BROADCAST_OK;return sb_check();
}
static bool spectrum_scene_background_step(bool allow){
 if(!allow)return spectrum_scene_background_pause();
 if(!sb_check())return false;
 if(spectrum_bg.busy)return sb_fail();
 spectrum_bg.busy=true;
 portable_contexts_client*c=&spectrum_bg.contexts;bool ok=true;uint32_t now=0;
 if(!spectrum_bg.rt->monotonic_ms(&now)||!sb_check())ok=false;
 if(ok&&c->api){if(!c->loaded)ok=sb_policy();if(ok&&contexts_fingerprint_api(c->api)){contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only};ok=sb_fp(CONTEXTS_FP_CONFIG,&cfg);if(ok&&(uint32_t)(now-c->fingerprint_checkpoint_at)>=60000){ok=sb_checkpoint(false);c->fingerprint_checkpoint_at=now;}}
  if(ok){contexts_policy_v1 p=c->policy;p.awake=true;p.audio_allowed=false;ok=c->api->step(c->api->context,&p)&&sb_check();}
  if(ok){spectrum_bg.status.contexts=(contexts_status_v1){.struct_size=sizeof(contexts_status_v1)};ok=c->api->status(c->api->context,&spectrum_bg.status.contexts)&&sb_check();if(ok&&(spectrum_bg.status.contexts.struct_size!=sizeof(contexts_status_v1)||spectrum_bg.status.contexts.cleanup_pending))ok=false;spectrum_bg.status.contexts_valid=ok;}}
 if(ok){int rc=battery_broadcast_step(&spectrum_bg.broadcast,true);ok=rc==BATTERY_BROADCAST_OK||rc==BATTERY_BROADCAST_UNAVAILABLE;if(ok&&rc==0){rc=battery_broadcast_read(&spectrum_bg.broadcast,&spectrum_bg.status.broadcast);ok=rc==0;spectrum_bg.status.broadcast_valid=ok;spectrum_bg.status.broadcast_available=true;}}
 spectrum_bg.busy=false;spectrum_bg.paused=false;return ok?sb_check():sb_fail();
}
static bool spectrum_scene_background_close(void){if(!spectrum_scene_background_pause())return false;
 if(!sb_release(&spectrum_bg.contexts.grant))return false;
 spectrum_bg.contexts.api=NULL;spectrum_bg.initialized=false;return sb_check();}
static inline spectrum_scene_background_status spectrum_scene_background_status_copy(void){return spectrum_bg.status;}
/* Same conservative foreground-owner bound as the published resident adapter.
 * Pure copied state: no service call, no claim about a fresh RF sample. */
static inline bool spectrum_scene_background_capture_active(void){
 const portable_contexts_client*c=&spectrum_bg.contexts;
 return c->loaded&&c->policy.enabled&&(c->policy.audio_allowed||c->policy.radio_allowed);
}
