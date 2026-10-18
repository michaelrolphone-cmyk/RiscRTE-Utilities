/* Full-profile production Alarms/Countdown/adapter/Watch bridge diagnostic with modeled services. */
#define main old_fixture_main
#define risc_runtime_get_api old_get_runtime
#define portable_input_navigation_open old_navigation_open
#define portable_input_navigation_close old_navigation_close
#define portable_app_alarm_sleep old_alarm_sleep
#define grants fixture_grants
#include ALARMS_DIAG_PERIPHERALS
#undef grants
#undef main
#undef risc_runtime_get_api
#undef portable_input_navigation_open
#undef portable_input_navigation_close
#undef portable_app_alarm_sleep
#include "PortablePowerStatus.h"
#include "PortableQuickRadios.h"
#include "PortableQuickPreferences.h"
#include "TelemetryBroadcastV1.h"
#include "ContextsServiceV1.h"
#include "RiscPowerKeyV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscDisplayPowerV1.h"
#include "PortableMotionTap.h"
#include ALARMS_DIAG_ATOMIC
void diag_trace(const char*);bool diag_poll(unsigned,t5_app_input_t*);unsigned diag_activity(void),diag_idle(void),diag_cancel(void),diag_down(void),diag_failed(void);bool diag_drain(void);
static unsigned mode,light_entries,bridge_entries,retained,first_visible,alarm_pumps,denials;
static bool alarm_cue,bt_on;
static bool cue_finished,scripted_input;
static unsigned snapshot_fault;static bool presentation_fault;
static void held_now(void);
static const risc_runtime_api_v1 diag_runtime;
static bool editor_seen;static void observe_controller(void);
static unsigned quick_opens;
static bool diagnostic(const char *s){if(strstr(s,"quick-controls-open"))quick_opens++;return fake_diag(s);}
static bool healthy(risc_runtime_health_v1*h){observe_controller();h->struct_size=sizeof(*h);h->uptime_ms=ticks;return true;}
static bool retain(void){retained++;fprintf(stderr,"RETAIN mode=%u ticks=%u\n",mode,ticks);return true;}
static bool exit_requested(void){return (mode==6||mode==7)&&ticks>=500;}
static bool info(void*c,risc_display_info_v1*s){fake_info(c,s);s->api_version=1;s->struct_size=sizeof(*s);s->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;return true;}
static bool brightness(void*c,uint16_t l,uint16_t max){(void)c;assert(max&&l<=max);return true;}
bool diag_quick_modal(void);
static bool presented(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){if(presentation_fault)return false;if(diag_quick_modal())quick_opens=1;bool ok=fake_present(c,t,s);if(!first_visible&&s->state==RISC_DISPLAY_PRESENT_COMPLETE){first_visible=ticks;printf("VISIBLE token=%llu time=%u\n",(unsigned long long)t,ticks);}return ok;}
static risc_display_output_api_v1 output;
static bool raw_poll(void*c,size_t n){if(mode>=14&&!scripted_input){(void)c;assert(n==1);polls++;return true;}uint64_t before=fake_touch_state.sequence;bool ok=fake_touch_poll(c,n);if(fake_touch_state.sequence!=before)printf("RAW time=%u seq=%llu contacts=%u queued=%u visible=%u pending=%llu cancel=%u\n",ticks,(unsigned long long)fake_touch_state.sequence,fake_touch_state.contact_count,fake_event_count,first_visible,(unsigned long long)pending_token,diag_cancel());return ok;}
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
static int32_t alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alarm_cue?ALARM_STATE_CUE:ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t alarm_step(void*c){(void)c;if(alarm_cue&&++alarm_pumps>=2){alarm_cue=false;cue_finished=true;printf("CUE finish=%u\n",ticks);}return ALARM_OK;}
static int32_t alarm_resume(void*c,const alarm_sleep_v1*s){(void)c;(void)s;return ALARM_OK;}
static const alarm_service_sleep_v1 alarm_full={.base={1,sizeof(alarm_full),NULL,alarm_status,alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step},.resume_sleep=alarm_resume};
static bool wake_pending(void*c,bool*p){(void)c;*p=false;return true;}
static int32_t light_for(void*c,uint32_t ms,risc_light_sleep_result_v1*r){(void)c;light_entries++;assert(!frames&&!subs&&!pending_token);printf("LIGHT time=%u requested_ms=%u pending_raw=%u queued=%u\n",ticks,ms,fake_touch_state.contact_count,fake_event_count);*r=(risc_light_sleep_result_v1){.struct_size=sizeof(*r),.wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO};ticks+=3100;return RISC_LIGHT_SLEEP_OK;}
static int32_t deep_set(void*c,uint32_t ms){(void)c;(void)ms;assert(!"Unexpected Deep");return RISC_DEEP_SLEEP_UNSUPPORTED;}
static bool tap_info(void*c,twatch_tap_info_v1*i){(void)c;*i=(twatch_tap_info_v1){sizeof(*i),TWATCH_TAP_BMA423,0,255,4};return true;}
static bool tap_config(void*c,uint16_t v){(void)c;(void)v;return true;}
static bool tap_observe(void*c,twatch_tap_observation_v1*v){(void)c;*v=(twatch_tap_observation_v1){.struct_size=sizeof(*v)};return true;}
static const twatch_motion_api_v1 motion={.api_version=1,.struct_size=sizeof(motion),.prepare_wake=yes,.wake_pending=wake_pending,.resume_wake=yes,.tap_info=tap_info,.tap_configure=tap_config,.tap_observe_begin=tap_config,.tap_observe=tap_observe};
static const risc_power_sleep_api_v1 pmu={.api_version=1,.struct_size=sizeof(pmu),.prepare_sleep=yes,.resume=yes,.light_sleep_for=light_for,.sleep_wake_pending=wake_pending,.light_sleep_set=light_for,.deep_sleep_set=deep_set};
static const risc_display_power_api_v1 panel={.api_version=1,.struct_size=RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE,.prepare_sleep=yes,.resume=yes};
static bool storage_fail;static unsigned storage_writes;
static int32_t controller_put(void*c,const char*k,const void*b,uint32_t n){storage_writes++;if(storage_fail)return RISC_KEY_VALUE_IO;return fake_put(c,k,b,n);}
static const risc_key_value_v1 controller_kv={1,sizeof(controller_kv),NULL,fake_get,controller_put};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 assert(g->struct_size==sizeof(*g));const void*api=NULL;
 if(!strcmp(n,"display.output")&&v==1&&id==0)api=&output;
 else if(!strcmp(n,"input.touch.raw")&&v==1&&id==0)api=&raw_touch;
 else if(!strcmp(n,"board.battery")&&v==1&&id==0)api=&gauge;
 else if(!strcmp(n,"rtc.clock")&&v==2&&id==0)api=&rtc_api;
 else if(!strcmp(n,"storage.key-value")&&v==1&&(id==1||id==3))api=&controller_kv;
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
 static uint32_t slot;g->api=api;g->slot=++slot;g->generation=1;fixture_grants++;return true;
}
static bool release(risc_runtime_capability_v1*g){check_pending_image();assert(g->api&&g->slot&&g->generation&&fixture_grants);if(g->api==&output)assert(!pending_token&&!frames);uint32_t size=g->struct_size;*g=(risc_runtime_capability_v1){.struct_size=size};fixture_grants--;return true;}
static const risc_runtime_api_v1 diag_runtime={.api_version=1,.struct_size=sizeof(diag_runtime),.health=healthy,.yield_ms=fake_yield,.diagnostic=diagnostic,.request_launch=fake_launch,.acquire=acquire,.release=release,.retain_invocation=retain,.exit_requested=exit_requested};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&diag_runtime:NULL;}
#include ALARMS_DIAG_APP
static void observe_controller(void){if(alarm_page==1)editor_seen=true;}
int __real_portable_app_alarm_sleep(const risc_runtime_api_v1*,const risc_display_output_api_v1*,const risc_battery_gauge_api_v1*,const alarm_service_v1*);
int __wrap_portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){bridge_entries++;diag_trace("bridge-entry");int rc=__real_portable_app_alarm_sleep(r,d,b,a);printf("BRIDGE result=%d\n",rc);return rc;}
static void step(const char*n){t5_app_input_t out={0};bool ok=diag_poll(0,&out);diag_trace(n);printf("INPUT ok=%u tap=%u x=%d y=%d back=%u\n",ok,out.tapped,out.touch_x,out.touch_y,!!(out.buttons&T5_APP_BUTTON_BACK));assert(ok&&!retained);}
static void held_now(void){unsigned t=script_step();for(unsigned i=0;i<8;i++)actions[action_count++]=(typeof(actions[0])){t+i,(int)test_width/2,(int)test_height/2};}
static void controller_start(void){
 app=t5_app_get_api(1);utility_frame_reset();
#ifdef PORTABLE_ALARMS_NATIVE_UI
 portable_alarms_native_begin();
#endif
 assert(open_dependencies());assert(alarm_writer_load(&writer,storage,DAILY_ALARM_KIND)==0);refresh_status();
 alarm_page=alarm_field=0;editing=false;notice="";values[0]=10;values[1]=5;values[2]=1;
#if DAILY_ALARM_KIND == 1
 volume_uncertain=false;volume_message="";volume_load();
#endif
}
#if defined(PORTABLE_ALARMS_NATIVE_UI) && !defined(ALARMS_EXPECT_ORIGINAL)
bool diag_visible(void);unsigned long long diag_generation(void);unsigned diag_owner(void);
static int point_x,point_y;
static t5_app_input_t one(void){t5_app_input_t in={0};assert(diag_poll(0,&in));assert(!diag_failed()&&!retained);return in;}
static void neutral(void){for(unsigned i=0;i<4;i++){t5_app_input_t in=one();assert(!in.tapped);}}
static void edge(bool down){ticks++;risc_touch_contact_v1 c={.id=1,.x=point_x,.y=point_y};fake_edge(down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,c);fake_touch_state.contact_count=down;fake_touch_state.contacts[0]=c;fake_touch_state.timestamp_ms=ticks;}
static void dispatch(t5_app_input_t in){if(in.tapped){assert(alarm_tap(in.touch_x,in.touch_y));}}
static void fresh_tap(void){unsigned before=values[alarm_field];edge(true);t5_app_input_t in=one();assert(!in.tapped);edge(false);in=one();assert(in.tapped&&in.touch_x==point_x&&in.touch_y==point_y);dispatch(in);assert(values[alarm_field]!=before);neutral();}
static void submitted(void){for(unsigned i=0;!pending_token&&i<20000;i++){assert(!one().tapped);}assert(pending_token);}
static void completed_one(void){unsigned target=present_completions+1;for(unsigned i=0;present_completions<target&&i<20000;i++){assert(!one().tapped);}assert(present_completions==target);}
static void manual_start(void){
 controller_start();alarm_page=2;alarm_field=1;editing=true;
 alarms_native_layout l=alarm_native_layout();point_x=l.plus.x+l.plus.w/2;point_y=l.plus.y+l.plus.h/2;
 present_delay=30;draw();submitted();
}

bool diag_quick_visible(void);bool diag_sleep(void);void diag_quick_points(int *,int *,int *);
static void paint(void){draw();assert(diag_drain());for(unsigned i=0;i<4&&utility_frame_pending;i++){draw();assert(diag_drain());}assert(!utility_frame_pending&&!pending_token);neutral();}
static bool controller_tap(alarms_native_rect rect){point_x=rect.x+rect.w/2;point_y=rect.y+rect.h/2;edge(true);assert(!one().tapped);edge(false);t5_app_input_t in=one();assert(in.tapped);bool accepted=alarm_tap(in.touch_x,in.touch_y);paint();return accepted;}
static void lifecycle_case(void){
 manual_start();assert(diag_drain()&&diag_visible());neutral();unsigned before=values[alarm_field];
 if(mode==25||mode==26){
  scripted_input=true;unsigned t=script_step()+1;int x=(int)test_width/2,slider,handle,right;diag_quick_points(&slider,&handle,&right);
  actions[action_count++]=(typeof(actions[0])){t,x,1};
  for(unsigned i=1;i<4;i++)actions[action_count++]=(typeof(actions[0])){t+i,x,(int)test_height-2};
  if(mode==26){actions[action_count++]=(typeof(actions[0])){t+24,right,slider};actions[action_count++]=(typeof(actions[0])){t+25,right,slider};}
  actions[action_count++]=(typeof(actions[0])){t+40,x,handle};
  unsigned deadline=(t+48)*25;
  while(ticks<deadline){t5_app_input_t in={0};assert(diag_poll(20,&in));assert(!in.tapped&&!retained);}
  assert(quick_opens==1&&!diag_quick_visible()&&values[alarm_field]==before&&!bridge_entries);
  if(mode==26){assert(uat.commits==1);int at=uat_find(PQA_VOLUME_KEY);assert(at>=0&&uat.records[at].size==1&&uat.records[at].value[0]==100);}
  else assert(!uat.commits);
  action_count=0;paint();point_x=alarm_native_layout().plus.x+alarm_native_layout().plus.w/2;point_y=alarm_native_layout().plus.y+alarm_native_layout().plus.h/2;
  /* Switch back to explicit queued edges after the scripted modal. */
  scripted_input=false;fresh_tap();
 }else if(mode==27){
  alarms_native_layout l=alarm_native_layout();
#if DAILY_ALARM_KIND == 1
  alarm_page=3;volume_load();paint();assert(volume_value==50);assert(controller_tap(l.plus));assert(volume_choice==60&&volume_value==50);
  storage_fail=true;assert(controller_tap(l.footer[1]));assert(volume_uncertain&&volume_value==50&&alarm_page==3);
  assert(!alarm_back());assert(!controller_tap(l.plus));assert(volume_choice==60);assert(!controller_tap(l.footer[0]));
  storage_fail=false;assert(controller_tap(l.footer[1]));assert(!volume_uncertain&&volume_value==60&&!alarm_page);
  assert(controller_tap(l.secondary));assert(volume_value==60&&volume_choice==60);assert(controller_tap(l.minus));assert(controller_tap(l.footer[0]));assert(volume_value==60);
#else
  alarm_page=0;paint();storage_fail=true;assert(controller_tap(l.footer[0]));assert(writer.uncertain&&!writer.saved.enabled&&writer.pending.revision==1);
  alarm_config pending=writer.pending;assert(controller_tap(l.footer[1]));assert(writer.uncertain&&!memcmp(&pending,&writer.pending,sizeof(pending)));
  storage_fail=false;assert(controller_tap(l.footer[0]));assert(!writer.uncertain&&writer.saved.enabled&&writer.saved.revision==1);
  assert(controller_tap(l.footer[1]));assert(!writer.saved.enabled&&writer.saved.revision==2);
#endif
  assert(storage_writes>=2);
 }else if(mode==28){
  unsigned private_grants=acquired;alarm_writer saved=writer;draw();submitted();assert(diag_sleep()&&!bridge_entries&&pending_token);
  assert(diag_drain());neutral();assert(diag_sleep());assert(bridge_entries==1&&light_entries==1&&acquired==private_grants&&values[alarm_field]==before&&!memcmp(&writer,&saved,sizeof(writer)));
  paint();fresh_tap();
 }else if(mode==29){
  edge(true);assert(!one().tapped);alarm_cue=true;alarm_pumps=0;assert(!one().tapped);assert(!alarm_cue&&cue_finished&&diag_cancel());
  edge(false);assert(!one().tapped);neutral();assert(values[alarm_field]==before&&!storage_writes&&!bridge_entries);paint();fresh_tap();
 }else assert(!"Unknown feature lifecycle");
 printf("LIFECYCLE case=%u quick_opens=%u atomic_commits=%u writes=%u sleep=%u PASS\n",mode,quick_opens,uat.commits,storage_writes,bridge_entries);
}

static void temporal_case(void){
 manual_start();assert(pending_token&&!diag_visible()&&diag_owner()==4);
 if(mode==14||mode==15){
  edge(true);if(mode==14)edge(false);assert(diag_drain());assert(diag_visible());neutral();
  if(mode==15){edge(false);assert(!one().tapped);neutral();}
  fresh_tap();
 }else{
  assert(diag_drain()&&diag_visible());neutral();
  if(mode==16||mode==17||mode==20||(mode>=21&&mode<=23)){edge(true);assert(!one().tapped);}
  draw();submitted();assert(pending_token&&!diag_visible()&&diag_owner()==4);
  if(mode==16){assert(diag_drain()&&diag_visible());edge(false);t5_app_input_t in=one();assert(in.tapped);unsigned before=values[alarm_field];dispatch(in);assert(values[alarm_field]!=before);neutral();fresh_tap();}
  else if(mode==17){
   unsigned long long generation=diag_generation();alarm_field=0;draw();assert(diag_generation()>generation);
   completed_one();assert(!diag_visible());if(utility_frame_pending)draw();assert(diag_drain()&&diag_visible());neutral();edge(false);assert(!one().tapped);neutral();fresh_tap();
  }else if(mode==18){
   unsigned long long generation=diag_generation();portable_alarms_native_begin();assert(diag_generation()>generation&&pending_token&&diag_owner()==4);
   draw();completed_one();assert(!diag_visible());if(utility_frame_pending)draw();assert(diag_drain()&&diag_visible());neutral();fresh_tap();
  }else if(mode==19){
   assert(diag_drain());portable_alarms_native_begin();portable_alarms_native_layout(UINT64_C(1)<<32|(uint64_t)test_width<<16|test_height);
   assert(app->frame_ready());app->clear();app->present(false);submitted();assert(pending_token&&diag_owner()==0);assert(diag_drain()&&!diag_visible());
   edge(true);assert(!one().tapped);edge(false);assert(!one().tapped);neutral();draw();assert(diag_drain()&&diag_visible());neutral();fresh_tap();
  }else if(mode==20){
   edge(false);edge(true);edge(false);assert(diag_drain()&&diag_visible());t5_app_input_t in=one();assert(in.tapped);unsigned before=values[alarm_field];dispatch(in);assert(values[alarm_field]!=before);assert(!one().tapped);neutral();fresh_tap();
  }else if(mode>=21&&mode<=23){
   snapshot_fault=mode-20;bool drained=diag_drain();assert(!diag_visible());
   if(mode==21){assert(!drained&&retained==1&&diag_failed());return;}
   assert(drained&&!retained);snapshot_fault=0;edge(false);neutral();draw();assert(diag_drain()&&diag_visible());neutral();fresh_tap();
  }else if(mode==24){
   presentation_fault=true;assert(!diag_drain()&&retained==1&&diag_failed()&&pending_token);check_pending_image();return;
  }else assert(!"Unknown temporal case");
 }
 assert(!pending_token&&!frames);printf("TEMPORAL case=%u visible=%u generation=%llu raw_sequence=%llu value=%u PASS\n",mode,diag_visible(),diag_generation(),(unsigned long long)fake_touch_state.sequence,values[alarm_field]);
}
#endif
int main(int argc,char**argv){
 setvbuf(stdout,NULL,_IONBF,0);assert(argc==4);test_width=(unsigned)atoi(argv[2]);test_height=(unsigned)atoi(argv[3]);mode=(unsigned)atoi(argv[1]);
 uat_reset();raw_touch=touch_api;raw_touch.poll=raw_poll;raw_touch.snapshot=raw_snapshot;raw_touch.unsubscribe=raw_unsub;
 output=display_api;output.get_info=info;output.present_status=presented;output.set_brightness=brightness;memset(pixels,0xa5,sizeof(pixels));present_delay=3;
 assert(app_module_init()==0);diag_trace("initialized");
#if defined(PORTABLE_ALARMS_NATIVE_UI) && !defined(ALARMS_EXPECT_ORIGINAL)
 if(mode>=25)lifecycle_case();else if(mode>=14)temporal_case();else
#endif
 if(mode==6||mode==7){
  present_delay=120;int x=120,y=80;
#ifdef PORTABLE_ALARMS_NATIVE_UI
  app=t5_app_get_api(1);alarms_native_layout l=alarm_native_layout();x=l.primary.x+l.primary.w/2;y=l.primary.y+l.primary.h/2;
#endif
  actions[action_count++]=(typeof(actions[0])){mode==6?2:8,x,y};app_main();
  printf("ACTION case=%u first_visible=%u page=%u entries=%u\n",mode,first_visible,alarm_page,bridge_entries);observe_controller();
#ifdef PORTABLE_ALARMS_NATIVE_UI
  assert(editor_seen==(mode==7));
#else
  assert(editor_seen); /* Measured old editor early-contact behavior. */
#endif
 }else{
  controller_start();draw();assert(diag_drain());diag_trace("visible");step("clear-presentation-cancellation");
  if(mode==0||mode==8){ticks=diag_activity()+diag_idle();step("idle-no-input");assert(bridge_entries==1);assert(light_entries==(mode==0));}
  else if(mode==1){ticks=diag_activity()+diag_idle();held_now();step("fresh-at-deadline");assert(!bridge_entries&&diag_down());}
  else if(mode==2||mode==3||mode==4){
   step("warmup");ticks=diag_activity()+diag_idle()-1000;alarm_cue=true;alarm_pumps=0;step("cue-finished");assert(diag_cancel()&&!bridge_entries);
   if(mode==3){step("clear-cancellation");assert(!bridge_entries);}
   ticks=diag_activity()+diag_idle();
   if(mode==4){step("cancel-no-input");
#ifndef ALARMS_EXPECT_ORIGINAL
   assert(!bridge_entries);step("after-cancellation-no-input");
#endif
   assert(bridge_entries==1);}
   else {held_now();step(mode==3?"fresh-after-neutral":"fresh-after-cue-cancel");if(mode==3)assert(!bridge_entries&&diag_down());else{
#ifdef ALARMS_EXPECT_ORIGINAL
   assert(bridge_entries==1&&light_entries==1); /* Measured legacy idle/cancel race. */
#else
   assert(!bridge_entries&&!light_entries);step("read-held-after-cancellation");assert(!bridge_entries);
#endif
   }}
  }else assert(!"Unknown lifecycle case");
 }
 if(mode==21||mode==24){unsigned g=fixture_grants,f=frames,u=subs;app_module_fini();assert(fixture_grants==g&&frames==f&&subs==u&&g&&retained==1);check_pending_image();printf("CUSTODY case=%u retained=%u grants=%u subs=%u pending=%llu PASS\n",mode,retained,fixture_grants,subs,(unsigned long long)pending_token);return 0;}
 close_dependencies();app_module_fini();printf("RESULT case=%u bridge=%u light=%u retains=%u grants=%u frames=%u subs=%u pending=%llu denials=%u atomic_commits=%u\n",mode,bridge_entries,light_entries,retained,fixture_grants,frames,subs,(unsigned long long)pending_token,denials,uat.commits);assert(!fixture_grants&&!frames&&!subs&&!pending_token&&!retained);return 0;
}
