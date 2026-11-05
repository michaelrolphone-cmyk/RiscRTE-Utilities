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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void);int app_module_init(void);void app_module_fini(void);
static unsigned ticks,polls,grants,frames,subs,presents;
static unsigned stop_poll=120;
static unsigned script_step(void){return ticks/25u;}
static unsigned native_width=240,native_height=240,native_latency;
static unsigned native_submitted;
static bool native_pending;
static uint16_t pixels[502*506],frozen_pixels[502*506];
static unsigned gap_at;static bool gap_sent;

static const char *directory;
static struct {unsigned at;int x,y;} actions[256];static unsigned action_count;
static struct {char key[16];unsigned char bytes[64];uint32_t size;} cells[32];
static void verify_native_labels(void);
static void fixture_tick(void);
static bool fixture_nav(risc_input_navigation_frame_v1 *s);
static int fixture_sleep(void);
static int32_t fixture_alarm_status(alarm_status_v1 *s);
static int32_t fixture_alarm_step(void);
static void fixture_yield(uint32_t n);
static void fixture_diagnostic(const char *s);
static void save_frame(void) {
 verify_native_labels();
 if(!strcmp(directory,"-")||presents>2)return;
 char path[1024];snprintf(path,sizeof(path),"%s/frame-%03u.ppm",directory,presents);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",native_width,native_height);
 for(unsigned y=0;y<native_height;y++){for(unsigned x=0;x<native_width;x++){uint16_t v=pixels[y*(native_width+4)+x];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}for(unsigned x=native_width;x<native_width+4;x++)assert(pixels[y*(native_width+4)+x]==0xa5a5);}
 assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){h->uptime_ms=ticks;return script_step()<stop_poll;}
static void fake_yield(uint32_t n){ticks+=n;fixture_yield(n);}
static bool fake_diag(const char*s){fixture_diagnostic(s);return true;}
static bool fake_launch(const char*s){printf("launch=%s\n",s);stop_poll=script_step();return true;}
static bool fake_info(void*c,risc_display_info_v1*s){(void)c;*s=(risc_display_info_v1){.width=native_width,.height=native_height,.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(!frames&&!native_pending);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=native_width,.height=native_height,.stride_bytes=(native_width+4)*2,.size_bytes=(native_width+4)*native_height*2,.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){(void)c;assert(frames&&f==1);frames=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;assert(frames&&f==1);frames=0;*t=++presents;native_pending=true;native_submitted=ticks;save_frame();memcpy(frozen_pixels,pixels,(native_width+4)*native_height*2);return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;assert(t);assert(!memcmp(frozen_pixels,pixels,(native_width+4)*native_height*2));s->state=ticks-native_submitted<native_latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(s->state==RISC_DISPLAY_PRESENT_COMPLETE)native_pending=false;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
static risc_touch_snapshot_v1 fake_touch_state;
static risc_touch_event_v1 fake_events[RISC_TOUCH_QUEUE_LENGTH];
static unsigned fake_event_head,fake_event_count;
static bool fake_script(void*,risc_touch_snapshot_v1*);
static uint64_t fake_sub(void*c){subs++;fake_event_head=fake_event_count=0;assert(fake_script(c,&fake_touch_state));return 1;}
static bool fake_unsub(void*c,uint64_t n){(void)c;assert(n==1&&subs);subs--;return true;}
static bool fake_script(void*c,risc_touch_snapshot_v1*s){(void)c;*s=(risc_touch_snapshot_v1){.width=native_width,.height=native_height};for(unsigned i=0;i<action_count;i++)if(actions[i].at==script_step()){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=actions[i].x,.y=actions[i].y};}return true;}
/* Scripted contacts generate the same ordered report stream exposed by the
 * snapshot; capture frequency during raster work does not advance the script. */
static void fake_edge(unsigned kind,risc_touch_contact_v1 point){
 assert(fake_event_count<RISC_TOUCH_QUEUE_LENGTH);fake_events[(fake_event_head+fake_event_count++)%RISC_TOUCH_QUEUE_LENGTH]=(risc_touch_event_v1){.sequence=++fake_touch_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=point.id,.x=point.x,.y=point.y};
}
static bool fake_touch_poll(void*c,size_t n){
 assert(n==1);polls++;fixture_tick();risc_touch_snapshot_v1 next;assert(fake_script(c,&next));
 bool before=fake_touch_state.contact_count!=0,after=next.contact_count!=0;
 if(before&&!after)fake_edge(RISC_TOUCH_EVENT_UP,fake_touch_state.contacts[0]);
 if(after){if(!before)fake_edge(RISC_TOUCH_EVENT_DOWN,next.contacts[0]);else if(memcmp(&fake_touch_state.contacts[0],&next.contacts[0],sizeof(next.contacts[0])))fake_edge(RISC_TOUCH_EVENT_MOVE,next.contacts[0]);}
 next.sequence=fake_touch_state.sequence;next.timestamp_ms=ticks;fake_touch_state=next;return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*out){(void)c;assert(n==1);if(gap_at&&script_step()==gap_at&&!gap_sent){gap_sent=true;fake_event_count=0;return -1;}if(!fake_event_count)return 0;*out=fake_events[fake_event_head];fake_event_head=(fake_event_head+1)%RISC_TOUCH_QUEUE_LENGTH;--fake_event_count;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=fake_touch_state;return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){(void)c;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){(void)c;*s=(twatch_rtc_time_v1){2026,10,4,0,20,34,12};return true;}
static bool fake_write(void*c,const twatch_rtc_time_v1*s){(void)c;(void)s;assert(!"Unexpected RTC write in read-only audit");return false;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc,.write=fake_write};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){(void)c;*s=0;for(unsigned i=0;i<32;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(n<=64&&strlen(k)<16);unsigned i;for(i=0;i<32&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<32);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){(void)c;return fixture_alarm_status(s);}
static int32_t fake_alarm_step(void*c){(void)c;return fixture_alarm_step();}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){(void)c;return fixture_nav(s);}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;return fixture_sleep();}
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)id;assert(g->struct_size==sizeof(*g));if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&kv_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else return false;grants++;return true;}
static bool fake_release(risc_runtime_capability_v1*g){assert(g->api&&grants);g->api=NULL;grants--;return true;}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=fake_acquire,.release=fake_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime_api:NULL;}
int main(int argc,char**argv){assert(argc>=2);directory=argv[1];memset(pixels,0xa5,sizeof(pixels));if(argc>2){FILE*f=fopen(argv[2],"r");assert(f);while(action_count<256&&fscanf(f,"%u %d %d",&actions[action_count].at,&actions[action_count].x,&actions[action_count].y)==3)action_count++;fclose(f);}assert(app_module_init()==0);app_main();app_module_fini();assert(!grants&&!frames&&!subs);printf("Production app/adapter capture: %u frames, %u polls, %u ms; all grants released; stride guards intact\n",presents,polls,ticks);return 0;}
