#include "AlarmServiceV1.h"
typedef struct {alarm_service_v1 service;uint32_t output_modes;} alarm_service_outputs_v1;
#include "PortableNativeTimeToolbar.h"
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
static bool fail_store_release,zone_change,fail_pause;
#include "TelemetryBroadcastV1.h"
#include "RiscBluetoothTelemetryV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscProviderV2.h"
static const telemetry_broadcast_v1 *broadcast;
static unsigned broadcast_acquires,broadcast_releases,high_water,publishes,closes;
static bool live,close_fails,deny_broadcast,deny_store,release_broadcast_fails;
static void zone(const char *id){(void)id;}
static unsigned changed_at;static int64_t clock_shift;static bool lose_time;
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
static int32_t test_read(void*c,risc_realtime_snapshot_v1*s){assert(c&&!retained_flag&&reader_live);native_reads++;if(zone_change&&polls>=50){unsigned before=puts_count;zone("Asia/Kathmandu");puts_count=before;zone_change=false;}if(native_read_error)return native_read_error;if(changed_at&&polls>=changed_at&&lose_time)return RISC_REALTIME_IO;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=native_validity,.epoch_seconds=native_validity==RISC_REALTIME_VALID?native_epoch+ticks/1000+((changed_at&&polls>=changed_at)?clock_shift:0):0,.monotonic_before_us=(uint64_t)ticks*1000,.monotonic_after_us=(uint64_t)ticks*1000,.reserved=native_reserved};return RISC_REALTIME_OK;}
static const risc_realtime_api_v1 reader_api={1,sizeof(reader_api),&reader_live,test_read};
static int32_t test_get(void*c,const char*k,void*b,uint32_t n,uint32_t*s){assert(!retained_flag);if(!strcmp(k,"stopwatch_utc")){if(kv_context)return RISC_KEY_VALUE_CONTEXT;if(kv_unknown)return -88;}return fake_get(c,k,b,n,s);}
static int32_t test_put(void*c,const char*k,const void*b,uint32_t n){assert(!retained_flag);if(kv_context)return RISC_KEY_VALUE_CONTEXT;if(kv_unknown)return -88;if(fail_pause&&polls>=80&&polls<140){puts_count++;return RISC_KEY_VALUE_IO;}if(kv_io&&(!kv_commits?polls<100:true)){if(kv_commits)(void)fake_put(c,k,b,n);else puts_count++;return RISC_KEY_VALUE_IO;}return fake_put(c,k,b,n);}
static const risc_key_value_v1 test_kv={1,sizeof(test_kv),NULL,test_get,test_put};
static int32_t test_status(void*c,alarm_status_v1*s){assert(!retained_flag);service_calls++;if(service_retained){*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_BLOCKED,.error=-9,.output_uncertain=1};return ALARM_OK;}return fake_alarm_status(c,s);}
static int32_t test_step(void*c){assert(!retained_flag);service_calls++;return service_retained?-9:fake_alarm_step(c);}
static alarm_service_descriptor_v2 test_alarm={.base={2,sizeof(test_alarm),NULL,test_status,test_step,test_step,fake_alarm_ack,fake_alarm_prepare,test_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!retained_flag);if(!strcmp(n,"rtc.clock")){assert(!"Native app/toolbar must never acquire RTC");return false;}if(!strcmp(n,"runtime.realtime.control")){assert(!"No native control authority");return false;}
 if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(v==1&&!id);if(deny_broadcast)return false;g->api=broadcast;fixture_grants++;broadcast_acquires++;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&deny_store)return false;
 else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(v==1&&!id&&!reader_live);if(deny_reader)return false;reader_live=true;fixture_grants++;g->api=&reader_api;}
 else {if(!strcmp(n,ALARM_SERVICE_CAPABILITY))assert(v==2);if(!legacy_fixture_acquire(n,!strcmp(n,ALARM_SERVICE_CAPABILITY)?1:v,id,g))return false;if(g->api==&kv_api)g->api=&test_kv;if(g->api==&alarm_api)g->api=&test_alarm;}
 if(fixture_grants>high_water)high_water=fixture_grants;
 assert(high_water<=16);
 g->slot=(!strcmp(n,"storage.key-value")&&id==2)?2:1;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){assert(!retained_flag);if(g->api==broadcast){if(release_broadcast_fails)return false;broadcast_releases++;fixture_grants--;}
 else if(g->api==&reader_api){native_release_count++;if(release_reader_failure)return false;assert(reader_live);reader_live=false;fixture_grants--;}
 else if(fail_store_release&&g->api==&test_kv&&g->slot==2)return false;
 else if(g->api==&battery_api){battery_releases++;fixture_grants--;}
 else if(!legacy_fixture_release(g))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static const risc_runtime_api_v1 native_runtime={.api_version=1,.struct_size=sizeof(native_runtime),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=test_acquire,.release=test_release,.retain_invocation=test_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&native_runtime:NULL;}

static uint64_t clock_ms(void*c){(void)c;return ticks;}
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;assert(!retained_flag);if(i>=3)return 0;static const risc_telemetry_field_v1 fields[]={{1,RISC_TELEMETRY_BATTERY_PERCENT,"Battery"},{2,RISC_TELEMETRY_VOLTAGE_MV,"Battery voltage"},{3,RISC_TELEMETRY_CHARGING,"Charging"}};*out=fields[i];return 1;}
static int32_t read_field(void*c,uint32_t id,int32_t*out){(void)c;assert(!retained_flag&&id>=1&&id<=3);*out=id==1?73:id==2?3970:1;return 1;}
static bool publish(void*c,const uint32_t*ids,uint32_t count,bool consent,uint64_t*out){(void)c;assert(!retained_flag&&consent&&!live&&count==3&&ids[0]==1&&ids[1]==2&&ids[2]==3);publishes++;*out=1;live=true;return true;}
static bool radio_poll(void*c,uint64_t token,uint32_t limit){(void)c;assert(!retained_flag&&live&&token==1&&limit==4);return true;}
static bool radio_status(void*c,risc_ble_telemetry_status_v1*out){(void)c;assert(!retained_flag);*out=(risc_ble_telemetry_status_v1){.struct_size=sizeof(*out),.state=live?RISC_BLE_TELEMETRY_PUBLISHING:RISC_BLE_TELEMETRY_OFF,.updates=publishes};return true;}
static bool close_radio(void*c,uint64_t token){(void)c;assert(!retained_flag&&token==1);closes++;if(close_fails)return false;live=false;return true;}
static const risc_bluetooth_telemetry_v1 radio_api={1,sizeof(radio_api),NULL,enumerate,read_field,publish,radio_poll,radio_status,close_radio};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,clock_ms,NULL};
extern const risc_driver_v2 *t5_driver_get(uint32_t);
#include NOVA_APP_SOURCE
static void click(unsigned at,int x,int y){actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
static void record(const char *key,uint8_t marker,uint8_t value){uint8_t b[]={marker,1,value,value^0xa5u};assert(fake_put(NULL,key,b,4)==0);}
int main(int argc,char**argv){
 assert(argc==3);unsigned scenario=(unsigned)atoi(argv[1]);directory=argv[2];paper_profile=true;memset(pixels,0xa5,sizeof(pixels));stop_poll=300;home_at=250;
 const risc_driver_v2 *driver=t5_driver_get(2);risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock_api},{"bluetooth.telemetry",1,&radio_api}};assert(driver->start(deps,2));broadcast=driver->capability;
 bool retained=false;
 if(scenario!=1)record("quick_radio",0x51,scenario==2?4:3);
 if(scenario>=3)record("ble_broadcast",0x62,1);
 puts_count=0;
 switch(scenario){
 case 0:click(20,330,620);click(50,240,620);click(100,240,620);break;
 case 1:case 2:click(20,330,620);click(50,240,620);break;
 case 3:click(20,330,620);break;
 case 4:close_fails=true;retained=true;break;
 case 5:deny_broadcast=true;retained=true;break;
 case 6:deny_store=true;retained=true;break;
 case 7:release_broadcast_fails=true;retained=true;break;
 case 8:record("ble_broadcast",0x62,2);puts_count=0;click(20,330,620);break;
 case 9:record("ble_broadcast",0x62,0);puts_count=0;click(20,330,620);break;
 case 10:fail_put_once=true;click(20,330,620);click(50,240,620);click(100,240,620);break;
 default:assert(0);
 }
 int init=app_module_init();if(!init)app_main();
 if(retained){if(scenario==4)app_module_fini();assert(retained_flag&&retain_count==1);unsigned old=fixture_grants;app_module_fini();assert(fixture_grants==old);}
 else {
  assert(!retained_flag&&launches==1);app_module_fini();assert(!fixture_grants&&!frames&&!subs&&!live&&broadcast_acquires==broadcast_releases);
  if(scenario==0){assert(puts_count==2&&publishes&&closes);bool enabled=true;assert(portable_broadcast_control_load(&kv_api,&enabled)&&!enabled);}
  if(scenario==1||scenario==2){assert(!publishes&&puts_count==1);bool enabled=false;assert(portable_broadcast_control_load(&kv_api,&enabled)&&enabled);}
  if(scenario==3)assert(publishes);
  if(scenario==8||scenario==9)assert(!publishes&&!puts_count);
  if(scenario==10)assert(puts_count==2);
  assert(driver->quiesce());driver->stop();
 }
 printf("Battery paper broadcast case=%u peak-live=%u publish=%u close=%u retained=%u PASS\n",scenario,high_water,publishes,closes,retained_flag);
}
