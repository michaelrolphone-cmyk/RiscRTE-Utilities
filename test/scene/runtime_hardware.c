#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscRealtimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTimeZone.h"
#include "AlarmOutputV1.h"
#include "AlarmRecords.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern unsigned scene_test_invocation(void);
extern unsigned scene_test_width(void);
extern void scene_test_frame(const void*,size_t);
extern uint64_t scene_test_millis(void);
extern int scene_test_mode(void);
static uint8_t pixels[800*480*2+32];
static bool held,subscribed;static uint64_t sequence;static unsigned polls,frame_polls,previous_invocation,io_count,submits,releases;
static bool info(void*c,risc_display_info_v1*out){(void)c;++io_count;unsigned w=scene_test_width();*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=w,.height=w==800?480:240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(w==800?1:5),.preferred_format=w==800?1:5};return true;}
static bool acquire(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;++io_count;assert(!held);held=true;memset(pixels,0xa5,sizeof(pixels));unsigned w=scene_test_width(),h=w==800?480:240;assert(f==(w==800?1u:5u));unsigned stride=w==800?w/8:w*2;*out=(risc_display_surface_v1){++sequence,pixels+16,w,h,stride,stride*h,f};return true;}
static void release(void*c,uint64_t frame){(void)c;++io_count;assert(held&&frame==sequence);held=false;++releases;unsigned bytes=scene_test_width()==800?48000:115200;for(unsigned i=0;i<16;i++){assert(pixels[i]==0xa5);assert(pixels[16+bytes+i]==0xa5);}}
static bool submit(void*c,uint64_t frame,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)d;++io_count;assert(held&&frame==sequence&&n==0&&o->queue_policy==RISC_DISPLAY_QUEUE_FIFO);*t=sequence;frame_polls=0;++submits;scene_test_frame(pixels+16,scene_test_width()==800?48000:115200);return true;}
static bool display_status(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;++io_count;assert(held&&t==sequence);out->state=++frame_polls<3?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t subscribe(void*c){(void)c;++io_count;assert(!subscribed);subscribed=true;return 1;}
static bool unsubscribe(void*c,uint64_t id){(void)c;++io_count;assert(subscribed&&id==1);subscribed=false;return true;}
static bool touch_poll(void*c,size_t n){(void)c;++io_count;assert(n<=2);return true;}
static int32_t touch_next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)out;++io_count;assert(id==1&&subscribed);return 0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;++io_count;*out=(risc_touch_snapshot_v1){.width=scene_test_width()==800?480:240,.height=scene_test_width()==800?800:240};return true;}
static bool navigation(void*c,risc_input_navigation_frame_v1*out){
    (void)c;++io_count;unsigned invocation=scene_test_invocation();if(invocation!=previous_invocation){previous_invocation=invocation;polls=0;}
    *out=(risc_input_navigation_frame_v1){0};assert(++polls<1000);
    const uint32_t first[]={RISC_NAV_DOWN,RISC_NAV_CONFIRM,RISC_NAV_DOWN,RISC_NAV_RIGHT,RISC_NAV_HOME};
    const uint32_t second[]={RISC_NAV_RIGHT,RISC_NAV_DOWN,RISC_NAV_CONFIRM,RISC_NAV_HOME};
    if(polls%20==10){unsigned index=polls/20;if(invocation==1&&index<5)out->pressed=first[index];else if(invocation==2&&index<4)out->pressed=second[index];}
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
const void *scene_test_hardware(unsigned id){assert(id<8);const void*apis[]={&display,&touch,&nav,&rtc,&haptic,&audio,&clock_api,&realtime};return apis[id];}
unsigned scene_test_hardware_io(void){return io_count;}
void scene_test_hardware_closed(void){assert(!held&&!subscribed&&submits==releases);}
