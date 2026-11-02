#include <assert.h>
#include <stdio.h>
#include "files_background.h"
static risc_runtime_api_v1 rt,other;static const risc_runtime_api_v1*current;
static contexts_fingerprint_service_v1 svc;static unsigned calls,retains,steps,pauses,gets,releases,writes,imports,saves,fp_calls;
static bool ble_present,ble_default,ble_fail,ble_cleanup;static unsigned ble_steps,ble_pauses,ble_acquires,ble_releases;static telemetry_broadcast_policy_v1 ble_applied;
static bool absent,with_fp,dirty,replace_step,fail_pause,fail_release,cleanup,fp_reject,replace_fp,reentrant,clock_present;
static void touch(void);
static bool extra_mode,extra_room,extra_conflict;static unsigned room_claims,room_reports;
static contexts_policy_v1 observed_policy;
static alarm_service_v1 extra_alarm;
static bool extra_atomic;static risc_key_value_atomic_v1 extra_atomic_api;
static bool extra_fp(uint32_t,void*);
static int extra_get(const char*,void*,uint32_t,uint32_t*);
static int extra_put(const char*,const void*,uint32_t);
static void extra_tests(void);
static bool rtc_present,rtc_ok,rtc_dirty,rtc_replace,rtc_reenter,rtc_pause_reenter,rtc_dirty_acquire,rtc_replace_acquire,rtc_dirty_release,rtc_replace_release,realtime_dirty;
static unsigned rtc_reads,realtime_acquires;static risc_rtc_utc_api_v1 rtc_api;static risc_rtc_utc_snapshot_v1 rtc_sample;
static bool rtc_read(void*c,risc_rtc_utc_snapshot_v1*q){(void)c;touch();rtc_reads++;if(rtc_replace){current=&other;return false;}if(rtc_reenter){uint64_t x=0;assert(!files_background_clock(&rt,&x));return false;}if(rtc_pause_reenter){assert(!files_background_pause());return false;}if(rtc_ok||rtc_dirty)*q=rtc_sample;return rtc_ok;}
static bool context_disabled,low_battery,invalid_settings;
static bool file_present;static unsigned reject_op;static int read_error;
static int kv_error,data_error,clock_error,replace_error;static uint32_t dirty_sources;static risc_realtime_snapshot_v1 clock_sample;
static void touch(void){assert(!retains&&current==&rt);calls++;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return current;}
static bool alive(void*c){(void)c;return current==&rt&&!retains;}
static void retain(void*c){(void)c;retains++;}
static bool mono(uint32_t*n){touch();*n=100;return true;}
static bool health(risc_runtime_health_v1*h){touch();h->uptime_ms=100;return true;}
static bool cstep(void*c,const contexts_policy_v1*p){(void)c;touch();if(!extra_mode)assert(p->radio_allowed==(!low_battery&&!invalid_settings));assert(p->awake);observed_policy=*p;steps++;if(replace_step)current=&other;return true;}
static bool cpause(void*c){(void)c;touch();pauses++;return !fail_pause;}
static bool cstatus(void*c,contexts_status_v1*s){(void)c;touch();*s=(contexts_status_v1){.struct_size=sizeof(*s),.cleanup_pending=cleanup};if(extra_room){s->audio=(contexts_source_status_v1){.source=CONTEXTS_AUDIO,.room_entry=1,.preset_entry=room_claims?1:0,.model_generation=3,.room_slot=0,.current=true,.room_valid=true};strcpy(s->audio.room_name,"office");if(extra_conflict){s->radio=s->audio;s->radio.source=CONTEXTS_RADIO;strcpy(s->radio.room_name,"elsewhere");}}return true;}
static unsigned captures;static bool fail_capture,replace_capture,reenter_capture;
static bool caudio(void*c){(void)c;touch();captures++;if(replace_capture)current=&other;if(reenter_capture){reenter_capture=false;assert(!files_background_capture());}return !fail_capture;}
static bool fingerprint(void*c,uint32_t op,void*q){(void)c;touch();fp_calls++;if(extra_mode&&(op==CONTEXTS_FP_EVALUATE||op==CONTEXTS_FP_WORKFLOW))return extra_fp(op,q);if(reentrant){reentrant=false;assert(!files_background_pause());return false;}if(replace_fp){current=&other;return true;}if(fp_reject||op==reject_op)return false;
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=q;*s=(contexts_fingerprint_status_v1){.struct_size=sizeof(*s),.sources=CONTEXTS_ALL,.dirty_sources=dirty_sources};}
 if(op==CONTEXTS_FP_IMPORT)imports++;
 if(op==CONTEXTS_FP_EXPORT){contexts_fingerprint_record_v1*r=q;assert(r->capacity>=4);memcpy(r->bytes,"CFP!",4);r->size=4;}
 if(op==CONTEXTS_FP_SAVED){saves++;dirty_sources=0;}
 return true;
}
static int get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;touch();gets++;*n=0;if(extra_mode&&strcmp(k,"contexts_on")&&strcmp(k,"battery_edge")&&strcmp(k,"context_sources")&&strcmp(k,"context_timing"))return extra_get(k,b,cap,n);if(kv_error)return kv_error;if(!strcmp(k,"battery_edge")&&low_battery){assert(cap>=4);unsigned char v[]={0x42,1,1,0xa4};memcpy(b,v,4);*n=4;return 0;}if(!strcmp(k,"contexts_on")){assert(cap>=4);unsigned char v[]={0x43,1,1,0xa4};if(context_disabled){v[2]=0;v[3]=0xa5;}if(invalid_settings)v[3]=0;memcpy(b,v,4);*n=4;return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int put(void*c,const char*k,const void*v,uint32_t n){(void)c;if(extra_mode)return extra_put(k,v,n);(void)k;(void)v;(void)n;assert(!"readonly policy never writes");return -1;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static int stat_(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;(void)n;touch();*s=0;*r=0;if(data_error)return data_error;if(file_present&&!strcmp(n,"context-fingerprints.cfp")){*s=4;*r=7;return 0;}return RISC_APP_DATA_NOT_FOUND;}
static int read_(void*c,const char*n,uint64_t r,void*b,uint32_t cap,uint32_t*s,uint64_t*a){(void)c;touch();assert(file_present&&!strcmp(n,"context-fingerprints.cfp")&&r==7&&cap>=4);if(read_error)return read_error;memcpy(b,"CFP!",4);*s=4;*a=7;return 0;}
static int replace_(void*c,const char*n,uint64_t r,const void*b,uint32_t s){(void)c;touch();assert(!strcmp(n,"context-fingerprints.cfp")&&!r&&s==4&&!memcmp(b,"CFP!",4));writes++;return replace_error;}
static risc_app_data_v1 data={1,sizeof(data),NULL,stat_,read_,replace_};
static int clock_read(void*c,risc_realtime_snapshot_v1*q){(void)c;touch();if(!clock_error||realtime_dirty)*q=clock_sample;return clock_error;}
static risc_realtime_api_v1 clock_api={1,sizeof(clock_api),NULL,clock_read};
static bool bstep(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;touch();assert(allow||files_bg.broadcast.save_failed);ble_steps++;ble_applied=*p;return !ble_fail;}
static bool bpause(void*c){(void)c;touch();ble_pauses++;return !ble_fail;}
static bool bstatus(void*c,telemetry_broadcast_status_v1*p){(void)c;touch();*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p),.enabled=ble_applied.enabled,.settings_valid=ble_applied.settings_valid,.cleanup_pending=ble_cleanup};return true;}
static int benum(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;assert(!"no field polling in wrapper");return -1;}
static int bread(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;assert(!"no field polling in wrapper");return -1;}
static bool bdefault(void*c,telemetry_broadcast_policy_v1*p){(void)c;touch();*p=(telemetry_broadcast_policy_v1){.struct_size=sizeof(*p),.enabled=ble_default,.settings_valid=true};return true;}
static const telemetry_broadcast_policy_api_v1 ble={.base={1,sizeof(ble),NULL,bstep,bpause,bstatus,benum,bread},.tag=TELEMETRY_BROADCAST_POLICY_TAG,.version=1,.defaults=bdefault};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){touch();assert(v==(!strcmp(n,"rtc.clock")?2u:1u));const void*p=NULL;
 if(!strcmp(n,"rtc.clock")){assert(!id);if(rtc_replace_acquire){current=&other;return false;}if(rtc_dirty_acquire){g->slot=9;return false;}if(rtc_present)p=&rtc_api;}
 else if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)){assert(!id);if(!absent)p=&svc;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(id==1||(extra_mode&&id==3));p=&kv;}
 else if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)){assert(id==2||id==3||id==4);p=&data;}
 else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){realtime_acquires++;assert(!id);if(clock_present)p=&clock_api;}
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)&&extra_mode)p=&extra_alarm;
 else if(!strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){if(!extra_atomic)return false;p=&extra_atomic_api;}
 else {assert(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY));if(ble_present){p=&ble;ble_acquires++;}}
 if(!p){if(dirty)g->slot=1;return false;}g->api=p;g->slot=1;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){touch();releases++;if(g->api==&rtc_api){if(rtc_replace_release){current=&other;return false;}if(rtc_dirty_release){*g=(risc_runtime_capability_v1){.struct_size=1};return true;}}if(g->api==&ble)ble_releases++;if(fail_release)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void reset(bool fp){context_disabled=low_battery=invalid_settings=false;captures=0;fail_capture=replace_capture=reenter_capture=false;rtc_present=rtc_dirty=rtc_replace=rtc_reenter=rtc_pause_reenter=rtc_dirty_acquire=rtc_replace_acquire=rtc_dirty_release=rtc_replace_release=realtime_dirty=false;rtc_ok=true;rtc_reads=realtime_acquires=0;rtc_api=(risc_rtc_utc_api_v1){.base={.api_version=2,.struct_size=sizeof(rtc_api)},.utc_tag=RISC_RTC_UTC_TAG,.utc_version=1,.read_utc=rtc_read};rtc_sample=(risc_rtc_utc_snapshot_v1){.struct_size=sizeof(rtc_sample),.epoch_seconds=1800000000};memset(&files_bg,0,sizeof(files_bg));file_present=false;reject_op=0;read_error=0;ble_present=ble_fail=ble_cleanup=false;ble_default=true;ble_steps=ble_pauses=ble_acquires=ble_releases=0;ble_applied=(telemetry_broadcast_policy_v1){0};calls=retains=steps=pauses=gets=releases=writes=imports=saves=fp_calls=0;absent=dirty=replace_step=fail_pause=fail_release=cleanup=fp_reject=replace_fp=reentrant=clock_present=false;kv_error=data_error=clock_error=replace_error=0;dirty_sources=0;with_fp=fp;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.monotonic_ms=mono,.health=health};other=rt;current=&rt;svc=(contexts_fingerprint_service_v1){.base={1,fp?sizeof(svc):sizeof(svc.base),NULL,cstep,cpause,cstatus},.fingerprint_abi=0x31504643u,.fingerprint=fingerprint};svc.base.capture_audio=caudio;clock_sample=(risc_realtime_snapshot_v1){.struct_size=sizeof(clock_sample),.validity=RISC_REALTIME_VALID,.epoch_seconds=1800000000};}
static bool claim(void*c,uint32_t source,uint32_t slot,const char*name,uint32_t generation){(void)c;(void)source;(void)slot;(void)name;(void)generation;if(extra_room){++room_claims;return true;}return false;}
static bool preset_result(void*c,uint32_t source,uint32_t generation,uint32_t result){(void)c;(void)source;(void)generation;(void)result;if(extra_room){assert(room_claims==1&&!files_bg.status.pending_effects);++room_reports;}return true;}
static bool empty_observation(void*c,files_background_observation*out){(void)c;*out=(files_background_observation){0};return true;}
static bool init(void){svc.base.claim_preset=claim;svc.base.preset_result=preset_result;files_background_hooks hooks={.alive=alive,.retain=retain,.observe=empty_observation,.action_mask=PORTABLE_CONTEXT_ACTIONS};return files_background_init(&rt,&hooks);}
static bool test_step(bool allow){files_background_boundary b={.settled=true,.audio_allowed=true,.radio_allowed=true,.broadcast_allowed=true};return allow?files_background_step(&b):files_background_pause();}
static void terminal(void){unsigned before=calls;assert(!files_background_capture());assert(!test_step(true));assert(!files_background_pause());assert(!files_background_close());assert(calls==before);}
#define TEST(...) do{reset(false);__VA_ARGS__;tests++;}while(0)
int main(void){unsigned tests=0;
 TEST(absent=true;assert(init());assert(test_step(true));assert(!files_background_service());assert(files_background_close()));
 TEST(assert(init());assert(test_step(true));assert(steps==1&&gets);assert(files_background_pause());assert(test_step(true));assert(steps==2);assert(files_background_close()));
 TEST(absent=dirty=true;assert(!init()&&retains==1);terminal());
 TEST(assert(init());kv_error=RISC_KEY_VALUE_CONTEXT;assert(!test_step(true)&&retains==1&&gets==1);terminal());
 TEST(assert(init());kv_error=99;assert(!test_step(true)&&retains==1&&gets==1);terminal());
 TEST(assert(init());replace_step=true;assert(!test_step(true)&&retains==1);terminal());
 TEST(assert(init());fail_pause=true;assert(!files_background_pause()&&retains==1);terminal());
 TEST(assert(init());fail_release=true;assert(!files_background_close()&&retains==1);terminal());
 TEST(assert(init());cleanup=true;assert(!test_step(true)&&retains==1);terminal());
 TEST(reset(true);assert(init());assert(files_background_service()!=&svc.base);assert(test_step(true));dirty_sources=CONTEXTS_AUDIO;assert(files_background_models_save());assert(writes==1&&saves==1);assert(files_background_close()));
 TEST(reset(true);data_error=RISC_APP_DATA_CONTEXT;assert(!init()&&retains==1);terminal());
 TEST(reset(true);data_error=-99;assert(!init()&&retains==1);terminal());
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(files_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};fp_reject=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&!retains);fp_reject=false;assert(files_background_close()));
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(files_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};replace_fp=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&retains);terminal());
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(files_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};reentrant=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&retains);terminal());
 TEST(assert(init());uint64_t utc=99;assert(files_background_clock(&rt,&utc)&&utc==0);assert(files_background_close()));
 TEST(assert(init());clock_present=true;uint64_t utc=0;assert(files_background_clock(&rt,&utc)&&utc==1800000000);clock_sample.epoch_seconds=4102444799LL;assert(files_background_clock(&rt,&utc)&&utc==4102444799ULL);assert(files_background_close()));
 TEST(assert(init());clock_present=true;clock_sample.nanoseconds=1;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());clock_present=true;clock_sample.nanoseconds=1000;uint64_t utc;assert(files_background_clock(&rt,&utc)&&utc==1800000000);assert(files_background_close()));
 TEST(assert(init());clock_present=true;clock_error=RISC_REALTIME_CONTEXT;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());clock_present=true;clock_error=RISC_REALTIME_IO;uint64_t utc;assert(files_background_clock(&rt,&utc)&&!utc);assert(files_background_close()));
 TEST(ble_present=true;assert(init());for(unsigned i=0;i<4;i++)assert(test_step(true));assert(ble_steps==4&&ble_pauses>=1&&ble_acquires==ble_releases+1);assert(files_background_pause());assert(ble_releases==ble_acquires);assert(test_step(true));assert(ble_steps==5);assert(files_background_close()));
 TEST(ble_present=true;ble_default=false;assert(init());assert(test_step(true)&&!ble_applied.enabled&&ble_steps==1&&ble_pauses>=1);assert(files_background_close()));
 TEST(ble_present=true;assert(init());ble_fail=true;assert(!files_background_pause()&&retains==1);terminal());
 TEST(ble_present=true;assert(init());ble_cleanup=true;assert(!test_step(true)&&retains==1);terminal());
 TEST(assert(init());unsigned before=calls;risc_runtime_capability_v1 g={.struct_size=sizeof(g),.slot=3};assert(fbg_acquire("unused",0,&g)==-1&&calls==before&&retains);terminal());
 TEST(assert(init());unsigned before=calls;risc_runtime_capability_v1 g={.struct_size=1};assert(!fbg_release(&g)&&calls==before&&retains);terminal());
 TEST(assert(init());unsigned before=calls;risc_runtime_capability_v1 g={.struct_size=1,.api=&svc,.slot=1,.generation=1};assert(!fbg_release(&g)&&calls==before&&retains);terminal());
 TEST(assert(init());unsigned before=calls;assert(!init()&&calls==before&&retains);terminal());
 TEST(reset(true);data_error=RISC_APP_DATA_IO;assert(init());dirty_sources=CONTEXTS_AUDIO;assert(!files_background_models_save()&&!retains&&!writes);assert(files_background_close()));
 TEST(reset(true);assert(init());dirty_sources=CONTEXTS_AUDIO;replace_error=RISC_APP_DATA_COMMIT_UNKNOWN;assert(!files_background_models_save()&&!retains&&writes==1&&!saves);assert(dirty_sources==CONTEXTS_AUDIO));
 TEST(reset(true);file_present=true;assert(init()&&imports==2&&!files_bg.contexts.fingerprint_unread);assert(files_background_close()));
 TEST(reset(true);file_present=true;reject_op=CONTEXTS_FP_IMPORT;assert(init()&&!retains&&files_bg.contexts.fingerprint_unread==CONTEXTS_ALL);assert(files_background_close()));
 TEST(reset(true);file_present=true;read_error=RISC_APP_DATA_CONTEXT;assert(!init()&&retains==1);terminal());
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(files_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};fp_reject=cleanup=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&retains);terminal());
 TEST(assert(init());assert(!files_background_capture_active());assert(test_step(true));assert(files_background_capture_active());unsigned before=calls;assert(files_background_capture_active()&&calls==before);assert(files_background_pause());assert(!files_background_capture_active());assert(files_background_close()));

 /* Shared public UTC authority and custody regression cases. */
 TEST(assert(init());rtc_present=clock_present=true;uint64_t utc=0;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&rtc_reads==1&&!realtime_acquires&&!writes);assert(files_background_close()));
 TEST(assert(init());rtc_present=true;uint64_t utc=0;const int64_t values[]={946598400,946656000,946684800,2147483648LL,4102415999LL,4102444799LL,4102531199LL};for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){rtc_sample.epoch_seconds=values[i];assert(files_background_clock(&rt,&utc)&&utc==(uint64_t)values[i]);}assert(!realtime_acquires&&!writes);assert(files_background_close()));
 TEST(assert(init());rtc_present=clock_present=true;uint64_t utc=99;rtc_sample.epoch_seconds=946598399;assert(files_background_clock(&rt,&utc)&&!utc);rtc_sample.epoch_seconds=4102531200LL;assert(files_background_clock(&rt,&utc)&&!utc&&!realtime_acquires);assert(files_background_close()));
 TEST(assert(init());rtc_present=clock_present=true;rtc_api.base.struct_size=sizeof(rtc_api.base);uint64_t utc=0;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&!rtc_reads&&realtime_acquires==1);assert(files_background_close()));
 TEST(assert(init());rtc_present=clock_present=true;rtc_api.utc_tag=123;uint64_t utc=0;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&!rtc_reads&&realtime_acquires==1);assert(files_background_close()));
 TEST(assert(init());rtc_present=clock_present=true;rtc_ok=false;uint64_t utc=99;assert(files_background_clock(&rt,&utc)&&!utc&&rtc_reads==1&&!realtime_acquires);assert(files_background_close()));
 TEST(assert(init());rtc_present=clock_present=rtc_dirty=true;rtc_ok=false;uint64_t utc;unsigned before=releases;assert(!files_background_clock(&rt,&utc)&&retains&&releases==before&&!realtime_acquires);terminal());
 TEST(assert(init());rtc_present=true;rtc_api.utc_version=2;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!rtc_reads&&!realtime_acquires);terminal());
 TEST(assert(init());rtc_present=true;rtc_api.read_utc=NULL;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!rtc_reads);terminal());
 TEST(assert(init());rtc_present=true;rtc_api.base.struct_size=offsetof(risc_rtc_utc_api_v1,utc_version);uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!rtc_reads);terminal());
 TEST(assert(init());rtc_present=true;rtc_api.base.api_version=1;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());rtc_present=true;rtc_sample.reserved=1;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());rtc_present=true;rtc_sample.struct_size=1;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());rtc_present=rtc_replace=true;uint64_t utc;unsigned before=releases;assert(!files_background_clock(&rt,&utc)&&retains&&releases==before);terminal());
 TEST(assert(init());rtc_present=rtc_reenter=true;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&rtc_reads==1);terminal());
 TEST(assert(init());rtc_present=rtc_pause_reenter=true;uint64_t utc;unsigned before=pauses;assert(!files_background_clock(&rt,&utc)&&retains&&pauses==before);terminal());
 TEST(assert(init());rtc_dirty_acquire=true;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);terminal());
 TEST(assert(init());rtc_replace_acquire=true;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);terminal());
 TEST(assert(init());rtc_present=rtc_dirty_release=true;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);terminal());
 TEST(assert(init());rtc_present=rtc_replace_release=true;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);terminal());
 TEST(assert(init());rtc_present=fail_release=true;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());clock_present=realtime_dirty=true;clock_error=RISC_REALTIME_IO;uint64_t utc;assert(!files_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());assert(test_step(true));assert(files_bg.contexts.policy.audio_allowed&&files_bg.contexts.policy.radio_allowed);unsigned before_steps=steps,before_pauses=pauses;for(unsigned i=0;i<200;i++)assert(files_background_capture());assert(captures==200&&steps==before_steps&&pauses==before_pauses&&!writes);assert(files_background_pause());unsigned before=calls;assert(files_background_capture()&&calls==before);assert(files_background_close()));
 TEST(assert(init());assert(test_step(true));fail_capture=true;assert(!files_background_capture()&&retains==1);terminal());
 TEST(assert(init());assert(test_step(true));replace_capture=true;assert(!files_background_capture()&&retains==1);terminal());
 TEST(assert(init());assert(test_step(true));reenter_capture=true;assert(!files_background_capture()&&retains==1&&captures==1);terminal());
 TEST(absent=true;ble_present=true;assert(init());for(unsigned i=0;i<100;i++){assert(test_step(true));assert(files_background_capture());}assert(!captures&&ble_steps==100&&ble_pauses==1&&!retains);assert(files_background_close()));
 TEST(assert(init());unsigned before=calls;assert(files_background_capture()&&calls==before&&!captures);assert(files_background_close()));
 TEST(context_disabled=true;assert(init());assert(test_step(true)&&!files_background_capture_active());assert(files_background_capture()&&!captures&&!writes);assert(files_background_close()));
 TEST(low_battery=true;assert(init());assert(test_step(true)&&!files_bg.contexts.policy.audio_allowed&&!files_background_capture_active());assert(files_background_capture()&&!captures&&!writes);assert(files_background_close()));
 TEST(invalid_settings=true;assert(init());assert(test_step(true)&&!files_bg.contexts.policy.enabled);assert(files_background_capture()&&!captures&&!writes);assert(files_background_close()));
 printf("Files background %u custody/UTC/capture cases passed\n",tests);extra_tests();return 0;}
#include "actions.inc"
