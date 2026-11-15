#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscRealtimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTimeZone.h"
#include "AlarmOutputV1.h"
#include "AlarmRecords.h"
#include "PortablePowerStatus.h"
#include "RiscBluetoothTelemetryV1.h"
extern unsigned scene_test_observe(unsigned);
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern unsigned scene_test_invocation(void);
extern unsigned scene_test_width(void);
extern unsigned scene_test_height(void);
extern unsigned scene_test_format(void);
extern void scene_test_frame(const void*,size_t);
extern uint64_t scene_test_millis(void);
extern int scene_test_mode(void);
static uint8_t pixels[800*480*2+32];
static bool held,presenting,subscribed;static unsigned acquisitions,completions,nav_index,nav_wait,invocation_completions;static uint64_t sequence;static unsigned polls,frame_polls,previous_invocation,io_count,submits,releases;
static bool info(void*c,risc_display_info_v1*out){(void)c;++io_count;unsigned w=scene_test_width();*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=w,.height=scene_test_height(),.supported_formats=RISC_DISPLAY_FORMAT_BIT(scene_test_format()),.preferred_format=scene_test_format()};const char* raw=getenv("ALARM_SCENE_SAFE_INSET");if(raw){char* end=NULL;unsigned long inset=strtoul(raw,&end,10);assert(end!=raw&&!*end&&inset<w/2);out->safe_insets=(risc_display_insets_v1){inset,inset,inset,inset};out->flags|=RISC_DISPLAY_INFO_ROUND;}return true;}
static bool acquire(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;++io_count;assert(!held&&!presenting);held=true;++acquisitions;memset(pixels,0xa5,sizeof(pixels));unsigned w=scene_test_width(),h=scene_test_height();assert(f==scene_test_format());unsigned stride=scene_test_format()==1?(w+7)/8:w*2;*out=(risc_display_surface_v1){++sequence,pixels+16,w,h,stride,stride*h,f};return true;}
static void guards(void){unsigned bytes=(scene_test_format()==1?(scene_test_width()+7)/8:scene_test_width()*2)*scene_test_height();for(unsigned i=0;i<16;i++){assert(pixels[i]==0xa5);assert(pixels[16+bytes+i]==0xa5);}}
static void release(void*c,uint64_t frame){(void)c;++io_count;assert(held&&!presenting&&frame==sequence);held=false;++releases;guards();}
static bool submit(void*c,uint64_t frame,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)d;++io_count;assert(held&&frame==sequence&&n==0&&o->queue_policy==RISC_DISPLAY_QUEUE_FIFO);guards();*t=sequence;frame_polls=0;held=false;presenting=true;++submits;scene_test_frame(pixels+16,(scene_test_format()==1?(scene_test_width()+7)/8:scene_test_width()*2)*scene_test_height());return true;}
static bool display_status(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;++io_count;assert(!held&&presenting&&t==sequence);out->state=++frame_polls<3?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){presenting=false;++completions;}return true;}
static uint64_t subscribe(void*c){(void)c;++io_count;assert(!subscribed);subscribed=true;return 1;}
static bool unsubscribe(void*c,uint64_t id){(void)c;++io_count;assert(subscribed&&id==1);subscribed=false;return true;}
static bool touch_poll(void*c,size_t n){(void)c;++io_count;assert(n<=2);return true;}
static int32_t touch_next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)out;++io_count;assert(id==1&&subscribed);return 0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;++io_count;*out=(risc_touch_snapshot_v1){.width=scene_test_format()==1?scene_test_height():scene_test_width(),.height=scene_test_format()==1?scene_test_width():scene_test_height()};return true;}
static bool navigation(void*c,risc_input_navigation_frame_v1*out){
    (void)c;++io_count;unsigned invocation=scene_test_invocation();if(invocation!=previous_invocation){previous_invocation=invocation;polls=0;nav_index=nav_wait=0;invocation_completions=completions;}
    *out=(risc_input_navigation_frame_v1){0};if(++polls>=100000){fprintf(stderr,"STUCK held=%d presenting=%d completions=%u initial=%u wait=%u phase=%u route=%u focus=%u\n",held,presenting,completions,invocation_completions,nav_wait,nav_index,scene_test_observe(0),scene_test_observe(4));assert(0);}
    if(!held&&!presenting&&completions>invocation_completions&&++nav_wait>=15){
      nav_wait=0;
      if(nav_index==0)assert(scene_test_observe(1)==1);
      if(scene_test_observe(3)==3)return true;
      unsigned target=0;
      if(invocation==1){
       const unsigned targets[]={4,11,12,0,6,18,18,0};
       assert(nav_index<8);target=targets[nav_index];
       if(nav_index==6)assert(scene_test_observe(1)==0);
       if(nav_index==7)assert(scene_test_observe(1)==1);
       if(!target){out->pressed=nav_index==3?RISC_NAV_BACK:RISC_NAV_HOME;nav_index++;return true;}
      }else{if(nav_index==1){assert(scene_test_observe(1)==1);out->pressed=RISC_NAV_HOME;nav_index++;return true;}assert(nav_index==0);target=6;}
      if(scene_test_observe(4)!=target)out->pressed=RISC_NAV_DOWN;
      else{out->pressed=RISC_NAV_CONFIRM;nav_index++;}
    }
    return true;
}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;++io_count;assert(n<=1);if(n)assert(!strcmp(f->capability,"input.touch.raw"));return true;}
static bool reset(void*c){(void)c;++io_count;return true;}
static uint64_t now(void*c){(void)c;return scene_test_millis();}
static void sleep_ms(void*c,uint32_t n){(void)c;(void)n;assert(0);}
static int32_t realtime_read(void*c,risc_realtime_snapshot_v1*out){(void)c;uint64_t ms=scene_test_millis();*out=(risc_realtime_snapshot_v1){sizeof(*out),RISC_REALTIME_VALID,INT64_C(1791547200)+(int64_t)(ms/1000),0,0,ms*1000+1,ms*1000+2};return 0;}
static int32_t seed(void*c,int64_t s,uint32_t n){(void)c;(void)s;(void)n;assert(0);return -1;}
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;portable_timezone_civil p;assert(!portable_timezone_epoch_to_civil(INT64_C(1791547200)+8*3600+(int64_t)(scene_test_millis()/1000),&p));*out=(twatch_rtc_time_v1){(uint16_t)p.year,p.month,p.day,p.weekday,p.hour,p.minute,p.second};return true;}
static bool rtc_alarm(void*c,uint8_t m,uint8_t h,uint8_t d,uint8_t w,bool enabled){(void)c;(void)m;(void)h;(void)d;(void)w;(void)enabled;return true;}
static bool rtc_pending(void*c,bool*p,bool ack){(void)c;(void)ack;*p=false;return true;}
static bool effect(void*c,uint8_t n){(void)c;(void)n;return true;}
static bool stop_output(void*c){(void)c;return true;}
static bool audio_open(void*c,uint32_t n,uint8_t ch){(void)c;(void)n;(void)ch;return true;}
static bool audio_write(void*c,const int16_t*p,size_t n){(void)c;(void)p;(void)n;return true;}
static bool gain(void*c,uint16_t l,uint16_t m){(void)c;(void)l;(void)m;return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),NULL,info,acquire,release,submit,display_status,NULL,NULL};
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,navigation,foreground,reset};
static const twatch_rtc_api_v1 rtc={2,sizeof(rtc),NULL,rtc_read,NULL,rtc_alarm,rtc_pending};
static const twatch_haptic_api_v1 haptic={1,sizeof(haptic),NULL,effect,stop_output};
static const twatch_audio_out_api_v1 audio={1,sizeof(audio),NULL,audio_open,audio_write,gain,stop_output,stop_output};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,now,sleep_ms};
static const risc_realtime_control_api_v1 realtime={1,sizeof(realtime),NULL,realtime_read,seed};
static bool gauge_read(void*c,risc_battery_sample_v1*out){(void)c;++io_count;*out=(risc_battery_sample_v1){4000,70,PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT};return true;}
static const risc_battery_gauge_api_v1 gauge={1,sizeof(gauge),NULL,gauge_read};
static bool radio_active;static unsigned radio_polls,max_radio_polls;
static int32_t field(void*c,uint32_t i,risc_telemetry_field_v1*out){++io_count;(void)c;if(i)return 0;*out=(risc_telemetry_field_v1){1,RISC_TELEMETRY_BATTERY_PERCENT,"Battery"};return 1;}
static int32_t field_read(void*c,uint32_t i,int32_t*out){++io_count;(void)c;assert(i==1);*out=70;return 1;}
static bool publish(void*c,const uint32_t*i,uint32_t n,bool public_broadcast,uint64_t*t){++io_count;(void)c;(void)i;(void)n;assert(public_broadcast);radio_active=true;radio_polls=0;*t=1;return true;}
static bool radio_poll(void*c,uint64_t t,uint32_t n){++io_count;(void)c;assert(t==1&&radio_active);(void)n;if(++radio_polls>max_radio_polls)max_radio_polls=radio_polls;return true;}
static bool radio_status(void*c,risc_ble_telemetry_status_v1*out){++io_count;(void)c;*out=(risc_ble_telemetry_status_v1){.struct_size=sizeof(*out),.state=radio_active?RISC_BLE_TELEMETRY_PUBLISHING:RISC_BLE_TELEMETRY_OFF};return true;}
static bool radio_close(void*c,uint64_t t){++io_count;(void)c;(void)t;radio_active=false;return true;}
static const risc_bluetooth_telemetry_v1 radio={1,sizeof(radio),NULL,field,field_read,publish,radio_poll,radio_status,radio_close};
const void *scene_test_hardware(unsigned id){assert(id<10);const void*apis[]={&display,&touch,&nav,&rtc,&haptic,&audio,&clock_api,&realtime,&gauge,&radio};return apis[id];}
unsigned scene_test_hardware_io(void){return io_count;}
void scene_test_hardware_closed(void){assert(!radio_active);if(submits)assert(max_radio_polls>=5);assert(!held&&!presenting&&!subscribed&&acquisitions==submits+releases&&completions==submits);}
