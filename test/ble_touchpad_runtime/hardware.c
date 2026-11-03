#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscBluetoothHidV1.h"
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
extern int scene_test_pointer_position(int*,int*);
static risc_touch_event_v1 input_event;static bool input_pending;static risc_touch_snapshot_v1 input_state;
static unsigned gesture_step;static uint64_t gesture_due,input_sequence;
static bool hid_active;static uint8_t reported_buttons;
static void touch_edge(unsigned kind,unsigned id,int x,int y){
 input_event=(risc_touch_event_v1){++input_sequence,scene_test_millis(),(uint8_t)kind,(uint8_t)id,(uint16_t)x,(uint16_t)y};input_pending=true;
 unsigned at=0;while(at<input_state.contact_count&&input_state.contacts[at].id!=id)++at;
 if(kind==RISC_TOUCH_EVENT_DOWN){assert(at==input_state.contact_count&&at<2);input_state.contact_count++;}
 else assert(at<input_state.contact_count);
 if(kind==RISC_TOUCH_EVENT_UP)input_state.contacts[at]=input_state.contacts[--input_state.contact_count];
 else input_state.contacts[at]=(risc_touch_contact_v1){.id=id,.x=x,.y=y};
}
static bool touch_poll(void*c,size_t n){
 (void)c;++io_count;assert(n<=2&&!input_pending);
 if(nav_index!=1||!hid_active||scene_test_observe(3)!=RISC_HID_READY||scene_test_observe(5)||held||presenting||scene_test_millis()<gesture_due)return true;
 int x,y;if(!scene_test_pointer_position(&x,&y))return true;
 static const struct{unsigned kind,id;int x,y;unsigned wait;} trace[]={
 {1,1,0,0,40},{3,1,0,0,40},{1,1,0,0,40},{2,1,24,0,700},{3,1,24,0,350},
 {1,1,0,0,40},{2,1,10,0,40},{3,1,10,0,350},
 {1,1,0,0,40},{1,2,20,0,40},{3,1,0,0,40},{3,2,20,0,350},
 {1,1,-10,0,40},{1,2,10,0,40},{2,1,22,0,40},{2,2,42,0,40},{3,1,22,0,40},{3,2,42,0,350}
 };
 if(gesture_step<sizeof(trace)/sizeof(trace[0])){
  unsigned i=gesture_step++;touch_edge(trace[i].kind,trace[i].id,x+trace[i].x,y+trace[i].y);gesture_due=scene_test_millis()+trace[i].wait;
 }else {nav_index=2;}
 return true;
}
static int32_t touch_next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;++io_count;assert(id==1&&subscribed);if(!input_pending)return 0;*out=input_event;input_pending=false;return 1;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;++io_count;*out=input_state;out->sequence=input_sequence;out->timestamp_ms=scene_test_millis();out->width=scene_test_width();out->height=scene_test_height();return true;}
static bool navigation(void*c,risc_input_navigation_frame_v1*out){
 (void)c;++io_count;unsigned invocation=scene_test_invocation();
 if(invocation!=previous_invocation){previous_invocation=invocation;polls=0;nav_index=nav_wait=gesture_step=0;gesture_due=0;input_state=(risc_touch_snapshot_v1){0};invocation_completions=completions;}
 *out=(risc_input_navigation_frame_v1){0};
 if(++polls>10000){fprintf(stderr,"stuck phase=%u page=%u focus=%u token=%u state=%u terminal=%u dirty=%u reset=%u\n",nav_index,scene_test_observe(1),scene_test_observe(0),scene_test_observe(2),scene_test_observe(3),scene_test_observe(4),scene_test_observe(6),scene_test_observe(5));assert(0);}
 if(held||presenting||completions<=invocation_completions||++nav_wait<4)return true;
 nav_wait=0;
 if(scene_test_observe(7)){unsigned target=scene_test_observe(111);if(target)out->pressed=scene_test_observe(0)==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;nav_index=3;return true;}
 if(nav_index==0){
  unsigned target=scene_test_observe(103);if(!target)return true;
  if(scene_test_observe(0)!=target)out->pressed=RISC_NAV_DOWN;
  else {out->pressed=RISC_NAV_CONFIRM;nav_index=1;}return true;
 }
 if(nav_index==1)return true;
 if(!scene_test_observe(7)){out->pressed=RISC_NAV_HOME;nav_index++;}return true;
}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;++io_count;assert(n<=1);if(n)assert(!strcmp(f->capability,"input.touch.raw"));return true;}
static bool reset(void*c){(void)c;++io_count;return true;}
static uint64_t now(void*c){(void)c;return scene_test_millis();}
static void sleep_ms(void*c,uint32_t n){(void)c;(void)n;assert(0);}
static const risc_display_output_api_v1 display={1,sizeof(display),NULL,info,acquire,release,submit,display_status,NULL,NULL};
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,navigation,foreground,reset};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,now,sleep_ms};
static unsigned alarm_phase,alarm_acks;static bool alarm_output;
static unsigned opens,closes,reports,neutral_reports,clicks,drags,scrolls;static int motion_x;
static bool hid_open(void*c,const char*n,bool pair,uint64_t*t){(void)c;(void)pair;++io_count;assert(!hid_active&&!strcmp(n,"BLE Touchpad"));hid_active=true;alarm_phase=0;*t=++opens;return true;}
static bool hid_poll(void*c,uint64_t t,uint32_t n){(void)c;++io_count;assert(hid_active&&t==opens&&n==8);return true;}
static bool hid_status(void*c,uint64_t t,risc_bluetooth_hid_status_v1*out){(void)c;++io_count;assert(hid_active&&t==opens);*out=(risc_bluetooth_hid_status_v1){sizeof(*out),RISC_HID_READY,15,0,0,opens};return true;}
static bool hid_confirm(void*c,uint64_t t,bool a){(void)c;(void)t;(void)a;assert(0);return false;}
static bool hid_keyboard(void*c,uint64_t t,uint8_t m,const uint8_t keys[6]){(void)c;(void)t;(void)m;(void)keys;assert(0);return false;}
static bool hid_mouse(void*c,uint64_t t,uint8_t b,int8_t x,int8_t y,int8_t w){(void)c;(void)y;(void)w;++io_count;assert(hid_active&&t==opens);if(b&&!reported_buttons)clicks++;if(b&&x)drags++;reported_buttons=b;motion_x+=x;reports++;return true;}
static bool hid_scroll(void*c,uint64_t t,uint8_t b,int16_t x,int16_t y,int16_t wx,int16_t wy){if(wx||wy)scrolls++;return hid_mouse(c,t,b,(int8_t)x,(int8_t)y,0);}
static bool hid_release(void*c,uint64_t t){(void)c;++io_count;assert(hid_active&&t==opens);reported_buttons=0;neutral_reports++;return true;}
static bool hid_close(void*c,uint64_t t){(void)c;++io_count;assert(hid_active&&t==opens);reported_buttons=0;hid_active=false;closes++;return true;}
static bool hid_forget(void*c){(void)c;assert(!hid_active);return true;}
static bool hid_battery(void*c,uint64_t t,uint8_t p){(void)c;++io_count;assert(hid_active&&t==opens&&p==255);return true;}
static const risc_bluetooth_hid_scroll_api_v1 hid_api={.base={.base={1,sizeof(hid_api),NULL,hid_open,hid_poll,hid_status,hid_confirm,hid_keyboard,hid_mouse,hid_release,hid_close,hid_forget,hid_battery}},.mouse_scroll=hid_scroll};
#include "AlarmServiceV2.h"
static alarm_token_v1 alarm_token(void){return (alarm_token_v1){ALARM_KIND_ALARM,1,1234,scene_test_invocation()};}
static int32_t alarm_status(void*c,alarm_status_v1*out){(void)c;++io_count;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=alarm_phase==1?ALARM_STATE_CUE:alarm_phase==2?ALARM_STATE_ALERT:alarm_phase==3?ALARM_STATE_DISMISSING:ALARM_STATE_READY,.mode=ALARM_MODE_SOUND};if(alarm_phase==2||alarm_phase==3){out->occurrence=alarm_token();strcpy(out->label,"Reserved alarm");}return 0;}
static int32_t alarm_step(void*c){(void)c;++io_count;if(!alarm_phase&&hid_active&&reported_buttons){alarm_phase=1;return ALARM_PENDING;}if(alarm_phase==1){assert(!hid_active&&!held&&!presenting);alarm_phase=2;alarm_output=true;}else if(alarm_phase==2){assert(!hid_active);alarm_output=true;}else if(alarm_phase==3){assert(!hid_active);alarm_output=false;alarm_phase=4;}return 0;}
static int32_t alarm_refresh(void*c){(void)c;return 0;}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){(void)c;alarm_token_v1 expected=alarm_token();assert(alarm_phase==2&&!memcmp(t,&expected,sizeof(expected)));alarm_acks++;alarm_phase=3;return 0;}
static int32_t alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;(void)t;assert(0);return ALARM_INVALID;}
static int32_t alarm_stop(void*c){(void)c;++io_count;alarm_output=false;return 0;}
static const alarm_service_descriptor_v2 alarm_api={{2,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_sleep,alarm_stop},ALARM_SERVICE_DESCRIPTOR_TAG,1,3,0,NULL};
const void*scene_test_hardware(unsigned id){const void*apis[]={&display,&touch,&nav,&hid_api,&clock_api,&alarm_api};assert(id<6);return apis[id];}
unsigned scene_test_hardware_io(void){return io_count;}
bool scene_test_storage_safe(void){return true;}
bool scene_test_exit_safe(void){return !hid_active&&!held&&!presenting;}
void scene_test_hardware_closed(void){
 if(submits){assert(opens==2&&reports>=4&&neutral_reports>=2);if(getenv("TOUCHPAD_ALARM"))assert(alarm_acks==2&&!alarm_output);else assert(clicks>=6&&drags>=2&&scrolls>=2&&motion_x>0);}
 assert(!hid_active&&!reported_buttons&&opens==closes);assert(!held&&!presenting&&!subscribed&&acquisitions==submits+releases&&completions==submits);
}
