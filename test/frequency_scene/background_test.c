#include "frequency_background.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static risc_runtime_api_v1 rt,other;
static const risc_runtime_api_v1*current=&rt;
static bool retained,absent,context_absent,broadcast_absent,prefix_only,defaults_on=true;
static bool ctx_cleanup,ctx_pause_ok=true,ble_pause_ok=true,step_ok=true,fp_reject,replace_identity,reenter;
static unsigned calls,retains,acquires,releases,kv_writes,context_steps,context_pauses,ble_steps,ble_pauses,imports,exports,saved,rules_calls,replaces;
static uint32_t clock_ns;
static uint32_t now,dirty,sources=CONTEXTS_ALL;static int32_t kv_error,data_error,replace_error,clock_error;
static bool rf_live,ble_live,dirty_acquire,dirty_release,bad_defaults,dirty_clock;
static unsigned data_error_at,lose_at;static bool present;static uint8_t radio=1;
static contexts_policy_v1 last_context;static telemetry_broadcast_policy_v1 last_ble;
static contexts_fingerprint_service_v1 contexts;
static telemetry_broadcast_policy_api_v1 broadcast;
static risc_key_value_v1 kv;static risc_app_data_v1 files[5];static risc_realtime_api_v1 wall;
static const char*scenario;
static bool rtc_present,rtc_fail,rtc_dirty,rtc_identity;static int64_t rtc_epoch;static unsigned native_reads;static risc_rtc_utc_api_v1 rtc;
static void mark(void);
static bool rtc_read(void*c,risc_rtc_utc_snapshot_v1*out){(void)c;mark();if(rtc_identity){current=&other;return false;}if(rtc_fail){if(rtc_dirty)out->epoch_seconds=3;return false;}*out=(risc_rtc_utc_snapshot_v1){sizeof(*out),0,rtc_epoch};return true;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1&&!retained?current:NULL;}
static void mark(void){assert(!retained);++calls;if(lose_at&&calls==lose_at)current=&other;}
static bool live(void*c){(void)c;return !retained&&current==&rt;}
static void retain(void*c){(void)c;if(!retained){retained=true;++retains;}}
static bool time_ms(uint32_t*out){mark();*out=now;return true;}
static bool health(risc_runtime_health_v1*out){mark();out->uptime_ms=now;return true;}
static int32_t get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){
 (void)c;mark();assert(!rf_live&&!ble_live);*n=0;if(kv_error)return kv_error;
 uint8_t b[4];unsigned len=4;
 if(!strcmp(k,"quick_radio")){b[0]=0x51;b[1]=1;b[2]=radio;b[3]=radio^0xa5;}
 else if(!strcmp(k,"contexts_on")){b[0]='C';b[1]=1;b[2]=1;b[3]=1^0xa5;}
 else if(!strcmp(k,"battery_edge"))return RISC_KEY_VALUE_NOT_FOUND;
 else if(!strcmp(k,"ble_broadcast"))return RISC_KEY_VALUE_NOT_FOUND;
 else if(!strcmp(k,"context_sources")){b[0]=CONTEXTS_ALL;len=1;}
 else if(!strcmp(k,"context_timing"))return RISC_KEY_VALUE_NOT_FOUND;
 else {assert(false);return -99;}
 assert(cap>=len);memcpy(out,b,len);*n=len;return 0;
}
static int32_t put(void*c,const char*k,const void*p,uint32_t n){(void)c;(void)k;(void)p;(void)n;mark();++kv_writes;assert(false);return -99;}
static int32_t stat_file(void*c,const char*name,uint32_t*n,uint64_t*rev){
 mark();assert(!rf_live&&!ble_live);unsigned instance=(unsigned)(uintptr_t)c;assert(instance>=2&&instance<=4);*n=0;*rev=0;
 if(data_error_at==1)return data_error;
 if(instance==4){assert(!strcmp(name,"context-rules.ctx"));return RISC_APP_DATA_NOT_FOUND;}
 assert(!strcmp(name,"context-fingerprints.cfp"));if(!present)return RISC_APP_DATA_NOT_FOUND;
 *n=4;*rev=7;return 0;
}
static int32_t read_file(void*c,const char*name,uint64_t rev,void*out,uint32_t cap,uint32_t*n,uint64_t*actual){
 (void)c;mark();assert(!rf_live&&!ble_live);assert(!strcmp(name,"context-fingerprints.cfp")&&rev==7&&cap>=4);*n=0;*actual=0;
 if(data_error_at==2)return data_error;
 memcpy(out,"CFP1",4);*n=4;*actual=7;return 0;
}
static int32_t replace_file(void*c,const char*name,uint64_t rev,const void*p,uint32_t n){
 (void)c;mark();assert(!rf_live&&!ble_live);++replaces;assert(!strcmp(name,"context-fingerprints.cfp")&&rev==(present?7u:0u)&&n==4&&!memcmp(p,"DATA",4));return replace_error;
}
static int32_t clock_read(void*c,risc_realtime_snapshot_v1*out){(void)c;mark();native_reads++;if(clock_error){if(dirty_clock)out->nanoseconds=3;return clock_error;}*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_VALID,.epoch_seconds=4102444799,.nanoseconds=clock_ns,.monotonic_before_us=1,.monotonic_after_us=2};return 0;}
static bool ctx_step(void*c,const contexts_policy_v1*p){(void)c;mark();++context_steps;assert(p->struct_size==sizeof(*p)&&!p->audio_allowed&&p->awake);last_context=*p;rf_live=p->enabled&&p->radio_allowed;return step_ok;}
static bool ctx_pause(void*c){(void)c;mark();++context_pauses;if(ctx_pause_ok)rf_live=false;return ctx_pause_ok;}
static bool ctx_status(void*c,contexts_status_v1*out){(void)c;mark();if(reenter){reenter=false;assert(!frequency_background_step(true));return false;}*out=(contexts_status_v1){.struct_size=sizeof(*out),.state=CONTEXTS_LIVE,.cleanup_pending=ctx_cleanup};return true;}
static bool request_export(void*c,uint32_t s){(void)c;(void)s;mark();assert(false);return false;}
static bool begin_export(void*c,uint32_t s){return request_export(c,s);}
static bool export_record(void*c,uint32_t s,uint32_t k,uint32_t i,const void*p,uint32_t n){(void)k;(void)i;(void)p;(void)n;return request_export(c,s);}
static bool finish_export(void*c,uint32_t s,uint32_t r){(void)r;return request_export(c,s);}
static int32_t label(void*c,uint32_t s,uint32_t i,contexts_label_v1*p){(void)c;(void)s;(void)i;(void)p;mark();assert(false);return -1;}
static bool claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){(void)c;(void)s;(void)i;(void)n;(void)g;mark();assert(false);return false;}
static bool result(void*c,uint32_t s,uint32_t g,uint32_t r){(void)c;(void)s;(void)g;(void)r;mark();assert(false);return false;}
static bool capture_audio(void*c){(void)c;mark();assert(false);return false;}
static bool fingerprint(void*c,uint32_t op,void*request){
 (void)c;mark();if(replace_identity){current=&other;return true;}
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=request;*s=(contexts_fingerprint_status_v1){.struct_size=sizeof(*s),.sources=sources,.dirty_sources=dirty};return true;}
 if(op==CONTEXTS_FP_CONFIG){contexts_fingerprint_config_v1*p=request;assert(p->struct_size==sizeof(*p)&&!(p->sources&~CONTEXTS_ALL));if(p->utc_seconds)assert(p->utc_seconds==4102444799);return true;}
 if(op==CONTEXTS_FP_IMPORT){++imports;contexts_fingerprint_record_v1*r=request;assert(r->size==4&&!memcmp(r->bytes,"CFP1",4));return !fp_reject;}
 if(op==CONTEXTS_FP_EXPORT){++exports;contexts_fingerprint_record_v1*r=request;assert(r->capacity>=4);memcpy(r->bytes,"DATA",4);r->size=4;return !fp_reject;}
 if(op==CONTEXTS_FP_SAVED){++saved;contexts_fingerprint_record_v1*r=request;dirty&=~r->source;return true;}
 if(op==CONTEXTS_FP_RULES){++rules_calls;contexts_rules_v1*r=request;assert(r->set&&r->available);return true;}
 assert(false);return false;
}
static bool ble_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;mark();++ble_steps;assert(allow&&p->struct_size==sizeof(*p));last_ble=*p;ble_live=p->enabled&&p->settings_valid&&p->radios_allowed;return step_ok;}
static bool ble_pause(void*c){(void)c;mark();++ble_pauses;if(ble_pause_ok)ble_live=false;return ble_pause_ok;}
static bool ble_status(void*c,telemetry_broadcast_status_v1*p){(void)c;mark();*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p)};return true;}
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;mark();assert(false);return -1;}
static int32_t ble_read(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;mark();assert(false);return -1;}
static bool defaults(void*c,telemetry_broadcast_policy_v1*p){(void)c;mark();*p=(telemetry_broadcast_policy_v1){.struct_size=sizeof(*p),.enabled=defaults_on,.settings_valid=!bad_defaults};return true;}
static bool acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 mark();++acquires;assert((v==1||(!strcmp(n,"rtc.clock")&&v==2))&&g->struct_size==sizeof(*g));if(dirty_acquire){g->slot=99;return false;}if(absent)return false;if(!strcmp(n,"rtc.clock")&&!rtc_present)return false;
 const void*p=NULL;
 if(!strcmp(n,"rtc.clock")){assert(v==2&&!instance);p=&rtc;}
 else if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)){if(context_absent)return false;assert(!instance);p=&contexts;}
 else if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){if(broadcast_absent)return false;assert(!instance);p=&broadcast;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);p=&kv;}
 else if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)){assert(instance>=2&&instance<=4);p=&files[instance];}
 else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(!instance);p=&wall;}
 else assert(false);
 g->api=p;g->slot=acquires;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){mark();++releases;assert(g->api&&g->slot&&g->generation);*g=(risc_runtime_capability_v1){.struct_size=dirty_release?0:sizeof(*g)};return true;}
static void no_more(void){unsigned n=calls;assert(!frequency_background_step(true));assert(!frequency_background_pause());assert(!frequency_background_cleanup());assert(!frequency_background_init(&rt,live,retain,NULL));assert(calls==n&&retains==1);}
int main(int argc,char**argv){
 assert(argc==2);scenario=argv[1];
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.health=health,.monotonic_ms=time_ms};
 contexts=(contexts_fingerprint_service_v1){.base={.api_version=1,.struct_size=sizeof(contexts),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=request_export,.begin_export=begin_export,.export_record=export_record,.finish_export=finish_export,.label=label,.claim_preset=claim,.preset_result=result,.capture_audio=capture_audio},.fingerprint_abi=0x31504643,.fingerprint=fingerprint};
 broadcast=(telemetry_broadcast_policy_api_v1){.base={.api_version=1,.struct_size=sizeof(broadcast),.step=ble_step,.pause=ble_pause,.status=ble_status,.enumerate=enumerate,.read=ble_read},.tag=TELEMETRY_BROADCAST_POLICY_TAG,.version=1,.defaults=defaults};
 kv=(risc_key_value_v1){1,sizeof(kv),NULL,get,put};wall=(risc_realtime_api_v1){1,sizeof(wall),NULL,clock_read};for(unsigned i=2;i<=4;i++)files[i]=(risc_app_data_v1){1,sizeof(files[i]),(void*)(uintptr_t)i,stat_file,read_file,replace_file};
 if(!strncmp(scenario,"rtc-",4)){
  rtc_present=true;rtc_epoch=!strcmp(scenario,"rtc-low")?INT64_C(946656000):INT64_C(4102415999);
  rtc=(risc_rtc_utc_api_v1){.base={.api_version=2,.struct_size=sizeof(rtc)},.utc_tag=RISC_RTC_UTC_TAG,.utc_version=1,.read_utc=rtc_read};
  rtc_fail=!strcmp(scenario,"rtc-failed")||!strcmp(scenario,"rtc-dirty");rtc_dirty=!strcmp(scenario,"rtc-dirty");rtc_identity=!strcmp(scenario,"rtc-identity");
  if(!strcmp(scenario,"rtc-prefix"))rtc.base.struct_size=sizeof(rtc.base);
  bool malformed=!strcmp(scenario,"rtc-version")||!strcmp(scenario,"rtc-null");
  if(!strcmp(scenario,"rtc-version"))rtc.utc_version=99;
  if(!strcmp(scenario,"rtc-null"))rtc.read_utc=NULL;
  fbg=(frequency_background_state){.rt=&rt,.alive=live,.retain=retain};uint64_t epoch=999;
  bool ok=fbg_clock(&epoch);
  if(rtc_dirty||rtc_identity||malformed){assert(!ok&&retained);no_more();}
  else{assert(ok&&!retained);assert(epoch==(rtc_fail?0:!strcmp(scenario,"rtc-prefix")?UINT64_C(4102444799):(uint64_t)rtc_epoch));assert(native_reads==(!strcmp(scenario,"rtc-prefix")?1u:0u));}
  puts(scenario);return 0;
 }
 if(!strcmp(scenario,"absent"))absent=true;
 if(!strcmp(scenario,"dirty-acquire"))dirty_acquire=true;
 if(!strcmp(scenario,"defaults-invalid"))bad_defaults=true;
 if(!strcmp(scenario,"prefix-context"))contexts.base.struct_size=CONTEXTS_SERVICE_V1_SIZE;
 if(!strcmp(scenario,"prefix-ble")){context_absent=true;broadcast.base.struct_size=sizeof(telemetry_broadcast_v1);prefix_only=true;}
 if(!strcmp(scenario,"defaults-off"))defaults_on=false;
 if(!strcmp(scenario,"bluetooth"))radio=2;
 if(!strcmp(scenario,"airplane"))radio=4;
 if(!strcmp(scenario,"import")||!strcmp(scenario,"import-rejected"))present=true;
 if(!strcmp(scenario,"import-rejected"))fp_reject=true;
 if(!strcmp(scenario,"stat-context")||!strcmp(scenario,"stat-retained")||!strcmp(scenario,"stat-unknown")){data_error_at=1;data_error=!strcmp(scenario,"stat-context")?RISC_APP_DATA_CONTEXT:!strcmp(scenario,"stat-retained")?RISC_APP_DATA_RETAINED:-99;}
 if(!strcmp(scenario,"read-context")){present=true;data_error_at=2;data_error=RISC_APP_DATA_CONTEXT;}
 if(!strcmp(scenario,"init-reentry"))reenter=true;
 if(!strcmp(scenario,"init-identity"))replace_identity=true;
 if(!strcmp(scenario,"clock-submicro"))clock_ns=1;
 if(!strcmp(scenario,"clock-micro"))clock_ns=1000;
 if(!strcmp(scenario,"clock-context"))clock_error=RISC_REALTIME_CONTEXT;
 if(!strcmp(scenario,"clock-dirty-error")){clock_error=RISC_REALTIME_IO;dirty_clock=true;}
 bool initialized=frequency_background_init(&rt,live,retain,NULL);
 if(!strncmp(scenario,"stat-",5)||!strcmp(scenario,"read-context")||!strcmp(scenario,"init-reentry")||!strcmp(scenario,"init-identity")||!strcmp(scenario,"clock-context")||!strcmp(scenario,"dirty-acquire")||!strcmp(scenario,"clock-dirty-error")||!strcmp(scenario,"clock-submicro")){assert(!initialized&&retained);no_more();}
 else {
  assert(initialized&&!retained);
  {unsigned n=calls;assert(frequency_background_available()==((absent||context_absent?0u:1u)|(absent||broadcast_absent?0u:2u)));assert(calls==n);}
  if(!strcmp(scenario,"kv-context"))kv_error=RISC_KEY_VALUE_CONTEXT;
  if(!strcmp(scenario,"kv-unknown"))kv_error=-99;
  if(!strcmp(scenario,"step-failed"))step_ok=false;
  if(!strcmp(scenario,"ble-pause-failed"))ble_pause_ok=false;
  if(!strcmp(scenario,"pause-failed"))ctx_pause_ok=false;
  if(!strcmp(scenario,"status-cleanup"))ctx_cleanup=true;
  if(!strcmp(scenario,"identity-before"))current=&other;
  if(!strcmp(scenario,"step-reentry"))reenter=true;
  if(!strcmp(scenario,"kv-context")||!strcmp(scenario,"kv-unknown")||!strcmp(scenario,"step-failed")||!strcmp(scenario,"status-cleanup")||!strcmp(scenario,"identity-before")||!strcmp(scenario,"step-reentry")||!strcmp(scenario,"defaults-invalid")||!strcmp(scenario,"ble-pause-failed")){assert(!frequency_background_step(true)&&retained);no_more();}
  else if(!strcmp(scenario,"pause-failed")){assert(!frequency_background_pause()&&retained);no_more();}
  else {
   assert(frequency_background_step(true));
   if(!strcmp(scenario,"absent")){assert(acquires==2&&!context_steps&&!ble_steps);unsigned n=calls;assert(frequency_background_step(true)&&calls==n);}
   else {
    assert(last_context.audio_allowed==false);
    if(!context_absent)assert(last_context.enabled&&last_context.radio_allowed==(radio==1));
    assert(last_ble.settings_valid==!prefix_only&&last_ble.enabled==(defaults_on&&!prefix_only)&&last_ble.radios_allowed==(radio==2));
    if(!strcmp(scenario,"availability")){unsigned n=calls;assert(frequency_background_available()==3&&calls==n);assert(frequency_background_pause());n=calls;assert(frequency_background_available()==3&&calls==n);}
    if(!strcmp(scenario,"capture-active")){assert(frequency_background_step(true));unsigned n=calls;assert(frequency_background_capture_active()&&calls==n);assert(frequency_background_pause());n=calls;assert(!frequency_background_capture_active()&&calls==n);}
    if(!strcmp(scenario,"continuous")){unsigned pauses=context_pauses,blepause=ble_pauses;for(unsigned i=0;i<32;i++){now+=16;assert(frequency_background_step(true));}assert(context_steps==33&&ble_steps==33&&context_pauses==pauses&&ble_pauses==blepause);}
    if(!strcmp(scenario,"policy-refresh")){unsigned p=ble_pauses;now=1000;assert(frequency_background_step(true)&&ble_pauses==p+1);}
    if(!strcmp(scenario,"import"))assert(imports==2);
    if(!strcmp(scenario,"import-rejected")){assert(imports==2&&fbg.unread==CONTEXTS_ALL);dirty=CONTEXTS_ALL;assert(frequency_background_pause()&&!replaces);}
    if(!strcmp(scenario,"save-retry")){dirty=CONTEXTS_AUDIO;replace_error=RISC_APP_DATA_BUSY;assert(frequency_background_pause()&&replaces==1&&!saved);replace_error=0;assert(frequency_background_pause()&&replaces==2&&saved==1);}
    if(!strcmp(scenario,"export-retry")){dirty=CONTEXTS_AUDIO;fp_reject=true;assert(frequency_background_pause()&&!replaces&&!saved);fp_reject=false;assert(frequency_background_pause()&&replaces==1&&saved==1);}
    if(!strcmp(scenario,"save")||!strcmp(scenario,"save-unknown")||!strcmp(scenario,"save-retained")){
     dirty=CONTEXTS_ALL;if(!strcmp(scenario,"save-unknown"))replace_error=RISC_APP_DATA_COMMIT_UNKNOWN;if(!strcmp(scenario,"save-retained"))replace_error=RISC_APP_DATA_RETAINED;
     bool ok=frequency_background_pause();
     if(replace_error==RISC_APP_DATA_RETAINED){assert(!ok&&retained&&replaces==1&&!saved);no_more();goto done;}
     assert(ok&&replaces==2&&saved==(replace_error?0u:2u));unsigned attempts=replaces;assert(frequency_background_pause()&&replaces==attempts);
    }
   }
   if(!strcmp(scenario,"dirty-release")){dirty_release=true;assert(!frequency_background_cleanup()&&retained);no_more();}
   else assert(frequency_background_cleanup()&&!retained);
  }
 }
done:
 assert(!kv_writes);printf("frequency background %s PASS\n",scenario);return 0;
}
