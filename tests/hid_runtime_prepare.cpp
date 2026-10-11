/* Production Runtime admission of generated manifests. No image execution or
 * hardware backend: capability providers are declared fixture modules. */
#include "bootstrap/Runtime.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
using namespace RiscBoot;
namespace fs=std::filesystem;
static void save(const fs::path&p,const JsonDocument&j){std::ofstream f(p);std::string s;serializeJson(j,s);f<<s;assert(f.good());}
static JsonDocument read(const fs::path&p){JsonDocument j;assert(readJson(p.c_str(),j));return j;}
static int32_t get(void*,uint32_t,const char*,void*,uint32_t,uint32_t*){assert(false);return -1;}
static int32_t put(void*,uint32_t,const char*,const void*,uint32_t){assert(false);return -1;}
int main(int argc,char**argv){
 assert(argc==3);fs::path packages=argv[1],root=argv[2];fs::create_directories(root);
 std::ofstream(root/"board.json")<<R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"hid-fixture","revision":"test","buses":[],"devices":[]})";
 KeyValueBackend kv{nullptr,get,put,4096};Port port{[](){return true;},[](risc_runtime_health_v1*){return true;},[](uint32_t){},[](const char*){return true;},nullptr,&kv};
 for(const char*name:{"ble_touchpad","ble_buttons"}){
  auto m=read(packages/(std::string(name)+".json"));save(root/"app.json",m);
  JsonDocument boot;boot["board"]="board.json";boot["default_app"]=m["file_name"];
  auto app=boot["app_capabilities"].to<JsonArray>().add<JsonObject>();app["manifest"]="app.json";
  auto grants=app["grants"].to<JsonArray>();auto drivers=boot["drivers"].to<JsonArray>();
  JsonDocument board;board["schema"]="riscrte.board-hardware";board["schema_version"]=1;board["board_id"]="hid-fixture";board["revision"]="test";board["buses"].to<JsonArray>();auto devices=board["devices"].to<JsonArray>();
  bool paper=false;for(JsonObjectConst req:m["requires"].as<JsonArrayConst>())if(!strcmp(req["capability"],"input.navigation"))paper=true;
  unsigned index=0;
  for(JsonObjectConst req:m["requires"].as<JsonArrayConst>()){
   const char*cap=req["capability"];unsigned api=req["api"];
   unsigned instance=!strcmp(cap,"storage.key-value")?1:!strcmp(cap,"input.touch.raw")?(paper?4:6):paper&&!strcmp(cap,"display.output")?3:paper&&!strcmp(cap,"input.navigation")?6:paper&&!strcmp(cap,"board.battery")?7:paper&&!strcmp(cap,"rtc.clock")?8:0;
   auto grant=grants.add<JsonObject>();grant["capability"]=cap;grant["api"]=api;grant["instance_id"]=instance;
   if(!strcmp(cap,"storage.key-value"))continue;
   JsonDocument d;d["type"]="driver";d["id"]="fixture-"+std::to_string(index);d["version"]="1.0.0";d["driver_abi"]=2;d["architecture"]="xtensa-esp32s3";d["file_name"]="fixture.elf";d["requires"].to<JsonArray>();auto provided=d["provides"].to<JsonArray>().add<JsonObject>();provided["capability"]=cap;provided["api"]=api;
   std::string path="provider-"+std::to_string(index++)+".json";auto selection=drivers.add<JsonObject>();selection["manifest"]=path;
   if(instance){
    selection["instance_id"]=instance;auto hw=d["requires"].as<JsonArray>().add<JsonObject>();hw["capability"]="hardware.device";hw["api"]=1;
    auto compatible=d["hardware_compatibility"].to<JsonArray>().add<JsonObject>();compatible["compatible"]="test,gpio";compatible["revisions"].to<JsonArray>().add("test");compatible["config_type"]="gpio.bank";compatible["config_version"]=1;
    auto dev=devices.add<JsonObject>();dev["instance_id"]=instance;auto chip=dev["chip"].to<JsonObject>();chip["vendor"]="test";chip["model"]="gpio";chip["revision"]="test";dev["compatible"]="test,gpio";dev["config_type"]="gpio.bank";dev["config_version"]=1;auto config=dev["config"].to<JsonObject>();config["pins"].to<JsonArray>().add(instance);config["active_high"]=true;config["pull_up"]=false;config["debounce_us"]=0;config["long_press_us"]=0;config["click_min_us"]=0;
   }
   save(root/path,d);
  }
  if(!strcmp(name,"ble_buttons")){auto g=grants.add<JsonObject>();g["capability"]="storage.key-value";g["api"]=1;g["instance_id"]=11;}
  save(root/"board.json",board);save(root/"boot.json",boot);
  {Runtime runtime(port);bool ok=runtime.prepare(root.c_str());if(!ok)fprintf(stderr,"%s: %s\n",name,runtime.error());assert(ok);}
  m["profile"]="unsupported";save(root/"app.json",m);{Runtime runtime(port);assert(!runtime.prepare(root.c_str()));}
  m.remove("profile");save(root/"app.json",m);grants.remove(0);save(root/"boot.json",boot);{Runtime runtime(port);assert(!runtime.prepare(root.c_str()));}
  printf("%s: production Runtime prepare accepts canonical package; rejects unsupported field and missing grant\n",name);
 }
}
