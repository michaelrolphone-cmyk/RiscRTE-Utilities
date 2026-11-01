// Production Runtime, ProviderGraph, dynamic loader, scene provider and Frequency.
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
using RiscBoot::Runtime;
static std::string root,mode;
static unsigned width,height,format,invocations,finis,unloads,visits,frames;
static uint64_t elapsed;
extern "C" const void* scene_test_hardware(unsigned);
extern "C" void scene_test_hardware_closed();
extern "C" unsigned scene_test_invocation(){return invocations;}
extern "C" unsigned scene_test_width(){return width;}
extern "C" unsigned scene_test_height(){return height;}
extern "C" unsigned scene_test_format(){return format;}
extern "C" uint64_t scene_test_millis(){return elapsed;}
extern "C" void scene_test_frame(const void*p,size_t n){++frames;std::ofstream f(root+"/frame-"+(frames==1?std::string("first"):std::string("latest"))+".raw",std::ios::binary);f.write(static_cast<const char*>(p),n);assert(f.good());}
extern "C" void scene_test_lifecycle(unsigned event){
 if(event==1){++invocations;assert(invocations<=2&&finis==invocations-1&&unloads==invocations-1);}
 else if(event==2){++finis;assert(finis==invocations);}
 else{assert(event==3);++unloads;assert(unloads==finis);}
}
extern "C" unsigned frequency_test_io_count(void);
static unsigned(*observer)();static bool audio_live;static unsigned audio_opens,audio_closes,audio_writes;
extern "C" void scene_test_observer(unsigned(*fn)()){observer=fn;}
extern "C" unsigned scene_test_navigation(){return observer?observer():0;}
extern "C" void scene_test_result(unsigned hz,unsigned level,bool playing,bool owned){assert(!playing&&!owned);if(mode!="no-ui")assert(hz==540&&level==99);}
extern "C" bool frequency_audio_open(){assert(!audio_live);audio_live=true;++audio_opens;return true;}
extern "C" bool frequency_audio_write(size_t n){assert(audio_live&&n==256);++audio_writes;elapsed+=16;return true;}
extern "C" bool frequency_audio_close(){assert(audio_live);audio_live=false;++audio_closes;return true;}
extern "C" unsigned frequency_audio_writes(){return audio_writes;}
extern "C" int scene_test_launch(){
 assert(finis==invocations&&unloads==invocations);
 if(invocations)assert(!dlopen((root+"/frequency.elf").c_str(),RTLD_NOW|RTLD_NOLOAD));
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

 if(mode!="no-ui"){driver("audio","audio.output",1,{});driver("display","display.output",1,{});driver("touch","input.touch.raw",1,{});driver("navigation","input.navigation",1,{});
 driver("scene-presentation-profile","ui.presentation-profile",1,{});
 driver("scene-host","ui.scene",1,{{"display.output",1},{"input.touch.raw",1},{"input.navigation",1},{"platform.clock",1},{"ui.presentation-profile",1}});}
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*id:{"launcher","frequency"}){
  JsonDocument doc;doc["type"]="application";doc["id"]=id;doc["version"]="0.2.0";doc["architecture"]="xtensa-esp32s3";doc["file_name"]=std::string(id)+".elf";doc["entry"]="app_main";
  auto p=policies.add<JsonObject>();p["manifest"]=std::string(id)+".json";p["grants"].to<JsonArray>();doc["requires"].to<JsonArray>();
  if(!strcmp(id,"frequency")){auto q=doc["optional"].to<JsonArray>().add<JsonObject>();q["capability"]="ui.scene";q["api"]=1;auto aq=doc["optional"].as<JsonArray>().add<JsonObject>();aq["capability"]="audio.output";aq["api"]=1;if(mode!="no-ui"){auto g=p["grants"].to<JsonArray>().add<JsonObject>();g["capability"]="ui.scene";g["api"]=1;g["instance_id"]=0;auto ag=p["grants"].as<JsonArray>().add<JsonObject>();ag["capability"]="audio.output";ag["api"]=1;ag["instance_id"]=0;}}
  file(std::string(id)+".json",serialized(doc));
 }
 file("boot.json",serialized(boot));
}
int main(int argc,char**argv){
 assert(argc==6);root=argv[1];width=std::stoul(argv[2]);height=std::stoul(argv[3]);format=std::stoul(argv[4]);mode=argv[5];configure();
 RiscBoot::Port port{owner,health,delay,log_line};port.monotonicMs=[](uint32_t*out){*out=(uint32_t)elapsed;return true;};port.bindPlatforms=[](Runtime&r){return r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,scene_test_hardware(3));};
 {RiscTest::FileSources files({root.c_str()});Runtime r(port);assert(files.bind(r));if(!r.prepare(root.c_str())){fprintf(stderr,"prepare: %s\n",r.error());return 2;}if(!r.run()){fprintf(stderr,"run: %s\n",r.error());return 3;}assert(!r.retained());}
 assert((mode=="no-ui"?frames==0:frames>0)&&invocations==2&&finis==2&&unloads==2&&visits==3);scene_test_hardware_closed();assert(!audio_live);if(mode=="no-ui")assert(frequency_test_io_count()==0);assert(mode=="no-ui"?audio_opens==0:audio_opens==2&&audio_closes==2&&audio_writes>=10);
 printf("actual Runtime Frequency %ux%u %s PASS inits=%u finis=%u actual-unloads=%u frames=%u\n",width,height,mode.c_str(),invocations,finis,unloads,frames);
}
