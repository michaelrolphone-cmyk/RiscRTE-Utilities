#pragma once
/* Audio-session background owner. Public Context/Fingerprint and BLE protocols
 * only: no Runtime proxy, foreground audio, automatic rules/presets or setters.
 * Origin contracts: System96be PortableContextsClient/Fingerprints/Rules;
 * Utilities bf111 battery_broadcast + TelemetryBroadcastPolicyV1. */
#include "RiscRuntimeV1.h"
#include "RiscRuntimeClockV1.h"
#include "RiscAppDataV1.h"
#include "RiscRealtimeV1.h"
#include "RiscRtcUtcV1.h"
#include "PortableContextPreferences.h"
#include "PortableRadioPolicy.h"
#include "PortableLowBattery.h"
#include "ContextFingerprintService.h"
#include "TelemetryBroadcastPolicyV1.h"
#include <string.h>
typedef struct {
 const risc_runtime_api_v1 *rt;bool(*alive)(void*);void(*retain)(void*);void*context;
 bool utc_available;
 bool terminal,busy,contexts_tried,broadcast_tried,context_loaded,broadcast_loaded;
 risc_runtime_capability_v1 cg,bg,kg,fg,tg;
 const contexts_service_v1 *contexts;const telemetry_broadcast_v1*broadcast;
 const risc_key_value_v1*kv;
 contexts_policy_v1 policy;telemetry_broadcast_policy_v1 ble_policy;
 uint32_t sources,unread,save_blocked,checkpoint_at,ble_at;
 bool temporal_only,context_active,broadcast_active;int32_t model_error;
 uint8_t bytes[72000];contexts_rules_v1 rules;
} frequency_background_state;
static frequency_background_state fbg;
static bool fbg_fail(void){if(!fbg.terminal){fbg.terminal=true;if(fbg.retain)fbg.retain(fbg.context);}return false;}
static bool fbg_live(void){
 if(fbg.terminal)return false;
 if(!fbg.rt||risc_runtime_get_api(1)!=fbg.rt||!fbg.alive||!fbg.alive(fbg.context))return fbg_fail();
 return true;
}
static int fbg_acquire_version(const char*name,uint32_t version,uint32_t instance,risc_runtime_capability_v1*g){
 if(!fbg_live())return -1;
 if(g->api||g->slot||g->generation)return fbg_fail(),-1;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=fbg.rt->acquire(name,version,instance,g);if(!fbg_live())return -1;
 if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):(g->api||g->slot||g->generation)))return fbg_fail(),-1;
 return ok?1:0;
}
static int fbg_acquire(const char*name,uint32_t instance,risc_runtime_capability_v1*g){return fbg_acquire_version(name,1,instance,g);}
static bool fbg_release(risc_runtime_capability_v1*g){
 if(!fbg_live())return false;
 if(!g->api){if(g->slot||g->generation)return fbg_fail();return true;}
 risc_runtime_capability_v1 copy=*g;bool ok=fbg.rt->release(&copy);
 if(!fbg_live())return false;
 if(!ok||copy.struct_size!=sizeof(copy)||copy.api||copy.slot||copy.generation)return fbg_fail();
 *g=(risc_runtime_capability_v1){0};return true;
}
static bool fbg_time(uint32_t*out){if(!fbg_live())return false;bool ok=risc_runtime_monotonic_ms_v1(fbg.rt,out);return fbg_live()&&(ok||fbg_fail());}
static bool fbg_data_result(int32_t rc){
 if(!fbg_live())return false;
 return (rc<=0&&rc>=RISC_APP_DATA_BUSY&&rc!=RISC_APP_DATA_CONTEXT&&rc!=RISC_APP_DATA_RETAINED)||fbg_fail();
}
static int32_t fbg_get(void*c,const char*key,void*bytes,uint32_t capacity,uint32_t*size){
 (void)c;if(!fbg_live())return RISC_KEY_VALUE_CONTEXT;
 int32_t rc=fbg.kv->get(fbg.kv->context,key,bytes,capacity,size);
 if(!fbg_live())return RISC_KEY_VALUE_CONTEXT;
 if(rc>0||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT||
    (rc==RISC_KEY_VALUE_OK&&*size>capacity)||
    (rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&*size)){
  (void)fbg_fail();return RISC_KEY_VALUE_CONTEXT;
 }
 return rc;
}
static int32_t fbg_put(void*c,const char*k,const void*p,uint32_t n){(void)c;(void)k;(void)p;(void)n;(void)fbg_fail();return RISC_KEY_VALUE_CONTEXT;}
static bool fbg_kv_open(void){
 int got=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&fbg.kg);if(got<=0)return false;
 fbg.kv=fbg.kg.api;
 if(!fbg.kv||fbg.kv->api_version!=1||fbg.kv->struct_size<sizeof(*fbg.kv)||!fbg.kv->get||!fbg.kv->put)return fbg_fail();
 return true;
}
static bool fbg_context_status(void){
 if(!fbg.contexts)return true;
 contexts_status_v1 s={.struct_size=sizeof(s)};bool ok=fbg.contexts->status(fbg.contexts->context,&s);
 if(!fbg_live())return false;
 return (ok&&s.struct_size==sizeof(s)&&s.state<=CONTEXTS_RETAINED&&s.state!=CONTEXTS_RETAINED&&!s.cleanup_pending)||fbg_fail();
}
/* Storage policy reads and CFP checkpoints quiesce independent owners but
 * preserve cache validity. Never use rendering/held input as this boundary. */
static bool fbg_storage_boundary(void){
 if(!fbg_live())return false;
 if(fbg.broadcast){bool ok=fbg.broadcast->pause(fbg.broadcast->context);if(!fbg_live()||!ok)return fbg_fail();fbg.broadcast_active=false;}
 if(fbg.contexts){bool ok=fbg.contexts->pause(fbg.contexts->context);if(!fbg_live()||!ok)return fbg_fail();if(!fbg_context_status())return false;fbg.context_active=false;}
 return true;
}
/* False fingerprint edits may be ordinary rejection. Copied service custody
 * must still be clean before any subsequent storage/release callback. */
static bool fbg_fp(uint32_t operation,void*request,bool*accepted){
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(fbg.contexts);
 if(!fbg_live()||!fp)return false;
 bool ok=fp->fingerprint(fp->base.context,operation,request);if(!fbg_live())return false;
 if(!fbg_context_status())return false;
 *accepted=ok;return true;
}
static bool fbg_clock(uint64_t*out){
 *out=0;fbg.utc_available=false;
 /* An explicitly configured provider UTC reader is authoritative, including
  * a failed read: never substitute a potentially stale native seeded clock. */
 int got=fbg_acquire_version("rtc.clock",2,0,&fbg.tg);
 if(got<0)return false;
 if(got){
  uint32_t prefix[2];memcpy(prefix,fbg.tg.api,sizeof(prefix));
  if(prefix[0]!=2||prefix[1]<sizeof(risc_rtc_clock_prefix_v2))return fbg_fail();
  if(risc_rtc_utc_status_v1(fbg.tg.api)<0)return fbg_fail();
  risc_rtc_utc_client_v1 client={0};
  if(risc_rtc_utc_bind_v1(fbg.tg.api,&client)){
   risc_rtc_utc_snapshot_v1 sample={sizeof(sample),0,0},initial=sample;
   bool ok=client.read(client.context,&sample);if(!fbg_live())return false;
   if((!ok&&memcmp(&sample,&initial,sizeof(sample)))||(ok&&(sample.struct_size!=sizeof(sample)||sample.reserved)))return fbg_fail();
   if(ok&&sample.epoch_seconds>=INT64_C(946598400)&&sample.epoch_seconds<=INT64_C(4102531199))*out=(uint64_t)sample.epoch_seconds;
   fbg.utc_available=*out!=0;return fbg_release(&fbg.tg);
  }
  if(!fbg_release(&fbg.tg))return false;
 }
 got=fbg_acquire(RISC_REALTIME_CAPABILITY,0,&fbg.tg);if(got<=0)return got==0;
 const risc_realtime_api_v1*c=fbg.tg.api;
 if(c->api_version!=1||c->struct_size<sizeof(*c)||!c->read)return fbg_fail();
 risc_realtime_snapshot_v1 s={.struct_size=sizeof(s)},initial=s;int32_t rc=c->read(c->context,&s);
 if(!fbg_live())return false;
 if(rc!=RISC_REALTIME_OK&&rc!=RISC_REALTIME_INVALID&&rc!=RISC_REALTIME_IO)return fbg_fail();
 if(rc&&memcmp(&s,&initial,sizeof(s)))return fbg_fail();
 if(!rc){
  if(s.struct_size!=sizeof(s)||s.validity>RISC_REALTIME_VALID||s.reserved||s.nanoseconds>=1000000000u||s.nanoseconds%1000u||s.monotonic_after_us<s.monotonic_before_us)return fbg_fail();
  if(s.validity==RISC_REALTIME_UNSET&&(s.epoch_seconds||s.nanoseconds))return fbg_fail();
  if(s.validity==RISC_REALTIME_VALID&&s.epoch_seconds>=946684800&&s.epoch_seconds<=INT64_C(4102444799))*out=(uint64_t)s.epoch_seconds;
 }
 fbg.utc_available=*out!=0;return fbg_release(&fbg.tg);
}
static bool fbg_record(unsigned source,bool load){
 contexts_fingerprint_status_v1 status={.struct_size=sizeof(status)};bool ok=false;
 if(!fbg_fp(CONTEXTS_FP_STATUS,&status,&ok))return false;
 if(!ok||status.struct_size!=sizeof(status)||(status.sources&~CONTEXTS_ALL)||(status.dirty_sources&~CONTEXTS_ALL))return fbg_fail();
 if(load&&(!(status.sources&source)||(status.dirty_sources&source)))return true;
 if(!load&&(!(status.dirty_sources&source)||((fbg.unread|fbg.save_blocked)&source)))return true;
 int got=fbg_acquire(RISC_SHARED_DATA_CAPABILITY,source==CONTEXTS_AUDIO?2:3,&fbg.fg);
 if(got<=0){if(got<0)return false;if(load)fbg.unread|=source;fbg.model_error=RISC_APP_DATA_UNAVAILABLE;return true;}
 const risc_app_data_v1*f=fbg.fg.api;
 if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return fbg_fail();
 uint32_t size=0;uint64_t revision=0;
 int32_t rc=f->stat(f->context,"context-fingerprints.cfp",&size,&revision);if(!fbg_data_result(rc))return false;
 contexts_fingerprint_record_v1 record={.struct_size=sizeof(record),.source=source,.bytes=fbg.bytes,.capacity=sizeof(fbg.bytes)};
 if(!rc&&(!revision||size>sizeof(fbg.bytes)))rc=RISC_APP_DATA_INVALID;
 if(load){
  if(!rc){uint64_t actual=0;rc=f->read(f->context,"context-fingerprints.cfp",revision,fbg.bytes,sizeof(fbg.bytes),&record.size,&actual);if(!fbg_data_result(rc))return false;
   if(!rc&&(actual!=revision||record.size!=size))rc=RISC_APP_DATA_INVALID;
   if(!rc){if(!fbg_fp(CONTEXTS_FP_IMPORT,&record,&ok))return false;if(!ok)rc=RISC_APP_DATA_INVALID;}
  }
  if(rc==RISC_APP_DATA_NOT_FOUND)rc=0;
  if(rc)fbg.unread|=source;else fbg.unread&=~source;
 }else{
  if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;revision=0;}
  if(!rc){if(!fbg_fp(CONTEXTS_FP_EXPORT,&record,&ok))return false;
   if(!ok||record.bytes!=fbg.bytes||record.capacity!=sizeof(fbg.bytes)||record.struct_size!=sizeof(record)||record.source!=source||record.size>sizeof(fbg.bytes))rc=RISC_APP_DATA_INVALID;
   if(!rc){rc=f->replace(f->context,"context-fingerprints.cfp",revision,fbg.bytes,record.size);if(!fbg_data_result(rc))return false;}
   if(!rc){if(!fbg_fp(CONTEXTS_FP_SAVED,&record,&ok))return false;if(!ok)rc=RISC_APP_DATA_STALE;}
  }
  if(rc==RISC_APP_DATA_COMMIT_UNKNOWN)fbg.save_blocked|=source; // ambiguous commit is not blind-replayed; clean failures remain retryable
 }
 if(rc)fbg.model_error=rc;
 return fbg_release(&fbg.fg);
}
static bool fbg_checkpoint(bool load){
 if(!fbg_live()||!fbg.contexts)return fbg_live();
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(fbg.contexts);if(!fp)return true;
 if(!fbg_storage_boundary())return false;
 bool ok=false;uint64_t utc=0;if(!fbg_clock(&utc))return false;
 contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=fbg.sources,.temporal_only=fbg.temporal_only,.utc_seconds=utc};
 if(!fbg_fp(CONTEXTS_FP_CONFIG,&cfg,&ok)||!ok)return fbg_fail();
 return fbg_record(CONTEXTS_AUDIO,load)&&fbg_record(CONTEXTS_RADIO,load);
}
static bool fbg_rules_load(void){
 if(!contexts_fingerprint_api(fbg.contexts))return true;
 int got=fbg_acquire(RISC_SHARED_DATA_CAPABILITY,4,&fbg.fg);if(got<=0)return got==0;
 const risc_app_data_v1*f=fbg.fg.api;if(f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace)return fbg_fail();
 fbg.rules=(contexts_rules_v1){.struct_size=sizeof(fbg.rules),.set=true};cr_init(&fbg.rules.store);
 uint32_t size=0;uint64_t revision=0,actual=0;int32_t rc=f->stat(f->context,"context-rules.ctx",&size,&revision);if(!fbg_data_result(rc))return false;
 if(!rc&&size==CR_FILE_BYTES&&revision){uint32_t n=0;rc=f->read(f->context,"context-rules.ctx",revision,fbg.bytes,sizeof(fbg.bytes),&n,&actual);if(!fbg_data_result(rc))return false;
  if(!rc&&(n!=size||actual!=revision||!cr_decode(&fbg.rules.store,fbg.bytes,n)))rc=RISC_APP_DATA_INVALID;
 }else if(rc==RISC_APP_DATA_NOT_FOUND)rc=0;else if(!rc)rc=RISC_APP_DATA_INVALID;
 if(!fbg_release(&fbg.fg))return false;
 if(rc){fbg.model_error=rc;return true;}
 fbg.rules.available=true;bool ok=false;return fbg_fp(CONTEXTS_FP_RULES,&fbg.rules,&ok)&&(ok||fbg_fail());
}
static bool fbg_open_contexts(void){
 if(fbg.contexts_tried)return true;
 int got=fbg_acquire(CONTEXTS_SERVICE_CAPABILITY,0,&fbg.cg);if(got<0)return false;fbg.contexts_tried=true;if(!got)return true;
 fbg.contexts=fbg.cg.api;const contexts_service_v1*c=fbg.contexts;
 if(c->api_version!=1||c->struct_size<CONTEXTS_SERVICE_V1_SIZE||!c->step||!c->pause||!c->status||!c->request_export||!c->begin_export||!c->export_record||!c->finish_export||!c->label||!c->claim_preset||!c->preset_result||!c->capture_audio)return fbg_fail();
 if(!fbg_context_status()||!fbg_checkpoint(true)||!fbg_rules_load())return false;
 return fbg_time(&fbg.checkpoint_at);
}
static bool fbg_open_broadcast(void){
 if(fbg.broadcast_tried)return true;
 int got=fbg_acquire(TELEMETRY_BROADCAST_CAPABILITY,0,&fbg.bg);if(got<0)return false;fbg.broadcast_tried=true;if(!got)return true;
 fbg.broadcast=fbg.bg.api;const telemetry_broadcast_v1*b=fbg.broadcast;
 return (b->api_version==1&&b->struct_size>=sizeof(*b)&&b->step&&b->pause&&b->status&&b->enumerate&&b->read)||fbg_fail();
}
static bool fbg_policy(bool contexts){
 if(!fbg_storage_boundary())return false;
 risc_key_value_v1 view={1,sizeof(view),NULL,fbg_get,fbg_put};
 if(contexts)fbg.policy=(contexts_policy_v1){.struct_size=sizeof(fbg.policy),.sources=fbg.sources};
 else fbg.ble_policy=(telemetry_broadcast_policy_v1){.struct_size=sizeof(fbg.ble_policy)};
 if(!fbg_kv_open())return fbg_live();
 uint8_t radio=0;bool valid=portable_radio_load(&view,&radio);if(!fbg_live())return false;
 if(contexts){bool enabled=false,low=true;
  bool e=portable_context_enabled_load(&view,&enabled);if(!fbg_live())return false;
  bool l=portable_low_battery_load(&view,&low);if(!fbg_live())return false;
  fbg.policy.enabled=valid&&e&&l&&enabled;fbg.policy.radio_allowed=valid&&(radio&7u)==PORTABLE_RADIO_WIFI&&!low;
  uint8_t sources=CONTEXTS_ALL;uint32_t n=0;int32_t rc=fbg_get(NULL,"context_sources",&sources,1,&n);if(!fbg_live())return false;
  if(rc!=RISC_KEY_VALUE_NOT_FOUND&&(rc||n!=1||(sources&~CONTEXTS_ALL)))sources=0;
  fbg.sources=sources;fbg.policy.sources=sources;
  uint8_t timing=0;n=0;rc=fbg_get(NULL,"context_timing",&timing,1,&n);if(!fbg_live())return false;fbg.temporal_only=!rc&&n==1&&timing==1;
 }else{
  uint8_t b[4]={0};uint32_t n=0;int32_t rc=fbg_get(NULL,TELEMETRY_BROADCAST_KEY,b,sizeof(b),&n);if(!fbg_live())return false;
  bool enabled=false,settings=false;
  if(!rc&&n==4&&b[0]==0x62&&b[1]==1&&b[2]<=1&&b[3]==(uint8_t)(b[2]^0xa5u)){enabled=b[2]!=0;settings=true;}
  if(rc==RISC_KEY_VALUE_NOT_FOUND&&fbg.broadcast->struct_size>=sizeof(telemetry_broadcast_policy_api_v1)){
   const telemetry_broadcast_policy_api_v1*x=(const void*)fbg.broadcast;if(x->tag!=TELEMETRY_BROADCAST_POLICY_TAG||x->version!=1||!x->defaults)return fbg_fail();
   telemetry_broadcast_policy_v1 d={.struct_size=sizeof(d)};bool ok=x->defaults(x->base.context,&d);
   if(!fbg_live())return false;
   if(!ok||d.struct_size!=sizeof(d)||!d.settings_valid||d.radios_allowed)return fbg_fail();
   enabled=d.enabled;settings=true;
  }
  fbg.ble_policy.enabled=enabled;fbg.ble_policy.settings_valid=valid&&settings;fbg.ble_policy.radios_allowed=valid&&(radio&6u)==PORTABLE_RADIO_BLUETOOTH;
 }
 return fbg_release(&fbg.kg);
}
static bool frequency_background_init(const risc_runtime_api_v1*rt,bool(*alive_fn)(void*),void(*retain_fn)(void*),void*context){
 if(fbg.terminal)return false;
 if(fbg.busy||fbg.cg.api||fbg.bg.api||fbg.kg.api||fbg.fg.api||fbg.tg.api)return fbg_fail();
 fbg=(frequency_background_state){.rt=rt,.alive=alive_fn,.retain=retain_fn,.context=context,.sources=CONTEXTS_ALL};
 if(!fbg_live())return false;
 if(rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release)return fbg_fail();
 fbg.busy=true;bool ok=fbg_open_contexts()&&fbg_open_broadcast();fbg.busy=false;return ok;
}
static bool frequency_background_pause(void){
 if(!fbg_live())return false;
 if(fbg.busy)return fbg_fail();
 fbg.busy=true;
 if(!fbg_storage_boundary()||!fbg_checkpoint(false)){fbg.busy=false;return false;}
 fbg.context_active=fbg.broadcast_active=false;
 fbg.context_loaded=fbg.broadcast_loaded=false;fbg.busy=false;return true;
}
static bool frequency_background_step(bool allow){
 if(!fbg_live())return false;
 if(!allow)return frequency_background_pause();
 if(!fbg.contexts&&!fbg.broadcast)return true;
 if(fbg.busy)return fbg_fail();
 fbg.busy=true;uint32_t now=0;
 if(!fbg_time(&now)){fbg.busy=false;return false;}
 if(fbg.contexts){
  if(!fbg.context_loaded){if(!fbg_policy(true)){fbg.busy=false;return false;}fbg.context_loaded=true;}
  if((uint32_t)(now-fbg.checkpoint_at)>=60000u){if(!fbg_checkpoint(false)){fbg.busy=false;return false;}fbg.checkpoint_at=now;}
  const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(fbg.contexts);
  if(fp){contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=fbg.sources,.temporal_only=fbg.temporal_only};bool accepted=false;if(!fbg_fp(CONTEXTS_FP_CONFIG,&cfg,&accepted)||!accepted){fbg.busy=false;return fbg_fail();}}
  contexts_policy_v1 p=fbg.policy;p.awake=true;p.audio_allowed=false;bool ok=fbg.contexts->step(fbg.contexts->context,&p);
  if(!fbg_live()||!ok||!fbg_context_status()){fbg.busy=false;return fbg_fail();}
  fbg.context_active=p.enabled&&p.radio_allowed;
 }
 if(fbg.broadcast){
  if(!fbg.broadcast_loaded||(uint32_t)(now-fbg.ble_at)>=1000u){if(!fbg_policy(false)){fbg.busy=false;return fbg_fail();}fbg.broadcast_loaded=true;fbg.ble_at=now;}
  bool ok=fbg.broadcast->step(fbg.broadcast->context,true,&fbg.ble_policy);if(!fbg_live()||!ok){fbg.busy=false;return fbg_fail();}
  fbg.broadcast_active=fbg.ble_policy.enabled&&fbg.ble_policy.settings_valid&&fbg.ble_policy.radios_allowed;
 }
 fbg.busy=false;return true;
}
static bool frequency_background_cleanup(void){
 if(!frequency_background_pause())return false;
 fbg.busy=true;
 if(!fbg_release(&fbg.bg)||!fbg_release(&fbg.cg)||!fbg_release(&fbg.kg)||!fbg_release(&fbg.fg)||!fbg_release(&fbg.tg))return false;
 fbg.contexts=NULL;fbg.broadcast=NULL;fbg.busy=false;return true;
}

/* Pure conservative bound from confirmed policy/stop results; no polling or
 * hardware calls. Quiet rendering must not manufacture a capture-free handoff. */
static inline bool frequency_background_capture_active(void){return fbg.context_active||fbg.broadcast_active;}

/* Pure capability availability, distinct from disabled policy or quiet state. */
static inline uint32_t frequency_background_available(void){return (fbg.contexts?1u:0u)|(fbg.broadcast?2u:0u);}

static inline bool frequency_background_utc_available(void){return fbg.utc_available;}
