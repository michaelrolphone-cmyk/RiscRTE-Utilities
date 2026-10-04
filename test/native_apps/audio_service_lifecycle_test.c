/* Independent review witness. Exact production service is linked separately. */
#define PORTABLE_AUDIO_SESSION
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_RETURN_APP "springboard.elf"
#define main unused_settings_fixture_main
#include "portable_settings_test.c"
#undef main
#include "AlarmRecords.h"
#include "AlarmOutputV1.h"
#include "RiscProviderV2.h"
#include "RiscPlatformClockV1.h"
#include "RiscBoundKeyValueV1.h"
#include <setjmp.h>
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static bool app_audio=true, close_fails, uncertain, expect_preempt;
static unsigned app_close_attempts, app_closes, dep_calls, storage_calls, audio_opens, audio_closes;
static unsigned sleep_calls, cleanup_io, deps_at_failed_close;
static int sleep_result;
static jmp_buf retained;
static const alarm_service_v1 *real_alarm;
static uint8_t values[5][ALARM_RECORD_SIZE];
static uint32_t sizes[5];

bool portable_audio_services_safe(void) { return !uncertain; }
bool portable_audio_suspend(void) {
 if(uncertain)return false;
 if(!app_audio)return true;
 app_close_attempts++;
 if(close_fails){deps_at_failed_close=dep_calls;uncertain=true;return false;}
 app_audio=false;app_closes++;return true;
}
static void review_yield(uint32_t ms) { if(uncertain&&failed)longjmp(retained,1);test_yield(ms); }
static void normal_dependency(void){assert(!uncertain);dep_calls++;}
static int key_index(const char *key) {
 const char *keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY};
 for(int i=0;i<5;i++)if(!strcmp(keys[i],key))return i;
 return -1; /* The Points build's absent optional records. */
}
static int32_t review_kv_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n) {
 (void)c;normal_dependency();storage_calls++;*n=0;int i=key_index(k);
 if(i<0||!sizes[i])return RISC_BOUND_KEY_VALUE_NOT_FOUND;
 assert(sizes[i]<=cap);memcpy(b,values[i],sizes[i]);*n=sizes[i];return RISC_BOUND_KEY_VALUE_OK;
}
static int32_t review_kv_put(void*c,const char*k,const void*b,uint32_t n) {
 (void)c;normal_dependency();storage_calls++;int i=key_index(k);assert(i==3||i==4);assert(n<=sizeof(values[i]));
 memcpy(values[i],b,n);sizes[i]=n;return RISC_BOUND_KEY_VALUE_OK;
}
static uint64_t clock_read(void*c){(void)c;return ticks;}
static bool alarm_rtc_read(void*c,twatch_rtc_time_v1*out){
 (void)c;normal_dependency();if(expect_preempt)assert(!app_audio);
 unsigned s=101+ticks/1000;*out=(twatch_rtc_time_v1){2000,1,1,6,0,(uint8_t)(s/60),(uint8_t)(s%60)};return true;
}
static bool haptic_effect(void*c,uint8_t e){(void)c;(void)e;normal_dependency();assert(!app_audio);return true;}
static bool haptic_stop(void*c){(void)c;normal_dependency();assert(!app_audio);cleanup_io++;return true;}
static bool alarm_audio_open(void*c,uint32_t rate,uint8_t channels){(void)c;normal_dependency();assert(rate==8000&&channels==1&&!app_audio);audio_opens++;return true;}
static bool alarm_audio_write(void*c,const int16_t*p,size_t n){(void)c;(void)p;normal_dependency();assert(n==256&&!app_audio);return true;}
static bool alarm_audio_gain(void*c,uint16_t a,uint16_t b){(void)c;(void)a;(void)b;return true;}
static bool alarm_audio_silence(void*c){(void)c;normal_dependency();assert(!app_audio);cleanup_io++;return true;}
static bool alarm_audio_close(void*c){(void)c;normal_dependency();assert(!app_audio);cleanup_io++;audio_closes++;return true;}
static const risc_bound_key_value_v1 kv={1,sizeof(kv),NULL,review_kv_get,review_kv_put};
static const risc_platform_clock_api_v1 clk={1,sizeof(clk),NULL,clock_read,NULL};
static const twatch_rtc_api_v1 rtc_clock={2,sizeof(rtc_clock),NULL,alarm_rtc_read,NULL,NULL,NULL};
static const twatch_haptic_api_v1 haptic={1,sizeof(haptic),NULL,haptic_effect,haptic_stop};
static const twatch_audio_out_api_v1 audio={1,sizeof(audio),NULL,alarm_audio_open,alarm_audio_write,alarm_audio_gain,alarm_audio_silence,alarm_audio_close};
static const risc_provider_dependency_v1 dependencies[]={
 {"storage.key-value.bound",1,&kv},{"platform.clock",1,&clk},{"rtc.clock",2,&rtc_clock},{"haptic.effect",1,&haptic},{"audio.output",1,&audio}};
static bool acquire_review(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*out){
 if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(v==1&&!i);out->api=real_alarm;out->slot=100;grants++;return true;}
 return runtime_api.acquire(n,v,i,out);
}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){
 (void)r;(void)d;(void)g;(void)a;assert(!app_audio&&!uncertain&&!subscriptions);sleep_calls++;return sleep_result;
}
static alarm_status_v1 status_copy(void){alarm_status_v1 s={.struct_size=sizeof(s)};assert(real_alarm->status(NULL,&s)==ALARM_OK);return s;}
static void pump_to_alert_direct(void){for(unsigned n=0;n<32&&status_copy().state!=ALARM_STATE_ALERT;n++)real_alarm->step(NULL);assert(status_copy().state==ALARM_STATE_ALERT&&!audio_opens);}
static void check_failed_cleanup(unsigned deps_before,unsigned grants_before){assert(uncertain&&failed&&app_audio&&app_close_attempts==1&&!app_closes);assert(dep_calls==deps_before&&grants==grants_before&&!sleep_calls&&!return_launches&&!cleanup_io);}
int main(int argc,char**argv){
 unsigned test=argc>1?(unsigned)atoi(argv[1]):0;scenario=100;
 const risc_driver_v2 *driver=t5_driver_get(2);assert(driver);real_alarm=driver->capability;
 alarm_config config={1,101,100,0,ALARM_KIND_ALARM,1};
 if(test<4){alarm_config_encode(&config,values[0]);sizes[0]=ALARM_RECORD_SIZE;values[2][0]=ALARM_MODE_SOUND;sizes[2]=1;}
 assert(driver->start(dependencies,5));
 risc_runtime_api_v1 r=runtime_api;r.acquire=acquire_review;r.yield_ms=review_yield;
 rt=&r;dg.struct_size=sizeof(dg);bg.struct_size=sizeof(bg);
 assert(acquire_review("display.output",1,0,&dg));display=dg.api;assert(display_info(NULL,&info));
 assert(portable_touch_open(&touch,&r));display_settled=true;alarm_pixels=malloc(sizeof(framebuffer));assert(alarm_pixels);assert(portable_alarm_open(&alarms,&r));
 clear();fill(0,0,240,240,0x1234);present(false);assert(alarm_pixels_valid);
 if(test==0||test==1){
  if(test==1){pump_to_alert_direct();expect_preempt=true;}
  unsigned steps=0;
  while(!audio_opens&&steps++<32){alarm_status_v1 before=status_copy();assert(alarm_audio_pump());
   if(test==0&&before.state==ALARM_STATE_LOADING&&alarms.status.state==ALARM_STATE_LOADING)assert(app_audio&&!app_closes);
   if(alarms.status.state==ALARM_STATE_ALERT)assert(!app_audio&&app_closes==1);
  }
  assert(audio_opens==1&&app_closes==1);assert(real_alarm->acknowledge(NULL,&alarms.status.occurrence)==ALARM_PENDING);
  for(unsigned n=0;n<64&&status_copy().state!=ALARM_STATE_READY;n++)assert(alarm_audio_pump());
  assert(status_copy().state==ALARM_STATE_READY&&audio_closes==1&&!app_audio&&app_closes==1);
  app_module_fini();assert(!grants);puts(test?"already-ALERT preempts before next real service dependency; no resume PASS":"zero-occurrence VERIFY_OCC preempts before real audio open; durable dismiss; no resume PASS");return 0;
 }
 if(test==2||test==3){
  if(test==2)pump_to_alert_direct();
  else {while(status_copy().state==ALARM_STATE_LOADING&&!sizes[3])assert(alarm_audio_pump());assert(app_audio);}
  close_fails=true;volatile unsigned deps_before=dep_calls,live=grants;
  if(!setjmp(retained)){assert(!alarm_audio_pump());alarm_failure();assert(!"must retain");}
  if(test==3)deps_before++; /* Exactly the VERIFY_OCC read before detecting ALERT. */
  check_failed_cleanup(deps_before,live);puts(test==2?"already-ALERT close failure retains before normal service I/O PASS":"newly-verified ALERT close failure retains before any later service I/O PASS");return 0;
 }
 bool consumed;assert(alarm_foreground(&consumed)&&!consumed&&app_audio&&!app_closes);
 if(test==4||test==5){close_fails=true;volatile unsigned deps_before=dep_calls,live=grants;
  if(!setjmp(retained)){if(test==4){failed=true;alarm_failure();}else {ticks=60000;t5_app_input_t input;poll(&input,20);}assert(!"must retain");}
  check_failed_cleanup(test==4?deps_before:deps_at_failed_close,live);puts(test==4?"UI failure with unconfirmed close retains without service I/O PASS":"sleep close failure retains without sleep/service I/O PASS");return 0;}
 if(test==6){failed=true;unsigned before=storage_calls;assert(!alarm_failure());assert(!app_audio&&app_closes==1&&storage_calls==before);app_module_fini();assert(!grants);puts("UI failure closes owned audio before alarm stop-only PASS");return 0;}
 if(test==7||test==8){sleep_result=test==7?0:-2;ticks=60000;t5_app_input_t input;
  bool ok=poll(&input,20);assert(ok==(test==7));assert(!app_audio&&app_closes==1&&sleep_calls==1);
  if(test==7){assert(!portable_app_sleep_retained());app_module_fini();assert(!grants);}
  else {unsigned live=grants,before=dep_calls;app_module_fini();assert(grants==live&&dep_calls==before&&portable_app_sleep_retained());}
  puts(test==7?"sleep refusal/resume leaves audio stopped PASS":"native-retained sleep leaves grants retained and audio stopped PASS");return 0;}
 if(test==9){close_fails=true;tap(3,10,10);unsigned live=grants;
  if(!setjmp(retained)){t5_app_input_t input;for(unsigned i=0;i<10;i++)poll(&input,20);assert(!"must retain");}
  check_failed_cleanup(deps_at_failed_close,live);puts("Back close failure retains before return launch PASS");return 0;}
 assert(!"unknown scenario");
}
