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
#include "PortableTextInputHandoff.h"
#include "AlarmServiceV1.h"
#include "PortableBluetoothHost.h"
#include "RiscBluetoothSensorsV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscTextEntryV1.h"
#include "RiscSceneV1.h"
#include "SceneProfileV1.h"
#include "ble_sensor_names.h"
static const risc_driver_v2 *sensor_driver,*scene_driver,*text_driver;
const risc_driver_v2 *ble_scene_driver_get(uint32_t);
const risc_driver_v2 *ble_text_driver_get(uint32_t);
const risc_driver_v2 *ble_profile_driver_get(uint32_t);
#include "WifiApi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void);int app_module_init(void);void app_module_fini(void);
static unsigned ticks,polls,grants,frames,subs,presents,text_grants,name_writes,grant_serial;
static bool text_granted;
#ifdef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
static bool native_retained;
static unsigned retained_grants,retained_subs,retained_frames;
static bool fake_retain_invocation(void){native_retained=true;retained_grants=grants;retained_subs=subs;retained_frames=frames;return true;}
#endif
#ifdef BLE_RESIDENT_RENDER
static bool runtime_lost;
#define CHECK_IO() assert(!runtime_lost&&!native_retained)
static bool loss_case(const char*name){const char*mode=getenv("BLE_RENDER_NAME");return mode&&!strcmp(mode,name);}
static const risc_text_entry_api_v1 text_proxy;
#else
#define CHECK_IO() ((void)0)
#endif
static bool is_text_api(const void*api){
#ifdef BLE_RESIDENT_RENDER
 return api==&text_proxy;
#else
 return api==text_driver->capability;
#endif
}
static risc_touch_event_v1 text_event;
static bool text_event_ready,text_down;
static uint64_t touch_sequence;
static int text_x,text_y;
static unsigned stop_poll=240;
static unsigned radio_sends,radio_claims,radio_closes,radio_state,launches,reports,radio_control_calls;static bool radio_owned;
static uint8_t command_ack[6];static bool ack_ready,advertising;
#ifdef BLE_PAPER_RENDER
#define PANEL_W 800
#define PANEL_H 480
#define TOUCH_W 480
#define TOUCH_H 800
#define PIXEL_FORMAT RISC_DISPLAY_FORMAT_MONO1
#define ROW_BYTES 100
#define STRIDE_BYTES 104
#else
#define PANEL_W 240
#define PANEL_H 240
#define TOUCH_W 240
#define TOUCH_H 240
#define PIXEL_FORMAT RISC_DISPLAY_FORMAT_RGB565
#define ROW_BYTES 480
#define STRIDE_BYTES 488
#endif
static unsigned char pixels[PANEL_H*STRIDE_BYTES];
static unsigned app_frames,host_frames;
#ifdef BLE_PAPER_RENDER
static bool physical_black(unsigned x,unsigned y){
 unsigned px=y,py=PANEL_H-1-x;
 return !!(pixels[py*STRIDE_BYTES+px/8]&(0x80u>>(px%8)));
}
#endif
static const char *directory;
static struct {unsigned at;int x,y;} actions[256];static unsigned action_count;
static struct {char key[16];unsigned char bytes[64];uint32_t size;} cells[32];
static void save_frame(void) {
 char path[1024];snprintf(path,sizeof(path),"%s/frame-%03u.ppm",directory,presents);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",TOUCH_W,TOUCH_H);
 for(unsigned y=0;y<TOUCH_H;y++)for(unsigned x=0;x<TOUCH_W;x++){
#ifdef BLE_PAPER_RENDER
  /* Read both presenters in the installed app's physical orientation. */
  unsigned px=y,py=PANEL_H-1-x;unsigned char v=(pixels[py*STRIDE_BYTES+px/8]&(0x80u>>(px%8)))?0:255;
  unsigned char rgb[]={v,v,v};
#else
  uint16_t v;memcpy(&v,pixels+y*STRIDE_BYTES+x*2,2);unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};
#endif
  assert(fwrite(rgb,1,3,f)==3);
 }
 for(unsigned y=0;y<PANEL_H;y++)for(unsigned x=ROW_BYTES;x<STRIDE_BYTES;x++)assert(pixels[y*STRIDE_BYTES+x]==0xa5);
 assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){CHECK_IO();h->uptime_ms=ticks;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 return true;
#else
 return polls<stop_poll;
#endif
}
static void fake_yield(uint32_t n){CHECK_IO();ticks+=n;}
static bool fake_diag(const char*s){CHECK_IO();fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){CHECK_IO();assert(!radio_owned&&!text_granted);assert(!strcmp(s,"springboard.elf"));launches++;printf("launch=%s\n",s);stop_poll=polls;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){CHECK_IO();(void)c;*s=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*s),.width=PANEL_W,.height=PANEL_H,.preferred_format=PIXEL_FORMAT,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(PIXEL_FORMAT),.flags=PIXEL_FORMAT==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE:0};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){CHECK_IO();(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=PANEL_W,.height=PANEL_H,.stride_bytes=STRIDE_BYTES,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){CHECK_IO();(void)c;assert(frames&&f==1);frames=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){CHECK_IO();(void)c;(void)r;(void)n;assert(frames&&f==1);frames=0;
#ifdef BLE_PAPER_RENDER
 assert(o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
#else
 (void)o;
#endif
 if(text_granted){
#ifdef BLE_PAPER_RENDER
 /* The visible Q-key frame must occupy the same physical coordinates used
  * by the installed portrait app and raw portrait touch source. */
 assert(physical_black(35,372));assert(!physical_black(39,376));
#endif
 host_frames++;
 }else app_frames++;*t=++presents;save_frame();return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){CHECK_IO();(void)c;assert(t);s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
static uint64_t fake_sub(void*c){CHECK_IO();(void)c;subs++;return 1;}
static bool fake_unsub(void*c,uint64_t n){CHECK_IO();(void)c;assert(n==1&&subs);
 if(text_granted&&host_frames&&getenv("BLE_RENDER_NAME")&&!strcmp(getenv("BLE_RENDER_NAME"),"retained"))return false;
 subs--;return true;}
static bool fake_touch_poll(void*c,size_t n){CHECK_IO();
 (void)c;assert(n==1||n==2);polls++;
 if(text_granted){
  bool down=false;int x=text_x,y=text_y;
  for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls&&actions[i].x>=0){down=true;x=actions[i].x;y=actions[i].y;}
  if(down||text_down){text_event=(risc_touch_event_v1){.sequence=++touch_sequence,.timestamp_ms=ticks,.kind=down?(text_down?RISC_TOUCH_EVENT_MOVE:RISC_TOUCH_EVENT_DOWN):RISC_TOUCH_EVENT_UP,.id=1,.x=(uint16_t)x,.y=(uint16_t)y};text_event_ready=true;}
  text_down=down;text_x=x;text_y=y;
 }
 return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){CHECK_IO();(void)c;assert(n==1);if(!text_event_ready)return 0;*e=text_event;text_event_ready=false;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){CHECK_IO();(void)c;*s=(risc_touch_snapshot_v1){.width=TOUCH_W,.height=TOUCH_H,.sequence=touch_sequence,.timestamp_ms=ticks};for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls&&actions[i].x>=0){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=actions[i].x,.y=actions[i].y};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){CHECK_IO();(void)c;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){CHECK_IO();(void)c;*s=(twatch_rtc_time_v1){2026,10,4,0,20,34,12};return true;}
static bool fake_write(void*c,const twatch_rtc_time_v1*s){CHECK_IO();(void)c;(void)s;assert(!"Unexpected RTC write in read-only audit");return false;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc,.write=fake_write};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){CHECK_IO();(void)c;*s=0;for(unsigned i=0;i<32;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);
#ifdef BLE_RESIDENT_RENDER
 if(text_grants&&!text_granted&&k[0]=='b'&&k[1]=='s'&&loss_case("loss-kv-get"))runtime_lost=true;
#endif
 return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){CHECK_IO();assert(!text_granted);if(k[0]=='b'&&k[1]=='s')name_writes++;(void)c;assert(n<=64&&strlen(k)<16);unsigned i;for(i=0;i<32&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<32);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;
#ifdef BLE_RESIDENT_RENDER
 if(text_grants&&!text_granted&&k[0]=='b'&&k[1]=='s'&&loss_case("loss-kv-put"))runtime_lost=true;
#endif
 return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){CHECK_IO();(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t fake_alarm_step(void*c){CHECK_IO();(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){CHECK_IO();(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){CHECK_IO();(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
#ifndef BLE_RESIDENT_RENDER
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step};
#else
#include "AlarmServiceV2.h"
static const alarm_service_descriptor_v2 alarm_api={.base={2,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
#endif
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){CHECK_IO();(void)c;*s=(risc_input_navigation_frame_v1){0};
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(polls>=stop_poll&&!text_granted)s->pressed=RISC_NAV_BACK;
#endif
 for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls){if(actions[i].x==-1)s->pressed=RISC_NAV_BACK;else if(actions[i].x==-2)s->pressed=RISC_NAV_HOME;}
 return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){CHECK_IO();(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){CHECK_IO();(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!"Unexpected hardware sleep in audit");return 0;}
static bool radio_enabled(void*c,bool on){CHECK_IO();(void)c;assert(!radio_owned);radio_control_calls++;radio_state=on?1:0;return true;}
static bool radio_status(void*c,uint8_t*out){CHECK_IO();(void)c;*out=(uint8_t)radio_state;return true;}
static bool radio_claim(void*c,uint64_t*out){CHECK_IO();(void)c;assert(!radio_owned);radio_owned=true;radio_state=1;radio_claims++;*out=1;reports=0;advertising=false;return true;}
static bool radio_send(void*c,uint64_t token,uint8_t type,const uint8_t*p,size_t n){CHECK_IO();(void)c;assert(radio_owned&&token==1&&type==1&&n==3u+p[2]);radio_sends++;uint8_t ack[]={0x0e,4,1,p[0],p[1],0};memcpy(command_ack,ack,6);ack_ready=true;if(p[0]==0x0c&&p[1]==0x20)advertising=true;return true;}
static int32_t radio_next(void*c,uint64_t token,uint8_t*type,uint8_t*p,size_t cap,size_t*n){CHECK_IO();
 (void)c;assert(radio_owned&&token==1&&cap>=1028);*type=4;
 if(ack_ready){memcpy(p,command_ack,6);*n=6;ack_ready=false;return 1;}
 if(advertising&&reports<6){uint8_t event[]={0x3e,30,2,1,0,1,1,2,3,4,5,6,18,4,9,'T','e','m',12,0x16,0xd2,0xfc,0x40,1,88,2,0xca,8,3,0x88,0x13,(uint8_t)-56};event[6]=(uint8_t)++reports;event[16]=(uint8_t)('0'+reports);memcpy(p,event,sizeof(event));*n=sizeof(event);return 1;}
 *n=0;return 0;
}
static int32_t radio_close(void*c,uint64_t token){CHECK_IO();(void)c;assert(radio_owned&&token==1);radio_owned=false;radio_closes++;advertising=ack_ready=false;return 1;}
static const portable_bluetooth_host_v1 radio_api={{1,sizeof(radio_api),NULL,NULL,NULL,radio_enabled,radio_status},radio_claim,radio_send,radio_next,radio_close};
static wifi_link_t wifi_status(void*c){(void)c;return WIFI_LINK_DOWN;}
static bool wifi_disconnect(void*c){CHECK_IO();(void)c;return true;}
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=wifi_status,.disconnect_checked=wifi_disconnect};
#ifdef BLE_RESIDENT_RENDER
#include "RiscRealtimeV1.h"
#include "PortableNativeTimeToolbar.h"
#include "RiscResidentShellV1.h"
#include "TelemetryBroadcastV1.h"
static unsigned realtime_reads,resident_polls,resident_controls,broadcast_pauses,broadcast_steps;
static bool realtime_live;
static int32_t fake_realtime_read(void*c,risc_realtime_snapshot_v1*s){CHECK_IO();(void)c;assert(realtime_live&&!native_retained);realtime_reads++;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=INT64_C(1791115200)+ticks/1000,.monotonic_before_us=(uint64_t)ticks*1000,.monotonic_after_us=(uint64_t)ticks*1000};return RISC_REALTIME_OK;}
static const risc_realtime_api_v1 realtime_api={1,sizeof(realtime_api),&realtime_live,fake_realtime_read};
static bool fake_broadcast_pause(void*c){CHECK_IO();(void)c;assert(!native_retained);broadcast_pauses++;return true;}
static bool fake_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){CHECK_IO();(void)c;(void)p;assert(!native_retained&&!allow);broadcast_steps++;return true;}
static bool fake_broadcast_status(void*c,telemetry_broadcast_status_v1*s){CHECK_IO();(void)c;assert(!native_retained);*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t fake_broadcast_enum(void*c,uint32_t i,risc_telemetry_field_v1*f){CHECK_IO();(void)c;(void)i;(void)f;assert(!native_retained);return 0;}
static int32_t fake_broadcast_read(void*c,uint32_t i,int32_t*v){CHECK_IO();(void)c;(void)i;(void)v;assert(!native_retained);return 0;}
static const telemetry_broadcast_v1 broadcast_api={1,sizeof(broadcast_api),NULL,fake_broadcast_step,fake_broadcast_pause,fake_broadcast_status,fake_broadcast_enum,fake_broadcast_read};
static int32_t fake_resident_checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){CHECK_IO();assert(!native_retained&&invocation==2&&q&&q->struct_size==sizeof(*q)&&r->struct_size==sizeof(*r)&&!frames);if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL)resident_polls++;if(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS){assert(!text_granted&&!radio_owned);resident_controls++;}return RISC_RESIDENT_OK;}
static bool fake_resident_get(risc_resident_client_v1*out){CHECK_IO();assert(!native_retained);*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=2,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=fake_resident_checkpoint};return true;}
#endif
#ifdef BLE_RESIDENT_RENDER
static int32_t proxy_text_open(void*c,const risc_text_entry_request_v1*q,uint64_t*t){(void)c;CHECK_IO();const risc_text_entry_api_v1*p=text_driver->capability;int32_t rc=p->open(p->context,q,t);if(loss_case("loss-open"))runtime_lost=true;return rc;}
static int32_t proxy_text_poll(void*c,uint64_t t,risc_text_entry_state_v1*q){(void)c;CHECK_IO();const risc_text_entry_api_v1*p=text_driver->capability;int32_t rc=p->poll(p->context,t,q);if(loss_case("loss-poll"))runtime_lost=true;return rc;}
static int32_t proxy_text_close(void*c,uint64_t t){(void)c;CHECK_IO();const risc_text_entry_api_v1*p=text_driver->capability;int32_t rc=p->close(p->context,t);if(loss_case("loss-close"))runtime_lost=true;return rc;}
static const risc_text_entry_api_v1 text_proxy={1,sizeof(text_proxy),NULL,proxy_text_open,proxy_text_poll,proxy_text_close};
#endif
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){CHECK_IO();(void)id;assert(g->struct_size==sizeof(*g));
#ifdef BLE_RESIDENT_RENDER
 assert(!native_retained);assert(strcmp(n,"rtc.clock")&&strcmp(n,"runtime.realtime-control")&&strcmp(n,"net.wifi"));
 if(!strcmp(n,"alarm.service"))assert(v==2);
 if(!strcmp(n,"runtime.realtime")){assert(v==1&&!id&&!realtime_live);realtime_live=true;g->api=&realtime_api;}
 else if(!strcmp(n,"telemetry.broadcast")){assert(v==1&&!id);g->api=&broadcast_api;}
 else if(!strcmp(n,"input.navigation")){assert(v==1);g->api=&nav_api;}
 else
#endif
if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&kv_api;else if(!strcmp(n,RISC_BLUETOOTH_SENSORS_CAPABILITY)&&v==1){assert(id==0);g->api=sensor_driver->capability;}else if(!strcmp(n,"bluetooth.hci")&&v==1)g->api=&radio_api;else if(!strcmp(n,"net.wifi")&&v==1)g->api=&wifi_api;else if(!strcmp(n,"alarm.service")&&v==
#ifdef BLE_RESIDENT_RENDER
 2
#else
 1
#endif
 )g->api=&alarm_api;else if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)&&v==1){
 if(getenv("BLE_RENDER_NAME")&&!strcmp(getenv("BLE_RENDER_NAME"),"unavailable"))return false;
 #ifdef BLE_RESIDENT_RENDER
 if(loss_case("loss-acquire-fail")){runtime_lost=true;return false;}
#endif
 assert(!text_granted);text_granted=true;text_grants++;
#ifdef BLE_RESIDENT_RENDER
 g->api=&text_proxy;
#else
 g->api=text_driver->capability;
#endif
 }else return false;g->slot=++grant_serial;g->generation=1;grants++;
#ifdef BLE_RESIDENT_RENDER
 if((is_text_api(g->api)&&loss_case("loss-acquire"))||(g->api==&kv_api&&text_grants&&!text_granted&&loss_case("loss-kv-acquire")))runtime_lost=true;
#endif
 return true;}
static bool fake_release(risc_runtime_capability_v1*g){CHECK_IO();assert(g->api&&grants);
#ifdef BLE_RESIDENT_RENDER
 assert(!native_retained);if(g->api==&realtime_api){assert(realtime_live);realtime_live=false;}
#endif
bool released_text=is_text_api(g->api);bool released_kv=g->api==&kv_api;
 if(released_text){assert(text_driver->quiesce()&&!frames);text_granted=false;}
#ifndef BLE_RESIDENT_RENDER
 (void)released_kv;
#endif
*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};grants--;
#ifdef BLE_RESIDENT_RENDER
 if((released_text&&loss_case("loss-release"))||(released_kv&&text_grants&&!text_granted&&loss_case("loss-kv-release")))runtime_lost=true;
#endif
 return true;}
static const risc_runtime_api_v1 runtime_api={
#ifdef BLE_RESIDENT_RENDER
 .resident_shell=fake_resident_get,
#endif
 .api_version=1,.struct_size=sizeof(runtime_api),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=fake_acquire,.release=fake_release,
#ifdef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
 .retain_invocation=fake_retain_invocation,
#endif
};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){
#ifdef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
 if(native_retained)return NULL;
#ifdef BLE_RESIDENT_RENDER
 if(runtime_lost)return NULL;
#endif
#endif
 return v==1?&runtime_api:NULL;}
static uint64_t sensor_now(void*c){CHECK_IO();(void)c;return ticks;}
int main(int argc,char**argv){assert(argc>=2);directory=argv[1];memset(pixels,0xa5,sizeof(pixels));if(argc>2){FILE*f=fopen(argv[2],"r");assert(f);while(action_count<256&&fscanf(f,"%u %d %d",&actions[action_count].at,&actions[action_count].x,&actions[action_count].y)==3)action_count++;fclose(f);}uint8_t policy[]={0x51,1,3,0xa6};if(getenv("BLE_RENDER_ENABLE")){policy[2]=1;policy[3]=0xa4;radio_state=0;}assert(fake_put(NULL,"quick_radio",policy,4)==0);
risc_platform_clock_api_v1 clock={1,sizeof(clock),NULL,sensor_now,NULL};risc_provider_dependency_v1 deps[]={{"bluetooth.hci",1,&radio_api},{"platform.clock",1,&clock}};
sensor_driver=t5_driver_get(2);assert(sensor_driver&&sensor_driver->start(deps,2));
scene_driver=ble_scene_driver_get(2);text_driver=ble_text_driver_get(2);assert(scene_driver&&text_driver);
const risc_driver_v2 *profile_driver=ble_profile_driver_get(2);assert(profile_driver&&profile_driver->start(NULL,0));
const risc_scene_profile_v1 *profile=profile_driver->capability;
risc_provider_dependency_v1 scene_deps[]={{"display.output",1,&display_api},{"input.touch.raw",1,&touch_api},{"input.navigation",1,&nav_api},{"platform.clock",1,&clock},{"ui.presentation-profile",1,profile}};
assert(scene_driver->start(scene_deps,5));risc_provider_dependency_v1 text_deps[]={{RISC_SCENE_CAPABILITY,1,scene_driver->capability}};assert(text_driver->start(text_deps,1));
uint8_t named[]={6,2,3,4,5,6};assert(ble_alias_save(&kv_api,1,named,"Kitchen sensor"));name_writes=0;
assert(app_module_init()==0);app_main();
#ifdef BLE_RESIDENT_RENDER
/* The paper app owns its header. Independently exercise its linked native-time
 * callback after clean app return, before adapter finalization. */
if(!native_retained){twatch_rtc_time_v1 local={0};assert(portable_app_native_local_time(&local));}
#endif
app_module_fini();
#ifdef BLE_RESIDENT_RENDER
if(runtime_lost){assert(native_retained&&portable_text_adapter_retained()&&!launches);assert(grants==retained_grants&&subs==retained_subs&&frames==retained_frames);assert(app_module_init()!=0);puts("Runtime loss: silent retention, frozen resources, no further provider/runtime I/O, and re-entry refused");return 0;}
#endif
if(getenv("BLE_RENDER_NAME")&&!strcmp(getenv("BLE_RENDER_NAME"),"retained")){
 assert(portable_text_adapter_retained()&&text_granted&&grants&&subs&&!frames&&!name_writes&&!launches);
 assert(!scene_driver->quiesce()&&!text_driver->quiesce());
 assert(app_module_init()!=0);
#ifdef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
 assert(native_retained&&grants==retained_grants&&subs==retained_subs&&frames==retained_frames);
#endif
 puts("Actual BLE app_main retained failed shared-host cleanup; fini did not free grants/subscriptions; re-entry refused");return 0;
}
assert(!grants&&!frames&&!subs&&!radio_owned);
#ifdef BLE_RESIDENT_RENDER
 fprintf(stderr,"resident evidence: realtime=%u polls=%u pauses=%u steps=%u live=%u\n",realtime_reads,resident_polls,broadcast_pauses,broadcast_steps,realtime_live);
 assert(!realtime_live&&realtime_reads>0&&resident_polls>0&&broadcast_pauses>0&&broadcast_steps>0);
 if(getenv("BLE_RENDER_HOME"))assert(!launches&&polls<stop_poll);
 if(getenv("BLE_RENDER_CONTROLS"))assert(resident_controls==1);
#endif
if(getenv("BLE_RENDER_SCAN"))assert(radio_claims>=1&&radio_sends==5&&radio_closes==radio_claims);else assert(!radio_claims&&!radio_sends);
if(getenv("BLE_RENDER_BACK"))assert(launches==1&&polls>=60);
if(getenv("BLE_RENDER_QUICK"))assert(radio_control_calls>=2);
if(getenv("BLE_RENDER_ENABLE"))assert(radio_control_calls==1&&radio_state==1);
if(getenv("BLE_RENDER_NAME")){
 const char*mode=getenv("BLE_RENDER_NAME");char alias[BLE_ALIAS_MAX+1];assert(ble_alias_load(&kv_api,1,named,alias)==1);
 if(!strcmp(mode,"unavailable")){assert(!text_grants&&!name_writes&&!strcmp(alias,"Kitchen sensor"));}
 else if(!strcmp(mode,"repeat")){assert(text_grants==3&&name_writes==2&&!strcmp(alias,"Kitchen sensorWW"));}
 else{assert(text_grants==1);if(!strcmp(mode,"save")){assert(name_writes==1&&!strcmp(alias,"Kitchen sensorW"));}else assert(!name_writes&&!strcmp(alias,"Kitchen sensor"));}
}
assert(app_frames>0);if(text_grants)assert(host_frames>0);assert(presents>0);assert(sensor_driver->quiesce());sensor_driver->stop();assert(text_driver->quiesce());text_driver->stop();assert(scene_driver->quiesce());scene_driver->stop();assert(profile_driver->quiesce());profile_driver->stop();
printf("Production app/adapter capture: %u frames, %u polls, %u ms; all grants released; stride guards intact\n",presents,polls,ticks);return 0;}
