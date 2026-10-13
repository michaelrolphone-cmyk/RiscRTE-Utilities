/* Real Spectrum app + adapter + text/scene/profile providers. Only physical services
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
#include "AudioInputV1.h"
#include "RiscAppDataV1.h"
#include "ContextsServiceV1.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void); int app_module_init(void); void app_module_fini(void);
const risc_driver_v2 *spectrum_scene_driver_get(uint32_t),*spectrum_text_driver_get(uint32_t),*spectrum_profile_driver_get(uint32_t);
static const risc_driver_v2 *scene_driver,*text_driver,*profile_driver;
#ifdef PORTABLE_TEXT_INPUT_POLICY
static risc_text_entry_api_v1_checkpoint text_proxy;
#else
static risc_text_entry_api_v1_long text_proxy;
#endif
#define TEXT_PROXY_BASE (*(risc_text_entry_api_v1 *)(void *)&text_proxy)
static const risc_text_entry_api_v1 *real_text;
static bool policy_home_pending;
static unsigned policy_battery,policy_sleep,policy_background,policy_contexts,policy_resumes;
static unsigned ticks,polls,grant_count,lease_count,subs,presents,app_frames,host_frames,host_session_frames;
static unsigned text_attempts,text_grants,text_opens,text_closes,text_close_again,grant_serial,launches;
static unsigned mic_opens,mic_closes,mic_reads,mic_host_reads,mic_sample,kv_writes,data_writes,editor,capture_serial,unavailable_frames;
static unsigned open_reads[32],close_reads[32],open_analysis[32],close_analysis[32],first_close_reads[32],first_close_analysis[32],session_closes[32];
static bool mic_owned,text_granted,native_retained,runtime_lost,in_host_call;
static unsigned retained_grants,retained_subs,retained_frames,retained_calls,io_calls,retained_allocations,allocation_count;
static void *allocations[64];
void *__real_malloc(size_t);void __real_free(void *);
void *__wrap_malloc(size_t n){void *p=__real_malloc(n);if(p){unsigned i;for(i=0;i<64&&allocations[i];i++);assert(i<64);allocations[i]=p;allocation_count++;}return p;}
void __wrap_free(void *p){if(p){unsigned i;for(i=0;i<64&&allocations[i]!=p;i++);assert(i<64&&allocation_count);allocations[i]=NULL;allocation_count--;}__real_free(p);}
static unsigned stop_at=90000,pending_token,pending_until,delay_ms;
static unsigned touch_head,touch_count,touch_edges,action_cursor,action_count,next_action_at;
static uint64_t touch_sequence;
static bool touch_down; static int touch_x,touch_y; static uint32_t navigation_pressed;
#ifndef SPECTRUM_TEST_WIDTH
#define SPECTRUM_TEST_WIDTH 240
#define SPECTRUM_TEST_HEIGHT 240
#endif
#define SPECTRUM_TEST_STRIDE (SPECTRUM_TEST_WIDTH+4)
static uint16_t pixels[SPECTRUM_TEST_HEIGHT*SPECTRUM_TEST_STRIDE];
static const char *directory,*mode;
static risc_touch_event_v1 touch_events[64];
static struct {unsigned delay;char gate[16],kind[16];int x,y;} actions[4096];
static struct {char key[32];unsigned char bytes[2048];uint32_t size;} cells[64];
static char opened_text[32][256],opened_label[32][40],terminal_text[32][256];
static uint32_t opened_capacity[32],terminal_state[32];
static void setup_editor(int which,int initial);
static void setup_scene(int which);
static unsigned analysis_count(void);
static void verify_capture(int after);
static bool is_mode(const char*s){return !strcmp(mode,s);}
#define IO() do{assert(!native_retained&&!runtime_lost);io_calls++;}while(0)
static void save_frame(const char *name){
 char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",SPECTRUM_TEST_WIDTH,SPECTRUM_TEST_HEIGHT);
 for(unsigned y=0;y<SPECTRUM_TEST_HEIGHT;y++){for(unsigned x=0;x<SPECTRUM_TEST_WIDTH;x++){uint16_t v=pixels[y*SPECTRUM_TEST_STRIDE+x];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}for(unsigned x=SPECTRUM_TEST_WIDTH;x<SPECTRUM_TEST_STRIDE;x++)assert(pixels[y*SPECTRUM_TEST_STRIDE+x]==0xa5a5);}assert(!fclose(f));
}
static bool fake_health(risc_runtime_health_v1*h){IO();h->uptime_ms=ticks;return true;}
static void fake_yield(uint32_t n){IO();ticks+=n;assert(ticks<120000&&"script failed to finish");}
static bool fake_diag(const char*s){IO();fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){IO();assert(!mic_owned&&!text_granted&&!lease_count&&!pending_token);assert(!strcmp(s,"springboard.elf"));launches++;return true;}
static bool fake_retain(void){assert(!native_retained);native_retained=true;retained_grants=grant_count;retained_subs=subs;retained_frames=lease_count;retained_calls=io_calls;retained_allocations=allocation_count;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){IO();(void)c;*s=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*s),.width=SPECTRUM_TEST_WIDTH,.height=SPECTRUM_TEST_HEIGHT,.preferred_format=RISC_DISPLAY_FORMAT_RGB565,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=delay_ms?RISC_DISPLAY_INFO_ASYNC_PRESENT:0};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){IO();(void)c;assert(!lease_count&&!pending_token&&(!text_granted||in_host_call));if(text_granted&&getenv("SPECTRUM_NO_PRESENT")){unavailable_frames++;return false;}lease_count=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=SPECTRUM_TEST_WIDTH,.height=SPECTRUM_TEST_HEIGHT,.stride_bytes=2*SPECTRUM_TEST_STRIDE,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){IO();(void)c;assert(lease_count&&f==1);lease_count=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){IO();(void)c;(void)r;(void)n;(void)o;assert(lease_count&&f==1&&!pending_token);lease_count=0;*t=++presents;pending_token=*t;pending_until=ticks+(text_granted?delay_ms:0);if(text_granted){host_frames++;save_frame("keyboard");}else{app_frames++;save_frame("app");}return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){IO();(void)c;assert(t);s->state=ticks<pending_until?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(s->state==RISC_DISPLAY_PRESENT_COMPLETE)pending_token=0;return true;}
static bool fake_wait(void*c,risc_display_present_token_v1 t,uint32_t ms,risc_display_present_status_v1*s){if(ticks<pending_until){unsigned wait=pending_until-ticks;if(wait>ms)wait=ms;fake_yield(wait);}return fake_present(c,t,s);}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present,.wait_present=fake_wait};
static uint64_t fake_sub(void*c){IO();(void)c;assert(!subs);subs=1;touch_head=touch_count=0;return 1;}
static bool fake_unsub(void*c,uint64_t n){IO();(void)c;assert(n==1&&subs);if(text_granted&&host_frames&&is_mode("retained"))return false;subs=0;touch_head=touch_count=0;return true;}
static bool action_ready(const char *gate){if(!strcmp(gate,"text"))return text_granted&&(host_frames>host_session_frames||getenv("SPECTRUM_NO_PRESENT"));if(!strcmp(gate,"pending"))return text_granted&&pending_token;assert(!strcmp(gate,"app"));return !text_granted&&app_frames;}
static bool fake_touch_poll(void*c,size_t n){IO();(void)c;assert(n==1||n==2);polls++;
 if(action_cursor>=action_count||ticks<next_action_at||!action_ready(actions[action_cursor].gate))return true;
 const char *kind=actions[action_cursor].kind;
 if(!strcmp(kind,"setup")){setup_editor(actions[action_cursor].x,actions[action_cursor].y);}
 else if(!strcmp(kind,"scene")){setup_scene(actions[action_cursor].x);}
 else if(!strcmp(kind,"idle")){}
 else if(!strcmp(kind,"nav")){assert(!touch_down&&!navigation_pressed);navigation_pressed=(uint32_t)actions[action_cursor].x;}
 else if(!strcmp(kind,"back")||!strcmp(kind,"home")){assert(!touch_down&&!navigation_pressed);navigation_pressed=!strcmp(kind,"back")?RISC_NAV_BACK:RISC_NAV_HOME;}
 else if(!strcmp(kind,"capture")){verify_capture(actions[action_cursor].x);char name[64];snprintf(name,sizeof(name),"capture-%02u",capture_serial++);save_frame(name);}
 else{unsigned event_kind;if(!strcmp(kind,"down")){assert(!touch_down);touch_down=true;event_kind=RISC_TOUCH_EVENT_DOWN;}else{assert(!strcmp(kind,"up")&&touch_down);touch_down=false;event_kind=RISC_TOUCH_EVENT_UP;}
 touch_x=actions[action_cursor].x;touch_y=actions[action_cursor].y;
#if SPECTRUM_TEST_WIDTH != 240
 if(!strcmp(actions[action_cursor].gate,"text")){if(touch_y==20){touch_x=touch_x>120?SPECTRUM_TEST_WIDTH-20:20;}else{touch_x=touch_x*SPECTRUM_TEST_WIDTH/240;touch_y=touch_y*SPECTRUM_TEST_HEIGHT/240;}}else{touch_x+=(SPECTRUM_TEST_WIDTH-240)/2;touch_y+=(SPECTRUM_TEST_HEIGHT-240)/2;}
#endif
 assert(touch_x>=0&&touch_x<SPECTRUM_TEST_WIDTH&&touch_y>=0&&touch_y<SPECTRUM_TEST_HEIGHT&&subs==1&&touch_count<64);
 touch_events[(touch_head+touch_count++)%64]=(risc_touch_event_v1){.sequence=++touch_sequence,.timestamp_ms=ticks,.kind=event_kind,.id=1,.x=(uint16_t)touch_x,.y=(uint16_t)touch_y};touch_edges++;}
 fprintf(stderr,"input %u %s %s %d %d\n",ticks,actions[action_cursor].gate,kind,actions[action_cursor].x,actions[action_cursor].y);
 if(++action_cursor<action_count)next_action_at=ticks+actions[action_cursor].delay;else stop_at=ticks+900;return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){IO();(void)c;assert(n==1&&subs);if(!touch_count)return 0;*e=touch_events[touch_head];touch_head=(touch_head+1)%64;touch_count--;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){IO();(void)c;*s=(risc_touch_snapshot_v1){.width=SPECTRUM_TEST_WIDTH,.height=SPECTRUM_TEST_HEIGHT,.sequence=touch_sequence,.timestamp_ms=ticks};if(touch_down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)touch_x,.y=(uint16_t)touch_y};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){IO();(void)c;if(text_granted)policy_battery++;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){IO();(void)c;*s=(twatch_rtc_time_v1){2026,10,12,1,14,0,0};return true;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){IO();(void)c;*s=0;for(unsigned i=0;i<64;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){IO();(void)c;assert(n<=2048&&strlen(k)<32&&!text_granted);kv_writes++;unsigned i;for(i=0;i<64&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<64);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 legacy_kv_api={1,sizeof(legacy_kv_api),NULL,fake_get,fake_put};
static const risc_key_value_v1 kv_api={2,sizeof(kv_api),NULL,fake_get,fake_put};
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
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){IO();(void)r;(void)d;(void)b;(void)a;assert(getenv("SPECTRUM_POLICY_STOPPED")&&text_granted&&!subs&&!lease_count&&!pending_token&&!mic_owned);policy_sleep++;return 1;}
int portable_app_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){return portable_app_alarm_sleep(r,d,b,NULL);}
uint32_t portable_app_sleep_budget_ms(void){return 900000;}
static bool fake_mic_open(void*c,uint32_t rate){IO();(void)c;assert(!mic_owned&&rate==16000);mic_owned=true;mic_opens++;return true;}
static bool fake_mic_read(void*c,int16_t*pcm,size_t n,size_t*got){IO();(void)c;assert(mic_owned&&n<=256);mic_reads++;if(text_granted)mic_host_reads++;for(size_t i=0;i<n;i++,mic_sample++){double t=(double)mic_sample/16000;pcm[i]=(int16_t)(8000*sin(6.283185307179586*440*t)+3500*sin(6.283185307179586*1000*t)+1500*sin(6.283185307179586*3200*t));}*got=n;ticks+=(uint32_t)(n*1000/16000);return true;}
static bool fake_mic_level(void*c,uint16_t*v){IO();(void)c;*v=5000;return mic_owned;}
static bool fake_mic_close(void*c){IO();(void)c;assert(mic_owned);mic_owned=false;mic_closes++;return true;}
static const twatch_audio_in_api_v1 mic_api={1,sizeof(mic_api),NULL,fake_mic_open,fake_mic_read,fake_mic_level,fake_mic_close};
static uint8_t test_disk[4][72000];static uint32_t test_sizes[4];static uint64_t test_revision[4]={1,1,1,1};
static unsigned test_bank(const char*n){if(!strcmp(n,"context-fingerprints.cfp"))return 3;if(!strcmp(n,"spectrum-neural.snn"))return 2;assert(!strcmp(n,"spectrum-events-a.sqt")||!strcmp(n,"spectrum-events-b.sqt"));return !strcmp(n,"spectrum-events-b.sqt");}
static int32_t test_stat(void*c,const char*n,uint32_t*z,uint64_t*r){IO();(void)c;unsigned b=test_bank(n);*z=test_sizes[b];*r=*z?test_revision[b]:0;return *z?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t test_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*z,uint64_t*r){IO();(void)c;unsigned b=test_bank(n);*z=0;*r=0;if(expected!=test_revision[b])return RISC_APP_DATA_STALE;assert(cap>=test_sizes[b]);memcpy(out,test_disk[b],test_sizes[b]);*z=test_sizes[b];*r=test_revision[b];return 0;}
static int32_t test_replace(void*c,const char*n,uint64_t expected,const void*in,uint32_t z){IO();(void)c;assert(!mic_owned&&!text_granted);unsigned b=test_bank(n);assert(expected==(test_sizes[b]?test_revision[b]:0)&&z<=sizeof(test_disk[b]));memcpy(test_disk[b],in,z);test_sizes[b]=z;test_revision[b]++;data_writes++;return 0;}
static const risc_app_data_v1 app_data_api={1,sizeof(app_data_api),NULL,test_stat,test_read,test_replace};
static int32_t proxy_open(void*c,const risc_text_entry_request_v1*q,uint64_t*t){IO();(void)c;assert(text_opens<32&&!lease_count&&!pending_token);open_reads[text_opens]=mic_reads;open_analysis[text_opens]=analysis_count();strcpy(opened_text[text_opens],q->text);strcpy(opened_label[text_opens],q->label);opened_capacity[text_opens++]=q->capacity;in_host_call=true;int32_t rc=real_text->open(real_text->context,q,t);in_host_call=false;if(is_mode("unavailable-open")){assert(real_text->close(real_text->context,*t)==RISC_TEXT_ENTRY_OK);*t=0;return RISC_TEXT_ENTRY_UNAVAILABLE;}if(is_mode("loss-open"))runtime_lost=true;return rc;}
static int32_t proxy_poll(void*c,uint64_t t,risc_text_entry_state_v1*q){IO();(void)c;in_host_call=true;int32_t rc=real_text->poll(real_text->context,t,q);in_host_call=false;if(rc==RISC_TEXT_ENTRY_OK){assert(strlen(q->text)<opened_capacity[text_opens-1]);strcpy(terminal_text[text_opens-1],q->text);terminal_state[text_opens-1]=q->state;}if(is_mode("loss-poll"))runtime_lost=true;return rc;}
static int32_t proxy_close(void*c,uint64_t t){IO();(void)c;in_host_call=true;int32_t rc=real_text->close(real_text->context,t);in_host_call=false;if(!session_closes[text_opens-1]++){first_close_reads[text_opens-1]=mic_reads;first_close_analysis[text_opens-1]=analysis_count();}close_reads[text_opens-1]=mic_reads;close_analysis[text_opens-1]=analysis_count();text_closes++;if(rc==RISC_TEXT_ENTRY_AGAIN)text_close_again++;if(is_mode("loss-close"))runtime_lost=true;return rc;}
#ifdef PORTABLE_TEXT_INPUT_POLICY
static const risc_text_entry_api_v1_checkpoint *real_checkpoint;
static int32_t proxy_enable(void*c,uint64_t t){if(getenv("SPECTRUM_POLICY_UNAVAILABLE"))return RISC_TEXT_ENTRY_UNAVAILABLE;in_host_call=true;int r=real_checkpoint->checkpoint_enable(c,t);in_host_call=false;return r;}
static int32_t proxy_snapshot(void*c,uint64_t t,risc_text_entry_checkpoint_state_v1*s){in_host_call=true;int r=real_checkpoint->checkpoint_snapshot(c,t,s);in_host_call=false;return r;}
static int32_t proxy_pause(void*c,uint64_t t){in_host_call=true;int r=real_checkpoint->pause(c,t);in_host_call=false;if(getenv("SPECTRUM_POLICY_PAUSED_HOME")&&r==RISC_TEXT_ENTRY_OK){policy_home_pending=true;return RISC_TEXT_ENTRY_AGAIN;}return r;}
static int32_t proxy_pause_focus(void*c,uint64_t t){in_host_call=true;int r=real_checkpoint->pause_for_focus_change(c,t);in_host_call=false;return r;}
static int32_t proxy_resume(void*c,uint64_t t,uint32_t f){policy_resumes++;if(is_mode("policy-retained"))return RISC_TEXT_ENTRY_RETAINED;in_host_call=true;int r=real_checkpoint->resume(c,t,f);in_host_call=false;return r;}
#endif
static uint8_t ble_enabled;
static bool fake_ble_enable(void*c,bool on){IO();(void)c;ble_enabled=on;return true;}
static bool fake_ble_status(void*c,uint8_t*s){IO();(void)c;*s=ble_enabled;return true;}
static const portable_bluetooth_control_v1 ble_api={.api_version=1,.struct_size=sizeof(ble_api),.set_enabled=fake_ble_enable,.status=fake_ble_status};
static wifi_link_t fake_wifi_status(void*c){IO();(void)c;return WIFI_LINK_DOWN;}
static bool fake_wifi_disconnect(void*c){IO();(void)c;return true;}
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=fake_wifi_status,.disconnect_checked=fake_wifi_disconnect};
static bool fake_broadcast_pause(void*c){IO();(void)c;return true;}
static bool fake_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){IO();if(text_granted)policy_background++;(void)c;(void)allow;(void)p;return true;}
static bool fake_broadcast_status(void*c,telemetry_broadcast_status_v1*s){IO();(void)c;*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t fake_broadcast_enum(void*c,uint32_t i,risc_telemetry_field_v1*f){IO();(void)c;(void)i;(void)f;return 0;}
static int32_t fake_broadcast_read(void*c,uint32_t i,int32_t*v){IO();(void)c;(void)i;(void)v;return 0;}
static const telemetry_broadcast_v1 broadcast_api={1,sizeof(broadcast_api),NULL,fake_broadcast_step,fake_broadcast_pause,fake_broadcast_status,fake_broadcast_enum,fake_broadcast_read};
/* Bounded base Contexts service: no background sensors, presets or exports.
 * The actual app-data-backed shared fingerprint pipeline remains in the app. */
static bool ctx_step(void*c,const contexts_policy_v1*p){IO();if(text_granted)policy_contexts++;(void)c;(void)p;return true;}
static bool ctx_pause(void*c){IO();(void)c;return true;}
static bool ctx_status(void*c,contexts_status_v1*s){IO();(void)c;*s=(contexts_status_v1){.struct_size=sizeof(*s),.state=CONTEXTS_PAUSED};return true;}
static bool ctx_source(void*c,uint32_t s){IO();(void)c;(void)s;return true;}
static bool ctx_record(void*c,uint32_t s,uint32_t k,uint32_t i,const void*b,uint32_t n){IO();(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static bool ctx_finish(void*c,uint32_t s,uint32_t r){IO();(void)c;(void)s;(void)r;return true;}
static int32_t ctx_label(void*c,uint32_t s,uint32_t i,contexts_label_v1*l){IO();(void)c;(void)s;(void)i;(void)l;return 0;}
static bool ctx_claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){IO();(void)c;(void)s;(void)i;(void)n;(void)g;return false;}
static bool ctx_result(void*c,uint32_t s,uint32_t g,uint32_t r){IO();(void)c;(void)s;(void)g;(void)r;return true;}
static const contexts_service_v1 contexts_api={.api_version=1,.struct_size=sizeof(contexts_api),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=ctx_source,.begin_export=ctx_source,.export_record=ctx_record,.finish_export=ctx_finish,.label=ctx_label,.claim_preset=ctx_claim,.preset_result=ctx_result,.capture_audio=ctx_pause};
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){IO();(void)id;assert(g->struct_size==sizeof(*g));
 if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)&&v==1&&!getenv("SPECTRUM_NO_CONTEXTS"))g->api=&contexts_api;else if(!strcmp(n,"net.wifi")&&v==1)g->api=&wifi_api;else if(!strcmp(n,"bluetooth.hci")&&v==1)g->api=&ble_api;else if(!strcmp(n,"telemetry.broadcast")&&v==1)g->api=&broadcast_api;else if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&legacy_kv_api;else if(!strcmp(n,"storage.key-value")&&v==2)g->api=&kv_api;else if(!strcmp(n,"audio.input")&&v==1)g->api=&mic_api;else if(!strcmp(n,"storage.app-data")&&v==1)g->api=&app_data_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else if(!strcmp(n,"input.navigation")&&v==1)g->api=&nav_api;
 else if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)&&v==1){text_attempts++;if(is_mode("missing-host")||(is_mode("missing-reopen")&&text_opens)||(is_mode("missing-once")&&text_attempts==1))return false;TEXT_PROXY_BASE.struct_size=is_mode("old-host")?sizeof(risc_text_entry_api_v1)-1:getenv("SPECTRUM_POLICY_MISSING_SUFFIX")?sizeof(risc_text_entry_api_v1_long):sizeof(text_proxy);assert(!text_granted);text_granted=true;text_grants++;host_session_frames=host_frames;g->api=&text_proxy;if(is_mode("old-reopen")&&text_opens)TEXT_PROXY_BASE.struct_size=sizeof(risc_text_entry_api_v1)-1;}else return false;
 g->slot=++grant_serial;g->generation=1;grant_count++;return true;
}
static bool fake_release(risc_runtime_capability_v1*g){IO();assert(g->api&&grant_count);if(g->api==&text_proxy){assert(text_driver->quiesce()&&!lease_count&&!pending_token);text_granted=false;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};grant_count--;return true;}
static bool fake_exit_requested(void){IO();return policy_home_pending;}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=fake_acquire,.release=fake_release,.retain_invocation=fake_retain,.exit_requested=fake_exit_requested};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return !native_retained&&!runtime_lost&&v==1?&runtime_api:NULL;}
static uint64_t clock_now(void*c){IO();(void)c;return ticks;}
#ifndef SPECTRUM_APP_SOURCE
#define SPECTRUM_APP_SOURCE "../../Apps/audio_spectrum.c"
#endif
#include SPECTRUM_APP_SOURCE
#ifdef SPECTRUM_BASELINE
bool portable_app_before_launch(const char*s){(void)s;return portable_audio_services_safe();}
#endif
static unsigned analysis_count(void){return signature_audio.transforms;}
static const char *draft_name(void){return editor==3?shared_name:editing.name;}
static void expect_num(const char *key,unsigned value){const char *s=getenv(key);if(s&&value!=(unsigned)strtoul(s,NULL,10)){fprintf(stderr,"%s expected %s actual %u\n",key,s,value);abort();}}
/* Fixture navigation uses the same production editor entrypoints; every edit,
 * accept, cancel, reopen and commit is dispatched through real input afterward. */
static void setup_editor(int which,int initial){
 assert(!text_granted);editor=(unsigned)which;
 if(which==0)open_label(-1,777);
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
 else assert(!"unknown scene");
}
static void verify_capture(int check){
 static unsigned stopped_reads;
 if(check==6){assert(!running&&!mic_owned&&mic_opens==mic_closes&&mic_reads>0);stopped_reads=mic_reads;}
 if(check==7){assert(!running&&!mic_owned&&mic_reads==stopped_reads);}
 if(check==8){assert(running&&mic_owned&&mic_reads>stopped_reads&&mic_opens==mic_closes+1);}
 if(check==0){assert(!text_granted);expect_num("SPECTRUM_EXPECT_EDITOR_PAGE",page);assert(!kv_writes&&!data_writes);const char *v=getenv("SPECTRUM_EXPECT_DRAFT");if(v){fprintf(stderr,"draft='%s' expected='%s'\n",draft_name(),v);assert(!strcmp(draft_name(),v));}}
 if(check==1){assert(!text_granted);const char*v=getenv("SPECTRUM_EXPECT_DRAFT");assert(v);if(editor==0){unsigned slot=getenv("SPECTRUM_SAVED_SLOT")?(unsigned)atoi(getenv("SPECTRUM_SAVED_SLOT")):4;assert(kv_writes==1&&labels[slot].present&&!strcmp(labels[slot].name,v));}else if(editor==2){assert(data_writes&&event_library.labels[0].present&&!strcmp(event_library.labels[0].name,v));}else if(editor==1){assert(signature_goal&&running&&!strcmp(signature_capture.name,v));}else{assert(cfa_learning_kind&&running&&!strcmp(cfa_learning.name,v));}}
 if(check==2){assert(!text_granted&&!kv_writes&&!data_writes);}
 if(check==3){assert(mic_host_reads>5&&signature_audio.transforms>5);}
 if(check==4){assert(!text_granted&&!signature_goal&&signatures[0].frames>=100&&kv_writes==1&&data_writes>=1);assert(!strcmp(signatures[0].name,getenv("SPECTRUM_EXPECT_DRAFT")));}
 if(check==5){assert(!text_granted&&!cfa_learning_kind&&data_writes>=1);int at=cf_profile_slot(&cfa_bank,CF_ROOM,getenv("SPECTRUM_EXPECT_DRAFT"));assert(at>=0&&cfa_bank.profiles[at].present[CF_AUDIO]);static cf_bank decoded;assert(cf_decode_source(&decoded,CF_AUDIO,test_disk[3],test_sizes[3]));assert(!strcmp(decoded.profiles[at].name,getenv("SPECTRUM_EXPECT_DRAFT")));}
}
int main(int argc,char**argv){
 assert(argc==4);directory=argv[1];mode=argv[3];if(getenv("SPECTRUM_EDITOR"))editor=(unsigned)atoi(getenv("SPECTRUM_EDITOR"));memset(pixels,0xa5,sizeof(pixels));FILE*f=fopen(argv[2],"r");assert(f);
 while(action_count<4096&&fscanf(f,"%u %15s %15s %d %d",&actions[action_count].delay,actions[action_count].gate,actions[action_count].kind,&actions[action_count].x,&actions[action_count].y)==5)action_count++;
 assert(feof(f));fclose(f);if(action_count)next_action_at=actions[0].delay;else stop_at=1000;
 const char *delayed=getenv("SPECTRUM_TEST_DELAY_MS");delay_ms=delayed?(unsigned)atoi(delayed):0;
 risc_platform_clock_api_v1 clock={1,sizeof(clock),NULL,clock_now,NULL};scene_driver=spectrum_scene_driver_get(2);text_driver=spectrum_text_driver_get(2);profile_driver=spectrum_profile_driver_get(2);assert(scene_driver&&text_driver&&profile_driver&&profile_driver->start(NULL,0));
 risc_provider_dependency_v1 scene_deps[]={{"display.output",1,&display_api},{"input.touch.raw",1,&touch_api},{"input.navigation",1,&nav_api},{"platform.clock",1,&clock},{"ui.presentation-profile",1,profile_driver->capability}};
 assert(scene_driver->start(scene_deps,5));risc_provider_dependency_v1 text_deps[]={{RISC_SCENE_CAPABILITY,1,scene_driver->capability}};assert(text_driver->start(text_deps,1));real_text=text_driver->capability;assert(real_text->struct_size>=sizeof(*real_text)&&real_text->struct_size<=sizeof(text_proxy));memcpy(&text_proxy,real_text,real_text->struct_size);TEXT_PROXY_BASE.open=proxy_open;TEXT_PROXY_BASE.poll=proxy_poll;TEXT_PROXY_BASE.close=proxy_close;if(is_mode("old-host"))TEXT_PROXY_BASE.struct_size=sizeof(risc_text_entry_api_v1)-1;
#ifdef PORTABLE_TEXT_INPUT_POLICY
 real_checkpoint=risc_text_entry_checkpoint(real_text);assert(real_checkpoint);
 text_proxy.checkpoint_enable=proxy_enable;text_proxy.checkpoint_snapshot=proxy_snapshot;text_proxy.pause=proxy_pause;text_proxy.pause_for_focus_change=proxy_pause_focus;text_proxy.resume=proxy_resume;
#endif
 assert(app_module_init()==0);app_main();app_module_fini();
 if(is_mode("retained")||is_mode("policy-retained")||!strncmp(mode,"loss-",5)){assert(native_retained&&portable_text_adapter_retained()&&!launches);assert(grant_count==retained_grants&&subs==retained_subs&&lease_count==retained_frames&&io_calls==retained_calls&&allocation_count==retained_allocations);assert(app_module_init()!=0&&io_calls==retained_calls);if(is_mode("policy-retained"))assert(policy_resumes==1);puts("PASS retained invocation: resources frozen, zero later I/O, reentry refused");return 0;}
 assert(!grant_count&&!lease_count&&!subs&&!mic_owned&&!text_granted&&!pending_token);assert(action_cursor==action_count&&!touch_down&&!navigation_pressed);
 expect_num("SPECTRUM_EXPECT_OPENS",text_opens);expect_num("SPECTRUM_EXPECT_KV_WRITES",kv_writes);expect_num("SPECTRUM_EXPECT_DATA_WRITES",data_writes);
 const char *want=getenv("SPECTRUM_EXPECT_DRAFT");if(want){fprintf(stderr,"final draft='%s' expected='%s'\n",draft_name(),want);assert(!strcmp(draft_name(),want));}
 for(unsigned i=0;i<text_opens;i++){assert(opened_capacity[i]==17);printf("session %u capacity %u initial '%s' terminal %u text '%s' capture %u -> %u analysis %u -> %u\n",i,opened_capacity[i],opened_text[i],terminal_state[i],terminal_text[i],open_reads[i],close_reads[i],open_analysis[i],close_analysis[i]);}
 if(getenv("SPECTRUM_EXPECT_REOPEN")){assert(text_opens==3&&!strcmp(opened_text[1],"Q")&&!strcmp(opened_text[2],"Q"));assert(!strcmp(terminal_text[1],"QW"));}
 if(getenv("SPECTRUM_EXPECT_PROGRESS")){assert(mic_host_reads>5&&mic_opens==mic_closes&&analysis_count()>5);}
 if(getenv("SPECTRUM_EXPECT_CLOSE_AGAIN"))assert(text_close_again>0);
 if(getenv("SPECTRUM_EXPECT_CLOSE_PROGRESS")){assert(text_opens==1&&close_reads[0]>first_close_reads[0]&&close_analysis[0]>first_close_analysis[0]);printf("close-AGAIN audio %u -> %u, analysis %u -> %u\n",first_close_reads[0],close_reads[0],first_close_analysis[0],close_analysis[0]);}
 if(getenv("SPECTRUM_NO_PRESENT"))assert(!host_frames&&unavailable_frames>0);
 if(getenv("SPECTRUM_POLICY_PAUSED_HOME"))assert(policy_home_pending&&!policy_resumes&&!policy_sleep&&!kv_writes&&!data_writes);
 if(getenv("SPECTRUM_POLICY_PROBE")){
  assert(policy_battery>=12&&policy_contexts>0&&policy_background>0);
  if(getenv("SPECTRUM_POLICY_STOPPED"))assert(policy_sleep==1&&!mic_host_reads);else assert(!policy_sleep&&mic_host_reads>500);
  printf("POLICY battery=%u sleep=%u background=%u contexts=%u resumes=%u capture=%u\n",policy_battery,policy_sleep,policy_background,policy_contexts,policy_resumes,mic_host_reads);
 }
 assert(app_frames>0);if(text_opens&&!is_mode("unavailable-open")&&!getenv("SPECTRUM_NO_PRESENT")&&!getenv("SPECTRUM_POLICY_UNAVAILABLE")&&!getenv("SPECTRUM_POLICY_MISSING_SUFFIX"))assert(host_frames>0);
 assert(text_driver->quiesce());text_driver->stop();assert(scene_driver->quiesce());scene_driver->stop();assert(profile_driver->quiesce());profile_driver->stop();assert(!allocation_count);
 printf("PASS real Spectrum app/adapter/shared hosts: mode=%s frames=%u app=%u host=%u input=%u/%u edges=%u ms=%u audio=%u host-audio=%u transforms=%u kv=%u data=%u close-again=%u; leases and stride guards clean\n",mode,presents,app_frames,host_frames,action_cursor,action_count,touch_edges,ticks,mic_reads,mic_host_reads,analysis_count(),kv_writes,data_writes,text_close_again);return 0;
}
