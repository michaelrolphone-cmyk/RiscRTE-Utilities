/* Real RF app + adapter + text/scene/profile providers. Only physical services
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
#include "radio_iq_v1.h"
#include "RiscRealtimeV1.h"
#include "RiscResidentShellV1.h"
#include "RiscAppDataV1.h"
#include "ContextsServiceV1.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void); int app_module_init(void); void app_module_fini(void);
const risc_driver_v2 *rf_scene_driver_get(uint32_t),*rf_text_driver_get(uint32_t),*rf_profile_driver_get(uint32_t);
static const risc_driver_v2 *scene_driver,*text_driver,*profile_driver;
static risc_text_entry_api_v1_checkpoint text_proxy;
static const risc_text_entry_api_v1 *real_text;
static unsigned ticks,polls,grant_count,lease_count,subs,presents,app_frames,host_frames,host_session_frames;
static unsigned text_attempts,text_grants,text_opens,text_closes,text_close_again,grant_serial,launches,fallback_backs,rf_store_reads,rf_store_acquires;
static unsigned radio_captures,radio_host_captures,radio_suspends,resident_polls,resident_policies,kv_writes,data_writes,editor,capture_serial,unavailable_frames;
static unsigned open_reads[32],close_reads[32],open_analysis[32],close_analysis[32],first_close_reads[32],first_close_analysis[32],session_closes[32];
static bool radio_owned,text_granted,native_retained,runtime_lost,in_host_call;
static unsigned retained_grants,retained_subs,retained_frames,retained_calls,io_calls,retained_allocations,allocation_count;
static void *allocations[4096];
void *__real_malloc(size_t);void __real_free(void *);
void *__wrap_malloc(size_t n){void *p=__real_malloc(n);if(p){unsigned i;for(i=0;i<4096&&allocations[i];i++);assert(i<4096);allocations[i]=p;allocation_count++;}return p;}
void __wrap_free(void *p){if(p){unsigned i;for(i=0;i<4096&&allocations[i]!=p;i++);assert(i<4096&&allocation_count);allocations[i]=NULL;allocation_count--;}__real_free(p);}
/* If a scenario has not exited explicitly, walk normal Back navigation to root. */
static unsigned stop_at=90000,pending_token,pending_until,delay_ms;
static int kv_fault,data_fault;
static unsigned touch_head,touch_count,touch_edges,action_cursor,action_count,next_action_at;
static uint64_t touch_sequence;
static bool touch_down; static int touch_x,touch_y; static uint32_t navigation_pressed;
#ifdef RF_TEST_PAPER
#define WIDTH 800u
#define HEIGHT 480u
#define STRIDE 104u
#define FORMAT RISC_DISPLAY_FORMAT_MONO1
#define VIEW_WIDTH 480u
#define VIEW_HEIGHT 800u
#else
#ifndef RF_TEST_WIDTH
#define RF_TEST_WIDTH 240u
#define RF_TEST_HEIGHT 240u
#endif
#define WIDTH RF_TEST_WIDTH
#define HEIGHT RF_TEST_HEIGHT
#define STRIDE (2u*(WIDTH+4u))
#define FORMAT RISC_DISPLAY_FORMAT_RGB565
#define VIEW_WIDTH WIDTH
#define VIEW_HEIGHT HEIGHT
#endif
static struct {unsigned char before[32],pixels[HEIGHT*STRIDE],after[32];} raster;
#define frame_pixels raster.pixels
static unsigned pending_hash;
static unsigned raster_hash(void){unsigned h=2166136261u;for(size_t i=0;i<sizeof(frame_pixels);i++)h=(h^frame_pixels[i])*16777619u;return h;}
static const char *directory,*mode;
static risc_touch_event_v1 touch_events[64];
static struct {unsigned delay;char gate[16],kind[16];int x,y;} actions[4096];
static struct {char key[32];unsigned char bytes[2048];uint32_t size;} cells[64];
static char opened_text[32][256],opened_label[32][40],terminal_text[32][256];
static uint32_t opened_capacity[32],terminal_state[32];
static void setup_editor(int which,int initial);
static void setup_scene(int which);
static unsigned analysis_count(void);
static void observe_analysis(void);
static void app_touch_position(int *x,int *y);
static bool fixture_live(void);
static void verify_pending_draft(void);
static void verify_capture(int after);
static bool is_mode(const char*s){return !strcmp(mode,s);}
#define IO() do{assert(!native_retained&&!runtime_lost);io_calls++;}while(0)
static void check_guards(void){
 for(unsigned i=0;i<32;i++)assert(raster.before[i]==0xa5&&raster.after[i]==0xa5);
 for(unsigned y=0;y<HEIGHT;y++)for(unsigned x=(FORMAT==RISC_DISPLAY_FORMAT_MONO1?WIDTH/8u:WIDTH*2u);x<STRIDE;x++)assert(frame_pixels[y*STRIDE+x]==0xa5);
}
static void save_frame(const char *name){
 check_guards();char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",VIEW_WIDTH,VIEW_HEIGHT);
 for(unsigned y=0;y<VIEW_HEIGHT;y++)for(unsigned x=0;x<VIEW_WIDTH;x++){
#ifdef RF_TEST_PAPER
  unsigned nx=y,ny=479u-x;unsigned char v=(frame_pixels[ny*STRIDE+nx/8u]&(0x80u>>(nx%8u)))?0u:255u;unsigned char rgb[]={v,v,v};
#else
  uint16_t v;memcpy(&v,frame_pixels+y*STRIDE+x*2u,2);unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};
#endif
  assert(fwrite(rgb,1,3,f)==3);
 }
 assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){IO();h->uptime_ms=ticks;return true;}
static void fake_yield(uint32_t n){IO();observe_analysis();ticks+=n;assert(ticks<240000&&"script failed to finish");}
static bool fake_diag(const char*s){IO();fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){IO();assert(!radio_owned&&!text_granted&&!lease_count&&!pending_token);assert(!strcmp(s,"springboard.elf")||!strcmp(s,"default.elf"));const char *want=getenv("RF_EXPECT_LAUNCH_TARGET");if(want)assert(!strcmp(s,want));launches++;return !(getenv("RF_LAUNCH_REFUSE")&&launches==1);}
static bool fake_retain(void){assert(!native_retained);native_retained=true;retained_grants=grant_count;retained_subs=subs;retained_frames=lease_count;retained_calls=io_calls;retained_allocations=allocation_count;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){IO();(void)c;*s=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*s),.width=WIDTH,.height=HEIGHT,.preferred_format=FORMAT,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(FORMAT),.flags=(delay_ms?RISC_DISPLAY_INFO_ASYNC_PRESENT:0)|(FORMAT==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE:0)};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){IO();(void)c;assert(!lease_count&&!pending_token&&(!text_granted||in_host_call));if(text_granted&&getenv("RF_NO_PRESENT")){unavailable_frames++;return false;}lease_count=1;*s=(risc_display_surface_v1){.frame=1,.pixels=frame_pixels,.width=WIDTH,.height=HEIGHT,.stride_bytes=STRIDE,.size_bytes=sizeof(frame_pixels),.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){IO();(void)c;assert(lease_count&&f==1);lease_count=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){IO();(void)c;(void)r;(void)n;(void)o;assert(lease_count&&f==1&&!pending_token);lease_count=0;*t=++presents;pending_token=*t;pending_hash=raster_hash();pending_until=ticks+(text_granted?delay_ms:0);if(text_granted){host_frames++;save_frame("keyboard");}else{app_frames++;save_frame("app");}return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){IO();(void)c;assert(t);if(pending_token)assert(raster_hash()==pending_hash);s->state=ticks<pending_until?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(s->state==RISC_DISPLAY_PRESENT_COMPLETE)pending_token=0;return true;}
static bool fake_wait(void*c,risc_display_present_token_v1 t,uint32_t ms,risc_display_present_status_v1*s){if(ticks<pending_until){unsigned wait=pending_until-ticks;if(wait>ms)wait=ms;fake_yield(wait);}return fake_present(c,t,s);}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present,.wait_present=fake_wait};
static uint64_t fake_sub(void*c){IO();(void)c;assert(!subs);subs=1;touch_head=touch_count=0;return 1;}
static bool fake_unsub(void*c,uint64_t n){IO();(void)c;assert(n==1&&subs);if(text_granted&&host_frames&&is_mode("retained"))return false;subs=0;touch_head=touch_count=0;return true;}
static bool action_ready(const char *gate){if(!strcmp(gate,"text"))return text_granted&&(host_frames>host_session_frames||getenv("RF_NO_PRESENT"));if(!strcmp(gate,"pending"))return text_granted&&pending_token;assert(!strcmp(gate,"app"));return !text_granted&&app_frames;}
static bool fake_touch_poll(void*c,size_t n){IO();(void)c;assert(n==1||n==2);polls++;
 if(action_cursor>=action_count||ticks<next_action_at||!action_ready(actions[action_cursor].gate))return true;
 const char *kind=actions[action_cursor].kind;
 if(!strcmp(kind,"setup")){setup_editor(actions[action_cursor].x,actions[action_cursor].y);}
 else if(!strcmp(kind,"scene")){setup_scene(actions[action_cursor].x);}
 else if(!strcmp(kind,"idle")){}
 else if(!strcmp(kind,"fault")){if(actions[action_cursor].x==1)kv_fault=actions[action_cursor].y;else {assert(actions[action_cursor].x==2);data_fault=actions[action_cursor].y;}}
 else if(!strcmp(kind,"nav")){assert(!touch_down&&!navigation_pressed);navigation_pressed=(uint32_t)actions[action_cursor].x;}
 else if(!strcmp(kind,"back")||!strcmp(kind,"home")){assert(!touch_down&&!navigation_pressed);navigation_pressed=!strcmp(kind,"back")?RISC_NAV_BACK:RISC_NAV_HOME;}
 else if(!strcmp(kind,"capture")){verify_capture(actions[action_cursor].x);char name[64];snprintf(name,sizeof(name),"capture-%02u",capture_serial++);save_frame(name);}
 else{unsigned event_kind;if(!strcmp(kind,"down")){assert(!touch_down);touch_down=true;event_kind=RISC_TOUCH_EVENT_DOWN;}else{assert(!strcmp(kind,"up")&&touch_down);touch_down=false;event_kind=RISC_TOUCH_EVENT_UP;}
 touch_x=actions[action_cursor].x;touch_y=actions[action_cursor].y;if(!strcmp(actions[action_cursor].gate,"app"))app_touch_position(&touch_x,&touch_y);
#ifdef RF_TEST_NATIVE_HEIGHT
 else if(touch_y==20){touch_x=touch_x>120?VIEW_WIDTH-20:20;}else{touch_x=touch_x*VIEW_WIDTH/240;touch_y=touch_y*VIEW_HEIGHT/240;}
#endif
 assert(touch_x>=0&&touch_x<(int)VIEW_WIDTH&&touch_y>=0&&touch_y<(int)VIEW_HEIGHT&&subs==1&&touch_count<64);
 touch_events[(touch_head+touch_count++)%64]=(risc_touch_event_v1){.sequence=++touch_sequence,.timestamp_ms=ticks,.kind=event_kind,.id=1,.x=(uint16_t)touch_x,.y=(uint16_t)touch_y};touch_edges++;}
 fprintf(stderr,"input %u %s %s %d %d\n",ticks,actions[action_cursor].gate,kind,actions[action_cursor].x,actions[action_cursor].y);
 if(++action_cursor<action_count)next_action_at=ticks+actions[action_cursor].delay;else stop_at=ticks+900;return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){IO();(void)c;assert(n==1&&subs);if(!touch_count)return 0;*e=touch_events[touch_head];touch_head=(touch_head+1)%64;touch_count--;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){IO();(void)c;*s=(risc_touch_snapshot_v1){.width=VIEW_WIDTH,.height=VIEW_HEIGHT,.sequence=touch_sequence,.timestamp_ms=ticks};if(touch_down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)touch_x,.y=(uint16_t)touch_y};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){IO();(void)c;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){IO();(void)c;*s=(twatch_rtc_time_v1){2026,10,12,1,14,0,0};return true;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){IO();(void)c;if(!strncmp(k,"rf_",3))rf_store_reads++;*s=0;for(unsigned i=0;i<64;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){IO();(void)c;assert(n<=2048&&strlen(k)<32&&!text_granted);kv_writes++;if(kv_fault)return RISC_KEY_VALUE_IO;unsigned i;for(i=0;i<64&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<64);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 legacy_kv_api={1,sizeof(legacy_kv_api),NULL,fake_get,fake_put};
static const risc_key_value_v1 kv_api={2,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){IO();(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t fake_alarm_step(void*c){IO();(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){IO();(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){IO();(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){IO();(void)c;*s=(risc_input_navigation_frame_v1){.pressed=navigation_pressed};navigation_pressed=0;if(ticks>=stop_at&&!text_granted){s->pressed|=RISC_NAV_BACK;fallback_backs++;}return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){IO();(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){IO();(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){IO();(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){IO();(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){IO();(void)r;(void)d;(void)b;(void)a;assert(!"unexpected hardware sleep");return 0;}
int portable_app_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){return portable_app_alarm_sleep(r,d,b,NULL);}
uint32_t portable_app_sleep_budget_ms(void){return 60000;}
static bool fake_radio_suspend(void*c){IO();(void)c;radio_suspends++;radio_owned=false;return true;}
static int fake_radio_capture(void*c,uint32_t*p,uint32_t count){IO();(void)c;assert(count>=256&&count<=8192&&!(count&(count-1)));radio_owned=true;radio_captures++;if(text_granted)radio_host_captures++;
 for(unsigned i=0;i<count;i++){double t=6.283185307179586*20*i/256.0;int a=(int)lround(120*cos(t)),b=(int)lround(120*sin(t));p[i]=((unsigned)a&1023u)|(((unsigned)b&1023u)<<10);}
 return RISC_RADIO_IQ_OK;
}
static bool fake_radio_caps(void*c,risc_radio_iq_capabilities_v1*out){IO();(void)c;assert(out->struct_size==sizeof(*out));*out=(risc_radio_iq_capabilities_v1){.struct_size=sizeof(*out),.flags=7,.controls=511,.min_pairs=1,.max_pairs=8192,.center_min_hz=1841666667u,.center_max_hz=2790000000u,.sample_rates_hz={16000000,80000000},.bandwidths_hz={20000000,40000000},.gain_selector_max=127,.rf_gain_max=511,.bb_gain_max=127,.filter_mask=0x3f3f,.dc_max=511,.iq_correction_mask=0x3f1f,.automatic_value=UINT32_MAX,.sample_format=1,.component_bits=10,.component_full_scale=512,.defaults=RISC_RADIO_IQ_SETTINGS_DEFAULT};return true;}
static int fake_radio_configured(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){assert(s&&s->struct_size==sizeof(*s)&&f&&f->struct_size==sizeof(*f));int result=fake_radio_capture(c,p,n);if(!result)*f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=7,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=n,.sample_format=1,.component_bits=10,.component_full_scale=512};return result;}
static const risc_radio_iq_extended_api_v1 radio_api={.base={{1,sizeof(radio_api),NULL,fake_radio_capture,fake_radio_suspend},NULL,NULL},.capabilities=fake_radio_caps,.capture_configured=fake_radio_configured};
static uint8_t test_disk[4][72000];static uint32_t test_sizes[4];static uint64_t test_revision[4]={1,1,1,1};
static unsigned saved_hash,saved_kv_writes,saved_data_writes,saved_store_reads,saved_store_acquires;
static unsigned persistent_hash(void){unsigned h=2166136261u;const unsigned char*p=(const unsigned char*)cells;for(size_t i=0;i<sizeof(cells);i++)h=(h^p[i])*16777619u;p=(const unsigned char*)test_sizes;for(size_t i=0;i<sizeof(test_sizes);i++)h=(h^p[i])*16777619u;p=(const unsigned char*)test_disk;for(size_t i=0;i<sizeof(test_disk);i++)h=(h^p[i])*16777619u;return h;}
static unsigned test_bank(const char*n){if(!strcmp(n,"context-fingerprints.cfp"))return 3;if(!strcmp(n,"rf-neural.rnn"))return 2;assert(!strcmp(n,"rf-events-a.rft")||!strcmp(n,"rf-events-b.rft"));return !strcmp(n,"rf-events-b.rft");}
static int32_t test_stat(void*c,const char*n,uint32_t*z,uint64_t*r){IO();rf_store_reads++;(void)c;unsigned b=test_bank(n);*z=test_sizes[b];*r=*z?test_revision[b]:0;return *z?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t test_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*z,uint64_t*r){IO();rf_store_reads++;(void)c;unsigned b=test_bank(n);*z=0;*r=0;if(expected!=test_revision[b])return RISC_APP_DATA_STALE;assert(cap>=test_sizes[b]);memcpy(out,test_disk[b],test_sizes[b]);*z=test_sizes[b];*r=test_revision[b];return 0;}
static int32_t test_replace(void*c,const char*n,uint64_t expected,const void*in,uint32_t z){IO();(void)c;assert(!radio_owned&&!text_granted);unsigned b=test_bank(n);assert(expected==(test_sizes[b]?test_revision[b]:0)&&z<=sizeof(test_disk[b]));data_writes++;if(data_fault)return RISC_APP_DATA_NO_SPACE;memcpy(test_disk[b],in,z);test_sizes[b]=z;test_revision[b]++;return 0;}
static const risc_app_data_v1 app_data_api={1,sizeof(app_data_api),NULL,test_stat,test_read,test_replace};
static int32_t proxy_open(void*c,const risc_text_entry_request_v1*q,uint64_t*t){IO();(void)c;assert(text_opens<32&&!lease_count&&!pending_token);open_reads[text_opens]=radio_captures;open_analysis[text_opens]=analysis_count();strcpy(opened_text[text_opens],q->text);strcpy(opened_label[text_opens],q->label);opened_capacity[text_opens++]=q->capacity;in_host_call=true;int32_t rc=real_text->open(real_text->context,q,t);in_host_call=false;if(is_mode("unavailable-open")){assert(real_text->close(real_text->context,*t)==RISC_TEXT_ENTRY_OK);*t=0;return RISC_TEXT_ENTRY_UNAVAILABLE;}if(is_mode("loss-open"))runtime_lost=true;return rc;}
static int32_t proxy_poll(void*c,uint64_t t,risc_text_entry_state_v1*q){IO();(void)c;in_host_call=true;int32_t rc=real_text->poll(real_text->context,t,q);in_host_call=false;if(rc==RISC_TEXT_ENTRY_OK){assert(strlen(q->text)<opened_capacity[text_opens-1]);strcpy(terminal_text[text_opens-1],q->text);terminal_state[text_opens-1]=q->state;}if(is_mode("loss-poll"))runtime_lost=true;return rc;}
static int32_t proxy_close(void*c,uint64_t t){IO();(void)c;in_host_call=true;int32_t rc=real_text->close(real_text->context,t);in_host_call=false;if(!session_closes[text_opens-1]++){first_close_reads[text_opens-1]=radio_captures;first_close_analysis[text_opens-1]=analysis_count();}close_reads[text_opens-1]=radio_captures;close_analysis[text_opens-1]=analysis_count();text_closes++;if(rc==RISC_TEXT_ENTRY_AGAIN){text_close_again++;verify_pending_draft();}if(is_mode("loss-close"))runtime_lost=true;return rc;}
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
/* Bounded base Contexts service: no background sensors, presets or exports.
 * The actual app-data-backed shared fingerprint pipeline remains in the app. */
static bool ctx_step(void*c,const contexts_policy_v1*p){IO();(void)c;(void)p;return true;}
static bool ctx_pause(void*c){IO();(void)c;return true;}
static bool ctx_status(void*c,contexts_status_v1*s){IO();(void)c;*s=(contexts_status_v1){.struct_size=sizeof(*s),.state=CONTEXTS_PAUSED};return true;}
static bool ctx_source(void*c,uint32_t s){IO();(void)c;(void)s;return true;}
static bool ctx_record(void*c,uint32_t s,uint32_t k,uint32_t i,const void*b,uint32_t n){IO();(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static bool ctx_finish(void*c,uint32_t s,uint32_t r){IO();(void)c;(void)s;(void)r;return true;}
static int32_t ctx_label(void*c,uint32_t s,uint32_t i,contexts_label_v1*l){IO();(void)c;(void)s;(void)i;(void)l;return 0;}
static bool ctx_claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){IO();(void)c;(void)s;(void)i;(void)n;(void)g;return false;}
static bool ctx_result(void*c,uint32_t s,uint32_t g,uint32_t r){IO();(void)c;(void)s;(void)g;(void)r;return true;}
static const contexts_service_v1 contexts_api={.api_version=1,.struct_size=sizeof(contexts_api),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=ctx_source,.begin_export=ctx_source,.export_record=ctx_record,.finish_export=ctx_finish,.label=ctx_label,.claim_preset=ctx_claim,.preset_result=ctx_result,.capture_audio=ctx_pause};
static int32_t fake_time(void*c,risc_realtime_snapshot_v1*s){IO();(void)c;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=1791792000+ticks/1000,.monotonic_before_us=(uint64_t)ticks*1000,.monotonic_after_us=(uint64_t)ticks*1000};return RISC_REALTIME_OK;}
static const risc_realtime_api_v1 time_api={1,sizeof(time_api),NULL,fake_time};
#ifdef ALARM_SERVICE_TAGGED_V2
static const alarm_service_descriptor_v2 alarm_v2={.base={2,sizeof(alarm_v2),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
#endif
static int32_t fake_checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){IO();assert(invocation==2&&q->struct_size==sizeof(*q)&&r->struct_size==sizeof(*r));assert(!text_granted&&!fixture_live());if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL)resident_polls++;if(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY)resident_policies++;return RISC_RESIDENT_OK;}
static bool fake_resident(risc_resident_client_v1*out){IO();*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=2,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=fake_checkpoint};return true;}
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){IO();if((!strcmp(n,"storage.key-value")&&v==2&&id==RF_STORAGE_INSTANCE)||(!strcmp(n,"storage.app-data")&&id==RF_APP_DATA_INSTANCE))rf_store_acquires++;assert(g->struct_size==sizeof(*g));
 if(!strcmp(n,"runtime.realtime")&&v==1)g->api=&time_api;
#ifdef ALARM_SERVICE_TAGGED_V2
 else if(!strcmp(n,"alarm.service")&&v==2)g->api=&alarm_v2;
#endif
 else if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)&&v==1&&!getenv("RF_NO_CONTEXTS"))g->api=&contexts_api;else if(!strcmp(n,"net.wifi")&&v==1)g->api=&wifi_api;else if(!strcmp(n,"bluetooth.hci")&&v==1)g->api=&ble_api;else if(!strcmp(n,"telemetry.broadcast")&&v==1)g->api=&broadcast_api;else if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&legacy_kv_api;else if(!strcmp(n,"storage.key-value")&&v==2)g->api=&kv_api;else if(!strcmp(n,"radio.iq")&&v==1)g->api=&radio_api;else if(!strcmp(n,"storage.app-data")&&v==1)g->api=&app_data_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else if(!strcmp(n,"input.navigation")&&v==1)g->api=&nav_api;
 else if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)&&v==1){text_attempts++;if(is_mode("missing-host")||(is_mode("missing-reopen")&&text_opens)||(is_mode("missing-once")&&text_attempts==1))return false;text_proxy.base.base.base.base.base.struct_size=is_mode("old-host")?sizeof(risc_text_entry_api_v1)-1:real_text->struct_size;assert(!text_granted);text_granted=true;text_grants++;host_session_frames=host_frames;g->api=&text_proxy;if(is_mode("old-reopen")&&text_opens)text_proxy.base.base.base.base.base.struct_size=sizeof(risc_text_entry_api_v1)-1;}else return false;
 g->slot=++grant_serial;g->generation=1;grant_count++;return true;
}
static bool fake_release(risc_runtime_capability_v1*g){IO();assert(g->api&&grant_count);if(g->api==&text_proxy){assert(text_driver->quiesce()&&!lease_count&&!pending_token);text_granted=false;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};grant_count--;return true;}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=fake_acquire,.release=fake_release,.retain_invocation=fake_retain,.resident_shell=fake_resident};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return !native_retained&&!runtime_lost&&v==1?&runtime_api:NULL;}
static uint64_t clock_now(void*c){IO();(void)c;return ticks;}
#ifndef RF_APP_SOURCE
#define RF_APP_SOURCE "../../Apps/waterfall.c"
#endif
#include RF_APP_SOURCE
static unsigned analysis_count(void){return rf_bulk?signature_audio.transforms:0;}
static unsigned final_analysis;
static void observe_analysis(void){if(rf_bulk)final_analysis=signature_audio.transforms;}
static bool fixture_live(void){return rf_bulk&&running&&owned;}
static void app_touch_position(int *x,int *y){
#ifdef PORTABLE_RF_NATIVE_CANVAS
 /* Existing scenario scripts describe semantic targets on the original240px
  * page. Native pixel-edge input has a separate independent fixture. */
 if(native_plot_page()){
  if(*y<46){*x=header_x(*x);return;}
  if(*y>=216){*x=*x*app->screen_width()/240;*y+=app->screen_height()-240;return;}
  if(cursor_visible&&*y>=50&&*y<78){char caption[40];cursor_layout(caption);*x=pill_x+pill_w/2;*y=pill_y+pill_h/2;return;}
  *x=PLOT_X+(*x-PLOT_X)*PLOT_W/228;*y=PLOT_Y+(*y-PLOT_Y)*PLOT_H/166;return;
 }
#endif
 *x=ui_x(*x);*y=ui_y(*y);
}
static const char *draft_name(void){return editor==3?shared_name:editing.name;}
static void verify_pending_draft(void){assert(!strcmp(editing.name,opened_text[text_opens-1]));assert(!strcmp(draft_name(),opened_text[text_opens-1]));}
static void expect_num(const char *key,unsigned value){const char *s=getenv(key);if(s&&value!=(unsigned)strtoul(s,NULL,10)){fprintf(stderr,"%s expected %s actual %u\n",key,s,value);abort();}}
/* Fixture navigation uses the same production editor entrypoints; every edit,
 * accept, cancel, reopen and commit is dispatched through real input afterward. */
static void setup_editor(int which,int initial){
 assert(!text_granted);editor=(unsigned)which;
 if(which==0)open_label(-1,2440000000u);
 else if(which==1)signature_open(-1,1);
 else if(which==2)event_open(0);
 else {assert(which==3);shared_open(CF_ROOM);shared_tap(120,210);}
 const char *v=initial==2?"Az 09!?_[]{}|~ab":initial?"Seed":"";strcpy(editing.name,v);if(which==3)strcpy(shared_name,v);dirty=true;
}
static void setup_scene(int which){
 assert(!text_granted);toast_message=NULL;started=true;running=false;capture_requested=false;
 if(which==0){started=false;set_page(PAGE_MAIN);}
 else if(which<=3){view=(unsigned)which-1;set_page(PAGE_MAIN);}
 else if(which==4){set_page(PAGE_CONTROLS);}
 else if(which==5){set_page(PAGE_CONTROLS);controls_scroll=150;}
 else if(which==6){setup_editor(0,1);}
 else if(which==7){set_page(PAGE_SIGNATURES);}
 else if(which==8){setup_editor(1,1);}
 else if(which==9){set_page(PAGE_EVENTS);}
 else if(which==10){setup_editor(2,1);}
 else if(which==11){event_edit_slot=0;set_page(PAGE_EVENT_CAPTURE);}
 else if(which==12){event_edit_slot=0;set_page(PAGE_EVENT_EXAMPLES);}
 else if(which==13){shared_open(CF_ROOM);}
 else if(which==14){shared_open(CF_EVENT);}
 else if(which==15){setup_editor(3,1);}
 else if(which==16){shared_kind=CF_ROOM;strcpy(cfa_learning.name,"Seed");cfa_learning_at=0;set_page(PAGE_SHARED_CAPTURE);}
 else if(which==17){lab_edit=true;view=2;set_page(PAGE_MAIN);}
 else if(which==18){set_page(PAGE_RECEIVER);}
 else assert(!"unknown scene");
}
static void verify_capture(int check){
 static unsigned stopped_captures;
 if(check==13){assert(!running&&!radio_owned&&radio_captures>0);stopped_captures=radio_captures;}
 if(check==14){assert(!running&&!radio_owned&&radio_captures==stopped_captures);}
 if(check==15){assert(running&&radio_owned&&radio_captures>stopped_captures);}
 if(check==0){assert(!text_granted);expect_num("RF_EXPECT_EDITOR_PAGE",page);assert(!kv_writes&&!data_writes);const char *v=getenv("RF_EXPECT_DRAFT");if(v){fprintf(stderr,"draft='%s' expected='%s'\n",draft_name(),v);assert(!strcmp(draft_name(),v));}}
 if(check==1){assert(!text_granted);const char*v=getenv("RF_SAVED_CHECK_NAME");if(!v)v=getenv("RF_EXPECT_DRAFT");assert(v);if(editor==0){unsigned slot=getenv("RF_SAVED_SLOT")?(unsigned)atoi(getenv("RF_SAVED_SLOT")):0;assert(kv_writes==1&&labels[slot].present&&!strcmp(labels[slot].name,v));char key[16];label_key(slot,key);unsigned i=0;while(i<64&&strcmp(cells[i].key,key))i++;rf_label decoded;assert(i<64&&rf_label_decode(&decoded,cells[i].bytes,cells[i].size)&&!strcmp(decoded.name,v));}else if(editor==2){assert(data_writes&&event_library.labels[0].present&&!strcmp(event_library.labels[0].name,v));static rt_library decoded;assert(rt_bank_decode(&decoded,0,test_disk[0],test_sizes[0])&&!strcmp(decoded.labels[0].name,v));}else if(editor==1){assert(signature_goal&&running&&!strcmp(signature_capture.name,v));}else{assert(cfa_learning_kind&&running&&!strcmp(cfa_learning.name,v));}}
 if(check==11){assert(editor==0?pending_save!=0:editor==1?signature_pending!=0:editor==2?event_files.pending>=0:cfa_changed&&cfa_learning_saved);}
 if(check==12){assert(page==PAGE_MAIN&&load_errors&&event_files.ready==0&&!cfa_loaded&&!pending_save&&!signature_pending&&event_files.pending<0&&!cfa_changed&&!event_neural_checkpoint&&!event_armed&&!signature_goal&&!cfa_learning_kind);}
 if(check==8){assert(event_armed&&!event_training.ready&&!text_granted);}
 if(check==10){saved_hash=persistent_hash();saved_kv_writes=kv_writes;saved_data_writes=data_writes;saved_store_reads=rf_store_reads;saved_store_acquires=rf_store_acquires;assert(saved_hash);}
 if(check==2){assert(!text_granted&&!kv_writes&&!data_writes);}
 if(check==6){assert(!text_granted&&!lease_count&&!pending_token&&launches==1);assert(page==PAGE_MAIN);}
 if(check==7){assert(!text_granted&&!launches&&!kv_writes&&!data_writes);expect_num("RF_EXPECT_EDITOR_PAGE",page);assert(!strcmp(draft_name(),(getenv("RF_SAVED_CHECK_NAME")?getenv("RF_SAVED_CHECK_NAME"):getenv("RF_EXPECT_DRAFT"))));}
 if(check==3){assert(radio_host_captures>5&&signature_audio.transforms>5);}
 if(check==4){assert(!text_granted&&!signature_goal&&signatures[0].frames>=64&&kv_writes==1&&data_writes>=1);assert(!strcmp(signatures[0].name,(getenv("RF_SAVED_CHECK_NAME")?getenv("RF_SAVED_CHECK_NAME"):getenv("RF_EXPECT_DRAFT"))));}
 if(check==5){assert(!text_granted&&!cfa_learning_kind&&data_writes>=1);int at=cf_profile_slot(&cfa_bank,CF_ROOM,getenv("RF_SAVED_CHECK_NAME")?getenv("RF_SAVED_CHECK_NAME"):getenv("RF_EXPECT_DRAFT"));assert(at>=0&&cfa_bank.profiles[at].present[CF_RF]);static cf_bank decoded;assert(cf_decode_source(&decoded,CF_RF,test_disk[3],test_sizes[3]));assert(!strcmp(decoded.profiles[at].name,(getenv("RF_SAVED_CHECK_NAME")?getenv("RF_SAVED_CHECK_NAME"):getenv("RF_EXPECT_DRAFT"))));}
}
int main(int argc,char**argv){
 assert(argc==4);directory=argv[1];mode=argv[3];if(getenv("RF_EDITOR"))editor=(unsigned)atoi(getenv("RF_EDITOR"));memset(&raster,0xa5,sizeof(raster));FILE*f=fopen(argv[2],"r");assert(f);
 while(action_count<4096&&fscanf(f,"%u %15s %15s %d %d",&actions[action_count].delay,actions[action_count].gate,actions[action_count].kind,&actions[action_count].x,&actions[action_count].y)==5)action_count++;
 assert(feof(f));fclose(f);if(action_count)next_action_at=actions[0].delay;else stop_at=1000;
 const char *delayed=getenv("RF_TEST_DELAY_MS");delay_ms=delayed?(unsigned)atoi(delayed):0;
 risc_platform_clock_api_v1 clock={1,sizeof(clock),NULL,clock_now,NULL};scene_driver=rf_scene_driver_get(2);text_driver=rf_text_driver_get(2);profile_driver=rf_profile_driver_get(2);assert(scene_driver&&text_driver&&profile_driver&&profile_driver->start(NULL,0));
 risc_provider_dependency_v1 scene_deps[]={{"display.output",1,&display_api},{"input.touch.raw",1,&touch_api},{"input.navigation",1,&nav_api},{"platform.clock",1,&clock},{"ui.presentation-profile",1,profile_driver->capability}};
 assert(scene_driver->start(scene_deps,5));risc_provider_dependency_v1 text_deps[]={{RISC_SCENE_CAPABILITY,1,scene_driver->capability}};assert(text_driver->start(text_deps,1));real_text=text_driver->capability;assert(real_text->struct_size>=sizeof(*real_text)&&real_text->struct_size<=sizeof(text_proxy));memcpy(&text_proxy,real_text,real_text->struct_size);text_proxy.base.base.base.base.base.open=proxy_open;text_proxy.base.base.base.base.base.poll=proxy_poll;text_proxy.base.base.base.base.base.close=proxy_close;if(is_mode("old-host"))text_proxy.base.base.base.base.base.struct_size=sizeof(risc_text_entry_api_v1)-1;
 assert(app_module_init()==0);app_main();app_module_fini();
#ifdef PORTABLE_RF_SHARED_TEXT_INPUT
 if(getenv("RF_EXPECT_HOME_EXIT")){assert(home_exit_requested&&!fallback_backs&&!capture_requested&&!running&&!signature_goal&&!event_armed&&!cfa_learning_kind&&!editing.name[0]&&!shared_name[0]);}
#endif
 if(is_mode("retained")||!strncmp(mode,"loss-",5)){assert(native_retained&&portable_text_adapter_retained()&&!launches);assert(grant_count==retained_grants&&subs==retained_subs&&lease_count==retained_frames&&io_calls==retained_calls&&allocation_count==retained_allocations);assert(app_module_init()!=0&&io_calls==retained_calls);puts("PASS retained invocation: resources frozen, zero later I/O, reentry refused");return 0;}
 assert(!grant_count&&!lease_count&&!subs&&!radio_owned&&!text_granted&&!pending_token);assert(action_cursor==action_count&&!touch_down&&!navigation_pressed);
 if(getenv("RF_EXPECT_NO_STORAGE_READS"))assert(saved_store_reads==rf_store_reads&&saved_store_acquires==rf_store_acquires);
 if(getenv("RF_EXPECT_PRESERVE_SAVED"))assert(saved_hash&&saved_hash==persistent_hash()&&saved_kv_writes==kv_writes&&saved_data_writes==data_writes);
 expect_num("RF_EXPECT_LAUNCHES",launches);expect_num("RF_EXPECT_OPENS",text_opens);expect_num("RF_EXPECT_KV_WRITES",kv_writes);expect_num("RF_EXPECT_DATA_WRITES",data_writes);
 const char *saved=getenv("RF_EXPECT_SAVED_LABEL");if(saved){unsigned i=0;while(i<64&&strcmp(cells[i].key,"rf_l0"))i++;rf_label decoded;assert(i<64&&rf_label_decode(&decoded,cells[i].bytes,cells[i].size)&&!strcmp(decoded.name,saved));}
 saved=getenv("RF_EXPECT_SAVED_SIGNATURE");if(saved){unsigned i=0;while(i<64&&strcmp(cells[i].key,"rf_s0"))i++;rf_signature decoded;assert(i<64&&rf_signature_decode(&decoded,cells[i].bytes,cells[i].size)&&!strcmp(decoded.name,saved));}
 saved=getenv("RF_EXPECT_SAVED_TEMPORAL");if(saved){static rt_library decoded;assert(rt_bank_decode(&decoded,0,test_disk[0],test_sizes[0])&&!strcmp(decoded.labels[0].name,saved));}
 saved=getenv("RF_EXPECT_SAVED_CONTEXT");if(saved){static cf_bank decoded;assert(cf_decode_source(&decoded,CF_RF,test_disk[3],test_sizes[3]));int at=cf_profile_slot(&decoded,CF_ROOM,saved);assert(at>=0&&decoded.profiles[at].present[CF_RF]&&!strcmp(decoded.profiles[at].name,saved));}
 const char *want=getenv("RF_EXPECT_DRAFT");if(want){fprintf(stderr,"final draft='%s' expected='%s'\n",draft_name(),want);assert(!strcmp(draft_name(),want));}
 for(unsigned i=0;i<text_opens;i++){assert(opened_capacity[i]==17);static const char*captions[]={"LABEL NAME","SIGNATURE NAME","EVENT NAME","SIGNAL NAME"};assert(!strcmp(opened_label[i],captions[editor]));printf("session %u capacity %u initial '%s' terminal %u text '%s' capture %u -> %u analysis %u -> %u\n",i,opened_capacity[i],opened_text[i],terminal_state[i],terminal_text[i],open_reads[i],close_reads[i],open_analysis[i],close_analysis[i]);}
 if(getenv("RF_EXPECT_REOPEN")){assert(text_opens==3&&!strcmp(opened_text[1],"Q")&&!strcmp(opened_text[2],"Q"));assert(!strcmp(terminal_text[1],"QW"));}
 if(getenv("RF_EXPECT_AWAKE")){assert(ticks>=65000&&radio_host_captures>500);}
 if(getenv("RF_EXPECT_PROGRESS")){assert(radio_host_captures>5&&final_analysis>5);}
 if(getenv("RF_EXPECT_CLOSE_AGAIN"))assert(text_close_again>0);
 if(getenv("RF_EXPECT_CLOSE_PROGRESS")){assert(text_opens==1&&close_reads[0]>first_close_reads[0]&&close_analysis[0]>first_close_analysis[0]);printf("close-AGAIN RF %u -> %u, analysis %u -> %u\n",first_close_reads[0],close_reads[0],first_close_analysis[0],close_analysis[0]);}
 if(getenv("RF_NO_PRESENT"))assert(!host_frames&&unavailable_frames>0);
 assert(app_frames>0);if(text_opens&&!is_mode("unavailable-open")&&!getenv("RF_NO_PRESENT"))assert(host_frames>0);
 assert(text_driver->quiesce());text_driver->stop();assert(scene_driver->quiesce());scene_driver->stop();assert(profile_driver->quiesce());profile_driver->stop();assert(!allocation_count);
 printf("PASS real RF app/adapter/shared hosts: mode=%s frames=%u app=%u host=%u input=%u/%u edges=%u ms=%u captures=%u host-captures=%u transforms=%u kv=%u data=%u close-again=%u; leases and stride guards clean\n",mode,presents,app_frames,host_frames,action_cursor,action_count,touch_edges,ticks,radio_captures,radio_host_captures,final_analysis,kv_writes,data_writes,text_close_again);return 0;
}
