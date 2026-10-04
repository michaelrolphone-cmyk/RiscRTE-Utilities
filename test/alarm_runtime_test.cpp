/* Real Runtime + Graph + Module + ordinary dynamic service/app ELFs.
 * Final hardware and NVS are modeled; this is not target or sleep qualification. */
#include "bootstrap/Runtime.h"
#define _Static_assert static_assert
#include "PortableRtcClock.h"
#undef _Static_assert
#include "AlarmRecords.h"
#include "RiscPlatformClockV1.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
static std::string root;static uint64_t ms;static unsigned app_runs,actions[7],writes;
static std::map<std::pair<uint32_t,std::string>,std::string> data;
extern "C" uint64_t alarm_test_ms(){return ms;}
extern "C" unsigned alarm_test_enter(){return app_runs++;}
extern "C" void alarm_test_advance(uint32_t n){ms+=n;}
extern "C" bool alarm_test_time(twatch_rtc_time_v1*out){unsigned t=100+ms/1000;*out={2000,1,1,6,0,(uint8_t)(t/60),(uint8_t)(t%60)};return true;}
extern "C" bool alarm_test_output(unsigned action){assert(action<7);++actions[action];return true;}
static uint64_t monotonic(void*){return ms;}
static const risc_platform_clock_api_v1 clock_api{1,sizeof(clock_api),nullptr,monotonic,nullptr};
static bool owner(){return true;}static bool health(risc_runtime_health_v1*){return true;}static void delay(uint32_t n){ms+=n;}static bool log_line(const char*s){puts(s);return true;}
static int32_t get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){assert(ns==1||ns==3||ns==4);auto it=data.find({ns,k});*n=0;if(it==data.end())return -1;assert(cap>=it->second.size());memcpy(b,it->second.data(),it->second.size());*n=it->second.size();return 0;}
static int32_t put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){assert(ns==3||ns==4);if(ns==4)assert(!strcmp(k,"alarm_occ")||!strcmp(k,"timer_occ"));data[{ns,k}]=std::string((const char*)b,n);++writes;return 0;}
static void file(const std::string&name,const std::string&body){std::ofstream(root+"/"+name)<<body;}
int main(int argc,char**argv){assert(argc==2);root=argv[1];
 file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 const char*ids[]={"test-clock","test-rtc","test-haptic","test-audio"};const char*caps[]={"platform.clock","rtc.clock","haptic.effect","audio.output"};
 std::string drivers="";for(unsigned i=1;i<4;i++){if(i>1)drivers+=",";drivers+="{\"manifest\":\"hw"+std::to_string(i)+".json\"}";file("hw"+std::to_string(i)+".json",std::string(R"({"type":"driver","id":")")+ids[i]+R"(","version":"0.1.0","driver_abi":2,"architecture":"xtensa-esp32s3","file_name":"hw)"+std::to_string(i)+R"(.elf","requires":[],"provides":[{"capability":")"+caps[i]+R"(","api":)"+(i==1?"2":"1")+"}]}");}
 drivers+=R"(,{"manifest":"service.json","key_value":[{"key":"alarm_cfg","namespace":3,"access":"read"},{"key":"timer_cfg","namespace":3,"access":"read"},{"key":"alarm_occ","namespace":4,"access":"read-write"},{"key":"timer_occ","namespace":4,"access":"read-write"},{"key":"alert_mode","namespace":1,"access":"read"}]})";
 std::string policies;
 for(const char*name:{"first","second"}){if(!policies.empty())policies+=",";std::string path=name;file(path+".json",std::string(R"({"type":"application","id":")")+name+R"(","version":"0.1.0","architecture":"xtensa-esp32s3","file_name":")"+name+R"(.elf","entry":"app_main","requires":[{"capability":"alarm.service","api":1},{"capability":"storage.key-value","api":1}]})");policies+=std::string(R"({"manifest":")")+name+R"(.json","grants":[{"capability":"alarm.service","api":1,"instance_id":0},{"capability":"storage.key-value","api":1,"instance_id":3}]})";}
 file("boot.json",std::string(R"({"board":"board.json","default_app":"first.elf","drivers":[)")+drivers+R"(],"app_capabilities":[)"+policies+"]}");
 const RiscBoot::KeyValueBackend backend{nullptr,get,put};RiscBoot::Runtime runtime({owner,health,delay,log_line,nullptr,&backend});
 assert(runtime.registerPlatform("platform.clock",1,RiscBoot::Runtime::Scope::Global,0,&clock_api));
 if(!runtime.prepare(root.c_str())){fprintf(stderr,"prepare: %s\n",runtime.error());return 1;}if(!runtime.run()){fprintf(stderr,"runtime: %s\n",runtime.error());return 1;}
 assert(app_runs==3&&actions[1]>0&&actions[2]>0&&writes==3);alarm_occurrence occurrence;
 const auto&raw=data.at({4,"timer_occ"});assert(alarm_occurrence_decode(&occurrence,(const uint8_t*)raw.data(),raw.size(),2)&&occurrence.state==ALARM_OCC_ACKED);
 assert(data.count({3,"timer_occ"})==0);puts("Actual ordinary alarm ELF, Runtime/Graph/Module, app handoff and namespace integration passed");
}
