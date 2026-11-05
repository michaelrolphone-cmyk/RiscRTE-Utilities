/* Production app coordinate mapping; fake controller/HID, no radio claim. */
#define HID_TOUCHPAD 0
#define PORTABLE_WATCH_NATIVE_HEIGHT
#define main original_hid_app_test_main
#include "hid_app_test.c"
#undef main
static int32_t wave_width(void){return 410;}
static int32_t wave_height(void){return 502;}
static risc_touch_event_v1 events[16];static unsigned pos,end;
static int32_t wave_next(void*c,uint64_t t,risc_touch_event_v1*e){(void)c;assert(t==33);if(pos==end)return 0;*e=events[pos++];return 1;}
int main(void){
 reset();t5_app_api_v1 wave=fake_app;wave.screen_width=wave_width;wave.screen_height=wave_height;app=&wave;sample.width=410;sample.height=502;
 start_session(false);assert(token);tick=20;pump();assert(!button_gate);
 risc_touch_api_v1 input=touch_api;input.next=wave_next;raw=&input;
 unsigned x[4]={60,180,60,180},y[4]={100,100,160,160};
 for(unsigned i=0;i<8;i++){
  unsigned b=i%4;uint16_t px=(x[b]*410+239)/240,py=(y[b]*410+239)/240;
  events[i*2]=(risc_touch_event_v1){.sequence=++raw_seq,.timestamp_ms=25+i*2,.kind=RISC_TOUCH_EVENT_DOWN,.id=1,.x=px,.y=py};
  events[i*2+1]=events[i*2];events[i*2+1].sequence=++raw_seq;events[i*2+1].timestamp_ms++;events[i*2+1].kind=RISC_TOUCH_EVENT_UP;
 }end=16;tick=45;
 for(unsigned i=0;i<8;i++){pump();tick+=20;}
 assert(key_reports==16&&report_count==16);
 for(unsigned i=0;i<8;i++){assert(reports[i*2].key==assignments[i%4].key&&reports[i*2].mod==assignments[i%4].modifiers);assert(reports[i*2+1].key==0&&reports[i*2+1].mod==0);}
 stop_session("done");assert(!grants&&!raw_subs&&!token);puts("Wave 410x502: four assignments twice, exact queued press/neutral order PASS");
}
