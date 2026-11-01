#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscUsbHidV1.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <memory>
#include <map>
#include <vector>
#define _Static_assert static_assert
#include "PortableRtcClock.h"
#include "AlarmRecords.h"
#include "AlarmOutputV1.h"
#include <string>
#include "RiscSceneComponentsV1.h"
#include "RiscRuntimeV1.h"
#include "RiscPowerKeyV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscDisplayPowerV1.h"
static std::string mode,activation;
static bool resident_mode,child_active,delayed_mode;
static unsigned host_entries,host_returns,host_unloads;
static uint64_t ready_at;static bool ready;
static bool (*home_observer)(void);
static bool (*alarm_observer)(void);
extern "C" void lora_alarm_observer(bool(*fn)(void)){alarm_observer=fn;}
extern "C" void lora_home_observer(bool(*fn)(void)){home_observer=fn;}
extern "C" void lora_home_event(unsigned e){if(e==1){++host_entries;ready=false;}else if(e==2)++host_returns;else ++host_unloads;}
extern "C" void lora_host_event(unsigned event,int result){if(event==1)++host_entries;else{++host_returns;assert(result==1);}}
static unsigned loads,unloads,calls,starts,stops,frames,nav;
static bool native_lost;
static void provider_io(const char*name){
 assert(!native_lost&&"provider I/O after native retention");++calls;
 if(mode!=std::string("native-")+name&&mode!=std::string("native-")+name+"-fail")return;
 const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r&&r->retain_invocation&&r->retain_invocation());
 assert(!risc_runtime_get_api(1));native_lost=true;
}
static bool held,touch_sub,keyboard_sub,attached,pending,bad_unsubscribe;
static uint8_t pixels[800*480*2];
static uint64_t ms;
static bool paper,contact_held;
static risc_touch_event_v1 touches[32];static unsigned touch_at,touch_count;static uint64_t touch_sequence;
static unsigned panel_width(){return paper?800u:240u;}
static unsigned panel_height(){return paper?480u:240u;}
static const void*app_pointer;
extern "C" void lora_app_event(unsigned n,const void*p){if(n==1){++loads;app_pointer=p;child_active=true;}else{assert(n==2);++unloads;child_active=false;}}
static risc_usb_keyboard_event_v1 queue[512];static unsigned queued,at;
static uint64_t sequence;
static std::string root;
static unsigned alarm_frames,alarm_acks,alarm_writes;static uint64_t alarm_at;
static std::map<std::string,std::vector<uint8_t>> alarm_saved;
static unsigned keys,sleeps,panel_prepares,panel_resumes,pmu_prepares,pmu_resumes,control_frames,gesture_step;
static uint64_t controls_at,editor_at,child_at;static bool radio_active;
static unsigned step,typed,sends,cancels,profile_puts,receives,packet_reads;
static uint8_t profile_bytes[32];
static uint64_t sent_at,pending_until,last_present_at;
extern "C" unsigned lora_radio_event(unsigned event){provider_io(event==2?"radio-send":event==5?"radio-cancel":"radio");if(event==2||event==3)radio_active=true;if(event==5&&mode!="cancel-failure")radio_active=false;if(event==2){++sends;sent_at=ms;}if(event==3)++receives;if(event==8)++packet_reads;if(event==5){++cancels;if(mode=="cancel-failure")return 0;}return 1;}
extern "C" unsigned lora_radio_mode(void){return mode=="malformed-radio"?1:mode=="changing-radio"?2:mode=="receive-controls"?3:0;}
static int32_t kv_get(void*,uint32_t ns,const char*key,void*out,uint32_t cap,uint32_t*n){provider_io("kv-get");*n=0;
 if(ns==3&&!strcmp(key,"timer_cfg")){uint32_t duration=mode=="text-idle-alarm"?70:10;alarm_config cfg={1,duration,0,duration,ALARM_KIND_COUNTDOWN,1};assert(cap>=32);alarm_config_encode(&cfg,(uint8_t*)out);*n=32;return 0;}
 if(ns==4){auto it=alarm_saved.find(key);if(it==alarm_saved.end())return -1;assert(cap>=it->second.size());*n=it->second.size();memcpy(out,it->second.data(),*n);return 0;}if(ns==9&&!strcmp(key,"lora_profile")){assert(cap>=32);memcpy(out,profile_bytes,32);*n=mode=="malformed-profile"?31:32;return 0;}return -1;}
static int32_t kv_put(void*,uint32_t ns,const char*key,const void*data,uint32_t n){provider_io("kv-put");if(ns==4){assert(n==32);alarm_occurrence occ{};assert(alarm_occurrence_decode(&occ,(const uint8_t*)data,n,ALARM_KIND_COUNTDOWN));++alarm_writes;alarm_saved[key]=std::vector<uint8_t>((const uint8_t*)data,(const uint8_t*)data+n);return 0;}assert(ns==9&&!strcmp(key,"lora_profile")&&n==32);assert(!memcmp(data,profile_bytes,32));++profile_puts;return 0;}
static void seed_profile(){memset(profile_bytes,0,32);profile_bytes[0]='L';profile_bytes[1]='M';profile_bytes[2]=2;profile_bytes[3]=3;auto put=[](unsigned at,uint32_t v){for(unsigned i=0;i<4;i++)profile_bytes[at+i]=(uint8_t)(v>>(8*i));};put(4,915000000);put(8,125000);profile_bytes[12]=8;profile_bytes[14]=7;profile_bytes[15]=5;profile_bytes[16]=32;profile_bytes[17]=1;uint32_t h=2166136261u;for(unsigned i=0;i<28;i++)h=(h^profile_bytes[i])*16777619u;put(28,h);}

static bool exit_requested,back_requested;
static int(*hit_point)(unsigned,unsigned,int*,int*);
extern "C" void lora_hit_register(int(*p)(unsigned,unsigned,int*,int*)){hit_point=p;}
static int tap(unsigned kind,unsigned action){
 if(!hit_point||touch_at!=touch_count||pending)return 0;
 int x=0,y=0;int found=hit_point(kind,action,&x,&y);if(!found)return 0;
 touch_at=touch_count=0;
 unsigned count=found>=3?3:2;
 for(unsigned i=0;i<count;i++){auto&e=touches[touch_count++];e={};e.id=1;e.x=found>=3?120:x;e.y=found>=3?(i?(found==3?60:180):(found==3?180:60)):y;e.kind=i==0?RISC_TOUCH_EVENT_DOWN:i==count-1?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_MOVE;}
 return found;
}
static uint64_t last_input;
static unsigned configurations,updates,injected,delivered,configure_at_injection,snapshots;
extern "C" void lora_scene_trace(unsigned event){
 if(event==1){++configurations;if(injected)assert(configurations==configure_at_injection);}
 if(event==2)++updates;
 if(event==3)++snapshots;
 if(event==2&&mode=="update-input-boundary"&&step==1&&!injected){assert(snapshots);injected=1;configure_at_injection=configurations;exit_requested=true;assert(tap(0,1)==1);}
}

static void(*probe)(risc_components_document_v1*,uint32_t*);
extern "C" void lora_probe_register(void(*p)(risc_components_document_v1*,uint32_t*)){probe=p;}
static void observe(risc_components_document_v1*d,uint32_t*f){assert(probe);probe(d,f);}
static uint32_t drive(){
 if(!child_active||ms-last_input<150||pending||!frames)return 0;
 risc_components_document_v1 d{};uint32_t f=0;observe(&d,&f);
 if(!d.node_count)return 0;
 last_input=ms;
 if(mode=="exit"||mode=="no-storage"||mode=="native-info"||mode=="native-status")return RISC_NAV_HOME;
 if(mode=="short-key")return 0;
 if(mode=="long-key")return keys?RISC_NAV_HOME:0;
 if(mode=="text-short-key"&&keys&&!keyboard_sub){assert(!typed);step=4;return RISC_NAV_HOME;}
 if(mode=="controls"){if(!control_frames||(home_observer&&home_observer()))return 0;return RISC_NAV_HOME;}
 if(mode=="idle"){if(!sleeps)return 0;return RISC_NAV_HOME;}
 if(mode=="alarm"){if(!alarm_acks)return 0;return RISC_NAV_HOME;}
 if((mode=="text-alarm"||mode=="text-idle-alarm")&&alarm_acks&&!keyboard_sub){assert(!typed);step=4;return RISC_NAV_HOME;}
 unsigned action=0;
 if(mode=="receive-alarm"||mode=="receive-controls"){if(step==0)action=6;else if(mode=="receive-alarm"?(!alarm_acks||ms-alarm_at<2000):(!control_frames||(home_observer&&home_observer())))return 0;else {assert(cancels);return RISC_NAV_HOME;}}
 else if(mode=="short-back")return RISC_NAV_BACK;
 else if(mode=="profile"){
  if(step==0)action=3;
  else if(step==1){if(d.screen_key!=3)return 0;action=9;}
  else {if(d.screen_key!=1)return 0;assert(profile_puts==1);return RISC_NAV_HOME;}
 }else if(mode=="receive-history"){
  if(step==0)action=6;
  else if(step==1){if(!packet_reads)return 0;action=5;}
  else if(step==2){if(d.screen_key!=5)return 0;action=64;}
  else{if(d.screen_key!=6)return 0;std::string message;for(unsigned i=0;i<d.node_count;i++)if(!strcmp(d.nodes[i].label,"Text"))message+=d.nodes[i].text;assert(message==std::string(255,'z'));step=4;return RISC_NAV_HOME;}
 }else if(step==0){assert(d.screen_key==1);action=2;}
 else if(step==1){if(d.screen_key!=2)return 0;action=8;}
 else if(step==2){
  if(!keyboard_sub){if(mode=="no-text"&&d.screen_key==2){step=4;return RISC_NAV_HOME;}return 0;}
  if(mode=="text-alarm"||mode=="text-idle-alarm")return 0;
  if(mode=="text-idle"&&!sleeps)return 0;
  if(mode=="text-short-key"&&!keys)return 0;
  if(!typed){for(unsigned i=0;i<240;i++)queue[queued++]={++sequence,7,3,4,0,0};typed=240;return 0;}
  if(at<queued)return 0;
  step=3;queue[queued++]={++sequence,7,3,0x28,0,0};return 0;
 }else if(step==3){
  if(keyboard_sub||d.screen_key!=2)return 0;
  std::string draft;for(unsigned i=0;i<d.node_count;i++)if(!strcmp(d.nodes[i].label,"Draft"))draft+=d.nodes[i].text;
  assert(draft==std::string(240,'a'));if(mode=="compose"||mode=="text-idle"||mode=="text-short-key"){step=4;return RISC_NAV_HOME;}action=7;
 }else if(step==4){
  if(mode=="no-radio"||mode=="malformed-radio"||mode=="malformed-profile"){assert(!sends);step=5;return RISC_NAV_HOME;}
  if(!sends||ms-sent_at<2000)return 0;
  assert(sends==1&&cancels);step=5;return RISC_NAV_BACK;
 }else if(step==5){if(d.screen_key!=1)return 0;action=5;
 }else if(step==6){if(d.screen_key!=5)return 0;action=64;
 }else if(step==7){if(d.screen_key!=6)return 0;std::string message;for(unsigned i=0;i<d.node_count;i++)if(!strcmp(d.nodes[i].label,"Text"))message+=d.nodes[i].text;assert(message==std::string(240,'a'));step=8;return RISC_NAV_HOME;
 }else return RISC_NAV_HOME;
 if(tap(0,action)==1)++step;
 return 0;
}
static bool info(void*,risc_display_info_v1*out){provider_io("info");*out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=panel_width();out->height=panel_height();out->supported_formats=RISC_DISPLAY_FORMAT_BIT(5);out->preferred_format=5;return true;}
static bool acquire(void*,uint32_t f,risc_display_surface_v1*out){provider_io("acquire");assert(!held&&f==5);held=true;*out={frames+1,pixels,panel_width(),panel_height(),panel_width()*2,sizeof(pixels),5};return true;}
static void release(void*,uint64_t f){provider_io("release");assert(held&&f);held=false;}
static bool submit(void*,uint64_t f,const risc_display_rect_v1*,size_t n,const risc_display_present_options_v1*,uint64_t*out){provider_io("submit");assert(held&&f&&!n);*out=++frames;held=false;last_present_at=ms;if(alarm_observer&&alarm_observer()){assert(!radio_active&&!keyboard_sub);++alarm_frames;if(!alarm_at)alarm_at=ms;}if(home_observer&&home_observer()){assert(!radio_active&&!keyboard_sub);++control_frames;if(!controls_at)controls_at=ms;}if(mode=="delayed-display"||delayed_mode){pending=true;pending_until=ms+100;}return true;}
static bool status(void*,uint64_t f,risc_display_present_status_v1*out){provider_io("status");assert(!held&&f);if(pending&&ms>=pending_until)pending=false;out->state=pending?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool brightness(void*,uint16_t,uint16_t){return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),nullptr,info,acquire,release,submit,status,nullptr,brightness};
static uint64_t subscribe(void*){provider_io("subscribe");assert(!touch_sub);touch_sub=true;return 1;}
static bool unsubscribe(void*,uint64_t s){provider_io("unsubscribe");assert(s==1&&touch_sub);if(bad_unsubscribe)return false;touch_sub=false;return true;}
static bool touch_poll(void*,size_t){provider_io("touch_poll");ms+=10;if(touch_at==touch_count){uint32_t intent=exit_requested?(uint32_t)RISC_NAV_HOME:back_requested?(uint32_t)RISC_NAV_BACK:drive();if(intent==RISC_NAV_HOME){exit_requested=true;(void)tap(0,1);}else if(intent==RISC_NAV_BACK){back_requested=tap(0,1)!=1;}}return true;}
static int32_t next(void*,uint64_t s,risc_touch_event_v1*out){provider_io("next");assert(s==1&&touch_sub);if((mode=="controls"||(mode=="receive-controls"&&(radio_active||gesture_step)))&&child_active&&gesture_step<3&&ms-child_at>500&&(gesture_step||(!pending&&ms-last_present_at>200))){++gesture_step;*out={};out->sequence=++touch_sequence;out->timestamp_ms=ms;out->id=1;out->x=120;out->y=gesture_step==1?2:80;out->kind=gesture_step==1?RISC_TOUCH_EVENT_DOWN:gesture_step==2?RISC_TOUCH_EVENT_MOVE:RISC_TOUCH_EVENT_UP;return 1;}if(touch_at==touch_count)return 0;*out=touches[touch_at++];out->sequence=++touch_sequence;out->timestamp_ms=ms;if(injected&&out->kind==RISC_TOUCH_EVENT_UP&&!delivered)++delivered;return 1;}
static bool snapshot(void*,risc_touch_snapshot_v1*out){provider_io("snapshot");*out={};out->width=paper?480:240;out->height=paper?800:240;out->sequence=touch_sequence;out->contact_count=contact_held?1:0;
 if(resident_mode&&ready&&!loads&&ms-ready_at>=100&&ms-ready_at<300){out->contact_count=1;out->contacts[0]={1,0,(uint16_t)(ms-ready_at<180?120:60),120};}if(controls_at&&ms-controls_at>=650&&ms-controls_at<750){out->contact_count=1;out->contacts[0]={1,0,120,230};}return mode!="native-snapshot-fail";}
static const risc_touch_api_v1 touch={1,sizeof(touch),nullptr,subscribe,unsubscribe,touch_poll,next,snapshot};
static bool navigation_poll(void*,risc_input_navigation_frame_v1*out){provider_io("navigation_poll");*out={};if(nav){out->pressed=nav;nav=0;++delivered;}else out->pressed=drive();return true;}
static bool foreground(void*,const risc_input_foreground_v1*,size_t){provider_io("foreground");return true;}
static bool reset(void*){provider_io("reset");nav=0;return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),nullptr,navigation_poll,foreground,reset};
static uint64_t now(void*){return ms;}
static void sleep(void*,uint32_t n){ms+=n;}
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),nullptr,now,sleep};
static uint64_t keyboard_subscribe(void*,uint64_t){provider_io("keyboard_subscribe");assert(!keyboard_sub);keyboard_sub=true;if(!editor_at)editor_at=ms;return 2;}
static bool keyboard_unsubscribe(void*,uint64_t s){provider_io("keyboard_unsubscribe");assert(s==2&&keyboard_sub);keyboard_sub=false;return true;}
static bool keyboard_poll(void*,size_t){provider_io("keyboard_poll");return true;}
static int32_t keyboard_next(void*,uint64_t s,risc_usb_keyboard_event_v1*out){provider_io("keyboard_next");assert(s==2&&keyboard_sub);if(at==queued)return 0;*out=queue[at++];return 1;}
static bool keyboard_snapshot(void*,risc_usb_keyboard_state_v1*out,size_t*count){provider_io("keyboard_snapshot");*count=attached?1:0;if(attached){*out={};out->device=7;out->connected=1;}return true;}
static const risc_usb_keyboard_api_v1 keyboard={1,sizeof(keyboard),nullptr,keyboard_subscribe,keyboard_unsubscribe,keyboard_poll,keyboard_next,keyboard_snapshot};
static bool key_events(void*,uint32_t*out){provider_io("power-key");if(alarm_observer&&alarm_observer()&&alarm_at&&ms-alarm_at>100&&!alarm_acks){assert(!held&&!pending&&!radio_active&&!keyboard_sub);*out=2;++alarm_acks;return true;}if(child_active&&!child_at)child_at=ms;*out=0;if(child_active&&!keys&&(((mode=="short-key"||mode=="long-key")&&ms-child_at>500)||(mode=="text-short-key"&&keyboard_sub&&ms-editor_at>300))){*out=mode=="long-key"?1:2;++keys;}return true;}
static const risc_power_key_api_v1 power_key={1,sizeof(power_key),nullptr,key_events};
static bool panel_prepare(void*){provider_io("panel-prepare");assert(!held&&!pending&&!radio_active&&!keyboard_sub);++panel_prepares;return true;}
static bool panel_resume(void*){provider_io("panel-resume");++panel_resumes;return true;}
static bool pmu_prepare(void*){provider_io("pmu-prepare");assert(panel_prepares==panel_resumes+1);++pmu_prepares;return true;}
static bool pmu_resume(void*){provider_io("pmu-resume");++pmu_resumes;return true;}
static int32_t light(void*,risc_light_sleep_result_v1*out){provider_io("light-sleep");assert(!held&&!pending&&!radio_active&&!keyboard_sub);++sleeps;if(mode=="text-idle-alarm")ms+=15000;out->wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO;return RISC_LIGHT_SLEEP_OK;}
static int32_t timed_light(void*c,uint32_t,risc_light_sleep_result_v1*out){return light(c,out);}
static bool wake_pending(void*,bool*out){*out=false;return true;}
static const risc_power_sleep_api_v1 power_sleep={1,sizeof(power_sleep),nullptr,pmu_prepare,pmu_resume,light,nullptr,timed_light,wake_pending,nullptr,nullptr,nullptr};
static const risc_display_power_api_v1 display_power={1,sizeof(display_power),nullptr,panel_prepare,panel_resume,nullptr,nullptr};
static bool rtc_read(void*,twatch_rtc_time_v1*out){provider_io("rtc");*out={2000,1,1,6,(uint8_t)(ms/3600000),(uint8_t)((ms/60000)%60),(uint8_t)((ms/1000)%60)};return true;}
static const twatch_rtc_api_v1 rtc={2,sizeof(rtc),nullptr,rtc_read,nullptr,nullptr,nullptr};
static bool haptic_effect(void*,uint8_t){provider_io("haptic");assert(!radio_active&&!keyboard_sub&&!held&&!pending);return true;}
static bool output_stop(void*){provider_io("output-stop");return true;}
static bool audio_open(void*,uint32_t,uint8_t){provider_io("audio-open");return true;}
static bool audio_write(void*,const int16_t*,size_t){provider_io("audio-write");return true;}
static const twatch_haptic_api_v1 haptic={1,sizeof(haptic),nullptr,haptic_effect,output_stop};
static const twatch_audio_out_api_v1 audio={1,sizeof(audio),nullptr,audio_open,audio_write,nullptr,output_stop,output_stop};
extern "C" const void*text_runtime_provider(const char*name){if(!strcmp(name,"haptic.effect"))return &haptic;if(!strcmp(name,"audio.output"))return &audio;if(!strcmp(name,"rtc.clock"))return &rtc;if(!strcmp(name,"input.power-key"))return &power_key;if(!strcmp(name,"power.sleep"))return &power_sleep;if(!strcmp(name,"display.power"))return &display_power;if(!strcmp(name,"display.output"))return &display;if(!strcmp(name,"input.touch.raw"))return &touch;if(!strcmp(name,"input.navigation"))return &navigation;if(!strcmp(name,"usb.hid.keyboard"))return &keyboard;assert(0);return nullptr;}
extern "C" bool text_runtime_provider_event(const char*name,unsigned event){if(event==1)++starts;else if(event==2)++stops;else if(event==3){if(!strcmp(name,"input.power-key"))return true;if(!strcmp(name,"power.sleep"))return pmu_prepares==pmu_resumes;if(!strcmp(name,"display.power"))return panel_prepares==panel_resumes;if(!strcmp(name,"display.output"))return !held;if(!strcmp(name,"input.touch.raw"))return !touch_sub;if(!strcmp(name,"usb.hid.keyboard"))return !keyboard_sub;}return true;}
static int64_t native_epoch;static uint64_t native_seed_ms;
static int32_t native_time_read(void*,risc_realtime_snapshot_v1*out){provider_io("native-time-read");*out={sizeof(*out),native_epoch?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,native_epoch?native_epoch+(int64_t)((ms-native_seed_ms)/1000):0,(uint32_t)(native_epoch?((ms-native_seed_ms)%1000)*1000000:0),0,ms*1000,ms*1000};return RISC_REALTIME_OK;}
static int32_t native_time_seed(void*,int64_t epoch,uint32_t){provider_io("native-time-seed");native_epoch=epoch;native_seed_ms=ms;return RISC_REALTIME_OK;}
static const risc_realtime_control_api_v1 native_time={1,sizeof(native_time),nullptr,native_time_read,native_time_seed};
int main(int argc,char**argv){
 assert(argc==4);root=argv[1];mode=argv[2];activation=argv[3];attached=true;
 if(mode.rfind("resident-",0)==0){resident_mode=true;mode=mode.substr(9);}
 if(mode.size()>8&&mode.substr(mode.size()-8)=="-delayed"){delayed_mode=true;mode.resize(mode.size()-8);}
 bool retained=mode.rfind("native-",0)==0||mode=="cancel-failure"||mode=="changing-radio";seed_profile();
 RiscBoot::Port port{[](){return true;},[](risc_runtime_health_v1*h){h->uptime_ms=ms;return !unloads;},[](uint32_t n){ms+=n;if(ms>=180000)fprintf(stderr,"timeout step=%u gesture=%u controls=%u radio=%d cancels=%u frames=%u touch=%d keys=%u\n",step,gesture_step,control_frames,radio_active,cancels,frames,touch_sub,keys);assert(ms<180000);},[](const char*s){fprintf(stderr,"Runtime: %s\n",s);if(!loads&&!ready&&strstr(s,"ready")){ready=true;ready_at=ms;}return true;}};
 RiscBoot::KeyValueBackend backend{nullptr,kv_get,kv_put};if(mode!="no-storage")port.keyValue=&backend;
 port.monotonicMs=[](uint32_t*out){*out=(uint32_t)ms;return true;};
 port.providerStorageSafe=[](){return !native_lost;};
 port.retainedDelay=[](uint32_t){};
 port.bindPlatforms=[](RiscBoot::Runtime&r){return r.registerRealtime(&native_time)&&r.registerPlatform("platform.clock",1,RiscBoot::Runtime::Scope::Global,0,&clock_api);};
 auto runtime=std::make_unique<RiscBoot::Runtime>(port);RiscTest::FileSources files({root});assert(files.bind(*runtime));if(!runtime->prepare(argv[1])){fprintf(stderr,"prepare: %s\n",runtime->error());abort();}bool ran=runtime->run();
 if(ran==retained){fprintf(stderr,"unexpected Runtime result: %s\n",runtime->error());assert(0);}
 assert(runtime->retained()==retained);Dl_info mapping{};
 if(retained){assert((native_lost||mode=="cancel-failure"||mode=="changing-radio")&&loads==1&&!unloads&&dladdr(app_pointer,&mapping));unsigned before=calls;assert(!runtime->run()&&calls==before);printf("LoRa actual Runtime %s/%s retained PASS\n",activation.c_str(),mode.c_str());fflush(stdout);std::_Exit(0);}
 if(resident_mode)assert(host_entries==2&&host_returns==2&&host_unloads==2);
 assert(loads==1&&unloads==1&&!held&&!touch_sub&&!keyboard_sub);assert(!dladdr(app_pointer,&mapping));runtime.reset();assert(starts==stops);
 if(mode=="update-input-boundary")fprintf(stderr,"boundary injected=%u delivered=%u configs=%u updates=%u snapshots=%u\n",injected,delivered,configurations,updates,snapshots);
 if(mode=="update-input-boundary")assert(injected==1&&delivered==1&&configurations==1&&updates>=1);
 if(mode=="profile")assert(profile_puts==1&&!sends&&!receives);
 if(mode=="receive-history")assert(step==4&&packet_reads==1&&!sends&&cancels);
 if(mode=="send"||mode=="delayed-display")assert(step==8&&sends==1);
 if(mode=="no-radio"||mode=="malformed-radio"||mode=="malformed-profile")assert(step==5&&!sends);
 if(mode=="alarm"||mode=="text-alarm"||mode=="receive-alarm"||mode=="text-idle-alarm"){assert(alarm_frames&&alarm_acks==1&&alarm_writes>=2);alarm_occurrence o{};auto &b=alarm_saved["timer_occ"];assert(alarm_occurrence_decode(&o,b.data(),b.size(),ALARM_KIND_COUNTDOWN)&&o.state==ALARM_OCC_ACKED);}
 if(mode=="short-key"||mode=="long-key"||mode=="text-short-key")assert(keys==1);
 if(mode=="controls"||mode=="receive-controls")assert(gesture_step==3&&control_frames>0);
 if(mode=="text-idle-alarm")assert(!typed&&step==4);
 if(mode=="idle"||mode=="text-idle"||mode=="text-idle-alarm")assert(sleeps==1&&panel_prepares==1&&panel_resumes==1&&pmu_prepares==1&&pmu_resumes==1);
 if(mode=="text-short-key")assert(!typed&&step==4);
 if(mode=="compose"||mode=="text-idle")assert(typed==240&&step==4);
 if(mode=="no-text")assert(!typed&&step==4);
 printf("alarm frames=%u acknowledgements=%u writes=%u\n",alarm_frames,alarm_acks,alarm_writes);
 printf("resident keys=%u sleep=%u controls_frames=%u homes=%u/%u/%u\n",keys,sleeps,control_frames,host_entries,host_returns,host_unloads);
 printf("scene configure=%u updates=%u injected=%u delivered=%u\n",configurations,updates,injected,delivered);
 printf("LoRa actual Runtime %s/%s PASS (%u frames, %u provider calls)\n",activation.c_str(),mode.c_str(),frames,calls);
}
