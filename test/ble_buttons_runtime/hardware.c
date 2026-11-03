#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
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
static uint64_t hold_started;static bool nav_held;
static bool navigation(void*c,risc_input_navigation_frame_v1*out){
 (void)c;++io_count;unsigned invocation=scene_test_invocation();
 if(invocation!=previous_invocation){previous_invocation=invocation;polls=0;nav_index=nav_wait=0;nav_held=false;invocation_completions=completions;}
 *out=(risc_input_navigation_frame_v1){0};
 if(++polls>5000){fprintf(stderr,"stuck phase=%u page=%u focus=%u token=%u state=%u terminal=%u dirty=%u\n",nav_index,scene_test_observe(1),scene_test_observe(0),scene_test_observe(2),scene_test_observe(3),scene_test_observe(4),scene_test_observe(6));assert(0);}
 if(nav_held){out->buttons=RISC_NAV_CONFIRM;if(scene_test_millis()-hold_started>=650){out->buttons=0;out->released=RISC_NAV_CONFIRM;nav_held=false;nav_index++;}return true;}
 if(held||presenting||completions<=invocation_completions||++nav_wait<8)return true;
 nav_wait=0;unsigned action=0;
 if(scene_test_observe(7)){unsigned target=scene_test_observe(116);if(target)out->pressed=scene_test_observe(0)==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;return true;}
 if(invocation==1&&nav_index<5){static const unsigned actions[]={4,32,10,12,1};action=actions[nav_index];}
 else {
  unsigned phase=nav_index-(invocation==1?5:0);
  if(phase==0){assert(scene_test_observe(5)==76);action=3;}
  else if(phase==1){if(scene_test_observe(3)!=5)return true;action=64;}
  else if(phase==2){if(!scene_test_observe(2)){nav_index++;return true;}action=2;}
  else {if(scene_test_observe(2))return true;out->pressed=RISC_NAV_HOME;nav_index++;return true;}
 }
 if(action==1){out->pressed=RISC_NAV_BACK;nav_index++;return true;}
 unsigned target=scene_test_observe(100+action);if(!target)return true;
 if(scene_test_observe(0)!=target)out->pressed=RISC_NAV_DOWN;
 else if(action==64){out->pressed=out->buttons=RISC_NAV_CONFIRM;nav_held=true;hold_started=scene_test_millis();}
 else {out->pressed=RISC_NAV_CONFIRM;nav_index++;}
 return true;
}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;++io_count;assert(n<=1);if(n)assert(!strcmp(f->capability,"input.touch.raw"));return true;}
static bool reset(void*c){(void)c;++io_count;return true;}
static uint64_t now(void*c){(void)c;return scene_test_millis();}
static void sleep_ms(void*c,uint32_t n){(void)c;(void)n;assert(0);}
static const risc_display_output_api_v1 display={1,sizeof(display),NULL,info,acquire,release,submit,display_status,NULL,NULL};
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,navigation,foreground,reset};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,now,sleep_ms};
#include "RiscBluetoothHidV1.h"
static unsigned alarm_phase,alarm_acks;static bool alarm_output;
static bool hid_active;static unsigned opens,closes,reports,neutral_reports;static uint8_t reported_key;
static bool hid_open(void*c,const char*n,bool pair,uint64_t*t){(void)c;(void)pair;++io_count;assert(!hid_active&&!strcmp(n,"BLE Buttons"));hid_active=true;alarm_phase=0;*t=++opens;return true;}
static bool hid_poll(void*c,uint64_t t,uint32_t n){(void)c;++io_count;assert(hid_active&&t==opens&&n==8);return true;}
static bool hid_status(void*c,uint64_t t,risc_bluetooth_hid_status_v1*out){(void)c;++io_count;assert(hid_active&&t==opens);*out=(risc_bluetooth_hid_status_v1){sizeof(*out),RISC_HID_READY,15,0,0,opens};return true;}
static bool hid_confirm(void*c,uint64_t t,bool a){(void)c;(void)t;(void)a;assert(0);return false;}
static bool hid_keyboard(void*c,uint64_t t,uint8_t m,const uint8_t keys[6]){(void)c;++io_count;assert(hid_active&&t==opens&&!m&&keys[0]==76);for(unsigned i=1;i<6;i++)assert(!keys[i]);reported_key=keys[0];reports++;return true;}
static bool hid_mouse(void*c,uint64_t t,uint8_t b,int8_t x,int8_t y,int8_t w){(void)c;(void)t;(void)b;(void)x;(void)y;(void)w;assert(0);return false;}
static bool hid_release(void*c,uint64_t t){(void)c;++io_count;assert(hid_active&&t==opens);reported_key=0;neutral_reports++;return true;}
static bool hid_close(void*c,uint64_t t){(void)c;++io_count;assert(hid_active&&t==opens);reported_key=0;hid_active=false;closes++;return true;}
static bool hid_forget(void*c){(void)c;assert(!hid_active);return true;}
static bool hid_battery(void*c,uint64_t t,uint8_t p){(void)c;++io_count;assert(hid_active&&t==opens&&p==255);return true;}
static const risc_bluetooth_hid_v1 hid_api={1,sizeof(hid_api),NULL,hid_open,hid_poll,hid_status,hid_confirm,hid_keyboard,hid_mouse,hid_release,hid_close,hid_forget,hid_battery};
#include "AlarmServiceV2.h"
static alarm_token_v1 alarm_token(void){return (alarm_token_v1){ALARM_KIND_ALARM,1,1234,scene_test_invocation()};}
static int32_t alarm_status(void*c,alarm_status_v1*out){(void)c;++io_count;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=alarm_phase==1?ALARM_STATE_CUE:alarm_phase==2?ALARM_STATE_ALERT:alarm_phase==3?ALARM_STATE_DISMISSING:ALARM_STATE_READY,.mode=ALARM_MODE_SOUND};if(alarm_phase==2||alarm_phase==3){out->occurrence=alarm_token();strcpy(out->label,"Reserved alarm");}return 0;}
static int32_t alarm_step(void*c){(void)c;++io_count;if(!alarm_phase&&hid_active&&reported_key){alarm_phase=1;return ALARM_PENDING;}if(alarm_phase==1){assert(!hid_active&&!held&&!presenting);alarm_phase=2;alarm_output=true;}else if(alarm_phase==2){assert(!hid_active);alarm_output=true;}else if(alarm_phase==3){assert(!hid_active);alarm_output=false;alarm_phase=4;}return 0;}
static int32_t alarm_refresh(void*c){(void)c;return 0;}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){(void)c;alarm_token_v1 expected=alarm_token();assert(alarm_phase==2&&!memcmp(t,&expected,sizeof(expected)));alarm_acks++;alarm_phase=3;return 0;}
static int32_t alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;(void)t;assert(0);return ALARM_INVALID;}
static int32_t alarm_stop(void*c){(void)c;++io_count;alarm_output=false;return 0;}
static const alarm_service_descriptor_v2 alarm_api={{2,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_sleep,alarm_stop},ALARM_SERVICE_DESCRIPTOR_TAG,1,3,0,NULL};
const void*scene_test_hardware(unsigned id){const void*apis[]={&display,&touch,&nav,&hid_api,&clock_api,&alarm_api};assert(id<6);return apis[id];}
unsigned scene_test_hardware_io(void){return io_count;}
bool scene_test_storage_safe(void){return true;}
bool scene_test_exit_safe(void){return !hid_active&&!held&&!presenting;}
void scene_test_hardware_closed(void){if(submits){assert(opens==2&&reports>=(getenv("BUTTONS_ALARM")?2u:4u)&&neutral_reports>=(getenv("BUTTONS_ALARM")?2u:4u));if(getenv("BUTTONS_ALARM"))assert(alarm_acks==2&&!alarm_output);}assert(!hid_active&&!reported_key&&opens==closes);assert(!held&&!presenting&&!subscribed&&acquisitions==submits+releases&&completions==submits);}
