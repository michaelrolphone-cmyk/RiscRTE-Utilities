// Production Runtime/Graph/Module/AppDataFiles + actual app, presenter, alarm
// control and alarm-service ELFs. Only physical devices are deterministic doubles.
#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "runtime/storage/AppDataFiles.h"
#include "RiscSceneStateV1.h"
#include "CountdownControlV1.h"
#include "RiscBoundKeyValueV2.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using RiscBoot::Runtime;
static std::string root,mode;
static unsigned width=240,height=240,format=5,invocations,finis,unloads,launcher_visits,kv_writes,frame_count;
static uint64_t millis;
static unsigned headless_phase;
static bool headless_done,exit_pending;
static RiscStorage::AppDataFiles* files;
static std::map<std::pair<uint32_t,std::string>,std::vector<uint8_t>> kv;
extern "C" const void* scene_test_hardware(unsigned);
extern "C" unsigned scene_test_hardware_io();
extern "C" void scene_test_hardware_closed();
extern "C" unsigned scene_test_invocation(){return invocations;}
extern "C" unsigned scene_test_width(){return width;}
extern "C" unsigned scene_test_height(){return height;}
extern "C" unsigned scene_test_format(){return format;}
extern "C" int scene_test_mode(){return mode=="headless";}
extern "C" uint64_t scene_test_millis(){return millis;}
extern "C" void scene_test_frame(const void*p,size_t n){
    ++frame_count;for(const char*name:{"frame-first.raw","frame-latest.raw"}){if(!strcmp(name,"frame-first.raw")&&frame_count!=1)continue;std::ofstream f(root+"/"+name,std::ios::binary);f.write(static_cast<const char*>(p),n);assert(f.good());}
}
static void checkpoint(unsigned expected_seconds,bool committed){
    uint8_t encoded[216],payload[120];uint32_t size=0;uint64_t revision=0,actual=0;
    assert(files->stat(66,"countdown.scene",&size,&revision)==0&&size==sizeof(encoded));
    assert(files->read(66,"countdown.scene",revision,encoded,sizeof(encoded),&size,&actual)==0);
    risc_scene_navigation_v1 path{};size_t length=0;
    assert(risc_scene_state_decode(encoded,size,1,&path,payload,sizeof(payload),&length));
    assert(length==120&&risc_scene_get32(payload)==expected_seconds);
    assert(risc_scene_get32(payload+12)==0);
    if(!committed)assert(path.depth==2&&path.routes[1]==2&&path.focus[1]==12);
    else assert(path.depth==1&&path.routes[0]==1);
}
extern "C" void scene_test_lifecycle(unsigned event){
    if(event==1){++invocations;assert(invocations<=2&&finis==invocations-1&&unloads==invocations-1);}
    else if(event==2){++finis;assert(finis==invocations);}
    else {assert(event==3);++unloads;assert(unloads==finis);}
}
extern "C" int scene_test_launch(){
    if(mode=="headless"){assert(!scene_test_hardware_io());return headless_done?-2:-1;}
    assert(finis==invocations&&unloads==invocations);
    if(invocations){void* h=dlopen((root+"/countdown.elf").c_str(),RTLD_NOW|RTLD_NOLOAD);assert(!h&&"previous application mapping is still resident");if(mode!="no-ui")checkpoint(301,invocations==2);}
    ++launcher_visits;return mode=="no-ui"&&invocations==2?-2:static_cast<int>(invocations);
}
extern "C" void scene_test_after_gui(const countdown_control_api_v1* control){
    assert(invocations==2&&unloads==2&&finis==2);
    const unsigned frames=frame_count,ui_calls=scene_test_hardware_io();
    countdown_control_snapshot_v1 status{};status.struct_size=sizeof(status);
    assert(!control->read(control->context,&status));
    assert(status.revision==1&&status.duration_seconds==301&&(status.flags&COUNTDOWN_CONTROL_ENABLED));
    bool fired=false;
    for(unsigned i=0;i<310*50;i++){
        millis+=20;assert(control->step(control->context)>=0);
        assert(!control->read(control->context,&status));
        if(status.flags&COUNTDOWN_CONTROL_ALERT){fired=true;break;}
    }
    fprintf(stderr,"POST GUI fired=%d rev=%u confirmed=%u flags=%u remain=%u state=%u error=%d status=%s ms=%llu\n",fired,status.revision,status.confirmed_revision,status.flags,status.remaining_seconds,status.schedule_state,status.error,status.status,(unsigned long long)millis);assert(fired&&status.revision==1&&status.confirmed_revision==1);
    assert(control->dismiss(control->context,&status.occurrence)>=0);
    countdown_control_command_v1 command{};command.struct_size=sizeof(command);
    assert(!control->prepare(control->context,COUNTDOWN_CONTROL_CANCEL,0,status.revision,&command));
    assert(!control->apply(control->context,&command));
    for(unsigned i=0;i<100;i++){millis+=20;assert(control->step(control->context)>=0);}
    assert(!control->read(control->context,&status));
    assert(status.revision==2&&!(status.flags&(COUNTDOWN_CONTROL_ALERT|COUNTDOWN_CONTROL_ENABLED)));
    assert(frames==frame_count&&ui_calls==scene_test_hardware_io());
}
static bool owner(){return true;}
static bool health(risc_runtime_health_v1*){return true;}
static bool log_line(const char*s){fprintf(stderr,"Runtime: %s\n",s);return true;}
static void delay(uint32_t n){
    if(mode!="headless"){millis+=n;assert(millis<60000 || invocations==2);return;}
    millis+=1000;assert(millis<4*3600*1000);
}
static int32_t get(void*,uint32_t ns,const char* key,void* dst,uint32_t capacity,uint32_t* size){
    auto i=kv.find({ns,key});*size=0;if(i==kv.end())return RISC_KEY_VALUE_NOT_FOUND;
    *size=i->second.size();if(capacity<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(dst,i->second.data(),*size);return 0;
}
static int32_t put(void*,uint32_t ns,const char* key,const void* src,uint32_t size){
    assert(size&&size<=RISC_BOUND_KEY_VALUE_V2_BLOB_MAX);kv[{ns,key}]=std::vector<uint8_t>(static_cast<const uint8_t*>(src),static_cast<const uint8_t*>(src)+size);++kv_writes;return 0;
}
static void file(const std::string& name,const std::string& contents){std::ofstream f(root+"/"+name);f<<contents;assert(f.good());}
static std::string serialized(const JsonDocument& doc){std::string s;serializeJson(doc,s);return s;}
static void configure(bool utc){
    file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
    JsonDocument boot;boot["board"]="board.json";boot["default_app"]="launcher.elf";boot["provider_activation"]=mode=="retained-providers"?"demand-retained":"demand";
    auto drivers=boot["drivers"].to<JsonArray>();
    auto driver=[&](const char* id,const char* cap,unsigned api,std::initializer_list<std::pair<const char*,unsigned>> requirements){
        JsonDocument doc;doc["type"]="driver";doc["id"]=id;doc["version"]="0.1.0";doc["driver_abi"]=2;doc["architecture"]="xtensa-esp32s3";doc["file_name"]=std::string(id)+".elf";
        auto rs=doc["requires"].to<JsonArray>();for(auto r:requirements){auto row=rs.add<JsonObject>();row["capability"]=r.first;row["api"]=r.second;}
        auto p=doc["provides"].to<JsonArray>().add<JsonObject>();p["capability"]=cap;p["api"]=api;
        std::string name=std::string(id)+".json";file(name,serialized(doc));auto row=drivers.add<JsonObject>();row["manifest"]=name;return row;
    };
    if(mode!="headless"){
        driver("display","display.output",1,{});driver("touch","input.touch.raw",1,{});driver("navigation","input.navigation",1,{});
        driver("scene-presentation-profile","ui.presentation-profile",1,{});
        driver("scene-host","ui.scene",1,{{"display.output",1},{"input.touch.raw",1},{"input.navigation",1},{"platform.clock",1},{"ui.presentation-profile",1}});
    }
    if(!utc){driver("rtc","rtc.clock",2,{});driver("haptic","haptic.effect",1,{});driver("audio","audio.output",1,{});}
    const char* clock=utc?"platform.realtime":"rtc.clock";unsigned cv=utc?1:2;
    JsonObject service=utc?driver("alarm-service","alarm.service",2,{{"storage.key-value.bound",2},{"platform.clock",1},{clock,cv}}):driver("alarm-service","alarm.service",1,{{"storage.key-value.bound",1},{"platform.clock",1},{clock,cv},{"haptic.effect",1},{"audio.output",1}});
    auto binding=[&](JsonObject d,const char* key,unsigned ns,const char* access){auto a=d["key_value"].is<JsonArray>()?d["key_value"].as<JsonArray>():d["key_value"].to<JsonArray>();auto b=a.add<JsonObject>();b["key"]=key;b["namespace"]=ns;b["access"]=access;};
    // C++17 does not extend the backing-array lifetime of initializer_lists
    // selected by a conditional range expression. Keep named arrays alive
    // through iteration; GCC 11 otherwise emitted corrupt fixture keys.
    const char* const config_keys[] = {utc ? "alarm_utc_cfg" : "alarm_cfg",
                                       utc ? "timer_utc_cfg" : "timer_cfg"};
    const char* const occurrence_keys[] = {utc ? "alarm_utc_occ" : "alarm_occ",
                                           utc ? "timer_utc_occ" : "timer_occ",
                                           utc ? "points_utc_occ" : "points_occ"};
    for(const char* k:config_keys)binding(service,k,3,"read");
    for(const char* k:occurrence_keys)if(!getenv("ALARM_SCENE_SCHEDULES_ONLY")||strncmp(k,"points",6))binding(service,k,4,"read-write");
    binding(service,"alert_mode",1,"read");binding(service,"alert_dnd",1,"read");if(!getenv("ALARM_SCENE_SCHEDULES_ONLY")){binding(service,utc?"points_utc_cfg":"points_cfg",5,"read");binding(service,utc?"time_zone":"alarm_volume",1,"read");}
    binding(service,"clock_edit_auth",getenv("ALARM_SCENE_SCHEDULES_ONLY")?4:6,"read-write");
    auto control=driver("countdown-control","countdown.control",1,{{"storage.key-value.bound",1},{"alarm.service",utc?2u:1u},{clock,cv}});
    binding(control,utc?"timer_utc_cfg":"timer_cfg",3,"read-write");
    auto policies=boot["app_capabilities"].to<JsonArray>();
    auto app=[&](const char* id,const char* elf,std::initializer_list<std::pair<const char*,unsigned>> requirements){
        JsonDocument doc;doc["type"]="application";doc["id"]=id;doc["version"]="0.3.0";doc["architecture"]="xtensa-esp32s3";doc["file_name"]=elf;doc["entry"]="app_main";
        auto p=policies.add<JsonObject>();std::string name=std::string(id)+".json";p["manifest"]=name;
        auto rs=doc["requires"].to<JsonArray>();if(!strcmp(id,"countdown"))rs=doc["optional"].to<JsonArray>();auto gs=p["grants"].to<JsonArray>();for(auto r:requirements){auto q=rs.add<JsonObject>();q["capability"]=r.first;q["api"]=1;if(mode=="no-ui"&&!strcmp(r.first,"ui.scene"))continue;auto g=gs.add<JsonObject>();g["capability"]=r.first;g["api"]=1;g["instance_id"]=r.second;}
        file(name,serialized(doc));
    };
    if(mode=="headless"){app("launcher","launcher.elf",{});app("headless","headless.elf",{{"countdown.control",0}});}
    else {app("launcher","launcher.elf",{{"countdown.control",0}});app("countdown","countdown.elf",{{"ui.scene",0},{"countdown.control",0},{"storage.app-data",66}});}
    file("boot.json",serialized(boot));
    // Existing encoded preference: Denver. Stored independently of app presentation.
    std::vector<uint8_t> zone(44);zone[0]='T';zone[1]='Z';zone[2]=1;memcpy(zone.data()+4,"America/Denver",14);uint8_t sum=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)sum^=zone[i];zone[3]=sum;kv[{1,"time_zone"}]=zone;
}
int main(int argc,char**argv){
    assert(argc==4);root=argv[1];bool utc=!strcmp(argv[2],"paper");width=utc?800:240;if(const char* selected=getenv("ALARM_SCENE_COLOR_WIDTH")){char* end=nullptr;unsigned long value=strtoul(selected,&end,10);assert(end!=selected&&!*end&&value>=160&&value<=600);width=static_cast<unsigned>(value);}height=width==800?480:width;format=width==800?1:5;if(const char* selected=getenv("ALARM_SCENE_COLOR_HEIGHT")){char* end=nullptr;unsigned long value=strtoul(selected,&end,10);assert(end!=selected&&!*end&&value>=160&&value<=800);height=static_cast<unsigned>(value);}assert((uint64_t)width*height<=800u*480u);mode=argv[3];configure(utc);
    std::filesystem::create_directory(root+"/appdata");RiscStorage::AppDataFiles data({nullptr,[](void*){return static_cast<uint32_t>(millis);},[](void*){return true;},malloc,free});files=&data;assert(data.configure((root+"/appdata").c_str()));
    RiscBoot::AppDataBackend storage{&data,
        [](void*c,uint32_t n,const char*p,uint32_t*s,uint64_t*r){return static_cast<RiscStorage::AppDataFiles*>(c)->stat(n,p,s,r);},
        [](void*c,uint32_t n,const char*p,uint64_t v,void*b,uint32_t z,uint32_t*s,uint64_t*r){return static_cast<RiscStorage::AppDataFiles*>(c)->read(n,p,v,b,z,s,r);},
        [](void*c,uint32_t n,const char*p,uint64_t v,const void*b,uint32_t z){return static_cast<RiscStorage::AppDataFiles*>(c)->replace(n,p,v,b,z);},
        [](void*c){return static_cast<RiscStorage::AppDataFiles*>(c)->exitSafe();}};
    RiscBoot::KeyValueBackend key_value{nullptr,get,put};key_value.maxBlobSize=RISC_BOUND_KEY_VALUE_V2_BLOB_MAX;RiscBoot::Port port{owner,health,delay,log_line};port.keyValue=&key_value;port.appData=&storage;
    port.controlExitRequested=[](){return exit_pending;};
    port.controlEnd=[](){bool was=exit_pending;exit_pending=false;return was;};
    port.bindPlatforms=[](Runtime&r){return r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,scene_test_hardware(6))&&r.registerRealtime(static_cast<const risc_realtime_control_api_v1*>(scene_test_hardware(7)));};
    {RiscTest::FileSources sourceFiles({root.c_str()});Runtime r(port);assert(sourceFiles.bind(r));if(!r.prepare(root.c_str())){fprintf(stderr,"prepare: %s\n",r.error());return 2;}assert(!scene_test_hardware_io());if(!r.run()){fprintf(stderr,"run: %s\n",r.error());return 3;}assert(!r.retained());}
    if(mode=="no-ui"){assert(frame_count==0&&kv_writes==0&&scene_test_hardware_io()==0);assert(invocations==2&&finis==2&&unloads==2&&launcher_visits==3);scene_test_hardware_closed();}
    else if(mode!="headless"){assert(frame_count>0);assert(invocations==2&&finis==2&&unloads==2&&launcher_visits==3);checkpoint(301,true);scene_test_hardware_closed();}
    else {
        assert(headless_done&&headless_phase==4&&!invocations&&!frame_count&&!scene_test_hardware_io());
        assert(!dlopen((root+"/headless.elf").c_str(),RTLD_NOW|RTLD_NOLOAD));
    }
    if(mode!="no-ui"){auto record=kv.at({3,utc?"timer_utc_cfg":"timer_cfg"});assert(record.size()==32);assert(kv_writes>=1);}
    printf("actual Runtime Countdown %ux%u format%u %s %s PASS inits=%u finis=%u actual-unloads=%u frames=%u KV-writes=%u\n",width,height,format,argv[2],argv[3],invocations,finis,unloads,frame_count,kv_writes);
}
