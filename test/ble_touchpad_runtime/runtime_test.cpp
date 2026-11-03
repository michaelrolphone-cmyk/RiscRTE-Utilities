// Real Runtime, app and held-input scene; physical HID/display/input and KV backend doubled.
#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "RiscBoundKeyValueV2.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using RiscBoot::Runtime;
static std::string root,mode;
static unsigned width=800,height=480,format=1,invocations,finis,unloads,visits,writes,frames,reads;
static uint64_t millis;
static unsigned(*observer)(unsigned);
static int(*pad_position)(int*,int*);
extern "C" void scene_test_pointer_register(int(*fn)(int*,int*)){pad_position=fn;}
extern "C" int scene_test_pointer_position(int*x,int*y){return pad_position?pad_position(x,y):0;}
static std::map<std::pair<uint32_t,std::string>,std::vector<uint8_t>> kv;
extern "C" const void* scene_test_hardware(unsigned);
extern "C" unsigned scene_test_hardware_io();
extern "C" void scene_test_hardware_closed();
extern "C" unsigned scene_test_invocation(){return invocations;}
extern "C" unsigned scene_test_width(){return width;}
extern "C" unsigned scene_test_height(){return height;}
extern "C" unsigned scene_test_format(){return format;}
extern "C" int scene_test_mode(){return 0;}
extern "C" uint64_t scene_test_millis(){return millis;}
extern "C" void scene_test_register(unsigned(*fn)(unsigned)){observer=fn;}
extern "C" unsigned scene_test_observe(unsigned k){assert(observer);return observer(k);}
extern "C" void scene_test_frame(const void*p,size_t n){++frames;if(frames==1){std::ofstream f(root+"/frame-first.raw",std::ios::binary);f.write(static_cast<const char*>(p),n);assert(f.good());}}
extern "C" void scene_test_lifecycle(unsigned event){if(event==1){++invocations;assert(invocations<=2&&finis==invocations-1&&unloads==invocations-1);}else if(event==2){++finis;assert(finis==invocations);}else{assert(event==3);++unloads;assert(unloads==finis);}}
extern "C" int scene_test_launch(){assert(finis==invocations&&unloads==invocations);if(invocations){assert(!dlopen((root+"/ble_touchpad.elf").c_str(),RTLD_NOW|RTLD_NOLOAD));assert(kv.at({1,"quick_radio"})==std::vector<uint8_t>({0x51,1,2,0xa7}));}++visits;return invocations;}
static bool owner(){return true;}
static bool health(risc_runtime_health_v1*h){h->uptime_ms=millis;return true;}
static bool log_line(const char*s){fprintf(stderr,"Runtime: %s\n",s);return true;}
extern "C" void scene_test_hardware_debug();
static void delay(uint32_t n){millis+=n;if(millis>=90000&&observer)fprintf(stderr,"timeout page=%u focus=%u hid=%u state=%u terminal=%u dirty=%u alarm=%u\n",observer(1),observer(0),observer(2),observer(3),observer(4),observer(6),observer(7));if(millis>=90000&&observer){fprintf(stderr,"detail reset=%u alarmstate=%u reserved=%u session=%u hiddirty=%u\n",observer(5),observer(9),observer(10),observer(11),observer(12));scene_test_hardware_debug();}assert(millis<90000);}
static int32_t get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){++reads;auto i=kv.find({ns,k});*n=0;if(i==kv.end())return RISC_KEY_VALUE_NOT_FOUND;*n=i->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,i->second.data(),*n);return 0;}
static int32_t put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){assert(ns==11&&!strcmp(k,"hid_buttons")&&n==40);++writes;const uint8_t*p=static_cast<const uint8_t*>(b);kv[{ns,k}]=std::vector<uint8_t>(p,p+n);return 0;}
extern "C" bool scene_test_exit_safe();
extern "C" bool scene_test_storage_safe();
static void file(const std::string&n,const std::string&s){std::ofstream f(root+"/"+n);f<<s;assert(f.good());}
static std::string json(const JsonDocument&d){std::string s;serializeJson(d,s);return s;}
static void configure(){
 file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 JsonDocument boot;boot["board"]="board.json";boot["default_app"]="launcher.elf";boot["provider_activation"]=mode=="retained-providers"?"demand-retained":"demand";auto drivers=boot["drivers"].to<JsonArray>();
 auto driver=[&](const char*id,const char*cap,std::initializer_list<const char*> requirements,uint32_t api=1){JsonDocument d;d["type"]="driver";d["id"]=id;d["version"]="0.1.0";d["driver_abi"]=2;d["architecture"]="xtensa-esp32s3";d["file_name"]=std::string(id)+".elf";auto rs=d["requires"].to<JsonArray>();for(auto c:requirements){auto r=rs.add<JsonObject>();r["capability"]=c;r["api"]=1;}auto p=d["provides"].to<JsonArray>().add<JsonObject>();p["capability"]=cap;p["api"]=api;std::string name=std::string(id)+".json";file(name,json(d));drivers.add<JsonObject>()["manifest"]=name;};
 driver("display","display.output",{});driver("touch","input.touch.raw",{});driver("navigation","input.navigation",{});driver("hid","bluetooth.hid",{});driver("scene-presentation-profile","ui.presentation-profile",{});
 driver("scene-host","ui.scene",{"display.output","input.touch.raw","input.navigation","platform.clock","ui.presentation-profile"});

 if(getenv("TOUCHPAD_ALARM"))driver("alarm","alarm.service.facade",{},2);
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*id:{"launcher","ble_touchpad"}){JsonDocument d;d["type"]="application";d["id"]=id;d["version"]="0.2.0";d["architecture"]="xtensa-esp32s3";d["file_name"]=std::string(id)+".elf";d["entry"]="app_main";d["requires"].to<JsonArray>();auto optional=d["optional"].to<JsonArray>();auto p=policies.add<JsonObject>();std::string name=std::string(id)+".json";p["manifest"]=name;auto gs=p["grants"].to<JsonArray>();if(!strcmp(id,"ble_touchpad")){for(const char*c:{"ui.scene","bluetooth.hid","storage.key-value","alarm.service.facade"}){auto r=optional.add<JsonObject>();r["capability"]=c;r["api"]=!strcmp(c,"alarm.service.facade")?2:1;if(!strcmp(c,"alarm.service.facade")&&!getenv("TOUCHPAD_ALARM"))continue;if(mode=="no-ui"&&!strcmp(c,"ui.scene"))continue;if(getenv("TOUCHPAD_NO_HID")&&!strcmp(c,"bluetooth.hid"))continue;auto g=gs.add<JsonObject>();g["capability"]=c;g["api"]=!strcmp(c,"alarm.service.facade")?2:1;g["instance_id"]=!strcmp(c,"storage.key-value")?1:0;if(!strcmp(c,"storage.key-value")){auto other=gs.add<JsonObject>();other["capability"]=c;other["api"]=1;other["instance_id"]=11;}}}file(name,json(d));}
 file("boot.json",json(boot));kv[{1,"ble_broadcast"}]={0x62,1,1,0xa4};kv[{1,"quick_radio"}]={0x51,1,2,0xa7};
}
int main(int argc,char**argv){assert(argc==5);root=argv[1];width=std::strtoul(argv[2],nullptr,10);height=std::strtoul(argv[3],nullptr,10);format=5;mode=argv[4];configure();RiscBoot::KeyValueBackend storage{nullptr,get,put};storage.maxBlobSize=RISC_BOUND_KEY_VALUE_V2_BLOB_MAX;RiscBoot::Port port{owner,health,delay,log_line};port.keyValue=&storage;port.appExitSafe=scene_test_exit_safe;port.providerStorageSafe=scene_test_storage_safe;port.bindPlatforms=[](Runtime&r){return r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,scene_test_hardware(4));};{RiscTest::FileSources sources({root.c_str()});Runtime r(port);assert(sources.bind(r));if(!r.prepare(root.c_str())){fprintf(stderr,"prepare:%s\n",r.error());return 2;}assert(!scene_test_hardware_io());if(!r.run()){fprintf(stderr,"run:%s\n",r.error());return 3;}assert(!r.retained());}assert(invocations==2&&finis==2&&unloads==2&&visits==3);scene_test_hardware_closed();if(mode=="no-ui")assert(!frames&&!writes&&!reads&&!scene_test_hardware_io());else assert(frames&&!writes);assert(kv.at({1,"quick_radio"})==std::vector<uint8_t>({0x51,1,2,0xa7}));printf("Touchpad actual Runtime %ux%u %s PASS frames=%u writes=%u unloads=%u\n",width,height,mode.c_str(),frames,writes,unloads);}
