#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "AudioInputV1.h"
#include "RiscKeyValueV1.h"
#include "observer.h"
#include "RiscResidentAttentionV1.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>
static std::string mode;static uint32_t width,height,format;static uint64_t milliseconds,last_input,present_at,touch_sequence,last_pcm;static unsigned entries,loads,unloads,starts,stops,frames,io,nav_events,phase,writes,mic_reads,mic_opens,mic_closes,reads_while_presenting,text_entries;static bool held,touch_sub,mic_owned,contact,up_pending;static uint16_t tx,ty;static unsigned entry_reads;static spectrum_test_observer observer;static const void*last_mapping;static std::vector<uint8_t>pixels;static std::map<std::string,std::vector<uint8_t>>values;
static unsigned native_barriers,unsafe_barriers,attention_queries,capture_observations,manual_rejections,alarm_policies,attention_stage,pending_attention,entry_opens,entry_closes,entry_policies,entry_rejections,quiet_observations,text_start_reads,editor_reads;
static risc_resident_attention_client_v1 resident_host;
extern "C" bool spectrum_test_resident(void){return mode=="attention";}
static bool native_exit_safe(){native_barriers++;if(mic_owned){unsafe_barriers++;return false;}return true;}
static bool storage_safe(){return true;}
static int32_t host_observe(void*,uint32_t stamp,risc_resident_attention_v1*q){assert(q->struct_size==sizeof(*q)&&q->monotonic_ms==stamp);attention_queries++;if(mic_owned)capture_observations++;q->pending_flags=pending_attention;if(!pending_attention)quiet_observations++;return 0;}
static int32_t host_reject(void*,uint32_t bits){assert(bits&&!(bits&~6u)&&(bits&pending_attention)==bits);manual_rejections++;pending_attention&=~bits;quiet_observations=0;return 0;}
static int32_t host_dispatch(void*,const risc_resident_request_v1*q,risc_resident_reply_v1*r){assert(!mic_owned);r->flags=0;if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){if(pending_attention&RISC_RESIDENT_ATTENTION_ALARM)r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;return 0;}assert(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY&&!held&&!touch_sub&&(pending_attention&RISC_RESIDENT_ATTENTION_ALARM));alarm_policies++;pending_attention&=~RISC_RESIDENT_ATTENTION_ALARM;attention_stage=alarm_policies-entry_policies==1?4:5;return 0;}
extern "C" void spectrum_test_host(){const risc_runtime_api_v1*api=risc_runtime_get_api(1);assert(api);resident_host={};resident_host.base.struct_size=sizeof(resident_host);assert(api->resident_shell(&resident_host.base)&&risc_resident_attention_available_v1(&resident_host));risc_resident_callbacks_v1 cb{1,sizeof(cb),nullptr,host_dispatch,nullptr,nullptr};assert(resident_host.base.register_shell(resident_host.base.invocation,&cb)==0);risc_resident_attention_callbacks_v1 ac{1,sizeof(ac),RISC_RESIDENT_ATTENTION_TAG,RISC_RESIDENT_ATTENTION_ALL,nullptr,host_observe,host_reject};assert(resident_host.register_attention(resident_host.base.invocation,&ac)==0);for(unsigned i=0;i<2;i++){risc_resident_result_v1 result{};result.struct_size=sizeof(result);assert(resident_host.base.run_foreground(resident_host.base.invocation,"default.elf",&result)==0);assert(!mic_owned&&!held&&!touch_sub);}}
static void endpoint(){io++;}
extern "C" void spectrum_test_mapping(unsigned event,spectrum_test_observer fn){if(event==1){loads++;observer=fn;last_mapping=(const void*)fn;}else{assert(event==2&&observer==fn);unloads++;observer=nullptr;}}
extern "C" unsigned spectrum_test_entry(){entries++;phase=0;last_input=milliseconds;entry_reads=mic_reads;last_pcm=milliseconds;entry_opens=mic_opens;entry_closes=mic_closes;entry_policies=alarm_policies;entry_rejections=manual_rejections;attention_stage=pending_attention=quiet_observations=0;assert(!held&&!touch_sub&&!mic_owned);return entries;}
static bool info(void*,risc_display_info_v1*out){endpoint();*out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=width;out->height=height;out->supported_formats=RISC_DISPLAY_FORMAT_BIT(format);out->preferred_format=format;return true;}
static bool acquire(void*,uint32_t f,risc_display_surface_v1*out){endpoint();assert(!held&&f==format);held=true;*out={frames+1,pixels.data(),width,height,format==1?(width+7)/8:width*2,(uint32_t)pixels.size(),format};return true;}
static void release(void*,uint64_t f){endpoint();assert(held&&f);held=false;}
static bool submit(void*,uint64_t f,const risc_display_rect_v1*,size_t n,const risc_display_present_options_v1*,uint64_t*out){endpoint();assert(held&&f&&!n);*out=++frames;present_at=milliseconds;held=false;return true;}
static bool present(void*,uint64_t f,risc_display_present_status_v1*out){endpoint();assert(f&&!held);out->state=milliseconds-present_at<80?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),nullptr,info,acquire,release,submit,present,nullptr,nullptr};
static uint64_t subscribe(void*){endpoint();assert(!touch_sub);touch_sub=true;return 1;}
static bool unsubscribe(void*,uint64_t id){endpoint();assert(id==1&&touch_sub&&!contact);touch_sub=false;return true;}
static bool touch_poll(void*,size_t){endpoint();return true;}
static int32_t next(void*,uint64_t id,risc_touch_event_v1*e){endpoint();assert(id==1&&touch_sub);if(!observer)return 0;spectrum_test_view v{};observer(&v);if(up_pending){up_pending=false;contact=false;*e={++touch_sequence,milliseconds,RISC_TOUCH_EVENT_UP,1,tx,ty};last_input=milliseconds;phase++;return 1;}if(mode=="attention"&&attention_stage<4)return 0;if(!v.canvas||milliseconds-last_input<120)return 0;if(phase==1&&v.transforms>=3){tx=width/2;ty=height/2;}else if(phase==2&&v.cursor){tx=width*5/8;ty=height-5*v.scale;}else return 0;contact=up_pending=true;*e={++touch_sequence,milliseconds,RISC_TOUCH_EVENT_DOWN,1,tx,ty};return 1;}
static bool snapshot(void*,risc_touch_snapshot_v1*out){endpoint();*out={};out->width=width;out->height=height;out->sequence=touch_sequence;out->timestamp_ms=milliseconds;if(contact){out->contact_count=1;out->contacts[0]={1,0,tx,ty};}return true;}
static const risc_touch_api_v1 touch={1,sizeof(touch),nullptr,subscribe,unsubscribe,touch_poll,next,snapshot};
static unsigned focus_action(const spectrum_test_view&v){for(unsigned i=0;i<v.document.node_count;i++)if(v.document.nodes[i].id==v.navigation.focus[0])return v.document.nodes[i].action;return 0;}
static bool navigation_poll(void*,risc_input_navigation_frame_v1*out){endpoint();*out={};if(!observer||milliseconds-last_input<120)return true;spectrum_test_view v{};observer(&v);unsigned button=0;
 if(mode=="attention"){
  if(attention_stage==1&&manual_rejections-entry_rejections==1&&v.canvas&&phase==1){assert(mic_opens-entry_opens==1&&mic_closes==entry_closes);button=RISC_NAV_BACK;phase=5;}
  else if(attention_stage==1&&v.scene&&phase==5&&quiet_observations>=2){pending_attention=RISC_RESIDENT_ATTENTION_CONTROLS;attention_stage=2;return true;}
  else if(attention_stage==2&&manual_rejections-entry_rejections==2&&v.canvas){assert(mic_owned&&mic_opens-entry_opens==1&&mic_closes==entry_closes);pending_attention=RISC_RESIDENT_ATTENTION_ALARM;attention_stage=3;phase=1;return true;}
  else if(attention_stage==5&&v.scene){assert(mic_owned&&mic_opens-entry_opens==3&&mic_closes-entry_closes==2);button=RISC_NAV_HOME;}
 }
 if(!button&&phase==0&&v.canvas&&frames){button=RISC_NAV_CONFIRM;phase=1;}
 else if(!button&&phase==3&&v.scene){unsigned target=5;button=focus_action(v)==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;}
 else if(!button&&v.text){if(phase!=4){phase=4;text_entries++;text_start_reads=mic_reads;last_input=milliseconds;return true;}if(mode=="attention"){if(mic_reads-text_start_reads>=16&&attention_stage==4){pending_attention=RISC_RESIDENT_ATTENTION_ALARM;return true;}}else if(mic_reads-entry_reads>40)button=RISC_NAV_HOME;}
 if(button){out->pressed=out->released=button;last_input=milliseconds;nav_events++;}return true;}
static bool foreground(void*,const risc_input_foreground_v1*,size_t){endpoint();return true;}
static bool nav_reset(void*){endpoint();return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),nullptr,navigation_poll,foreground,nav_reset};
static bool mic_open(void*,uint32_t rate){endpoint();assert(rate==16000&&!mic_owned);mic_owned=true;mic_opens++;return true;}
static bool mic_read(void*,int16_t*p,size_t n,size_t*got){endpoint();assert(mic_owned&&n==256);if(milliseconds-last_pcm<16){*got=0;return true;}last_pcm=milliseconds;mic_reads++;if(phase==4)editor_reads++;if(milliseconds-present_at<80)reads_while_presenting++;for(size_t i=0;i<n;i++)p[i]=(i%16<8)?1000:-1000;*got=n;return true;}
static bool mic_close(void*){endpoint();assert(mic_owned);mic_owned=false;mic_closes++;return true;}
static const twatch_audio_in_api_v1 microphone={1,sizeof(microphone),nullptr,mic_open,mic_read,nullptr,mic_close};
static int32_t kv_get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){endpoint();assert(ns==7);auto it=values.find(k);if(it==values.end()){*n=0;return RISC_KEY_VALUE_NOT_FOUND;}*n=it->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;std::memcpy(b,it->second.data(),*n);return 0;}
static int32_t kv_put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){endpoint();assert(ns==7);writes++;values[k]=std::vector<uint8_t>((const uint8_t*)b,(const uint8_t*)b+n);return 0;}
static RiscBoot::KeyValueBackend keys{nullptr,kv_get,kv_put,RISC_KEY_VALUE_V2_BLOB_MAX};
extern "C" const void*spectrum_test_provider(const char*n){if(!std::strcmp(n,"display.output"))return &display;if(!std::strcmp(n,"input.touch.raw"))return &touch;if(!std::strcmp(n,"input.navigation"))return &navigation;if(!std::strcmp(n,"audio.input"))return &microphone;assert(false);return nullptr;}
extern "C" bool spectrum_test_provider_event(const char*n,unsigned e){if(e==1)starts++;else if(e==2)stops++;else if(e==3){if(!std::strcmp(n,"display.output"))return !held;if(!std::strcmp(n,"input.touch.raw"))return !touch_sub;if(!std::strcmp(n,"audio.input"))return !mic_owned;}return true;}
static uint64_t clock_ms(void*){return milliseconds;}
static void clock_sleep(void*,uint32_t){assert(false);}
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),nullptr,clock_ms,clock_sleep};
int main(int argc,char**argv){assert(argc==6);mode=argv[2];width=std::strtoul(argv[3],nullptr,10);height=std::strtoul(argv[4],nullptr,10);format=std::strtoul(argv[5],nullptr,10);pixels.resize(width*height*2);
RiscBoot::Port port{[](){return true;},[](risc_runtime_health_v1*h){h->uptime_ms=milliseconds;return true;},[](uint32_t n){milliseconds+=n;if(mode=="attention"&&attention_stage==0&&mic_reads-entry_reads>=200){assert(mic_owned&&mic_opens-entry_opens==1&&mic_closes==entry_closes&&quiet_observations);pending_attention=RISC_RESIDENT_ATTENTION_EXIT;attention_stage=1;}if(milliseconds>120000){spectrum_test_view v{};if(observer)observer(&v);std::fprintf(stderr,"Timeout entry%u phase%u frames%u nav%u reads%u page%u scene%llu canvas%llu text%llu flags%u\n",entries,phase,frames,nav_events,mic_reads,v.page,(unsigned long long)v.scene,(unsigned long long)v.canvas,(unsigned long long)v.text,v.flags);std::abort();}},[](const char*){return true;},nullptr,&keys};
if(mode=="attention"){port.appExitSafe=native_exit_safe;port.providerStorageSafe=storage_safe;}port.monotonicMs=[](uint32_t*out){*out=milliseconds;return true;};port.bindPlatforms=[](RiscBoot::Runtime&r){return r.registerPlatform("platform.clock",1,RiscBoot::Runtime::Scope::Global,0,&clock_api);};RiscBoot::Runtime runtime(port);RiscTest::FileSources files({std::string(argv[1])});assert(files.bind(runtime));if(!runtime.prepare(argv[1])){std::fprintf(stderr,"Admission: %s\n",runtime.error());return 1;}if(!runtime.run()){std::fprintf(stderr,"Runtime: %s\n",runtime.error());return 2;}assert(!runtime.retained()&&entries==2&&loads==2&&unloads==2&&!held&&!touch_sub&&!mic_owned&&!writes);Dl_info mapping{};assert(!dladdr(last_mapping,&mapping));if(mode=="no-ui")assert(!io&&!frames&&!starts);else assert(frames&&nav_events&&mic_reads>80&&reads_while_presenting&&text_entries==2&&mic_opens==mic_closes);if(mode=="attention")assert(!unsafe_barriers&&native_barriers&&capture_observations>100&&manual_rejections==4&&alarm_policies==4&&mic_opens==6&&mic_closes==6&&editor_reads>=32);std::printf("Spectrum Runtime %s %ux%u frames%u reads%u while-presenting%u text%u unloads%u PASS\n",mode.c_str(),width,height,frames,mic_reads,reads_while_presenting,text_entries,unloads);}
