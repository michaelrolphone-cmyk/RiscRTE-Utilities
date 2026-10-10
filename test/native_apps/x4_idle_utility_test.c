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
#include "paper_utility_peripherals.h"
#pragma GCC diagnostic pop
#undef fake_acquire
#undef fake_release
#undef risc_runtime_get_api
#include "RiscRealtimeV1.h"
#include "AlarmServiceV2.h"
#include "PortableNativeCustody.h"
static bool reader_live,retained_flag,deny_reader,release_reader_failure;
static unsigned idle_scenario,high_water;
static bool aged,deny_private_after_init;
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
static int32_t test_status(void*c,alarm_status_v1*s){assert(!retained_flag);service_calls++;if(service_retained){*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_BLOCKED,.error=-9,.output_uncertain=1};return ALARM_OK;}return fake_alarm_status(c,s);}
static int32_t test_step(void*c){assert(!retained_flag);service_calls++;return service_retained?-9:fake_alarm_step(c);}
static alarm_service_descriptor_v2 test_alarm={.base={2,sizeof(test_alarm),NULL,test_status,test_step,test_step,fake_alarm_ack,fake_alarm_prepare,test_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
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
#include "x4_idle_utility_providers.h"
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!retained_flag);if(deny_private_after_init&&!strcmp(n,"storage.key-value")&&id==3)return false;
 if(idle_scenario==2&&!strcmp(n,"x4.power"))return false;
 if(!strcmp(n,"rtc.clock")){assert(!"Native app/toolbar must never acquire RTC");return false;}if(!strcmp(n,"runtime.realtime.control")){assert(!"No native control authority");return false;}

#ifdef PORTABLE_BLE_BROADCAST
 if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(v==1&&!id);fixture_grants++;g->api=&test_broadcast;}
 else
#endif
 if(idle_acquire(n,v,id,g)){fixture_grants++;}
 else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(v==1&&!id&&!reader_live);if(deny_reader)return false;reader_live=true;fixture_grants++;g->api=&reader_api;}
 else {if(!strcmp(n,ALARM_SERVICE_CAPABILITY))assert(v==2);if(!legacy_fixture_acquire(n,!strcmp(n,ALARM_SERVICE_CAPABILITY)?1:v,id,g))return false;if(g->api==&kv_api)g->api=&test_kv;if(g->api==&alarm_api)g->api=&test_alarm;}
 if(g->api==&display_api)g->api=&idle_display;
 if(g->api==&touch_api)g->api=&idle_touch;
 if(g->api==&battery_api)g->api=&idle_gauge;
 if(fixture_grants>high_water)high_water=fixture_grants;
 assert(high_water<=16);
 g->slot=1;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){assert(!retained_flag);if(g->api==&reader_api){native_release_count++;if(release_reader_failure)return false;assert(reader_live);reader_live=false;fixture_grants--;}

#ifdef PORTABLE_BLE_BROADCAST
 else if(g->api==&test_broadcast){if(bt_release_fail)return false;fixture_grants--;}
#endif
 else if(g->api==&idle_gauge){battery_releases++;fixture_grants--;}
 else if(!legacy_fixture_release(g))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static const risc_runtime_api_v1 native_runtime={.api_version=1,.struct_size=sizeof(native_runtime),.health=fake_health,.yield_ms=idle_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=test_acquire,.release=test_release,.retain_invocation=test_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&native_runtime:NULL;}
#include NOVA_APP_SOURCE
int main(int argc,char **argv) {
 assert(argc==2);idle_scenario=(unsigned)atoi(argv[1]);
 paper_profile=true;stop_poll=250;home_at=200;memset(pixels,0xa5,sizeof(pixels));
 idle_setup();
 const uint8_t intent[]={0x51,1,3,3^0xa5};assert(fake_put(NULL,"quick_radio",intent,4)==0);
 if(idle_scenario==5){const uint8_t off=0;assert(fake_put(NULL,"brightness",&off,1)==0);}
 assert(app_module_init()==0);
 deny_private_after_init=idle_scenario==4;
 app_main();
 bool expect_retained=idle_scenario==2||idle_scenario==3||idle_scenario==4;
 if(expect_retained){assert(retained_flag&&retain_count==1&&!launches);unsigned held=fixture_grants;app_module_fini();assert(fixture_grants==held&&polls==frozen_polls&&ticks==frozen_ticks&&presents==frozen_presents&&service_calls==frozen_calls);assert(!light_entries);}
 else {
  assert(!retained_flag&&aged&&light_entries==1&&alarm_resumes==1&&launches==1);
  assert(storage_prepares==1&&storage_commits==1&&storage_resumes==1&&touch_prepares==1&&touch_resumes==1&&panel_prepares==1&&panel_resumes==1);
  if(idle_scenario==1||idle_scenario==5){assert(!bluetooth_on);uint8_t value[16];uint32_t bytes=0;assert(fake_get(NULL,"sleep_idle",value,sizeof(value),&bytes)==0);assert(bytes);}
  else assert(bluetooth_on);
  if(idle_scenario==5)assert(brightness==0);
  app_module_fini();assert(!fixture_grants&&!frames&&!subs);
 }
 printf("Utility idle app=%u scenario=%u peak-live=%u Light=%u retained=%u PASS\n",NOVA_APP_ID,idle_scenario,high_water,light_entries,retained_flag);
 return 0;
}
