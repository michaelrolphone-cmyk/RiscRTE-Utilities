// Real Runtime, app, scene components and broadcast provider; physical APIs doubled.
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
static unsigned width=800,height=480,format=1,invocations,finis,unloads,visits,writes,frames;
static uint64_t millis;
static bool atomic_mode;
static uint64_t atomic_revision=1,atomic_token;
static int atomic_context;
static unsigned(*observer)(unsigned);
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
extern "C" int scene_test_launch(){assert(finis==invocations&&unloads==invocations);if(invocations){assert(!dlopen((root+"/battery.elf").c_str(),RTLD_NOW|RTLD_NOLOAD));assert(kv.at({1,"ble_broadcast"})==std::vector<uint8_t>({0x62,1,1,0xa4}));}++visits;return invocations;}
static bool owner(){return true;}
static bool health(risc_runtime_health_v1*h){h->uptime_ms=millis;return true;}
static bool log_line(const char*s){fprintf(stderr,"Runtime: %s\n",s);return true;}
static void delay(uint32_t n){millis+=n;assert(millis<90000);}
static int32_t get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){auto i=kv.find({ns,k});*n=0;if(i==kv.end())return RISC_KEY_VALUE_NOT_FOUND;*n=i->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,i->second.data(),*n);return 0;}
static int32_t put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){assert(ns==1&&!strcmp(k,"ble_broadcast")&&n==4);auto p=static_cast<const uint8_t*>(b);assert(p[0]==0x62&&p[1]==1&&p[2]<=1&&p[3]==(p[2]^0xa5));kv[{ns,k}]=std::vector<uint8_t>(p,p+n);++writes;return 0;}
static const RiscBoot::AtomicSettingsEndpoint::Operations atomic_ops={
 [](void*){return 1u;},[](void*){return atomic_token==0;},[](void*){return false;},
 [](void*,risc_key_value_atomic_state_v1*out){*out={};out->struct_size=sizeof(*out);out->namespace_id=1;out->revision=atomic_revision;out->flags=RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC;out->max_mutations=1;out->max_value_bytes=64;out->max_group_bytes=512;out->max_intent_bytes=176;return 0;},
 [](void*,uint64_t rev,const char*k,void*b,uint32_t n,uint32_t*z){if(rev!=atomic_revision)return int32_t(RISC_ATOMIC_STALE);return get(nullptr,1,k,b,n,z);},
 [](void*,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*intent,uint32_t z,uint64_t*t){assert(rev==atomic_revision&&n==1&&intent&&z==176&&!atomic_token);assert(!put(nullptr,1,m->key,m->value,m->size));++atomic_revision;*t=atomic_token=atomic_revision;return 0;},
 [](void*,uint64_t t,risc_key_value_atomic_receipt_v1*out){assert(t==atomic_token);*out={sizeof(*out),RISC_ATOMIC_OK,atomic_revision,atomic_token};atomic_token=0;return 0;},
 [](void*,uint64_t t){assert(t==atomic_token);return 0;}
};
static void file(const std::string&n,const std::string&s){std::ofstream f(root+"/"+n);f<<s;assert(f.good());}
static std::string json(const JsonDocument&d){std::string s;serializeJson(d,s);return s;}
static void configure(){
 file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 JsonDocument boot;boot["board"]="board.json";boot["default_app"]="launcher.elf";boot["provider_activation"]=mode=="retained-providers"?"demand-retained":"demand";auto drivers=boot["drivers"].to<JsonArray>();
 auto driver=[&](const char*id,const char*cap,std::initializer_list<const char*> requirements){JsonDocument d;d["type"]="driver";d["id"]=id;d["version"]="0.1.0";d["driver_abi"]=2;d["architecture"]="xtensa-esp32s3";d["file_name"]=std::string(id)+".elf";auto rs=d["requires"].to<JsonArray>();for(auto c:requirements){auto r=rs.add<JsonObject>();r["capability"]=c;r["api"]=1;}auto p=d["provides"].to<JsonArray>().add<JsonObject>();p["capability"]=cap;p["api"]=1;std::string name=std::string(id)+".json";file(name,json(d));drivers.add<JsonObject>()["manifest"]=name;};
 driver("display","display.output",{});driver("touch","input.touch.raw",{});driver("navigation","input.navigation",{});driver("gauge","board.battery",{});driver("radio","bluetooth.telemetry",{});driver("scene-presentation-profile","ui.presentation-profile",{});
 driver("scene-host","ui.scene",{"display.output","input.touch.raw","input.navigation","platform.clock","ui.presentation-profile"});
 driver("telemetry-broadcast","telemetry.broadcast",{"platform.clock","bluetooth.telemetry"});
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*id:{"launcher","battery"}){JsonDocument d;d["type"]="application";d["id"]=id;d["version"]="0.2.0";d["architecture"]="xtensa-esp32s3";d["file_name"]=std::string(id)+".elf";d["entry"]="app_main";d["requires"].to<JsonArray>();auto optional=d["optional"].to<JsonArray>();auto p=policies.add<JsonObject>();std::string name=std::string(id)+".json";p["manifest"]=name;auto gs=p["grants"].to<JsonArray>();if(!strcmp(id,"battery")){for(const char*c:{"ui.scene","board.battery","telemetry.broadcast","storage.key-value","storage.preferences","storage.key-value.atomic"}){auto r=optional.add<JsonObject>();r["capability"]=c;r["api"]=1;if(mode=="no-ui"&&!strcmp(c,"ui.scene"))continue;if(atomic_mode&&!strcmp(c,"storage.preferences"))continue;if(!atomic_mode&&!strcmp(c,"storage.key-value.atomic"))continue;auto g=gs.add<JsonObject>();g["capability"]=c;g["api"]=1;g["instance_id"]=!strncmp(c,"storage.",8)?1:0;}}file(name,json(d));}
 file("boot.json",json(boot));kv[{1,"ble_broadcast"}]={0x62,1,1,0xa4};kv[{1,"quick_radio"}]={0x51,1,2,0xa7};
}
int main(int argc,char**argv){assert(argc==5);root=argv[1];width=std::strtoul(argv[2],nullptr,10);height=std::strtoul(argv[3],nullptr,10);format=width==800?1:5;mode=argv[4];atomic_mode=getenv("BATTERY_ATOMIC")!=nullptr;configure();RiscBoot::KeyValueBackend storage{nullptr,get,put};storage.maxBlobSize=RISC_BOUND_KEY_VALUE_V2_BLOB_MAX;RiscBoot::Port port{owner,health,delay,log_line};port.keyValue=&storage;port.bindPlatforms=[](Runtime&r){if(atomic_mode&&!r.registerAtomicKeyValue("battery",1,RiscBoot::AtomicSettingsEndpoint{&atomic_context,&atomic_ops}))return false;return r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,scene_test_hardware(6));};{RiscTest::FileSources sources({root.c_str()});Runtime r(port);assert(sources.bind(r));if(!r.prepare(root.c_str())){fprintf(stderr,"prepare:%s\n",r.error());return 2;}assert(!scene_test_hardware_io());if(!r.run()){fprintf(stderr,"run:%s\n",r.error());return 3;}assert(!r.retained());}assert(invocations==2&&finis==2&&unloads==2&&visits==3);scene_test_hardware_closed();if(mode=="no-ui")assert(!frames&&!writes&&!scene_test_hardware_io());else assert(frames&&writes==2);assert(kv.at({1,"ble_broadcast"})==std::vector<uint8_t>({0x62,1,1,0xa4}));printf("Battery actual Runtime %ux%u %s PASS frames=%u writes=%u unloads=%u\n",width,height,mode.c_str(),frames,writes,unloads);}
