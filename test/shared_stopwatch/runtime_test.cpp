// Production Runtime, ProviderGraph, dynamic loader, scene provider and Stopwatch.
// Physical display/input are deterministic doubles. Host-only, not device execution.
#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include "RiscRealtimeV1.h"
using RiscBoot::Runtime;
static std::string root,mode;
static unsigned width,height,format,invocations,finis,unloads,visits,frames;
static uint64_t elapsed;
static unsigned writes;
static std::map<std::string,std::vector<uint8_t>> saved;
static int32_t get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){assert(ns==2);auto i=saved.find(k);if(i==saved.end()){*n=0;return RISC_KEY_VALUE_NOT_FOUND;}*n=i->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,i->second.data(),*n);return 0;}
static int32_t put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){assert(ns==2&&n==20&&!strcmp(k,"stopwatch_utc"));++writes;saved[k]=std::vector<uint8_t>((const uint8_t*)b,(const uint8_t*)b+n);return 0;}

extern "C" const void* scene_test_hardware(unsigned);
extern "C" void scene_test_hardware_closed();
extern "C" unsigned scene_test_invocation(){return invocations;}
extern "C" unsigned scene_test_width(){return width;}
extern "C" unsigned scene_test_height(){return height;}
extern "C" unsigned scene_test_format(){return format;}
extern "C" uint64_t scene_test_millis(){return elapsed;}
extern "C" void scene_test_frame(const void*p,size_t n){++frames;std::ofstream f(root+"/frame-"+std::to_string(frames)+".raw",std::ios::binary);f.write(static_cast<const char*>(p),n);assert(f.good());}
extern "C" void scene_test_lifecycle(unsigned event){
 if(event==1){++invocations;assert(invocations<=2&&finis==invocations-1&&unloads==invocations-1);}
 else if(event==2){++finis;assert(finis==invocations);}
 else{assert(event==3);++unloads;assert(unloads==finis);}
}
extern "C" void scene_test_result(unsigned ms,bool running){if(mode=="no-ui"){assert(writes==0&&!running);return;}assert(writes==invocations);assert(running==(invocations==1));if(invocations==2)assert(ms>=2000);printf("Stopwatch observed elapsed=%u running=%d writes=%u\n",ms,running,writes);}
extern "C" int scene_test_launch(){
 assert(finis==invocations&&unloads==invocations);
 if(invocations)assert(!dlopen((root+"/stopwatch.elf").c_str(),RTLD_NOW|RTLD_NOLOAD));
 if(invocations==1)elapsed+=2000;
 ++visits;return invocations;
}
static bool owner(){return true;}
static bool health(risc_runtime_health_v1*){return true;}
static bool log_line(const char*s){fprintf(stderr,"Runtime: %s\n",s);return true;}
static void delay(uint32_t n){elapsed+=n;assert(elapsed<120000);}
static void file(const std::string&name,const std::string&data){std::ofstream f(root+"/"+name);f<<data;assert(f.good());}
static std::string serialized(const JsonDocument&doc){std::string s;serializeJson(doc,s);return s;}
static void configure(){
 file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 JsonDocument boot;boot["board"]="board.json";boot["default_app"]="launcher.elf";boot["provider_activation"]=mode=="retained-providers"?"demand-retained":"demand";
    auto drivers=boot["drivers"].to<JsonArray>();
    auto driver=[&](const char* id,const char* cap,unsigned api,std::initializer_list<std::pair<const char*,unsigned>> requirements){
        JsonDocument doc;doc["type"]="driver";doc["id"]=id;doc["version"]="0.1.0";doc["driver_abi"]=2;doc["architecture"]="xtensa-esp32s3";doc["file_name"]=std::string(id)+".elf";
        auto rs=doc["requires"].to<JsonArray>();for(auto r:requirements){auto row=rs.add<JsonObject>();row["capability"]=r.first;row["api"]=r.second;}
        auto p=doc["provides"].to<JsonArray>().add<JsonObject>();p["capability"]=cap;p["api"]=api;
        std::string name=std::string(id)+".json";file(name,serialized(doc));auto row=drivers.add<JsonObject>();row["manifest"]=name;return row;
    };

 driver("display","display.output",1,{});driver("touch","input.touch.raw",1,{});driver("navigation","input.navigation",1,{});
 driver("scene-presentation-profile","ui.presentation-profile",1,{});
 driver("scene-host","ui.scene",1,{{"display.output",1},{"input.touch.raw",1},{"input.navigation",1},{"platform.clock",1},{"ui.presentation-profile",1}});
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*id:{"launcher","stopwatch"}){
  JsonDocument doc;doc["type"]="application";doc["id"]=id;doc["version"]="0.2.0";doc["architecture"]="xtensa-esp32s3";doc["file_name"]=std::string(id)+".elf";doc["entry"]="app_main";
  doc["requires"].to<JsonArray>();auto p=policies.add<JsonObject>();p["grants"].to<JsonArray>();p["manifest"]=std::string(id)+".json";
  if(!strcmp(id,"stopwatch")){for(const char*cap:{"ui.scene","storage.key-value","runtime.realtime"}){auto q=(doc["optional"].is<JsonArray>()?doc["optional"].as<JsonArray>():doc["optional"].to<JsonArray>()).add<JsonObject>();q["capability"]=cap;q["api"]=1;if(mode=="no-ui"&&!strcmp(cap,"ui.scene"))continue;auto g=p["grants"].is<JsonArray>()?p["grants"].as<JsonArray>().add<JsonObject>():p["grants"].to<JsonArray>().add<JsonObject>();g["capability"]=cap;g["api"]=1;g["instance_id"]=!strcmp(cap,"storage.key-value")?2:0;}}

  file(std::string(id)+".json",serialized(doc));
 }
 file("boot.json",serialized(boot));
}
int main(int argc,char**argv){
 assert(argc==6);root=argv[1];width=std::stoul(argv[2]);height=std::stoul(argv[3]);format=std::stoul(argv[4]);mode=argv[5];configure();
 RiscBoot::KeyValueBackend storage{nullptr,get,put};RiscBoot::Port port{owner,health,delay,log_line};port.keyValue=&storage;port.bindPlatforms=[](Runtime&r){return r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,scene_test_hardware(3))&&r.registerRealtime(static_cast<const risc_realtime_control_api_v1*>(scene_test_hardware(4)));};
 {RiscTest::FileSources files({root.c_str()});Runtime r(port);assert(files.bind(r));if(!r.prepare(root.c_str())){fprintf(stderr,"prepare: %s\n",r.error());return 2;}if(!r.run()){fprintf(stderr,"run: %s\n",r.error());return 3;}assert(!r.retained());}
 assert((mode=="no-ui"?frames==0:frames>0)&&invocations==2&&finis==2&&unloads==2&&visits==3);scene_test_hardware_closed();
 printf("actual Runtime Stopwatch %ux%u %s PASS inits=%u finis=%u actual-unloads=%u frames=%u\n",width,height,mode.c_str(),invocations,finis,unloads,frames);
}
