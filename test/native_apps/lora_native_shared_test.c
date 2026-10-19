/* Native LoRa app + adapter + text/scene/profile providers. Only physical services
 * are fixtures. The production app is included verbatim below so final state
 * assertions can observe its private state without introducing test hooks. */
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "RiscKeyValueV1.h"
#include "PortableNavigation.h"
#include "PortableAppSleep.h"
#include "PortableTextInputHandoff.h"
#include "RiscPlatformClockV1.h"
#include "RiscTextEntryV1.h"
#include "RiscSceneV1.h"
#include "SceneProfileV1.h"
#include "RiscProviderV2.h"
#include "TelemetryBroadcastV1.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
#include "lora_messages_model.h"
#include "lora_messages_native_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void); int app_module_init(void); void app_module_fini(void);
const risc_driver_v2 *lora_scene_driver_get(uint32_t),*lora_text_driver_get(uint32_t),*lora_profile_driver_get(uint32_t);
static const risc_driver_v2 *scene_driver,*text_driver,*profile_driver;
static risc_text_entry_api_v1_long text_proxy;
static const risc_text_entry_api_v1_long *real_text;
static unsigned ticks,polls,grant_count,lease_count,subs,presents,app_frames,host_frames,host_session_frames;
static unsigned host_attempts;
static unsigned app_delay_ms,display_failures;
static unsigned text_grants,text_opens,text_closes,text_close_again,grant_serial,launches;
static unsigned radio_sends,radio_configures,radio_cancels,radio_receives,radio_state,selected_profile;
static bool radio_owned,text_granted,native_retained,runtime_lost,in_host_call;
static unsigned retained_grants,retained_subs,retained_frames,retained_calls,io_calls,retained_allocations,allocation_count;
static void *allocations[64];
void *__real_malloc(size_t);void __real_free(void *);
void *__wrap_malloc(size_t n){void *p=__real_malloc(n);if(p){unsigned i;for(i=0;i<64&&allocations[i];i++);assert(i<64);allocations[i]=p;allocation_count++;}return p;}
void __wrap_free(void *p){if(p){unsigned i;for(i=0;i<64&&allocations[i]!=p;i++);assert(i<64&&allocation_count);allocations[i]=NULL;allocation_count--;}__real_free(p);}
static unsigned stop_at=90000,pending_token,pending_until,delay_ms;
static unsigned touch_head,touch_count,touch_edges,action_cursor,action_count,next_action_at;
static uint64_t touch_sequence;
static bool touch_down; static int touch_x,touch_y; static uint32_t navigation_pressed;
static unsigned display_width=240,display_height=240,display_stride=244;
static uint16_t guarded_pixels[516*512+8];
static uint16_t *pixels=guarded_pixels+4;
static const char *directory,*mode;
static risc_touch_event_v1 touch_events[64];
static struct {unsigned delay;char gate[16],kind[16];int x,y;} actions[4096];
static struct {char key[32];unsigned char bytes[128];uint32_t size;} cells[64];
static char opened_text[32][256],opened_label[32][40],terminal_text[32][256];
static uint32_t opened_capacity[32],terminal_state[32];
static char sent_text[241];static size_t sent_length;
static bool reviewed_bytes[240];static unsigned reviewed_lines;
static void verify_capture(int after);
static void trace_state(void);
static bool fail_display_status(void);
static bool is_mode(const char*s){return !strcmp(mode,s);}
#define IO() do{assert(!native_retained&&!runtime_lost);io_calls++;}while(0)
static void verify_guards(void){
 for(unsigned i=0;i<4;i++)assert(guarded_pixels[i]==0xa5a5);
 for(unsigned y=0;y<display_height;y++)for(unsigned x=display_width;x<display_stride;x++)assert(pixels[y*display_stride+x]==0xa5a5);
 for(unsigned i=display_height*display_stride+4;i<sizeof(guarded_pixels)/sizeof(*guarded_pixels);i++)assert(guarded_pixels[i]==0xa5a5);
}
static void save_frame(const char *name){
 verify_guards();
 char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",display_width,display_height);
 for(unsigned y=0;y<display_height;y++){for(unsigned x=0;x<display_width;x++){uint16_t v=pixels[y*display_stride+x];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}}
 assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){IO();h->uptime_ms=ticks;return true;}
static void fake_yield(uint32_t n){IO();ticks+=n;assert(ticks<120000&&"script failed to finish");}
static bool fake_diag(const char*s){IO();fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){IO();fprintf(stderr,"launch ms=%u radio=%u text=%u lease=%u pending=%u until=%u\n",ticks,radio_owned,text_granted,lease_count,pending_token,pending_until);assert(!radio_owned&&!text_granted&&!lease_count&&!pending_token);assert(!strcmp(s,"springboard.elf"));launches++;return true;}
static bool fake_retain(void){assert(!native_retained);native_retained=true;retained_grants=grant_count;retained_subs=subs;retained_frames=lease_count;retained_calls=io_calls;retained_allocations=allocation_count;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){IO();(void)c;*s=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*s),.width=display_width,.height=display_height,.preferred_format=RISC_DISPLAY_FORMAT_RGB565,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=delay_ms?RISC_DISPLAY_INFO_ASYNC_PRESENT:0};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){IO();(void)c;assert(!lease_count&&!pending_token&&(!text_granted||in_host_call));lease_count=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=display_width,.height=display_height,.stride_bytes=display_stride*2,.size_bytes=display_stride*display_height*2,.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){IO();(void)c;assert(lease_count&&f==1);lease_count=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){IO();(void)c;(void)r;(void)n;(void)o;assert(lease_count&&f==1&&!pending_token);lease_count=0;*t=++presents;pending_token=*t;pending_until=ticks+(text_granted?delay_ms:app_delay_ms);verify_guards();if(text_granted){host_frames++;if(host_frames==1)save_frame("keyboard");}else{app_frames++;if(app_frames==1)save_frame("app");}return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){IO();(void)c;assert(t);if(fail_display_status()){display_failures++;return false;}s->state=ticks<pending_until?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(s->state==RISC_DISPLAY_PRESENT_COMPLETE)pending_token=0;return true;}
static bool fake_wait(void*c,risc_display_present_token_v1 t,uint32_t ms,risc_display_present_status_v1*s){if(ticks<pending_until){unsigned wait=pending_until-ticks;if(wait>ms)wait=ms;fake_yield(wait);}return fake_present(c,t,s);}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present,.wait_present=fake_wait};
static uint64_t fake_sub(void*c){IO();(void)c;assert(!subs);subs=1;touch_head=touch_count=0;return 1;}
static bool fake_unsub(void*c,uint64_t n){IO();(void)c;assert(n==1&&subs);if(text_granted&&host_frames&&(is_mode("retained")||is_mode("retained-listen")))return false;subs=0;touch_head=touch_count=0;return true;}
static bool action_ready(const char *gate){if(!strcmp(gate,"text"))return text_granted&&host_frames>host_session_frames;if(!strcmp(gate,"pending"))return text_granted&&pending_token;assert(!strcmp(gate,"app"));return !text_granted&&app_frames;}
static bool fake_touch_poll(void*c,size_t n){IO();(void)c;assert(n==1||n==2);polls++;
 if(action_cursor>=action_count||ticks<next_action_at||!action_ready(actions[action_cursor].gate))return true;
 const char *kind=actions[action_cursor].kind;
 if(!strcmp(kind,"back")||!strcmp(kind,"home")||!strcmp(kind,"back-held")){assert((!touch_down||!strcmp(kind,"back-held"))&&!navigation_pressed);navigation_pressed=strcmp(kind,"home")?RISC_NAV_BACK:RISC_NAV_HOME;}
 else if(!strcmp(kind,"capture")){save_frame(actions[action_cursor].x?"capture-after":"capture-before");verify_capture(actions[action_cursor].x);}
 else{unsigned event_kind;if(!strcmp(kind,"down")){assert(!touch_down);touch_down=true;event_kind=RISC_TOUCH_EVENT_DOWN;}else{assert(!strcmp(kind,"up")&&touch_down);touch_down=false;event_kind=RISC_TOUCH_EVENT_UP;}
 touch_x=actions[action_cursor].x;touch_y=actions[action_cursor].y;assert(touch_x>=0&&touch_x<(int)display_width&&touch_y>=0&&touch_y<(int)display_height&&subs==1&&touch_count<64);
 touch_events[(touch_head+touch_count++)%64]=(risc_touch_event_v1){.sequence=++touch_sequence,.timestamp_ms=ticks,.kind=event_kind,.id=1,.x=(uint16_t)touch_x,.y=(uint16_t)touch_y};touch_edges++;}
 trace_state();fprintf(stderr,"input %u %s %s %d %d\n",ticks,actions[action_cursor].gate,kind,actions[action_cursor].x,actions[action_cursor].y);
 if(++action_cursor<action_count)next_action_at=ticks+actions[action_cursor].delay;else stop_at=ticks+900;return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){IO();(void)c;assert(n==1&&subs);if(!touch_count)return 0;*e=touch_events[touch_head];touch_head=(touch_head+1)%64;touch_count--;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){IO();(void)c;*s=(risc_touch_snapshot_v1){.width=display_width,.height=display_height,.sequence=touch_sequence,.timestamp_ms=ticks};if(touch_down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)touch_x,.y=(uint16_t)touch_y};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){IO();(void)c;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){IO();(void)c;*s=(twatch_rtc_time_v1){2026,10,12,1,14,0,0};return true;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){IO();(void)c;*s=0;for(unsigned i=0;i<64;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){IO();(void)c;assert(n<=128&&strlen(k)<32&&!text_granted);unsigned i;for(i=0;i<64&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<64);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){IO();(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t fake_alarm_step(void*c){IO();(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){IO();(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){IO();(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){IO();(void)c;*s=(risc_input_navigation_frame_v1){.pressed=navigation_pressed};navigation_pressed=0;if(ticks>=stop_at&&!text_granted)s->pressed|=RISC_NAV_HOME;return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){IO();(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){IO();(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){IO();(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){IO();(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){IO();(void)r;(void)d;(void)b;(void)a;assert(!"unexpected hardware sleep");return 0;}
int portable_app_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){return portable_app_alarm_sleep(r,d,b,NULL);}
uint32_t portable_app_sleep_budget_ms(void){return 900000;}
static bool radio_configure(void*c,const twatch_lora_config_v2*p){IO();(void)c;assert(lm_profile_valid(p));radio_owned=true;radio_configures++;return true;}
static bool radio_send(void*c,const uint8_t*b,size_t n,uint32_t ms){IO();(void)c;assert(radio_owned&&!text_granted&&b&&n&&n<=240&&ms==60000);radio_sends++;radio_state=TW_LORA_TX;memcpy(sent_text,b,n);sent_text[n]=0;sent_length=n;return true;}
static bool radio_receive(void*c,uint32_t ms){IO();(void)c;assert(radio_owned&&ms==5000);radio_receives++;radio_state=TW_LORA_RX;return true;}
static bool radio_poll(void*c,twatch_lora_status_v2*s){IO();(void)c;assert(radio_owned);*s=(twatch_lora_status_v2){.state=radio_state==TW_LORA_TX?(is_mode("send-cancel")?TW_LORA_TX:TW_LORA_SENT):TW_LORA_RX};return true;}
static bool radio_read(void*c,uint8_t*b,size_t n,size_t*got){IO();(void)c;(void)b;(void)n;*got=0;return false;}
static bool radio_cancel(void*c){IO();(void)c;assert(radio_owned);radio_owned=false;radio_cancels++;return true;}
static bool radio_info(void*c,twatch_lora_profile_info_v1*out){IO();(void)c;assert(out->struct_size==sizeof(*out));*out=(twatch_lora_profile_info_v1){.struct_size=sizeof(*out),.supported_profiles=15,.selected_profile=selected_profile};return true;}
static bool radio_select(void*c,uint32_t choice){IO();(void)c;assert(!radio_owned);selected_profile=choice;return true;}
static twatch_radio_api_v2 radio_api={2,sizeof(radio_api),NULL,radio_configure,radio_send,radio_receive,radio_poll,radio_read,radio_cancel,radio_info,radio_select};
static int32_t proxy_open(void*c,const risc_text_entry_request_v1_long*q,uint64_t*t){IO();(void)c;assert(text_opens<32&&!lease_count&&!pending_token);strcpy(opened_text[text_opens],q->text);strcpy(opened_label[text_opens],q->label);opened_capacity[text_opens++]=q->capacity;in_host_call=true;int32_t rc=real_text->open_long(real_text->base.base.base.base.context,q,t);in_host_call=false;if(is_mode("loss-open"))runtime_lost=true;return rc;}
static int32_t proxy_poll(void*c,uint64_t t,risc_text_entry_state_v1_long*q){IO();(void)c;in_host_call=true;int32_t rc=real_text->poll_long(real_text->base.base.base.base.context,t,q);in_host_call=false;if(rc==RISC_TEXT_ENTRY_OK){assert(strlen(q->text)<opened_capacity[text_opens-1]);strcpy(terminal_text[text_opens-1],q->text);terminal_state[text_opens-1]=q->state;}if(is_mode("loss-poll"))runtime_lost=true;return rc;}
static int32_t proxy_close(void*c,uint64_t t){IO();(void)c;in_host_call=true;int32_t rc=real_text->base.base.base.base.close(real_text->base.base.base.base.context,t);in_host_call=false;text_closes++;if(rc==RISC_TEXT_ENTRY_AGAIN)text_close_again++;if(is_mode("loss-close"))runtime_lost=true;return rc;}
static uint8_t ble_enabled;
static bool fake_ble_enable(void*c,bool on){IO();(void)c;ble_enabled=on;return true;}
static bool fake_ble_status(void*c,uint8_t*s){IO();(void)c;*s=ble_enabled;return true;}
static const portable_bluetooth_control_v1 ble_api={.api_version=1,.struct_size=sizeof(ble_api),.set_enabled=fake_ble_enable,.status=fake_ble_status};
static wifi_link_t fake_wifi_status(void*c){IO();(void)c;return WIFI_LINK_DOWN;}
static bool fake_wifi_disconnect(void*c){IO();(void)c;return true;}
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=fake_wifi_status,.disconnect_checked=fake_wifi_disconnect};
static bool fake_broadcast_pause(void*c){IO();(void)c;return true;}
static bool fake_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){IO();(void)c;(void)allow;(void)p;return true;}
static bool fake_broadcast_status(void*c,telemetry_broadcast_status_v1*s){IO();(void)c;*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t fake_broadcast_enum(void*c,uint32_t i,risc_telemetry_field_v1*f){IO();(void)c;(void)i;(void)f;return 0;}
static int32_t fake_broadcast_read(void*c,uint32_t i,int32_t*v){IO();(void)c;(void)i;(void)v;return 0;}
static const telemetry_broadcast_v1 broadcast_api={1,sizeof(broadcast_api),NULL,fake_broadcast_step,fake_broadcast_pause,fake_broadcast_status,fake_broadcast_enum,fake_broadcast_read};
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){IO();(void)id;assert(g->struct_size==sizeof(*g));
 if(!strcmp(n,"net.wifi")&&v==1)g->api=&wifi_api;else if(!strcmp(n,"bluetooth.hci")&&v==1)g->api=&ble_api;else if(!strcmp(n,"telemetry.broadcast")&&v==1)g->api=&broadcast_api;else if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&kv_api;else if(!strcmp(n,"radio.lora")&&v==2)g->api=&radio_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else if(!strcmp(n,"input.navigation")&&v==1)g->api=&nav_api;
 else if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)&&v==1){host_attempts++;if(is_mode("missing-retry")&&host_attempts==1)return false;if(is_mode("old-retry"))text_proxy.base.base.base.base.struct_size=host_attempts==1?sizeof(risc_text_entry_api_v1_extended):sizeof(text_proxy);if(is_mode("missing-host")||(is_mode("missing-reopen")&&text_opens))return false;assert(!text_granted);text_granted=true;text_grants++;host_session_frames=host_frames;g->api=&text_proxy;if(is_mode("old-reopen")&&text_opens)text_proxy.base.base.base.base.struct_size=sizeof(risc_text_entry_api_v1_extended);}else return false;
 g->slot=++grant_serial;g->generation=1;grant_count++;return true;
}
static bool fake_release(risc_runtime_capability_v1*g){IO();assert(g->api&&grant_count);if(g->api==&text_proxy){assert(text_driver->quiesce()&&!lease_count&&!pending_token);text_granted=false;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};grant_count--;return true;}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=fake_acquire,.release=fake_release,.retain_invocation=fake_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return !native_retained&&!runtime_lost&&v==1?&runtime_api:NULL;}
static uint64_t clock_now(void*c){IO();(void)c;return ticks;}
#include "../../Apps/lora_messages.c"
void __real_portable_nova_native_text(unsigned,int,int,int,const char*,uint32_t);
void __wrap_portable_nova_native_text(unsigned face,int x,int y,int width,const char *value,uint32_t color){
 if(is_mode("review-240")&&page==PAGE_COMPOSE&&strlen(draft)==240&&x==lm_geometry.body.x+2&&y>=lm_geometry.body.y&&y<lm_geometry.body.y+lm_geometry.body.h&&color==NOVA_TEXT){
  assert(face==lm_native_face()&&width==lm_geometry.body.w-4);
  assert(portable_nova_measure(face,value)<=width);
  assert((y-lm_geometry.body.y)%lm_geometry.line_height==0);
  unsigned row=(unsigned)(y-lm_geometry.body.y)/lm_geometry.line_height;
  unsigned start=compose_offset+row*lm_native_columns();size_t count=strlen(value);
  assert(start+count<=240&&!memcmp(draft+start,value,count));
  for(unsigned i=0;i<count;i++)reviewed_bytes[start+i]=true;
  reviewed_lines++;
 }
 __real_portable_nova_native_text(face,x,y,width,value,color);
}
static bool fail_display_status(void){return is_mode("exit-display-failure")&&page==PAGE_HOME&&ticks>=stop_at&&pending_token;}
static void trace_state(void){
 uint64_t generation=0,sequence=0;bool ready=portable_lora_native_ready(&generation,&sequence);
 fprintf(stderr,"state page=%u native-ready=%u generation=%llu visible-sequence=%llu pending=%u\n",page,ready,(unsigned long long)generation,(unsigned long long)sequence,pending_token);
}
static void verify_capture(int after){
 if(is_mode("header-layout-cancel"))assert(page==PAGE_HOME&&!launches);
 if(is_mode("header-slide-cancel"))assert(page==PAGE_COMPOSE&&!launches);
 fprintf(stderr,"capture page=%u history=%u detail-length=%u sends=%u draft-length=%zu offset=%u\n",page,model.count,detail.length,radio_sends,strlen(draft),compose_offset);
 if(is_mode("draft-done")||is_mode("draft-repeat")||is_mode("draft-cancel")||is_mode("draft-header-back")||is_mode("delayed-done")||is_mode("delayed-back")||is_mode("rapid-pending"))assert(page==PAGE_COMPOSE&&!text_active);
 if(is_mode("send-history")||is_mode("send-cancel"))assert(page==PAGE_MESSAGE&&detail.length==1);
 if(is_mode("rf-recover-all"))assert(page==(after?PAGE_HOME:PAGE_KEYBOARD)&&!text_active);
 if(is_mode("rf-recover"))assert(page==(after?PAGE_HOME:PAGE_KEYBOARD)&&!text_active);
 if(is_mode("rf-invalid-back"))assert(page==(after?PAGE_RF:PAGE_KEYBOARD)&&!text_active);
}
static void expect_num(const char *key,unsigned value){const char *s=getenv(key);if(s&&value!=(unsigned)strtoul(s,NULL,10)){fprintf(stderr,"%s expected %s actual %u\n",key,s,value);abort();}}
static void print_rect(const char *name,lm_native_rect r,bool comma){printf("%s\"%s\":[%d,%d,%d,%d]",comma?",":"",name,r.x,r.y,r.w,r.h);}
static void print_layout(unsigned w,unsigned h){
 lm_native_layout l;assert(lm_native_layout_make(&l,(int)w,(int)h));putchar('{');print_rect("back",l.back,false);print_rect("full_footer",l.full_footer,true);
 char key[24];for(unsigned i=0;i<4;i++){snprintf(key,sizeof(key),"home%u",i);print_rect(key,l.home[i],true);}
 for(unsigned i=0;i<2;i++){snprintf(key,sizeof(key),"footer%u",i);print_rect(key,l.footer[i],true);}
 for(unsigned i=0;i<3;i++){snprintf(key,sizeof(key),"rf%u",i);print_rect(key,l.rf[i],true);}
 print_rect("history0",l.history[0],true);print_rect("body",l.body,true);lm_geometry=l;printf(",\"compose_step\":%u,\"detail_step\":%u",lm_native_compose_step(),lm_native_detail_step());puts("}");
}
int main(int argc,char**argv){if(argc==4&&!strcmp(argv[1],"--layout")){print_layout((unsigned)atoi(argv[2]),(unsigned)atoi(argv[3]));return 0;}assert(argc==6);directory=argv[1];mode=argv[3];display_width=(unsigned)atoi(argv[4]);display_height=(unsigned)atoi(argv[5]);assert(display_width>=160&&display_width<=512&&display_height>=160&&display_height<=512);display_stride=display_width+4;memset(guarded_pixels,0xa5,sizeof(guarded_pixels));FILE*f=fopen(argv[2],"r");assert(f);while(action_count<4096&&fscanf(f,"%u %15s %15s %d %d",&actions[action_count].delay,actions[action_count].gate,actions[action_count].kind,&actions[action_count].x,&actions[action_count].y)==5)action_count++;assert(feof(f));fclose(f);if(action_count)next_action_at=actions[0].delay;else stop_at=1000;
 const char *delayed=getenv("LORA_TEST_DELAY_MS");delay_ms=delayed?(unsigned)atoi(delayed):0;delayed=getenv("LORA_TEST_APP_DELAY_MS");app_delay_ms=delayed?(unsigned)atoi(delayed):0;
 twatch_lora_config_v2 p={.frequency_hz=915000000,.bandwidth_hz=125000,.preamble=8,.sf=7,.coding_rate=5,.power_dbm=0};uint8_t record[32];lm_profile_encode(&p,3,true,record);assert(fake_put(NULL,LM_PROFILE_KEY,record,32)==0);
 uint8_t policy[]={0x51,1,3,0xa6};assert(fake_put(NULL,"quick_radio",policy,4)==0);
 risc_platform_clock_api_v1 clock={1,sizeof(clock),NULL,clock_now,NULL};scene_driver=lora_scene_driver_get(2);text_driver=lora_text_driver_get(2);profile_driver=lora_profile_driver_get(2);assert(scene_driver&&text_driver&&profile_driver&&profile_driver->start(NULL,0));
 risc_provider_dependency_v1 scene_deps[]={{"display.output",1,&display_api},{"input.touch.raw",1,&touch_api},{"input.navigation",1,&nav_api},{"platform.clock",1,&clock},{"ui.presentation-profile",1,profile_driver->capability}};
 assert(scene_driver->start(scene_deps,5));risc_provider_dependency_v1 text_deps[]={{RISC_SCENE_CAPABILITY,1,scene_driver->capability}};assert(text_driver->start(text_deps,1));real_text=text_driver->capability;assert(risc_text_entry_long(&real_text->base.base.base.base));text_proxy=*real_text;text_proxy.open_long=proxy_open;text_proxy.poll_long=proxy_poll;text_proxy.base.base.base.base.close=proxy_close;if(is_mode("old-host"))text_proxy.base.base.base.base.struct_size=sizeof(risc_text_entry_api_v1_extended);
 assert(app_module_init()==0);app_main();app_module_fini();
 if(is_mode("retained")||is_mode("retained-listen")||is_mode("exit-display-failure")||!strncmp(mode,"loss-",5)){assert(native_retained&&portable_text_adapter_retained()&&!launches);assert(grant_count==retained_grants&&subs==retained_subs&&lease_count==retained_frames&&io_calls==retained_calls&&allocation_count==retained_allocations);assert(app_module_init()!=0&&io_calls==retained_calls);assert(!radio_sends);if(is_mode("exit-display-failure"))assert(display_failures==1&&pending_token);if(is_mode("retained-listen"))assert(radio_owned&&radio_receives&&!radio_cancels);puts("PASS retained invocation: resources frozen, zero later I/O, reentry refused");return 0;}
 assert(!grant_count&&!lease_count&&!subs&&!radio_owned&&!text_granted&&!pending_token);assert(action_cursor==action_count&&!touch_down&&!navigation_pressed);
 expect_num("LORA_EXPECT_PAGE",page);expect_num("LORA_EXPECT_OPENS",text_opens);expect_num("LORA_EXPECT_SENDS",radio_sends);expect_num("LORA_EXPECT_HISTORY",model.count);expect_num("LORA_EXPECT_LENGTH",(unsigned)strlen(draft));expect_num("LORA_EXPECT_CLOSE_AGAIN",text_close_again);
 const char *want=getenv("LORA_EXPECT_DRAFT");if(want){fprintf(stderr,"draft='%s' expected='%s'\n",draft,want);assert(!strcmp(draft,want));}
 if(!is_mode("listen-edit"))assert(radio_configures==radio_sends);
 if(radio_sends){assert(sent_length==strlen(draft)&&!strcmp(sent_text,draft));assert(model.count==1&&model.history[0].length==sent_length);assert(model.history[0].status==(is_mode("send-cancel")?LM_OUT_UNKNOWN:LM_OUT_SENT));}
 for(unsigned i=0;i<text_opens;i++){assert(opened_capacity[i]==(!strcmp(opened_label[i],"COMPOSE MESSAGE")?241:12));printf("session %u capacity %u initial-length %zu terminal %u length %zu\n",i,opened_capacity[i],strlen(opened_text[i]),terminal_state[i],strlen(terminal_text[i]));}
 if(is_mode("draft-repeat")||is_mode("slow-display")){assert(text_opens==3&&!strcmp(opened_text[1],"Q")&&!strcmp(opened_text[2],"Q"));assert(!strcmp(terminal_text[1],"QW"));}
 if(is_mode("review-240")){assert(strlen(draft)==240&&!compose_offset&&reviewed_lines);for(unsigned i=0;i<240;i++)assert(reviewed_bytes[i]);printf("reviewed all 240 bytes in %u measured native lines, step=%u\n",reviewed_lines,lm_native_compose_step());}
 if(is_mode("missing-retry")||is_mode("old-retry"))assert(host_attempts==2&&text_opens==1);
 if(is_mode("limit-240")){assert(strlen(terminal_text[0])==240&&strlen(draft)==240&&text_opens==2&&strlen(opened_text[1])==240);}
 if(is_mode("rf-limits")){assert(text_opens==6&&rf_fields==63&&model.profile.frequency_hz==915000000&&editing_profile.frequency_hz==915000000&&model.profile.bandwidth_hz==125000&&model.profile.sf==7&&model.profile.coding_rate==5&&model.profile.power_dbm==0&&model.profile.preamble==8);for(unsigned i=0;i<6;i++)assert(opened_capacity[i]==12&&strlen(terminal_text[i])==11);}
 if(is_mode("rf-invalid-back")){assert(text_opens==2&&!strcmp(opened_text[1],"915000000")&&model.profile.frequency_hz==915000000&&editing_profile.frequency_hz==915000000);}
 if(is_mode("slow-display")||is_mode("delayed-done")||is_mode("delayed-home")||is_mode("delayed-back")||is_mode("rapid-pending"))assert(text_close_again>0);
 if(is_mode("rf-recover")){assert(text_opens==2&&!strcmp(opened_text[1],"915000000Q")&&editing_profile.frequency_hz==916000000&&model.profile.frequency_hz==916000000&&model.profile_valid);}
 if(is_mode("rf-recover-all")){const char*invalid[]={"0","333","1","2","99","1"};assert(text_opens==12);for(unsigned i=0;i<6;i++)assert(!strcmp(opened_text[i*2+1],invalid[i]));}
 if(is_mode("rf-all")||is_mode("rf-recover-all")){assert(text_opens==(is_mode("rf-recover-all")?12u:6u)&&model.profile.frequency_hz==916000000&&model.profile.bandwidth_hz==250000&&model.profile.sf==8&&model.profile.coding_rate==6&&model.profile.power_dbm==-2&&model.profile.preamble==16);}
 if(is_mode("old-host")||is_mode("missing-host"))assert(!text_opens&&!radio_sends&&!host_frames);
 if(is_mode("listen-edit"))assert(radio_receives&&radio_cancels==1&&!radio_sends);
 verify_guards();save_frame("final");assert(app_frames>0);if(text_opens)assert(host_frames>0);assert(text_driver->quiesce());text_driver->stop();assert(scene_driver->quiesce());scene_driver->stop();assert(profile_driver->quiesce());profile_driver->stop();assert(!allocation_count);
 printf("PASS real LoRa app/adapter/shared hosts: mode=%s frames=%u app=%u host=%u input=%u/%u edges=%u ms=%u sends=%u configures=%u cancel=%u close-again=%u; leases and stride guards clean\n",mode,presents,app_frames,host_frames,action_cursor,action_count,touch_edges,ticks,radio_sends,radio_configures,radio_cancels,text_close_again);return 0;
}
