/* Native-only wrapper around the established deterministic app peripherals.
 * Production controllers and the shared adapter are compiled without rewrites. */
#include "RiscRealtimeV1.h"
#include "PortableNativeTimeToolbar.h"
#include "AlarmServiceV2.h"
static bool nu_live,nu_retained,nu_complete,nu_synthetic_exit;
static unsigned nu_reads,nu_opens,nu_closes,nu_alarm_opens;
static unsigned nu_frozen_ticks,nu_frozen_grants,nu_frozen_ops,nu_frozen_frames,nu_frozen_subs,nu_frozen_reads;
static uint32_t nu_frozen_raster;
static uint32_t nu_raster_hash(void){const uint8_t *p=(const void*)&NU_RASTER;uint32_t h=2166136261u;for(unsigned i=0;i<sizeof(NU_RASTER);i++)h=(h^p[i])*16777619u;return h;}
#ifdef PORTABLE_PAPER_TRANSITIONS
static unsigned nu_quick_open,nu_quick_close,nu_quick_restore;
static uint32_t nu_quick_background;
static bool nu_diagnostic(const char *text){
 if(strstr(text,"name=quick-controls-open")){nu_quick_open++;nu_quick_background=nu_raster_hash();}
 if(strstr(text,"name=quick-controls-close")){nu_quick_close++;if(nu_raster_hash()==nu_quick_background)nu_quick_restore++;}
 return NU_RUNTIME.diagnostic(text);
}
#endif
static void nu_check_motion(void){
#ifdef PORTABLE_PAPER_TRANSITIONS
 const char *expected=getenv("NATIVE_QUICK_EXPECT");
 if(expected){assert(nu_quick_open==(unsigned)atoi(expected)&&nu_quick_close==nu_quick_open&&nu_quick_restore==nu_quick_close);}
#endif
}
static bool nu_fault_case(void){const char *mode=getenv("NATIVE_TIME_CASE");return mode&&(!strcmp(mode,"context")||!strcmp(mode,"release-failure")||!strcmp(mode,"zone-context"));}
static bool nu_retain(void){assert(!nu_retained);nu_retained=true;nu_frozen_ticks=NU_TICKS;nu_frozen_grants=NU_GRANTS;nu_frozen_ops=NU_OPS;nu_frozen_frames=NU_FRAMES;nu_frozen_subs=NU_SUBS;nu_frozen_reads=nu_reads;nu_frozen_raster=nu_raster_hash();return true;}
void __real_free(void *pointer);void __wrap_free(void *pointer){assert(!nu_retained);__real_free(pointer);}
static void nu_check_retained(void){assert(nu_fault_case()&&nu_retained);app_module_fini();assert(nu_frozen_ticks==NU_TICKS&&nu_frozen_grants==NU_GRANTS&&nu_frozen_ops==NU_OPS&&nu_frozen_frames==NU_FRAMES&&nu_frozen_subs==NU_SUBS&&nu_frozen_reads==nu_reads&&nu_frozen_raster==nu_raster_hash());printf("Actual app/Quick native retention: frozen providers, pixels, grants and free calls verified\n");}
static int32_t nu_read(void *context,risc_realtime_snapshot_v1 *out){
 assert(context&&!nu_retained&&nu_live);nu_reads++;
 const char *mode=getenv("NATIVE_TIME_CASE");
 if(mode&&!strcmp(mode,"io"))return RISC_REALTIME_IO;
 if(mode&&!strcmp(mode,"context"))return RISC_REALTIME_CONTEXT;
 *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_VALID,.epoch_seconds=INT64_C(1791392400),.monotonic_before_us=(uint64_t)NU_TICKS*1000,.monotonic_after_us=(uint64_t)NU_TICKS*1000};
 if(mode&&!strcmp(mode,"unset")){out->validity=RISC_REALTIME_UNSET;out->epoch_seconds=0;}
 if(mode&&!strcmp(mode,"malformed"))out->reserved=1;
 return RISC_REALTIME_OK;
}
static const risc_realtime_api_v1 nu_native={1,sizeof(nu_native),&nu_live,nu_read};
static int32_t nu_get(void *context,const char *key,void *out,uint32_t cap,uint32_t *used){
 assert(!nu_retained);
 if(strcmp(key,"time_zone"))return NU_KV.get(context,key,out,cap,used);
 const char *mode=getenv("NATIVE_TIME_CASE");
 if(mode&&!strcmp(mode,"zone-context"))return RISC_KEY_VALUE_CONTEXT;
 if(mode&&!strcmp(mode,"zone-missing")){*used=0;return RISC_KEY_VALUE_NOT_FOUND;}
 uint8_t record[44]={0};record[0]='T';record[1]='Z';record[2]=1;strcpy((char*)record+4,"America/Denver");record[3]=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)record[3]^=record[i];
 if(mode&&!strcmp(mode,"zone-invalid"))record[3]^=1;
 *used=sizeof(record);if(cap<*used)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,record,*used);return RISC_KEY_VALUE_OK;
}
static risc_key_value_v1 nu_kv;
static alarm_service_descriptor_v2 nu_alarm;
static bool nu_health(risc_runtime_health_v1 *out){
 assert(!nu_retained);if(!NU_RUNTIME.health(out))nu_complete=true;return true;
}
static bool nu_launch(const char *target){
 assert(!nu_retained);if(nu_synthetic_exit){assert(!strcmp(target,"default.elf"));return true;}return NU_RUNTIME.request_launch(target);
}
static bool nu_nav_poll(void *context,risc_input_navigation_frame_v1 *out){
 bool ok=NU_NAV.poll(context,out);if(ok&&nu_complete){out->buttons=out->pressed=RISC_NAV_HOME;nu_synthetic_exit=true;}return ok;
}
static risc_input_navigation_api_v1 nu_nav;
static bool nu_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 assert(!nu_retained&&strcmp(name,"rtc.clock")&&strcmp(name,"runtime.realtime.control"));
 if(!strcmp(name,RISC_REALTIME_CAPABILITY)){
  assert(version==1&&instance==0&&!nu_live);const char *mode=getenv("NATIVE_TIME_CASE");if(mode&&!strcmp(mode,"absent"))return false;
  nu_live=true;nu_opens++;NU_GRANTS++;grant->api=&nu_native;
 }else if(!strcmp(name,"input.navigation")){assert(version==1&&!instance);nu_nav=NU_NAV;nu_nav.poll=nu_nav_poll;grant->api=&nu_nav;NU_GRANTS++;}
 else{
  bool alarm=!strcmp(name,ALARM_SERVICE_CAPABILITY);if(alarm){assert(version==2);nu_alarm_opens++;}
  if(!NU_ACQUIRE(name,alarm?1:version,instance,grant))return false;
  if(grant->api==&NU_KV){nu_kv=NU_KV;nu_kv.get=nu_get;grant->api=&nu_kv;}
  if(alarm){nu_alarm=(alarm_service_descriptor_v2){.base=*(const alarm_service_v1*)grant->api,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};nu_alarm.base.api_version=2;nu_alarm.base.struct_size=sizeof(nu_alarm);grant->api=&nu_alarm;}
 }
 grant->slot=1;grant->generation=1;return true;
}
static bool nu_release(risc_runtime_capability_v1 *grant){
 assert(!nu_retained);
 if(grant->api==&nu_native){assert(nu_live);const char *mode=getenv("NATIVE_TIME_CASE");if(mode&&!strcmp(mode,"release-failure"))return false;nu_live=false;nu_closes++;NU_GRANTS--;}
 else{
  if(grant->api==&nu_nav)grant->api=&NU_NAV;
  if(grant->api==&nu_kv)grant->api=&NU_KV;
  if(grant->api==&nu_alarm)grant->api=&NU_ALARM;
  if(!NU_RELEASE(grant))return false;
 }
 *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){
 if(version!=1)return NULL;
 static risc_runtime_api_v1 native_runtime;native_runtime=NU_RUNTIME;
 native_runtime.health=nu_health;native_runtime.request_launch=nu_launch;native_runtime.acquire=nu_acquire;native_runtime.release=nu_release;native_runtime.retain_invocation=nu_retain;
#ifdef PORTABLE_PAPER_TRANSITIONS
 native_runtime.diagnostic=nu_diagnostic;
#endif
 return &native_runtime;
}
static void nu_check_clock(void){
 if(nu_fault_case())return;
 twatch_rtc_time_v1 time={0};bool available=portable_app_native_local_time(&time);
 const char *mode=getenv("NATIVE_TIME_CASE");assert(!nu_retained&&!nu_live&&nu_opens==nu_closes);
 if(mode&&strcmp(mode,"zone-missing"))assert(!available);else {assert(available);assert(time.year==2026&&time.month==10&&time.day==7&&time.hour==(mode?17:11)&&time.minute==0);}
}
