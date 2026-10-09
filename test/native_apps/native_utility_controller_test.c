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
static void zone(const char *id);
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
 if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(v==1&&!id&&!reader_live);if(deny_reader)return false;reader_live=true;fixture_grants++;g->api=&reader_api;}
 else {if(!strcmp(n,ALARM_SERVICE_CAPABILITY))assert(v==2);if(!legacy_fixture_acquire(n,!strcmp(n,ALARM_SERVICE_CAPABILITY)?1:v,id,g))return false;if(g->api==&kv_api)g->api=&test_kv;if(g->api==&alarm_api)g->api=&test_alarm;}
 g->slot=(!strcmp(n,"storage.key-value")&&id==2)?2:1;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){assert(!retained_flag);if(g->api==&reader_api){native_release_count++;if(release_reader_failure)return false;assert(reader_live);reader_live=false;fixture_grants--;}
 else if(fail_store_release&&g->api==&test_kv&&g->slot==2)return false;
 else if(g->api==&battery_api){battery_releases++;fixture_grants--;}
 else if(!legacy_fixture_release(g))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static const risc_runtime_api_v1 native_runtime={.api_version=1,.struct_size=sizeof(native_runtime),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=test_acquire,.release=test_release,.retain_invocation=test_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&native_runtime:NULL;}
#include NOVA_APP_SOURCE
static void touch(unsigned at,int px,int py,int wx,int wy){assert(action_count<256);actions[action_count].at=at;actions[action_count].x=paper_profile?px:wx;actions[action_count++].y=paper_profile?py:wy;}
static void zone(const char *id){uint8_t data[44]={0};data[0]='T';data[1]='Z';data[2]=1;strcpy((char*)data+4,id);data[3]=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)data[3]^=data[i];(void)fake_put(NULL,"time_zone",data,44);puts_count=0;}
#if NOVA_APP_ID == 2
static void toggle_at(unsigned at){touch(at,90,628,48,204);}
static void reset_at(unsigned at){touch(at,240,628,120,204);}
static void seed_record(bool raw,bool future){
 sw_record record={.elapsed_ms=5000,.anchor_seconds=(uint32_t)(native_epoch-INT64_C(946684800))+(future?60u:0u)-10u,.running=true};uint8_t bytes[20];sw_encode(&record,bytes);
 if(raw){bytes[2]='0';sw_write32(bytes+16,sw_checksum(bytes));}
 (void)fake_put(NULL,raw?"stopwatch":"stopwatch_utc",bytes,sizeof(bytes));puts_count=0;
}
#endif
int main(int argc,char **argv){
 assert(argc==3);unsigned scenario=(unsigned)atoi(argv[1]);paper_profile=atoi(argv[2])!=0;directory=NULL;memset(pixels,0xa5,sizeof(pixels));stop_poll=260;home_at=220;zone("America/Denver");
 bool expected_retained=false;
#if NOVA_APP_ID == 2
 switch(scenario){
 case 0:toggle_at(20);toggle_at(100);break;
 case 1:toggle_at(20);toggle_at(60);toggle_at(100);toggle_at(140);break;
 case 2:toggle_at(20);toggle_at(100);reset_at(120);reset_at(140);break;
 case 3:kv_io=true;toggle_at(20);raw_home_at=60;toggle_at(120);toggle_at(160);break;
 case 4:kv_io=kv_commits=true;toggle_at(20);toggle_at(100);break;
 case 5:deny_reader=true;toggle_at(20);break;
 case 6:native_validity=RISC_REALTIME_UNSET;toggle_at(20);break;
 case 7:native_reserved=1;toggle_at(20);break;
 case 8:seed_record(false,false);toggle_at(100);break;
 case 9:seed_record(false,true);break;
 case 10:seed_record(true,false);break;
 case 11:changed_at=50;clock_shift=-120;toggle_at(20);toggle_at(180);break;
 case 12:changed_at=50;clock_shift=120;toggle_at(20);toggle_at(180);break;
 case 13:changed_at=50;lose_time=true;toggle_at(20);toggle_at(180);break;
 case 14:kv_context=true;expected_retained=true;break;
 case 15:native_read_error=RISC_REALTIME_CONTEXT;expected_retained=true;break;
 case 16:release_reader_failure=true;expected_retained=true;break;
 case 17:fail_store_release=true;break;
 case 18:zone("Asia/Kathmandu");toggle_at(20);toggle_at(100);break;
 case 19:cancel_contact=true;toggle_at(20);break;
 case 24:fail_pause=true;toggle_at(20);toggle_at(100);raw_home_at=120;toggle_at(160);break;
 case 21:seed_record(false,false);for(unsigned i=0;i<32;i++)if(!strcmp(cells[i].key,"stopwatch_utc")){cells[i].bytes[2]='0';sw_write32(cells[i].bytes+16,sw_checksum(cells[i].bytes));}break;
 case 22:native_epoch=INT64_C(2147483648);toggle_at(20);break;
 case 23:zone_change=true;toggle_at(20);toggle_at(100);break;
 case 20:toggle_at(20);touch(40,200,20,0,0);touch(41,200,100,0,0);touch(120,240,620,0,0);touch(121,240,560,0,0);break;
 default:assert(0);
 }
#else
 switch(scenario){
 case 0:
#if NOVA_APP_ID == 1
 touch(20,80,281,40,74);touch(40,186,281,93,74);
#else
 touch(20,200,628,50,214);touch(40,340,628,200,214);
#endif
 break;
 case 1:touch(20,200,20,0,0);touch(21,200,100,0,0);touch(120,240,620,0,0);touch(121,240,560,0,0);break;
 case 2:deny_reader=true;break;
 case 3:native_validity=RISC_REALTIME_UNSET;break;
 case 4:native_reserved=1;break;
 case 5:native_read_error=RISC_REALTIME_CONTEXT;expected_retained=true;break;
 case 6:release_reader_failure=true;expected_retained=true;break;
 case 7:service_retained=true;expected_retained=true;break;
 default:assert(0);
 }
#endif
 assert(app_module_init()==0);
#if NOVA_APP_ID != 2
 if(scenario>=2&&scenario<=6){twatch_rtc_time_v1 result={0};assert(!portable_app_native_local_time(&result));}
#endif
 app_main();
#if NOVA_APP_ID == 2
 if(scenario==17)expected_retained=true;
#endif
 if(expected_retained){assert(retained_flag&&retain_count==1&&portable_adapter_retained());unsigned before=fixture_grants;app_module_fini();assert(fixture_grants==before&&polls==frozen_polls&&ticks==frozen_ticks&&presents==frozen_presents&&frames==frozen_frames&&subs==frozen_subs&&native_reads==frozen_reads&&service_calls==frozen_calls&&pixel_hash()==frozen_pixels);}
 else{
  assert(!retained_flag&&!reader_live&&launches==1);
#if NOVA_APP_ID == 2
  switch(scenario){
  case 0:case 18:case 23:assert(!clock_state.running&&clock_state.elapsed_ms>1000&&clock_state.saved.elapsed_ms==clock_state.elapsed_ms&&puts_count==2);break;
  case 1:assert(!clock_state.running&&puts_count==4&&clock_state.elapsed_ms==clock_state.saved.elapsed_ms);break;
  case 2:assert(!clock_state.running&&!clock_state.elapsed_ms&&puts_count==3);break;
  case 24:assert(!save_uncertain&&!pending_pause&&!clock_state.running&&puts_count==3&&clock_state.saved.elapsed_ms==clock_state.elapsed_ms);break;
  case 3:assert(!save_uncertain&&!clock_state.running&&puts_count==3&&clock_state.elapsed_ms>1000);break;
  case 4:assert(!save_uncertain&&!clock_state.running&&puts_count==2);break;
  case 21:assert(!loaded&&!puts_count);break;
  case 5:case 6:case 7:case 10:case 19:case 22:assert(!clock_state.running&&!puts_count&&!clock_state.elapsed_ms);break;
  case 8:assert(!clock_state.running&&clock_state.approximate&&clock_state.elapsed_ms>=15000&&puts_count==1);break;
  case 9:assert(!clock_state.running&&clock_state.clock_changed&&!puts_count);break;
  case 11:case 12:case 13:assert(!clock_state.running&&!clock_state.clock_changed&&!pending_pause&&puts_count==2);break;
  case 20:assert(clock_state.running&&puts_count==1);break;
  default:assert(0);
  }
  if(scenario!=10){for(unsigned i=0;i<32;i++)assert(strcmp(cells[i].key,"stopwatch"));}
#elif NOVA_APP_ID == 1
  if(scenario==0)assert(!strcmp(calculator_state.text,"78"));
#else
  if(scenario==0)assert(power_details&&power_detail_page==1);
#endif
  app_module_fini();assert(!fixture_grants&&!frames&&!subs);
 }
 printf("Native controller app=%u paper=%u case=%u reads=%u retained=%u passed\n",NOVA_APP_ID,paper_profile,scenario,native_reads,retain_count);return 0;
}
