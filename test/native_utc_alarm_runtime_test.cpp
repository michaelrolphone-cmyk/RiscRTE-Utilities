#include "AlarmServiceV2.h"
/* Actual Runtime/Graph/Module and native-UTC visual provider ELF. Only native
 * UTC, monotonic and durable storage boundaries are modeled; no fake RTC. */
#define ALARM_NATIVE_UTC
#include "bootstrap/Runtime.h"
#include "RiscPlatformClockV1.h"
#include "AlarmRecords.h"
#include "PointsRecords.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
using RiscBoot::Runtime;
static std::string root;static uint64_t ms;static unsigned get_count,puts_count,reads,lifecycles;
static bool safe=true;static int scenario;
static std::map<std::pair<uint32_t,std::string>,std::string> data;
static bool owner(){return true;}static bool safety(){return safe;}
static bool health(risc_runtime_health_v1*){return true;}static bool log_line(const char*s){puts(s);return true;}
static void delay(uint32_t n){ms+=n;}static uint64_t monotonic(void*){return ms;}
static const risc_platform_clock_api_v1 clk{1,sizeof(clk),nullptr,monotonic,nullptr};
static int32_t native_read(void*,risc_realtime_snapshot_v1*out){reads++;*out={sizeof(*out),scenario==1?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,scenario==1?0:INT64_C(1791201600)+(int64_t)(ms/1000),0,0,ms*1000,ms*1000};return 0;}
static int32_t forbidden_seed(void*,int64_t,uint32_t){assert(false);return -1;}
static const risc_realtime_control_api_v1 native{1,sizeof(native),nullptr,native_read,forbidden_seed};
static bool bind(Runtime&r){return r.registerRealtime(&native)&&r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,&clk);}
static int32_t get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){assert(safe);get_count++;assert(ns==1||ns==3||ns==4||ns==5);auto found=data.find({ns,k});*n=0;if(found==data.end())return -1;assert(cap>=found->second.size());memcpy(b,found->second.data(),found->second.size());*n=found->second.size();return 0;}
static int32_t put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){assert(safe&&ns==4&&(!strcmp(k,"alarm_utc_occ")||!strcmp(k,"timer_utc_occ")||!strcmp(k,"points_utc_occ")));puts_count++;data[{ns,k}]=std::string((const char*)b,n);return 0;}
static const RiscBoot::KeyValueBackend kv{nullptr,get,put};
static void file(const char*name,const std::string&body){std::ofstream(root+"/"+name)<<body;}
static alarm_status_v1 status(const alarm_service_v1*s){unsigned calls=get_count+puts_count+reads;alarm_status_v1 v{1,sizeof(v)};assert(s->status(s->context,&v)==ALARM_OK&&calls==get_count+puts_count+reads);return v;}
extern "C" void test_native_alarm_lifecycle(){lifecycles++;}
extern "C" void test_native_alarm_app(){const auto*api=risc_runtime_get_api(1);assert(api);risc_runtime_capability_v1 grant{sizeof(grant)},denied{sizeof(denied)};assert(api->acquire("alarm.service",2,0,&grant));auto*s=(const alarm_service_v1*)grant.api;
 assert(alarm_service_descriptor(s)->output_modes==0);assert(!api->acquire("alarm.service",1,0,&denied));assert(!api->acquire("platform.realtime",1,0,&denied));assert(!api->acquire("runtime.realtime-control",1,0,&denied));assert(!api->acquire("rtc.clock",2,0,&denied));
 if(scenario==2){safe=false;int result=0;for(unsigned i=0;i<20&&result!=ALARM_RETAINED;i++)result=s->step(s->context);assert(result==ALARM_RETAINED);const auto before=get_count+puts_count+reads;assert(api->retain_invocation());assert(before==get_count+puts_count+reads);return;}
 for(unsigned i=0;i<50;i++){s->step(s->context);if(status(s).state==(scenario==1?ALARM_STATE_BLOCKED:ALARM_STATE_READY))break;}
 auto view=status(s);
 if(scenario==1){assert(view.state==ALARM_STATE_BLOCKED&&view.error==ALARM_RTC&&!puts_count);assert(api->release(&grant));return;}
 assert(view.state==ALARM_STATE_READY&&view.schedules[1].state==ALARM_SCHEDULE_ARMED);
 alarm_sleep_v1 plan{sizeof(plan)};int result;for(unsigned i=0;i<100;i++){result=s->prepare_sleep(s->context,&plan);if(result==0)break;assert(result==ALARM_PENDING);s->step(s->context);}assert(result==0&&plan.deadline==844516810);
 // A yield/poll without an explicit foreground phase must not reconcile.
 auto calls=get_count+puts_count+reads;api->yield_ms(1);assert(calls==get_count+puts_count+reads);
 ms=10000;for(unsigned i=0;i<60&&status(s).state!=ALARM_STATE_ALERT;i++)s->step(s->context);
 view=status(s);assert(view.state==ALARM_STATE_ALERT&&view.occurrence.kind==ALARM_KIND_COUNTDOWN);auto token=view.occurrence;
 assert(s->acknowledge(s->context,&token)==ALARM_PENDING);for(unsigned i=0;i<60&&status(s).state!=ALARM_STATE_READY;i++)s->step(s->context);
 view=status(s);assert(view.state==ALARM_STATE_READY&&view.schedules[1].state==ALARM_SCHEDULE_DISMISSED);assert(s->acknowledge(s->context,&token)==ALARM_OK);assert(api->release(&grant));}
static void stage(){
 file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 file("app.json",R"({"type":"application","id":"native-alarm-test","version":"1.0.0","architecture":"xtensa-esp32s3","file_name":"app.elf","entry":"app_main","requires":[{"capability":"alarm.service","api":2}]})");
 file("boot.json",R"({"board":"board.json","default_app":"app.elf","drivers":[{"manifest":"service.json","key_value":[{"key":"alarm_utc_cfg","namespace":3,"access":"read"},{"key":"timer_utc_cfg","namespace":3,"access":"read"},{"key":"alarm_utc_occ","namespace":4,"access":"read-write"},{"key":"timer_utc_occ","namespace":4,"access":"read-write"},{"key":"alert_mode","namespace":1,"access":"read"},{"key":"points_utc_cfg","namespace":5,"access":"read"},{"key":"points_utc_occ","namespace":4,"access":"read-write"},{"key":"alert_dnd","namespace":1,"access":"read"},{"key":"time_zone","namespace":1,"access":"read"}]}],"app_capabilities":[{"manifest":"app.json","grants":[{"capability":"alarm.service","api":2,"instance_id":0}]}]})");
 uint8_t bytes[64];alarm_config c{1,844516810,844516800,10,2,1};alarm_config_encode(&c,bytes);data[{3,"timer_utc_cfg"}]=std::string((char*)bytes,32);points_config points{1};points_config_encode(&points,bytes);data[{5,"points_utc_cfg"}]=std::string((char*)bytes,64);
 // Old records deliberately coexist and are never accessed or changed.
 data[{3,"timer_cfg"}]="OLD WALL TIME";data[{4,"points_occ"}]="OLD LEDGER";
}
int main(int argc,char**argv){assert(argc==2);root=argv[1];stage();for(scenario=0;scenario<3;scenario++){auto child=fork();assert(child>=0);if(!child){Runtime runtime({owner,health,delay,log_line,bind,&kv,safety,safety});assert(runtime.prepare(root.c_str())&&!get_count&&!reads&&!puts_count);bool result=runtime.run();assert(result==(scenario!=2));assert(runtime.retained()==(scenario==2));assert(lifecycles==(scenario==2?1u:2u));assert(data.at({3,"timer_cfg"})=="OLD WALL TIME"&&data.at({4,"points_occ"})=="OLD LEDGER");_exit(0);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}puts("Actual native UTC visual ELF: nine-key admission, safe polling, UTC sleep/ACK, cold UNSET and retained invocation passed");}
