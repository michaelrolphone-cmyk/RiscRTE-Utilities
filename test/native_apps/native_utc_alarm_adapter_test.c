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
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!retained_flag);if(!strcmp(n,"rtc.clock")){assert(!"Native app/toolbar must never acquire RTC");return false;}if(!strcmp(n,"runtime.realtime.control")){assert(!"No native control authority");return false;}
 if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(v==1&&!id&&!reader_live);if(deny_reader)return false;reader_live=true;fixture_grants++;g->api=&reader_api;}
 else {if(!strcmp(n,ALARM_SERVICE_CAPABILITY))assert(v==2);if(!legacy_fixture_acquire(n,!strcmp(n,ALARM_SERVICE_CAPABILITY)?1:v,id,g))return false;if(g->api==&kv_api)g->api=&test_kv;if(g->api==&alarm_api)g->api=&test_alarm;}
 g->slot=1;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){assert(!retained_flag);if(g->api==&reader_api){native_release_count++;if(release_reader_failure)return false;assert(reader_live);reader_live=false;fixture_grants--;}
 else if(g->api==&battery_api){battery_releases++;fixture_grants--;}
 else if(!legacy_fixture_release(g))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static const risc_runtime_api_v1 native_runtime={.api_version=1,.struct_size=sizeof(native_runtime),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=fake_launch,.acquire=test_acquire,.release=test_release,.retain_invocation=test_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&native_runtime:NULL;}
#include NOVA_APP_SOURCE
static void touch(unsigned at,int px,int py,int wx,int wy){assert(action_count<256);actions[action_count].at=at;actions[action_count].x=paper_profile?px:wx;actions[action_count++].y=paper_profile?py:wy;}
static void start(unsigned at){touch(at,110,628,50,206);}static void cancel(unsigned at){touch(at,340,628,174,206);}
static void zone(const char *id){uint8_t data[44]={0};data[0]='T';data[1]='Z';data[2]=1;strcpy((char*)data+4,id);data[3]=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)data[3]^=data[i];(void)fake_put(NULL,"time_zone",data,44);puts_count=0;}
int main(int argc,char **argv){assert(argc==4);directory=argv[1];unsigned scenario=(unsigned)atoi(argv[2]);paper_profile=atoi(argv[3])!=0;memset(pixels,0xa5,sizeof(pixels));stop_poll=220;home_at=180;
 switch(scenario){
 case 0:start(20);cancel(60);break;
 case 1:cancel_contact=true;start(20);break;
 case 2:start(20);start(21);start(22);home_at=80;break;
 case 3:kv_io=true;start(20);cancel(40);raw_home_at=60;start(80);start(120);home_at=160;break;
 case 4:kv_io=kv_commits=true;start(20);home_at=60;break;
 case 5:kv_context=true;break;
 case 6:kv_unknown=true;break;
 case 7:native_read_error=RISC_REALTIME_CONTEXT;start(20);break;
 case 8:native_read_error=-99;start(20);break;
 case 9:release_reader_failure=true;start(20);break;
 case 10:native_validity=RISC_REALTIME_UNSET;start(20);home_at=60;break;
 case 11:native_reserved=1;start(20);home_at=60;break;
 case 12:deny_reader=true;start(20);home_at=60;break;
 case 13:service_retained=true;start(20);break;
 case 14:zone("Asia/Kathmandu");start(20);home_at=60;break;
 case 15:touch(20,200,20,0,0);touch(21,200,100,0,0);touch(40,130,345,0,0);touch(60,140,450,0,0);touch(80,340,553,0,0);touch(120,240,620,0,0);touch(121,240,560,0,0);home_at=160;break;
 case 16:native_epoch=INT64_C(2147483647)-30;start(20);home_at=60;break;
 default:assert(0);}
 assert(app_module_init()==0);app_main();
 bool expect_retained=(scenario>=5&&scenario<=9)||scenario==13;
 if(expect_retained){assert(retained_flag&&retain_count==1&&portable_adapter_retained()&&!launches);unsigned before=fixture_grants;app_module_fini();assert(fixture_grants==before&&polls==frozen_polls&&ticks==frozen_ticks&&presents==frozen_presents&&frames==frozen_frames&&subs==frozen_subs&&fixture_grants==frozen_grants&&native_reads==frozen_reads&&service_calls==frozen_calls&&pixel_hash()==frozen_pixels);}
 else {assert(!retained_flag&&!reader_live);if(scenario==0)assert(puts_count==2&&!writer.saved.enabled);if(scenario==1)assert(!puts_count);if(scenario==2)assert(puts_count==1&&launches==1);if(scenario==3)assert(puts_count==3&&!writer.uncertain&&launches==1);if(scenario==4)assert(puts_count==1&&!writer.uncertain&&launches==1);if((scenario>=10&&scenario<=12)||scenario==16)assert(!puts_count&&launches==1);if(scenario==14){assert(puts_count==1&&launches==1);}app_module_fini();assert(!fixture_grants&&!frames&&!subs);}
 printf("Native alarm adapter kind=%u paper=%u scenario=%u: reads=%u releases=%u retained=%u\n",DAILY_ALARM_KIND,paper_profile,scenario,native_reads,native_release_count,retain_count);return 0;}
