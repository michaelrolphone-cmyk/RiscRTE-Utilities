/* Full-profile production Battery/adapter/Watch bridge diagnostic with modeled services. */
#define main old_fixture_main
#define risc_runtime_get_api old_get_runtime
#define portable_input_navigation_open old_navigation_open
#define portable_input_navigation_close old_navigation_close
#define portable_app_alarm_sleep old_alarm_sleep
#include BATTERY_DIAG_PERIPHERALS
#undef main
#undef risc_runtime_get_api
#undef portable_input_navigation_open
#undef portable_input_navigation_close
#undef portable_app_alarm_sleep
#include "PortablePowerStatus.h"
#include "PortableQuickRadios.h"
#include "TelemetryBroadcastV1.h"
#include "ContextsServiceV1.h"
#include "RiscPowerKeyV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscDisplayPowerV1.h"
#include "PortableMotionTap.h"
#include BATTERY_DIAG_ATOMIC
void diag_trace(const char*);bool diag_poll(unsigned,t5_app_input_t*);unsigned diag_activity(void),diag_idle(void),diag_cancel(void),diag_down(void),diag_failed(void);bool diag_drain(void);
static unsigned mode,light_entries,bridge_entries,retained,first_visible,alarm_pumps,denials;
static bool alarm_cue,bt_on,power_details,details_seen;
static unsigned details_at;
static bool cue_started,cue_finished,contact_injected;
static unsigned snapshot_fault;static bool presentation_fault;
static void held_now(void);
#if defined(PORTABLE_QUICK_NATIVE) && defined(PORTABLE_ALARM_NATIVE)
#define BATTERY_OVERLAY_CONTROLS
static void overlay_case(void);
static bool overlay_raw(void *,size_t);
static int32_t overlay_status(void *,alarm_status_v1 *);
static int32_t overlay_ack(void *,const alarm_token_v1 *);
static int32_t overlay_get(void *,const char *,void *,uint32_t,uint32_t *);
static unsigned overlay_brightness;
#endif
static const risc_runtime_api_v1 diag_runtime;
static void action_seen(void){if(power_details&&!details_seen){details_seen=true;details_at=ticks;printf("DETAILS time=%u first_visible=%u pending=%llu\n",ticks,first_visible,(unsigned long long)pending_token);}}
static bool healthy(risc_runtime_health_v1*h){action_seen();h->struct_size=sizeof(*h);h->uptime_ms=ticks;return true;}
static bool retain(void){retained++;fprintf(stderr,"RETAIN mode=%u ticks=%u\n",mode,ticks);return true;}
static bool exit_requested(void){action_seen();return mode>=11&&mode<=13?(light_entries||ticks>=60300):((mode==6||mode==7||mode==9||mode==10)&&ticks>=500);}
static bool info(void*c,risc_display_info_v1*s){fake_info(c,s);s->api_version=1;s->struct_size=sizeof(*s);s->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;if(mode==9||mode==10)s->flags&=~RISC_DISPLAY_INFO_ASYNC_PRESENT;return true;}
static bool brightness(void*c,uint16_t l,uint16_t max){(void)c;assert(max&&l<=max);
#ifdef BATTERY_OVERLAY_CONTROLS
 if(mode>=25)overlay_brightness=l;
#endif
 return true;}
static bool presented(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){if(presentation_fault)return false;bool ok=fake_present(c,t,s);if(!first_visible&&s->state==RISC_DISPLAY_PRESENT_COMPLETE){first_visible=ticks;printf("VISIBLE token=%llu time=%u\n",(unsigned long long)t,ticks);}return ok;}
static risc_display_output_api_v1 output;
static bool raw_poll(void*c,size_t n){
#ifdef BATTERY_OVERLAY_CONTROLS
 if(mode>=25)return overlay_raw(c,n);
#endif
if(mode>=14){(void)c;assert(n==1);polls++;return true;}if(mode==11&&cue_finished&&diag_cancel()&&!contact_injected){contact_injected=true;held_now();}uint64_t before=fake_touch_state.sequence;bool ok=fake_touch_poll(c,n);if(fake_touch_state.sequence!=before)printf("RAW time=%u seq=%llu contacts=%u queued=%u visible=%u pending=%llu cancel=%u\n",ticks,(unsigned long long)fake_touch_state.sequence,fake_touch_state.contact_count,fake_event_count,first_visible,(unsigned long long)pending_token,diag_cancel());return ok;}
static bool raw_unsub(void*c,uint64_t n){if(fake_touch_state.contact_count||fake_event_count)printf("UNSUB time=%u contacts=%u queued=%u cancel=%u\n",ticks,fake_touch_state.contact_count,fake_event_count,diag_cancel());return fake_unsub(c,n);}
static bool raw_snapshot(void*c,risc_touch_snapshot_v1*s){assert(!retained);if(snapshot_fault&&!pending_token){if(snapshot_fault==1)return false;fake_snapshot(c,s);if(snapshot_fault==2)s->sequence=0;else s->width=0;return true;}return fake_snapshot(c,s);}
static risc_touch_api_v1 raw_touch;
static bool gauge_read(void*c,risc_battery_sample_v1*s){(void)c;*s=(risc_battery_sample_v1){3970,42,PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT|PORTABLE_POWER_CHARGER_ENABLED};return true;}
static const risc_battery_gauge_api_v1 gauge={1,sizeof(gauge),NULL,gauge_read};
static bool yes(void*c){(void)c;return true;}
static bool key_events(void*c,uint32_t*e){(void)c;*e=0;return true;}
static const risc_power_key_api_v1 key_api={1,sizeof(key_api),NULL,key_events};
static wifi_link_t wifi_status(void*c){(void)c;return WIFI_LINK_DOWN;}
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=wifi_status,.disconnect_checked=yes};
static bool ble_set(void*c,bool v){(void)c;bt_on=v;return true;}
static bool ble_status(void*c,uint8_t*v){(void)c;*v=bt_on?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 ble_api={.api_version=1,.struct_size=sizeof(ble_api),.set_enabled=ble_set,.status=ble_status};
static bool tx_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;(void)allow;(void)p;return true;}
static bool tx_status(void*c,telemetry_broadcast_status_v1*s){(void)c;*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.settings_valid=true};return true;}
static int32_t tx_enum(void*c,uint32_t n,risc_telemetry_field_v1*out){(void)c;(void)n;(void)out;return 0;}
static int32_t tx_read(void*c,uint32_t n,int32_t*out){(void)c;(void)n;(void)out;return 0;}
static const telemetry_broadcast_v1 tx_api={1,sizeof(tx_api),NULL,tx_step,yes,tx_status,tx_enum,tx_read};
static bool context_step(void*c,const contexts_policy_v1*p){(void)c;(void)p;return true;}
static bool context_status(void*c,contexts_status_v1*s){(void)c;*s=(contexts_status_v1){.struct_size=sizeof(*s)};return true;}
static bool context_export(void*c,uint32_t n){(void)c;(void)n;return true;}
static bool context_record(void*c,uint32_t a,uint32_t b,uint32_t d,const void*e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;return true;}
static bool context_finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;return true;}
static int32_t context_label(void*c,uint32_t a,uint32_t b,contexts_label_v1*d){(void)c;(void)a;(void)b;(void)d;return 0;}
static bool context_claim(void*c,uint32_t a,uint32_t b,const char*n,uint32_t d){(void)c;(void)a;(void)b;(void)n;(void)d;return true;}
static bool context_result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;return true;}
static const contexts_service_v1 context_api={.api_version=1,.struct_size=CONTEXTS_SERVICE_V1_SIZE,.step=context_step,.pause=yes,.status=context_status,.request_export=context_export,.begin_export=context_export,.export_record=context_record,.finish_export=context_finish,.label=context_label,.claim_preset=context_claim,.preset_result=context_result,.capture_audio=yes};
static int32_t alarm_status(void*c,alarm_status_v1*s){
#ifdef BATTERY_OVERLAY_CONTROLS
 if(mode>=25)return overlay_status(c,s);
#endif
(void)c;if((mode==11||mode==13)&&!cue_started&&ticks>=(mode==13?58996:59996)){cue_started=alarm_cue=true;alarm_pumps=0;printf("CUE start=%u\n",ticks);}*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alarm_cue?ALARM_STATE_CUE:ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t alarm_step(void*c){(void)c;if(alarm_cue&&++alarm_pumps>=2){alarm_cue=false;cue_finished=true;printf("CUE finish=%u\n",ticks);}return ALARM_OK;}
static int32_t alarm_resume(void*c,const alarm_sleep_v1*s){(void)c;(void)s;return ALARM_OK;}
static int32_t temporal_alarm_ack(void *c,const alarm_token_v1 *t){
#ifdef BATTERY_OVERLAY_CONTROLS
 if(mode>=25)return overlay_ack(c,t);
#endif
 return fake_alarm_ack(c,t);
}
static const alarm_service_sleep_v1 alarm_full={.base={1,sizeof(alarm_full),NULL,alarm_status,alarm_step,fake_alarm_step,temporal_alarm_ack,fake_alarm_prepare,fake_alarm_step},.resume_sleep=alarm_resume};
static bool wake_pending(void*c,bool*p){(void)c;*p=false;return true;}
static int32_t light_for(void*c,uint32_t ms,risc_light_sleep_result_v1*r){(void)c;light_entries++;assert(!frames&&!subs&&!pending_token);printf("LIGHT time=%u requested_ms=%u pending_raw=%u queued=%u\n",ticks,ms,fake_touch_state.contact_count,fake_event_count);*r=(risc_light_sleep_result_v1){.struct_size=sizeof(*r),.wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO};ticks+=3100;return RISC_LIGHT_SLEEP_OK;}
static int32_t deep_set(void*c,uint32_t ms){(void)c;(void)ms;assert(!"Unexpected Deep");return RISC_DEEP_SLEEP_UNSUPPORTED;}
static bool tap_info(void*c,twatch_tap_info_v1*i){(void)c;*i=(twatch_tap_info_v1){sizeof(*i),TWATCH_TAP_BMA423,0,255,4};return true;}
static bool tap_config(void*c,uint16_t v){(void)c;(void)v;return true;}
static bool tap_observe(void*c,twatch_tap_observation_v1*v){(void)c;*v=(twatch_tap_observation_v1){.struct_size=sizeof(*v)};return true;}
static const twatch_motion_api_v1 motion={.api_version=1,.struct_size=sizeof(motion),.prepare_wake=yes,.wake_pending=wake_pending,.resume_wake=yes,.tap_info=tap_info,.tap_configure=tap_config,.tap_observe_begin=tap_config,.tap_observe=tap_observe};
static const risc_power_sleep_api_v1 pmu={.api_version=1,.struct_size=sizeof(pmu),.prepare_sleep=yes,.resume=yes,.light_sleep_for=light_for,.sleep_wake_pending=wake_pending,.light_sleep_set=light_for,.deep_sleep_set=deep_set};
static const risc_display_power_api_v1 panel={.api_version=1,.struct_size=RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE,.prepare_sleep=yes,.resume=yes};
#ifdef BATTERY_OVERLAY_CONTROLS
static const risc_key_value_v1 overlay_kv={1,sizeof(overlay_kv),NULL,overlay_get,fake_put};
#endif
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 assert(g->struct_size==sizeof(*g));const void*api=NULL;
 if(!strcmp(n,"display.output")&&v==1&&id==0)api=&output;
 else if(!strcmp(n,"input.touch.raw")&&v==1&&id==0)api=&raw_touch;
 else if(!strcmp(n,"board.battery")&&v==1&&id==0)api=&gauge;
 else if(!strcmp(n,"rtc.clock")&&v==2&&id==0)api=&rtc_api;
 else if(!strcmp(n,"storage.key-value")&&v==1&&id==1)api=
#ifdef BATTERY_OVERLAY_CONTROLS
 mode>=25?&overlay_kv:
#endif
 &kv_api;
 else if(!strcmp(n,"storage.key-value.atomic")&&v==1&&id==1)api=&uat_api;
 else if(!strcmp(n,"alarm.service")&&v==1&&id==0)api=mode==8?&alarm_api:&alarm_full.base;
 else if(!strcmp(n,"input.power-key")&&v==1&&id==0)api=&key_api;
 else if(!strcmp(n,"net.wifi")&&v==1&&id==15)api=&wifi_api;
 else if(!strcmp(n,"bluetooth.hci")&&v==1&&id==16)api=&ble_api;
 else if(!strcmp(n,"telemetry.broadcast")&&v==1&&id==0)api=&tx_api;
 else if(!strcmp(n,"contexts.service")&&v==1&&id==0)api=&context_api;
 else if(!strcmp(n,"power.sleep")&&v==1&&id==0)api=&pmu;
 else if(!strcmp(n,"motion.accel")&&v==1&&id==0)api=&motion;
 else if(!strcmp(n,"display.power")&&v==1&&id==0)api=&panel;
 if(!api){denials++;fprintf(stderr,"DENIED %s@%u instance=%llu\n",n,v,(unsigned long long)id);return false;}
 static uint32_t slot;g->api=api;g->slot=++slot;g->generation=1;grants++;return true;
}
static bool release(risc_runtime_capability_v1*g){check_pending_image();assert(g->api&&g->slot&&g->generation&&grants);if(g->api==&output)assert(!pending_token&&!frames);uint32_t size=g->struct_size;*g=(risc_runtime_capability_v1){.struct_size=size};grants--;return true;}
static const risc_runtime_api_v1 diag_runtime={.api_version=1,.struct_size=sizeof(diag_runtime),.health=healthy,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=acquire,.release=release,.retain_invocation=retain,.exit_requested=exit_requested};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&diag_runtime:NULL;}
#include BATTERY_DIAG_APP
int __real_portable_app_alarm_sleep(const risc_runtime_api_v1*,const risc_display_output_api_v1*,const risc_battery_gauge_api_v1*,const alarm_service_v1*);
int __wrap_portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){bridge_entries++;diag_trace("bridge-entry");int rc=__real_portable_app_alarm_sleep(r,d,b,a);printf("BRIDGE result=%d\n",rc);return rc;}
static void step(const char*n){t5_app_input_t out={0};bool ok=diag_poll(0,&out);diag_trace(n);printf("INPUT ok=%u tap=%u x=%d y=%d back=%u\n",ok,out.tapped,out.touch_x,out.touch_y,!!(out.buttons&T5_APP_BUTTON_BACK));assert(ok&&!retained);}
static void held_now(void){unsigned t=script_step();for(unsigned i=0;i<8;i++)actions[action_count++]=(typeof(actions[0])){t+i,60,210};}
#if defined(PORTABLE_BATTERY_NATIVE_UI) && !defined(BATTERY_EXPECT_ORIGINAL)
bool diag_visible(void);unsigned long long diag_generation(void);unsigned diag_owner(void);
static int point_x,point_y;
static t5_app_input_t one(void){t5_app_input_t in={0};assert(diag_poll(0,&in));assert(!diag_failed()&&!retained);return in;}
static void neutral(void){for(unsigned i=0;i<4;i++){t5_app_input_t in=one();assert(!in.tapped);}}
static void edge(bool down){ticks++;risc_touch_contact_v1 c={.id=1,.x=point_x,.y=point_y};fake_edge(down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,c);fake_touch_state.contact_count=down;fake_touch_state.contacts[0]=c;fake_touch_state.timestamp_ms=ticks;}
static void fresh_tap(void){edge(true);t5_app_input_t in=one();assert(!in.tapped);edge(false);in=one();assert(in.tapped&&in.touch_x==point_x&&in.touch_y==point_y);neutral();}
static void submitted(void){for(unsigned i=0;!pending_token&&i<20000;i++){assert(!one().tapped);}assert(pending_token);}
static void completed_one(void){unsigned target=present_completions+1;for(unsigned i=0;present_completions<target&&i<20000;i++){assert(!one().tapped);}assert(present_completions==target);}
static void manual_start(void){
 power_app=t5_app_get_api(1);utility_frame_reset();portable_battery_native_begin();
 battery_native_layout l;assert(battery_native_layout_make(&l,test_width,test_height,2));point_x=l.footer[0].x+l.footer[0].w/2;point_y=l.footer[0].y+l.footer[0].h/2;
 power_sample=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};power_read_ok=power_details=power_navigation_failed=false;power_detail_page=0;power_broadcast=power_broadcast_error=power_broadcast_requested=false;memset(&power_broadcast_state,0,sizeof(power_broadcast_state));power_refresh();present_delay=30;power_render();submitted();
}
static void temporal_case(void){
 manual_start();assert(pending_token&&!diag_visible()&&diag_owner()==3);
 if(mode==14||mode==15){
  edge(true);if(mode==14)edge(false);assert(diag_drain());assert(diag_visible());neutral();
  if(mode==15){edge(false);assert(!one().tapped);neutral();}
  fresh_tap();
 }else{
  assert(diag_drain()&&diag_visible());neutral();
  if(mode==16||mode==17||mode==20||(mode>=21&&mode<=23)){edge(true);assert(!one().tapped);}
  power_render();submitted();assert(pending_token&&!diag_visible()&&diag_owner()==3);
  if(mode==16){assert(diag_drain()&&diag_visible());edge(false);assert(one().tapped);neutral();fresh_tap();}
  else if(mode==17){
   unsigned long long generation=diag_generation();power_details=true;power_render();assert(diag_generation()>generation);
   completed_one();assert(!diag_visible());if(utility_frame_pending)power_render();assert(diag_drain()&&diag_visible());neutral();edge(false);assert(!one().tapped);neutral();fresh_tap();
  }else if(mode==18){
   unsigned long long generation=diag_generation();portable_battery_native_begin();assert(diag_generation()>generation&&pending_token&&diag_owner()==3);
   power_render();completed_one();assert(!diag_visible());if(utility_frame_pending)power_render();assert(diag_drain()&&diag_visible());neutral();fresh_tap();
  }else if(mode==19){
   assert(diag_drain());portable_battery_native_begin();portable_battery_native_layout(UINT64_C(1)<<32|(uint64_t)test_width<<16|test_height);
   assert(power_app->frame_ready());power_app->clear();power_app->present(false);submitted();assert(pending_token&&diag_owner()==0);assert(diag_drain()&&!diag_visible());
   edge(true);assert(!one().tapped);edge(false);assert(!one().tapped);neutral();power_render();assert(diag_drain()&&diag_visible());neutral();fresh_tap();
  }else if(mode==20){
   edge(false);edge(true);edge(false);assert(diag_drain()&&diag_visible());assert(one().tapped);assert(!one().tapped);neutral();fresh_tap();
  }else if(mode>=21&&mode<=23){
   snapshot_fault=mode-20;bool drained=diag_drain();assert(!diag_visible());
   if(mode==21){assert(!drained&&retained==1&&diag_failed());return;}
   assert(drained&&!retained);snapshot_fault=0;edge(false);neutral();power_render();assert(diag_drain()&&diag_visible());neutral();fresh_tap();
  }else if(mode==24){
   presentation_fault=true;assert(!diag_drain()&&retained==1&&diag_failed()&&pending_token);check_pending_image();return;
  }else assert(!"Unknown temporal case");
 }
 assert(!pending_token&&!frames);printf("TEMPORAL case=%u visible=%u generation=%llu raw_sequence=%llu PASS\n",mode,diag_visible(),diag_generation(),(unsigned long long)fake_touch_state.sequence);
}
#endif
#ifdef BATTERY_OVERLAY_CONTROLS
#include "battery_native_overlay_cases.h"
#endif
int main(int argc,char**argv){setvbuf(stdout,NULL,_IONBF,0);assert(argc==2||argc==4);if(argc==4){test_width=(unsigned)atoi(argv[2]);test_height=(unsigned)atoi(argv[3]);}mode=(unsigned)atoi(argv[1]);uat_reset();raw_touch=touch_api;raw_touch.poll=raw_poll;raw_touch.snapshot=raw_snapshot;raw_touch.unsubscribe=raw_unsub;output=display_api;output.get_info=info;output.present_status=presented;output.set_brightness=brightness;memset(pixels,0xa5,sizeof(pixels));present_delay=3;
 assert(app_module_init()==0);diag_trace("initialized");
 #if defined(PORTABLE_BATTERY_NATIVE_UI) && !defined(BATTERY_EXPECT_ORIGINAL)
 #ifdef BATTERY_OVERLAY_CONTROLS
 if(mode>=25)overlay_case();else
#endif
 if(mode>=14)temporal_case();else
#endif
 if(mode==6||mode==7||mode==9||mode==10||(mode>=11&&mode<=13)){present_delay=mode>=11?3:120;if(mode==12||mode==13)for(unsigned i=0;i<8;i++)actions[action_count++]=(typeof(actions[0])){2400+i,60,210};if(mode<11)actions[action_count++]=(typeof(actions[0])){mode==6||mode==9?2:8,60,210};app_main();printf("ACTION mode=%u first_visible=%u details_seen=%u details_at=%u entries=%u\n",mode,first_visible,details_seen,details_at,bridge_entries);if(mode==11)assert(bridge_entries==
#ifdef BATTERY_EXPECT_ORIGINAL
1
#else
0
#endif
&&light_entries==bridge_entries&&contact_injected);else if(mode==9
#ifndef BATTERY_EXPECT_ORIGINAL
||mode==6
#endif
)assert(!details_seen);else if(mode==12||mode==13)assert(!bridge_entries);else assert(details_seen);}
 else {
  power_app=t5_app_get_api(1);utility_frame_reset();power_sample=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};power_read_ok=power_details=power_navigation_failed=false;power_detail_page=0;power_broadcast=power_broadcast_error=power_broadcast_requested=false;memset(&power_broadcast_state,0,sizeof(power_broadcast_state));power_refresh();power_render();assert(diag_drain());diag_trace("visible");
#ifndef BATTERY_EXPECT_ORIGINAL
  if(mode==0||mode==1||mode==8)step("clear-presentation-cancellation");
#endif

  if(mode==0||mode==8){ticks=diag_activity()+diag_idle();step("idle-no-input");assert(bridge_entries==1);assert(light_entries==(mode==0));}
  else if(mode==1){ticks=diag_activity()+diag_idle();held_now();step("fresh-at-deadline");assert(!bridge_entries&&diag_down());}
  else if(mode==2||mode==3||mode==4||mode==5){
   step("warmup");ticks=diag_activity()+diag_idle()-1000;alarm_cue=true;alarm_pumps=0;step("cue-finished");assert(diag_cancel()&&!bridge_entries);
   if(mode==3){step("clear-cancellation");assert(!bridge_entries);}
   ticks=diag_activity()+diag_idle();
   if(mode==4){step("cancel-no-input");
#ifndef BATTERY_EXPECT_ORIGINAL
 assert(!bridge_entries);step("after-cancellation-no-input");
#endif
 assert(bridge_entries==1);}
   else {held_now();step(mode==3?"fresh-after-neutral":"fresh-after-cue-cancel");if(mode==3)assert(!bridge_entries&&diag_down());else {
#ifdef BATTERY_EXPECT_ORIGINAL
 assert(bridge_entries==1&&light_entries==1);
#else
 assert(!bridge_entries&&!light_entries);step("read-held-after-cancellation");assert(!bridge_entries);
#endif
 }}
  }
 }
 if(mode==21||mode==24){unsigned g=grants,f=frames,u=subs;app_module_fini();assert(grants==g&&frames==f&&subs==u&&g&&retained==1);check_pending_image();printf("CUSTODY case=%u retained=%u grants=%u subs=%u pending=%llu PASS\n",mode,retained,grants,subs,(unsigned long long)pending_token);return 0;}
 app_module_fini();printf("RESULT mode=%u bridge=%u light=%u retains=%u grants=%u frames=%u subs=%u pending=%llu denials=%u atomic_commits=%u\n",mode,bridge_entries,light_entries,retained,grants,frames,subs,(unsigned long long)pending_token,denials,uat.commits);assert(!grants&&!frames&&!subs&&!pending_token&&!retained);return 0;
}
