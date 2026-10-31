#include <assert.h>
#include <stdio.h>
#include "spectrum_scene_background.h"
static risc_runtime_api_v1 rt,other;static const risc_runtime_api_v1*current;
static contexts_fingerprint_service_v1 svc;static unsigned calls,retains,steps,pauses,gets,releases,writes,imports,saves,fp_calls;
static bool ble_present,ble_default,ble_fail,ble_cleanup;static unsigned ble_steps,ble_pauses,ble_acquires,ble_releases;static telemetry_broadcast_policy_v1 ble_applied;
static bool absent,with_fp,dirty,replace_step,fail_pause,fail_release,cleanup,fp_reject,replace_fp,reentrant,clock_present;
static bool file_present;static unsigned reject_op;static int read_error;
static int kv_error,data_error,clock_error,replace_error;static uint32_t dirty_sources;static risc_realtime_snapshot_v1 clock_sample;
static void touch(void){assert(!retains&&current==&rt);calls++;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return current;}
static bool alive(void*c){(void)c;return current==&rt&&!retains;}
static void retain(void*c){(void)c;retains++;}
static bool mono(uint32_t*n){touch();*n=100;return true;}
static bool health(risc_runtime_health_v1*h){touch();h->uptime_ms=100;return true;}
static bool cstep(void*c,const contexts_policy_v1*p){(void)c;touch();assert(!p->audio_allowed&&p->awake);steps++;if(replace_step)current=&other;return true;}
static bool cpause(void*c){(void)c;touch();pauses++;return !fail_pause;}
static bool cstatus(void*c,contexts_status_v1*s){(void)c;touch();*s=(contexts_status_v1){.struct_size=sizeof(*s),.cleanup_pending=cleanup};return true;}
static bool caudio(void*c){(void)c;assert(!"background microphone forbidden");return false;}
static bool fingerprint(void*c,uint32_t op,void*q){(void)c;touch();fp_calls++;if(reentrant){reentrant=false;assert(!spectrum_scene_background_pause());return false;}if(replace_fp){current=&other;return true;}if(fp_reject||op==reject_op)return false;
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=q;*s=(contexts_fingerprint_status_v1){.struct_size=sizeof(*s),.sources=CONTEXTS_ALL,.dirty_sources=dirty_sources};}
 if(op==CONTEXTS_FP_IMPORT)imports++;
 if(op==CONTEXTS_FP_EXPORT){contexts_fingerprint_record_v1*r=q;assert(r->capacity>=4);memcpy(r->bytes,"CFP!",4);r->size=4;}
 if(op==CONTEXTS_FP_SAVED){saves++;dirty_sources=0;}
 return true;
}
static int get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;touch();gets++;*n=0;if(kv_error)return kv_error;if(!strcmp(k,"contexts_on")){assert(cap>=4);unsigned char v[]={0x43,1,1,0xa4};memcpy(b,v,4);*n=4;return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;assert(!"readonly policy never writes");return -1;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static int stat_(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;(void)n;touch();*s=0;*r=0;if(data_error)return data_error;if(file_present&&!strcmp(n,"context-fingerprints.cfp")){*s=4;*r=7;return 0;}return RISC_APP_DATA_NOT_FOUND;}
static int read_(void*c,const char*n,uint64_t r,void*b,uint32_t cap,uint32_t*s,uint64_t*a){(void)c;touch();assert(file_present&&!strcmp(n,"context-fingerprints.cfp")&&r==7&&cap>=4);if(read_error)return read_error;memcpy(b,"CFP!",4);*s=4;*a=7;return 0;}
static int replace_(void*c,const char*n,uint64_t r,const void*b,uint32_t s){(void)c;touch();assert(!strcmp(n,"context-fingerprints.cfp")&&!r&&s==4&&!memcmp(b,"CFP!",4));writes++;return replace_error;}
static risc_app_data_v1 data={1,sizeof(data),NULL,stat_,read_,replace_};
static int clock_read(void*c,risc_realtime_snapshot_v1*q){(void)c;touch();if(!clock_error)*q=clock_sample;return clock_error;}
static risc_realtime_api_v1 clock_api={1,sizeof(clock_api),NULL,clock_read};
static bool bstep(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;touch();assert(allow);ble_steps++;ble_applied=*p;return !ble_fail;}
static bool bpause(void*c){(void)c;touch();ble_pauses++;return !ble_fail;}
static bool bstatus(void*c,telemetry_broadcast_status_v1*p){(void)c;touch();*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p),.enabled=ble_applied.enabled,.settings_valid=ble_applied.settings_valid,.cleanup_pending=ble_cleanup};return true;}
static int benum(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;assert(!"no field polling in wrapper");return -1;}
static int bread(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;assert(!"no field polling in wrapper");return -1;}
static bool bdefault(void*c,telemetry_broadcast_policy_v1*p){(void)c;touch();*p=(telemetry_broadcast_policy_v1){.struct_size=sizeof(*p),.enabled=ble_default,.settings_valid=true};return true;}
static const telemetry_broadcast_policy_api_v1 ble={.base={1,sizeof(ble),NULL,bstep,bpause,bstatus,benum,bread},.tag=TELEMETRY_BROADCAST_POLICY_TAG,.version=1,.defaults=bdefault};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){touch();assert(v==1);const void*p=NULL;
 if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)){assert(!id);if(!absent)p=&svc;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(id==1);p=&kv;}
 else if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)){assert(id==2||id==3||id==4);p=&data;}
 else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(!id);if(clock_present)p=&clock_api;}
 else {assert(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY));if(ble_present){p=&ble;ble_acquires++;}}
 if(!p){if(dirty)g->slot=1;return false;}g->api=p;g->slot=1;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){touch();releases++;if(g->api==&ble)ble_releases++;if(fail_release)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void reset(bool fp){memset(&spectrum_bg,0,sizeof(spectrum_bg));file_present=false;reject_op=0;read_error=0;ble_present=ble_fail=ble_cleanup=false;ble_default=true;ble_steps=ble_pauses=ble_acquires=ble_releases=0;ble_applied=(telemetry_broadcast_policy_v1){0};calls=retains=steps=pauses=gets=releases=writes=imports=saves=fp_calls=0;absent=dirty=replace_step=fail_pause=fail_release=cleanup=fp_reject=replace_fp=reentrant=clock_present=false;kv_error=data_error=clock_error=replace_error=0;dirty_sources=0;with_fp=fp;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.monotonic_ms=mono,.health=health};other=rt;current=&rt;svc=(contexts_fingerprint_service_v1){.base={1,fp?sizeof(svc):sizeof(svc.base),NULL,cstep,cpause,cstatus},.fingerprint_abi=0x31504643u,.fingerprint=fingerprint};svc.base.capture_audio=caudio;clock_sample=(risc_realtime_snapshot_v1){.struct_size=sizeof(clock_sample),.validity=RISC_REALTIME_VALID,.epoch_seconds=1800000000};}
static bool init(void){return spectrum_scene_background_init(&rt,alive,retain,NULL);}
static void terminal(void){unsigned before=calls;assert(!spectrum_scene_background_step(true));assert(!spectrum_scene_background_pause());assert(!spectrum_scene_background_close());assert(calls==before);}
#define TEST(...) do{reset(false);__VA_ARGS__;tests++;}while(0)
int main(void){unsigned tests=0;
 TEST(absent=true;assert(init());assert(spectrum_scene_background_step(true));assert(!spectrum_scene_background_service());assert(spectrum_scene_background_close()));
 TEST(assert(init());assert(spectrum_scene_background_step(true));assert(steps==1&&gets);assert(spectrum_scene_background_pause());assert(spectrum_scene_background_step(true));assert(steps==2);assert(spectrum_scene_background_close()));
 TEST(absent=dirty=true;assert(!init()&&retains==1);terminal());
 TEST(assert(init());kv_error=RISC_KEY_VALUE_CONTEXT;assert(!spectrum_scene_background_step(true)&&retains==1&&gets==1);terminal());
 TEST(assert(init());kv_error=99;assert(!spectrum_scene_background_step(true)&&retains==1&&gets==1);terminal());
 TEST(assert(init());replace_step=true;assert(!spectrum_scene_background_step(true)&&retains==1);terminal());
 TEST(assert(init());fail_pause=true;assert(!spectrum_scene_background_pause()&&retains==1);terminal());
 TEST(assert(init());fail_release=true;assert(!spectrum_scene_background_close()&&retains==1);terminal());
 TEST(assert(init());cleanup=true;assert(!spectrum_scene_background_step(true)&&retains==1);terminal());
 TEST(reset(true);assert(init());assert(spectrum_scene_background_service()!=&svc.base);assert(spectrum_scene_background_step(true));dirty_sources=CONTEXTS_AUDIO;assert(spectrum_scene_background_models_save());assert(writes==1&&saves==1);assert(spectrum_scene_background_close()));
 TEST(reset(true);data_error=RISC_APP_DATA_CONTEXT;assert(!init()&&retains==1);terminal());
 TEST(reset(true);data_error=-99;assert(!init()&&retains==1);terminal());
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(spectrum_scene_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};fp_reject=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&!retains);fp_reject=false;assert(spectrum_scene_background_close()));
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(spectrum_scene_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};replace_fp=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&retains);terminal());
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(spectrum_scene_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};reentrant=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&retains);terminal());
 TEST(assert(init());uint64_t utc=99;assert(spectrum_scene_background_clock(&rt,&utc)&&utc==0);assert(spectrum_scene_background_close()));
 TEST(assert(init());clock_present=true;uint64_t utc=0;assert(spectrum_scene_background_clock(&rt,&utc)&&utc==1800000000);clock_sample.epoch_seconds=4102444799LL;assert(spectrum_scene_background_clock(&rt,&utc)&&utc==4102444799ULL);assert(spectrum_scene_background_close()));
 TEST(assert(init());clock_present=true;clock_sample.nanoseconds=1;uint64_t utc;assert(!spectrum_scene_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());clock_present=true;clock_sample.nanoseconds=1000;uint64_t utc;assert(spectrum_scene_background_clock(&rt,&utc)&&utc==1800000000);assert(spectrum_scene_background_close()));
 TEST(assert(init());clock_present=true;clock_error=RISC_REALTIME_CONTEXT;uint64_t utc;assert(!spectrum_scene_background_clock(&rt,&utc)&&retains);terminal());
 TEST(assert(init());clock_present=true;clock_error=RISC_REALTIME_IO;uint64_t utc;assert(spectrum_scene_background_clock(&rt,&utc)&&!utc);assert(spectrum_scene_background_close()));
 TEST(ble_present=true;assert(init());for(unsigned i=0;i<4;i++)assert(spectrum_scene_background_step(true));assert(ble_steps==4&&ble_acquires==1&&!ble_releases&&ble_applied.enabled);assert(spectrum_scene_background_pause());assert(ble_releases==1);assert(spectrum_scene_background_step(true));assert(ble_acquires==2);assert(spectrum_scene_background_close()));
 TEST(ble_present=true;ble_default=false;assert(init());assert(spectrum_scene_background_step(true)&&!ble_applied.enabled&&ble_applied.settings_valid);assert(spectrum_scene_background_close()));
 TEST(ble_present=true;assert(init());ble_fail=true;assert(!spectrum_scene_background_pause()&&retains==1);terminal());
 TEST(ble_present=true;assert(init());ble_cleanup=true;assert(!spectrum_scene_background_step(true)&&retains==1);terminal());
 TEST(assert(init());unsigned before=calls;risc_runtime_capability_v1 g={.struct_size=sizeof(g),.slot=3};assert(sb_acquire("unused",0,&g)==-1&&calls==before&&retains);terminal());
 TEST(assert(init());unsigned before=calls;risc_runtime_capability_v1 g={.struct_size=1};assert(!sb_release(&g)&&calls==before&&retains);terminal());
 TEST(assert(init());unsigned before=calls;risc_runtime_capability_v1 g={.struct_size=1,.api=&svc,.slot=1,.generation=1};assert(!sb_release(&g)&&calls==before&&retains);terminal());
 TEST(assert(init());unsigned before=calls;assert(!init()&&calls==before&&retains);terminal());
 TEST(reset(true);data_error=RISC_APP_DATA_IO;assert(init());dirty_sources=CONTEXTS_AUDIO;assert(!spectrum_scene_background_models_save()&&!retains&&!writes);assert(spectrum_scene_background_close()));
 TEST(reset(true);assert(init());dirty_sources=CONTEXTS_AUDIO;replace_error=RISC_APP_DATA_COMMIT_UNKNOWN;assert(!spectrum_scene_background_models_save()&&!retains&&writes==1&&!saves);assert(dirty_sources==CONTEXTS_AUDIO));
 TEST(reset(true);file_present=true;assert(init()&&imports==2&&!spectrum_bg.contexts.fingerprint_unread);assert(spectrum_scene_background_close()));
 TEST(reset(true);file_present=true;reject_op=CONTEXTS_FP_IMPORT;assert(init()&&!retains&&spectrum_bg.contexts.fingerprint_unread==CONTEXTS_ALL);assert(spectrum_scene_background_close()));
 TEST(reset(true);file_present=true;read_error=RISC_APP_DATA_CONTEXT;assert(!init()&&retains==1);terminal());
 TEST(reset(true);assert(init());const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(spectrum_scene_background_service());contexts_profile_v1 q={.struct_size=sizeof(q)};fp_reject=cleanup=true;assert(!f->fingerprint(NULL,CONTEXTS_FP_PROFILE,&q)&&retains);terminal());
 TEST(assert(init());assert(!spectrum_scene_background_capture_active());assert(spectrum_scene_background_step(true));assert(spectrum_scene_background_capture_active());unsigned before=calls;assert(spectrum_scene_background_capture_active()&&calls==before);assert(spectrum_scene_background_pause());assert(!spectrum_scene_background_capture_active());assert(spectrum_scene_background_close()));
 printf("Spectrum background %u cases passed\n",tests);return 0;}
