/* The reused API 1 peripheral double is private to this fixture. The production
 * native adapter below receives only the tagged API 2 test_alarm descriptor. */
#include "AlarmServiceV1.h"
typedef struct {alarm_service_v1 service;uint32_t output_modes;} alarm_service_outputs_v1;
/* Actual controllers plus production shared adapter. Hardware alone is fake. */
#define fake_acquire legacy_fixture_acquire
#define fake_release legacy_fixture_release
#define risc_runtime_get_api legacy_fixture_runtime_get_api
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define portable_input_navigation_open inherited_navigation_open
#include "paper_utility_peripherals.h"
#undef portable_input_navigation_open
#pragma GCC diagnostic pop
#undef fake_acquire
#undef fake_release
#undef risc_runtime_get_api
#include "RiscRealtimeV1.h"
#include "AlarmServiceV2.h"
#include "PortableNativeCustody.h"
static unsigned scenario;static bool modal_acked;
static bool reader_live,retained_flag,deny_reader,release_reader_failure;
static bool kv_context,kv_unknown,kv_io,kv_commits,service_retained;
static int32_t native_read_error;
static unsigned native_reads,retain_count,native_release_count,service_calls;
static int64_t native_epoch=INT64_C(1791115200);
static uint32_t native_validity=RISC_REALTIME_VALID,native_reserved;
static unsigned frozen_polls,frozen_ticks,frozen_presents,frozen_frames,frozen_subs,frozen_grants,frozen_reads,frozen_calls;
static uint32_t frozen_pixels;
static uint32_t pixel_hash(void){uint32_t h=2166136261u;for(unsigned i=0;i<sizeof(pixels);i++)h=(h^((unsigned char*)pixels)[i])*16777619u;return h;}
static bool test_retain(void){assert(!retained_flag);retained_flag=true;retain_count++;frozen_polls=polls;frozen_ticks=ticks;frozen_presents=presents;frozen_frames=frames;frozen_subs=subs;frozen_grants=fixture_grants;frozen_reads=native_reads;frozen_calls=service_calls;frozen_pixels=pixel_hash();return true;}
void __real_free(void *p);void __wrap_free(void *p){assert(!retained_flag);__real_free(p);}
static int32_t test_read(void*c,risc_realtime_snapshot_v1*s){assert(c&&!retained_flag&&reader_live);native_reads++;if(native_read_error)return native_read_error;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=native_validity,.epoch_seconds=native_validity==RISC_REALTIME_VALID?native_epoch+ticks/1000:0,.monotonic_before_us=(uint64_t)ticks*1000,.monotonic_after_us=(uint64_t)ticks*1000,.reserved=native_reserved};return RISC_REALTIME_OK;}
static const risc_realtime_api_v1 reader_api={1,sizeof(reader_api),&reader_live,test_read};
static int32_t test_get(void*c,const char*k,void*b,uint32_t n,uint32_t*s){assert(!retained_flag);if(!strcmp(k,"alarm_utc_cfg")||!strcmp(k,"timer_utc_cfg")){if(kv_context)return RISC_KEY_VALUE_CONTEXT;if(kv_unknown)return -88;}return fake_get(c,k,b,n,s);}
static int32_t test_put(void*c,const char*k,const void*b,uint32_t n){assert(!retained_flag);if(kv_context)return RISC_KEY_VALUE_CONTEXT;if(kv_unknown)return -88;if(kv_io&&(!kv_commits?polls<100:true)){if(kv_commits)(void)fake_put(c,k,b,n);else puts_count++;return RISC_KEY_VALUE_IO;}return fake_put(c,k,b,n);}
static const risc_key_value_v1 test_kv={1,sizeof(test_kv),NULL,test_get,test_put};
static int32_t test_status(void*c,alarm_status_v1*s){assert(!retained_flag);service_calls++;if(service_retained){*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_BLOCKED,.error=-9,.output_uncertain=1};return ALARM_OK;}if(scenario==6&&polls>=45&&!modal_acked){*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_ALERT,.snapshot=1,.mode=ALARM_MODE_VISUAL,.occurrence={DAILY_TEST_KIND,1,800000000,1}};strcpy(s->label,"TEST DEADLINE");return ALARM_OK;}return fake_alarm_status(c,s);}
static int32_t test_step(void*c){assert(!retained_flag);service_calls++;return service_retained?-9:fake_alarm_step(c);}
static int32_t fake_modal_ack(void*c,const alarm_token_v1*t){(void)c;assert(!retained_flag);assert(t->generation==1);modal_acked=true;return ALARM_OK;}
static alarm_service_descriptor_v2 test_alarm={.base={2,sizeof(test_alarm),NULL,test_status,test_step,test_step,fake_modal_ack,fake_alarm_prepare,test_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
#ifdef PORTABLE_BLE_BROADCAST
#include "TelemetryBroadcastV1.h"
static bool bt_active,bt_pause_fail,bt_release_fail;
static unsigned bt_pauses;
static bool bt_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;assert(!retained_flag);bt_active=allow&&p->enabled&&p->settings_valid&&p->radios_allowed;return true;}
static bool bt_pause(void*c){(void)c;assert(!retained_flag);bt_pauses++;if(bt_pause_fail)return false;bt_active=false;return true;}
static bool bt_status(void*c,telemetry_broadcast_status_v1*p){(void)c;assert(!retained_flag);*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p),.state=bt_active?TELEMETRY_BROADCAST_LIVE:TELEMETRY_BROADCAST_OFF};return true;}
static int32_t bt_enumerate(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;assert(!retained_flag);return 0;}
static int32_t bt_read(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;assert(!retained_flag);return 0;}
static const telemetry_broadcast_v1 test_broadcast={1,sizeof(test_broadcast),NULL,bt_step,bt_pause,bt_status,bt_enumerate,bt_read};
#endif
static const risc_input_navigation_api_v1 native_nav;
static const risc_touch_api_v1 native_touch;
static const risc_display_output_api_v1 native_display;
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!retained_flag);if(!strcmp(n,"rtc.clock")){assert(!"Native app/toolbar must never acquire RTC");return false;}if(!strcmp(n,"runtime.realtime.control")){assert(!"No native control authority");return false;}

#ifdef PORTABLE_BLE_BROADCAST
 if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(v==1&&!id);fixture_grants++;g->api=&test_broadcast;}
 else
#endif
 if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(v==1&&!id&&!reader_live);if(deny_reader)return false;reader_live=true;fixture_grants++;g->api=&reader_api;}
 else if(!strcmp(n,"input.navigation")){fixture_grants++;g->api=&native_nav;}
 else if(!strcmp(n,"input.touch.raw")){fixture_grants++;g->api=&native_touch;}
 else if(!strcmp(n,"display.output")){fixture_grants++;g->api=&native_display;}
 else {if(!strcmp(n,ALARM_SERVICE_CAPABILITY))assert(v==2);if(!legacy_fixture_acquire(n,!strcmp(n,ALARM_SERVICE_CAPABILITY)?1:v,id,g))return false;if(g->api==&kv_api)g->api=&test_kv;if(g->api==&alarm_api)g->api=&test_alarm;}
 g->slot=1;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){assert(!retained_flag);if(g->api==&reader_api){native_release_count++;if(release_reader_failure)return false;assert(reader_live);reader_live=false;fixture_grants--;}

#ifdef PORTABLE_BLE_BROADCAST
 else if(g->api==&test_broadcast){if(bt_release_fail)return false;fixture_grants--;}
#endif
 else if(g->api==&battery_api){battery_releases++;fixture_grants--;}
 else if(!legacy_fixture_release(g))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
#ifdef TEST_RESIDENT_CLIENT
#include "resident_test_bridge.h"
#define EXPECTED_HOME_LAUNCHES 0
#else
#define TEST_RESIDENT_BINDING
#define EXPECTED_HOME_LAUNCHES 1
#endif
#ifdef TEST_FOREGROUND_ALARM
static int32_t foreground_checkpoint(void){assert(!retained_flag);return 0;}
#define TEST_FOREGROUND_BINDING .service_checkpoint=foreground_checkpoint,
#else
#define TEST_FOREGROUND_BINDING
#endif
static const risc_runtime_api_v1 native_runtime={TEST_FOREGROUND_BINDING TEST_RESIDENT_BINDING .api_version=1,.struct_size=sizeof(native_runtime),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=test_acquire,.release=test_release,.retain_invocation=test_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&native_runtime:NULL;}
#include NOVA_APP_SOURCE

/* Real adapter starts without a surface. Hardware geometry has one 69px inset. */
static bool native_info(void*c,risc_display_info_v1*s){(void)c;*s=(risc_display_info_v1){.width=466,.height=466,.safe_insets={69,69,69,69},.nominal_refresh_millihz=60000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool native_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(!retained_flag&&!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=466,.height=466,.stride_bytes=936,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static bool native_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)o;assert(!retained_flag&&frames&&f==1);for(size_t i=0;i<n;i++)assert(r[i].x>=0&&r[i].y>=0&&r[i].x+(int)r[i].width<=466&&r[i].y+(int)r[i].height<=466);for(unsigned y=0;y<466;y++)assert(pixels[y*468+466]==0xa5a5&&pixels[y*468+467]==0xa5a5);frames=0;*t=++presents;return true;}
static const risc_display_output_api_v1 native_display={.api_version=1,.struct_size=sizeof(native_display),.get_info=native_info,.acquire=native_frame,.release=fake_frame_release,.submit=native_submit,.present_status=fake_present};
static risc_touch_snapshot_v1 native_snapshot={.width=466,.height=466};
static risc_touch_event_v1 native_event;static bool event_pending;static unsigned checks,initial_hour,initial_minute;
static void emit(unsigned kind,int x,int y){assert(!event_pending);native_snapshot.timestamp_ms=ticks;native_event=(risc_touch_event_v1){++native_snapshot.sequence,ticks,kind,1,x+69,y+69};event_pending=true;native_snapshot.contact_count=kind==RISC_TOUCH_EVENT_UP?0:1;native_snapshot.contacts[0]=(risc_touch_contact_v1){.id=1,.x=x+69,.y=y+69};}
static void tap_rect(unsigned at,alarm_native_rect r){if(polls==at)emit(RISC_TOUCH_EVENT_DOWN,r.x+r.width/2,r.y+r.height/2);if(polls==at+1)emit(RISC_TOUCH_EVENT_UP,r.x+r.width/2,r.y+r.height/2);}
static bool native_poll(void*c,size_t n){(void)c;(void)n;assert(!retained_flag);polls++;
 alarm_native_layout root=alarm_native_layout_make(328,328,DAILY_ALARM_KIND,0),list=alarm_native_layout_make(328,328,DAILY_ALARM_KIND,1),edit=alarm_native_layout_make(328,328,DAILY_ALARM_KIND,2);
 if(polls==5){assert(presents&&app->screen_width()==328&&app->screen_height()==328);initial_hour=values[0];initial_minute=values[1];checks++;}
 if(scenario==0){
  tap_rect(20,root.edit);tap_rect(30,list.fields[0]);tap_rect(40,edit.plus);
  if(polls==45){assert(alarm_page==2&&alarm_field==0&&values[0]==(DAILY_ALARM_KIND==1&&!alarm_24h()?(initial_hour/12)*12+(initial_hour%12+1)%12:(initial_hour+1)%(DAILY_ALARM_KIND==1?24:100))&&!puts_count);checks++;}
  tap_rect(50,edit.done);tap_rect(60,list.done);tap_rect(70,root.primary);
  if(polls==80){assert(writer.saved.enabled&&!writer.uncertain&&puts_count==1);checks++;}
  tap_rect(90,root.cancel);tap_rect(110,root.back);
 }else if(scenario==1){
  /* A dragged edit gesture cannot enter the nested editor. */
  if(polls==20)emit(RISC_TOUCH_EVENT_DOWN,root.edit.x+10,root.edit.y+10);
  if(polls==21)emit(RISC_TOUCH_EVENT_MOVE,root.edit.x+90,root.edit.y+10);
  if(polls==22)emit(RISC_TOUCH_EVENT_UP,root.edit.x+90,root.edit.y+10);
  /* Missing ordered release / sequence gap cannot invent a tap. */
  if(polls==30)emit(RISC_TOUCH_EVENT_DOWN,root.primary.x+10,root.primary.y+10);
  if(polls==31){native_snapshot.sequence++;native_snapshot.contact_count=0;}
  if(polls==40){assert(!alarm_page&&!puts_count);checks++;}
  tap_rect(50,root.primary);tap_rect(80,root.cancel);tap_rect(110,root.back);
 }else if(scenario==2){
  tap_rect(20,root.primary);tap_rect(40,root.edit);tap_rect(50,root.back);
  if(polls==70){assert(writer.uncertain&&!alarm_page&&!launches&&puts_count==1);checks++;}
  tap_rect(120,root.primary);tap_rect(140,root.cancel);tap_rect(160,root.back);
 }else if(scenario==3||scenario==5||scenario==6){
  tap_rect(20,root.edit);tap_rect(30,list.fields[1]);tap_rect(40,edit.plus);
  if(scenario==3){tap_rect(50,edit.back);}if(polls==55&&scenario!=6){assert(alarm_page==1&&!puts_count);checks++;}
  if(scenario==3){tap_rect(60,list.back);}if(polls==65&&scenario!=6){assert(!alarm_page&&!puts_count);checks++;}
  if(scenario==3)tap_rect(80,root.back);
  if(scenario==6&&polls==70){assert(modal_acked&&alarm_page==2&&values[1]==(initial_minute+1)%60&&!puts_count);checks++;}
 }else if(scenario==4){tap_rect(20,root.primary);}
 return true;
}
static int32_t native_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;assert(!retained_flag);if(scenario==1&&polls==31)return -1;if(!event_pending)return 0;*e=native_event;event_pending=false;return 1;}
static bool native_snap(void*c,risc_touch_snapshot_v1*s){(void)c;assert(!retained_flag);*s=native_snapshot;return true;}
static const risc_touch_api_v1 native_touch={1,sizeof(native_touch),NULL,fake_sub,fake_unsub,native_poll,native_next,native_snap};
static bool native_nav_read(void*c,risc_input_navigation_frame_v1*s){fake_nav(c,s);if((scenario==5&&(polls==50||polls==60||polls==80))||(scenario==6&&(polls==60||polls==80||polls==90||polls==100))){s->buttons=RISC_NAV_BACK;s->pressed=RISC_NAV_BACK;}return true;}
static const risc_input_navigation_api_v1 native_nav={1,sizeof(native_nav),NULL,native_nav_read,fake_foreground,fake_reset};
const risc_input_navigation_api_v1*portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &native_nav;}
int main(int argc,char**argv){assert(argc==3);directory=argv[1];scenario=atoi(argv[2]);paper_profile=false;stop_poll=220;memset(pixels,0xa5,sizeof(pixels));
 if(scenario==2){kv_io=true;raw_home_at=60;home_at=60;}
 if(scenario==4)kv_unknown=true;
 assert(!frames&&!presents&&!app_module_init());app_main();
 if(scenario==4){assert(retained_flag&&retain_count==1&&!launches);unsigned held=fixture_grants;app_module_fini();assert(held==fixture_grants&&polls==frozen_polls&&ticks==frozen_ticks&&presents==frozen_presents&&frames==frozen_frames&&subs==frozen_subs&&fixture_grants==frozen_grants&&native_reads==frozen_reads&&service_calls==frozen_calls&&pixel_hash()==frozen_pixels);}
 else{assert(!retained_flag&&checks>=1&&launches==1&&presents);if(scenario<=1)assert(puts_count==2&&!writer.saved.enabled);if(scenario==2)assert(puts_count==3&&!writer.uncertain&&!writer.saved.enabled);if(scenario>=3)assert(!puts_count);app_module_fini();assert(!fixture_grants&&!frames&&!subs&&!reader_live);}
 printf("PASS actual native adapter kind=%u scenario=%u checks=%u writes=%u frames=%u; modeled466 inset69, no device claim\n",DAILY_ALARM_KIND,scenario,checks,puts_count,presents);return 0;}
