/* Audit-only host peripheral fixture. Normal app and adapter sources are compiled
 * independently and unmodified, using delivered UI deployment feature macros. */
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "RiscKeyValueV1.h"
#include "PortableNavigation.h"
#include "PortableAppSleep.h"
#include "AlarmServiceV1.h"
#include "AudioInputV1.h"
#include "spectrum_signature_store.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void);int app_module_init(void);void app_module_fini(void);
static unsigned ticks,polls,grants,frames,subs,presents;
static unsigned stop_poll=240;
static bool mic_opened; static unsigned mic_rate, mic_frame, launches,writes;
static unsigned last_navigation_poll;
static uint16_t pixels[240*244];
static const char *directory;
static struct {unsigned at;int x,y;} actions[256];static unsigned action_count;
static struct {char key[16];unsigned char bytes[2048];uint32_t size;} cells[32];
static void save_frame(void) {
 char path[1024];snprintf(path,sizeof(path),"%s/frame-%03u.ppm",directory,presents);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned y=0;y<240;y++){for(unsigned x=0;x<240;x++){uint16_t v=pixels[y*244+x];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}for(unsigned x=240;x<244;x++)assert(pixels[y*244+x]==0xa5a5);}
 assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){h->uptime_ms=ticks;return polls<stop_poll;}
static void fake_yield(uint32_t n){ticks+=n;}
static bool fake_diag(const char*s){fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){assert(!mic_opened);launches++;if(getenv("SPECTRUM_LAUNCH_REFUSE_ONCE")&&launches==1)return false;if(getenv("SPECTRUM_EXIT_POLL"))assert(polls==(unsigned)strtoul(getenv("SPECTRUM_EXIT_POLL"),NULL,10));printf("launch=%s\n",s);stop_poll=polls;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){(void)c;*s=(risc_display_info_v1){.width=240,.height=240,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=240,.height=240,.stride_bytes=488,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){(void)c;assert(frames&&f==1);frames=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;assert(frames&&f==1);frames=0;*t=++presents;save_frame();return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;assert(t);s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
static uint64_t fake_sub(void*c){(void)c;subs++;return 1;}
static bool fake_unsub(void*c,uint64_t n){(void)c;assert(n==1&&subs);subs--;return true;}
static bool fake_touch_poll(void*c,size_t n){(void)c;assert(n==1);polls++;return true;}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;(void)e;return 0;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=(risc_touch_snapshot_v1){.width=240,.height=240};for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls&&actions[i].x>=0){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=actions[i].x,.y=actions[i].y};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){(void)c;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){(void)c;*s=(twatch_rtc_time_v1){2026,10,4,0,20,34,12};return true;}
static bool fake_write(void*c,const twatch_rtc_time_v1*s){(void)c;(void)s;assert(!"Unexpected RTC write in read-only audit");return false;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc,.write=fake_write};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){(void)c;*s=0;for(unsigned i=0;i<32;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){(void)c;writes++;assert(n<=2048&&strlen(k)<16);unsigned i;for(i=0;i<32&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<32);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 legacy_kv_api={1,sizeof(legacy_kv_api),NULL,fake_get,fake_put};
static const risc_key_value_v1 kv_api={2,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t fake_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){(void)c;*s=(risc_input_navigation_frame_v1){0};if(last_navigation_poll!=polls){last_navigation_poll=polls;for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls&&actions[i].x==-1){s->buttons=s->pressed=(uint32_t)actions[i].y;}}return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!"Unexpected hardware sleep in audit");return 0;}
static bool fake_mic_open(void*c,uint32_t rate){(void)c;assert(!mic_opened);assert(rate==16000);mic_opened=true;mic_rate=rate;mic_frame=0;return true;}
static bool fake_mic_read(void*c,int16_t*pcm,size_t n,size_t*got){(void)c;assert(mic_opened&&n<=256);for(size_t i=0;i<n;i++,mic_frame++){double t=(double)mic_frame/mic_rate;pcm[i]=(int16_t)(8000*sin(6.283185307179586*440*t)+3500*sin(6.283185307179586*1000*t)+1500*sin(6.283185307179586*3200*t));}*got=n;ticks+=(uint32_t)(n*1000/mic_rate);return true;}
static bool fake_mic_level(void*c,uint16_t*v){(void)c;*v=5000;return mic_opened;}
static bool fake_mic_close(void*c){(void)c;assert(mic_opened);mic_opened=false;return true;}
static const twatch_audio_in_api_v1 mic_api={1,sizeof(mic_api),NULL,fake_mic_open,fake_mic_read,fake_mic_level,fake_mic_close};
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)id;assert(g->struct_size==sizeof(*g));if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&legacy_kv_api;else if(!strcmp(n,"storage.key-value")&&v==2)g->api=&kv_api;else if(!strcmp(n,"audio.input")&&v==1)g->api=&mic_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else return false;grants++;return true;}
static bool fake_release(risc_runtime_capability_v1*g){assert(g->api&&grants);g->api=NULL;grants--;return true;}
static const risc_runtime_api_v1 runtime_api={1,sizeof(runtime_api),fake_health,fake_yield,fake_diag,fake_launch,fake_acquire,fake_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime_api:NULL;}
static void seed_signature_fixture(void){
 const char *value=getenv("SPECTRUM_SAMPLE_FIXTURE");if(!value)return;unsigned count=(unsigned)strtoul(value,NULL,10);assert(count==1||count==2);
 spectrum_signature_analyzer analyzer;spectrum_signature_init(&analyzer);int16_t pcm[256];
 for(unsigned chunk=0;chunk<2;chunk++){for(unsigned i=0;i<256;i++){double t=(double)(chunk*256+i)/16000;pcm[i]=(int16_t)(8000*sin(6.283185307179586*440*t)+3500*sin(6.283185307179586*1000*t)+1500*sin(6.283185307179586*3200*t));}assert(spectrum_signature_feed(&analyzer,pcm,256));}
 for(unsigned slot=0;slot<count;slot++){spectrum_signature profile={.kind=1,.frames=64};strcpy(profile.name,slot?"Similar room":"Office");for(unsigned k=0;k<128;k++)profile.sums[k]=(uint64_t)analyzer.power[k]*64;snprintf(cells[slot].key,sizeof(cells[slot].key),"spectrum_s%u",slot);cells[slot].size=SPECTRUM_SIGNATURE_RECORD_SIZE;assert(spectrum_signature_encode(&profile,cells[slot].bytes));}
}
static void check_signature_fixture(void){
 const char *value=getenv("SPECTRUM_EXPECT_SAMPLE");if(!value)return;unsigned expected=(unsigned)strtoul(value,NULL,10);spectrum_signature profile;bool found=false;for(unsigned i=0;i<32;i++)if(!strcmp(cells[i].key,"spectrum_s0")){assert(spectrum_signature_decode(&profile,cells[i].bytes,cells[i].size));assert(profile.kind==expected&&profile.frames==(expected==1?64u:1u));found=true;}assert(found&&writes==1);
}
int main(int argc,char**argv){assert(argc>=2);directory=argv[1];if(getenv("SPECTRUM_CAPTURE_POLLS")){unsigned n=(unsigned)strtoul(getenv("SPECTRUM_CAPTURE_POLLS"),NULL,10);assert(n>=10&&n<=10000);stop_poll=n;}memset(pixels,0xa5,sizeof(pixels));if(argc>2){FILE*f=fopen(argv[2],"r");assert(f);while(action_count<256&&fscanf(f,"%u %d %d",&actions[action_count].at,&actions[action_count].x,&actions[action_count].y)==3)action_count++;fclose(f);}seed_signature_fixture();assert(app_module_init()==0);app_main();app_module_fini();check_signature_fixture();assert(!grants&&!frames&&!subs&&!mic_opened);if(getenv("SPECTRUM_EXPECT_NO_WRITES"))assert(!writes);if(getenv("SPECTRUM_EXIT_POLL"))assert(launches==(getenv("SPECTRUM_LAUNCH_REFUSE_ONCE")?2u:1u));printf("Production app/adapter capture: %u frames, %u polls, %u ms; all grants released; stride guards intact\n",presents,polls,ticks);return 0;}
