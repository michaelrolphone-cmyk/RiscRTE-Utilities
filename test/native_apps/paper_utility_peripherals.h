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
static unsigned ticks,polls,fixture_grants,frames,subs,presents;
static bool paper_profile=true,refuse_launch,cancel_contact,fail_put_once,deny_private_store;
static unsigned battery_case,battery_acquires,battery_releases,battery_release_failures;
static bool fail_battery_release,battery_release_uncertain;
static risc_runtime_capability_v1 *battery_owner;
static unsigned home_at,raw_home_at,launches,puts_count;static char destination[64];
static unsigned stop_poll=120;
static uint16_t pixels[800*484];
static const char *directory;
static struct {unsigned at;int x,y;} actions[256];static unsigned action_count;
static struct {char key[16];unsigned char bytes[64];uint32_t size;} cells[32];
static void save_frame(void) {
 if(!directory)return;
 char path[1024];snprintf(path,sizeof(path),"%s/frame-%03u.%s",directory,presents,paper_profile?"pbm":"ppm");
 FILE*f=fopen(path,"wb");assert(f);
 if(paper_profile){fprintf(f,"P4\n480 800\n");unsigned char *p=(unsigned char*)pixels;
  for(unsigned y=0;y<800;y++)for(unsigned b=0;b<60;b++){unsigned char bits=0;for(unsigned bit=0;bit<8;bit++){unsigned x=b*8+bit;/* physical=(logical y,479-logical x) */if(p[(479-x)*104+y/8]&(0x80u>>(y%8)))bits|=0x80u>>bit;}fputc(bits,f);}
  for(unsigned y=0;y<480;y++)for(unsigned x=100;x<104;x++)assert(p[y*104+x]==0xa5);
 } else {fprintf(f,"P6\n240 240\n255\n");for(unsigned y=0;y<240;y++){for(unsigned x=0;x<240;x++){uint16_t v=pixels[y*244+x];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}for(unsigned x=240;x<244;x++)assert(pixels[y*244+x]==0xa5a5);}}
 assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){h->uptime_ms=ticks;return polls<stop_poll;}
static void fake_yield(uint32_t n){ticks+=n;}
static bool fake_diag(const char*s){fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){launches++;snprintf(destination,sizeof(destination),"%s",s);if(refuse_launch&&launches==1)return false;stop_poll=polls;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){(void)c;*s=(risc_display_info_v1){.width=paper_profile?800:240,.height=paper_profile?480:240,.nominal_refresh_millihz=paper_profile?1000:60000,.typical_present_latency_us=paper_profile?200000:16000,.flags=RISC_DISPLAY_INFO_PARTIAL_DAMAGE|(paper_profile?RISC_DISPLAY_INFO_RETAINS_IMAGE:0),.supported_formats=RISC_DISPLAY_FORMAT_BIT(paper_profile?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(!battery_release_uncertain);assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=paper_profile?800:240,.height=paper_profile?480:240,.stride_bytes=paper_profile?104:488,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){(void)c;assert(frames&&f==1);frames=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;assert(o);if(n){assert(n==1&&r->x>=0&&r->y>=0&&r->x+(int)r->width<=(paper_profile?800:240)&&r->y+(int)r->height<=(paper_profile?480:240));}assert(frames&&f==1);frames=0;*t=++presents;save_frame();return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;assert(t);s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
static uint64_t fake_sub(void*c){(void)c;subs++;return 1;}
static bool fake_unsub(void*c,uint64_t n){(void)c;assert(n==1&&subs);subs--;return true;}
static bool fake_touch_poll(void*c,size_t n){(void)c;assert(n==1);polls++;return true;}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;(void)e;return 0;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=(risc_touch_snapshot_v1){.width=paper_profile?480:240,.height=paper_profile?800:240};if(raw_home_at&&polls==raw_home_at)s->buttons=RISC_TOUCH_BUTTON_PRIMARY;for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=actions[i].x,.y=actions[i].y};if(cancel_contact){s->contact_count=2;s->contacts[1]=s->contacts[0];s->contacts[1].id=2;}}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){(void)c;assert(!battery_release_uncertain);if(battery_case==12||(battery_case==15&&polls<40))return false;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};if(battery_case==13){s->percent=0;s->flags=RISC_BATTERY_PROFILE_MISSING;}if(battery_case==14)s->flags=255;return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){(void)c;*s=(twatch_rtc_time_v1){2026,10,4,0,20,(uint8_t)(34+ticks/60000),(uint8_t)(12+ticks/1000%60)};return true;}
static bool fake_write(void*c,const twatch_rtc_time_v1*s){(void)c;(void)s;assert(!"Unexpected RTC write in read-only audit");return false;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc,.write=fake_write};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){(void)c;assert(!frames);*s=0;for(unsigned i=0;i<32;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!frames);puts_count++;if(fail_put_once){fail_put_once=false;return RISC_KEY_VALUE_IO;}assert(n<=64&&strlen(k)<16);unsigned i;for(i=0;i<32&&cells[i].key[0]&&strcmp(k,cells[i].key);i++){} assert(i<32);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=0};return ALARM_OK;}
static int32_t fake_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static alarm_service_outputs_v1 alarm_api={.service={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step},.output_modes=ALARM_MODE_VISUAL};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){(void)c;*s=(risc_input_navigation_frame_v1){0};if(home_at&&polls==home_at){s->buttons=RISC_NAV_HOME;s->pressed=RISC_NAV_HOME;}return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!"Unexpected hardware sleep in audit");return 0;}
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(g->struct_size==sizeof(*g));if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1){g->api=&battery_api;battery_owner=g;battery_acquires++;}else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1){assert(id==1 || (NOVA_APP_ID==2&&id==2) || ((NOVA_APP_ID==5||NOVA_APP_ID==4)&&id==3));if(deny_private_store&&id!=1)return false;g->api=&kv_api;}
else if(!strcmp(n,"input.navigation")&&v==1)g->api=&nav_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else {return false;}
if(strcmp(n,"storage.key-value"))assert(id==0);
fixture_grants++;return true;
}
static bool fake_release(risc_runtime_capability_v1*g){assert(g->api&&fixture_grants);
 if(g->api==&battery_api){assert(g==battery_owner);battery_releases++;
  if(fail_battery_release){fail_battery_release=false;battery_release_uncertain=true;battery_release_failures++;return false;}
  battery_release_uncertain=false;
 }
g->api=NULL;fixture_grants--;return true;}
static const risc_runtime_api_v1 runtime_api={1,sizeof(runtime_api),fake_health,fake_yield,fake_diag,fake_launch,fake_acquire,fake_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime_api:NULL;}
