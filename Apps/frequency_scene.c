/* One capability-driven Frequency application. Providers own native layout,
 * input and physical sleep. No device/profile dimensions or hardware calls. */
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscResidentIdleV1.h"
#include "RiscPowerKeyV1.h"
#include "RiscKeyValueV1.h"
#include "PortableSleepPolicy.h"
#include "AlarmServiceV1.h"
#include "frequency_session.h"
#include <stdio.h>
#include <string.h>
enum {FG_TOGGLE=1,FG_LESS,FG_MORE,FG_STEP10,FG_STEP100,FG_STEP1000,FG_QUIETER,FG_LOUDER,FG_BACK,FG_ACK,FG_RETRY};
 
static const risc_runtime_api_v1 *rt;
 
static bool terminal,leave,dirty,explicit_refresh,manual_neutral,alarm_modal,alarm_reserved;
 
static risc_runtime_capability_v1 grants[5];
 
static const risc_scene_api_v1 *scene;
 
static const risc_scene_components_api_v1 *components;
 
static const risc_scene_lifecycle_api_v1 *lifecycle;
 
static const twatch_audio_out_api_v1 *audio;
 
static const alarm_service_v1 *alarm;
 
static const risc_power_key_api_v1 *power_key;
 
static const risc_key_value_v1 *settings;
 
static frequency_session synth;
 
static risc_scene_resident_v1 resident;
 
static risc_components_document_v1 document,candidate;
 
static risc_scene_navigation_v1 navigation;
 
static uint64_t session,sequence;
 
static uint32_t activity,idle_ms,flags;
static unsigned pending_checkpoint;
static bool toggle_in_batch,idle_supported;
 
static alarm_status_v1 alarm_status;
 
static char notice[96];
 
static bool alive(void*c){(void)c;
 if(terminal)return false;
 if(risc_runtime_get_api(1)!=rt){terminal=true;
 return false;
 }return true;
 }
static void retain(void*c){(void)c;
 if(!alive(NULL))return;
 terminal=true;
 (void)rt->retain_invocation();
 }
static void say(const char*s){snprintf(notice,sizeof(notice),"%s",s);
 dirty=true;
 }
static bool now_ms(uint32_t*out){if(!alive(NULL))return false;
 bool ok=rt->monotonic_ms(out);
 if(!alive(NULL))return false;
 if(!ok)retain(NULL);
 return ok;
 }
static bool empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation;
 }
static bool acquire(unsigned slot,const char*name,unsigned version,unsigned instance){
 if(!alive(NULL))return false;
 
 risc_runtime_capability_v1*g=&grants[slot];
 if(!empty(g)){retain(NULL);
 return false;
 }
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool found=rt->acquire(name,version,instance,g);
 
 if(!alive(NULL))return false;
 
 if(g->struct_size!=sizeof(*g)||(found?(!g->api||!g->slot||!g->generation):!empty(g))){retain(NULL);
 return false;
 }return found;
 
}
static bool release_all(void){
 for(unsigned i=5;i-->0;){if(!alive(NULL))return false;
 if(empty(&grants[i]))continue;
 risc_runtime_capability_v1 g=grants[i];
 bool ok=rt->release(&g);
 if(!alive(NULL))return false;
 if(!ok||g.struct_size!=sizeof(g)||!empty(&g)){retain(NULL);
 return false;
 }grants[i]=g;
 }return true;
 
}
/* Check raw custody results before the unchanged published timer decoder. */
static int32_t timer_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;
 if(!alive(NULL))return RISC_KEY_VALUE_CONTEXT;
 
 int32_t rc=settings->get(settings->context,k,b,cap,n);
 if(!alive(NULL))return RISC_KEY_VALUE_CONTEXT;
 
 switch(rc){case RISC_KEY_VALUE_OK:case RISC_KEY_VALUE_NOT_FOUND:case RISC_KEY_VALUE_BUFFER_SMALL:case RISC_KEY_VALUE_INVALID:case RISC_KEY_VALUE_IO:case RISC_KEY_VALUE_BUSY:return rc;
 default:retain(NULL);
 return RISC_KEY_VALUE_CONTEXT;
 }
}
static int32_t no_put(void*c,const char*k,const void*b,uint32_t n){(void)c;
 (void)k;
 (void)b;
 (void)n;
 return RISC_KEY_VALUE_INVALID;
 }
static bool load_idle(void){
 idle_ms=PORTABLE_SLEEP_IDLE_MS;
 if(!settings)return true;
 
 risc_key_value_v1 view={.api_version=1,.struct_size=sizeof(view),.get=timer_get,.put=no_put};
 
 (void)portable_sleep_timer_load(&view,false,&idle_ms);
 return alive(NULL);
 
}
/* Background implementation is assembled solely from public Audio-session
 * clients;
  it never applies presets/rules or starts background microphone. */
#include "frequency_background.h"
static bool bg_pause(void){return frequency_background_pause()&&alive(NULL);
 }
static bool stop_tone(const char*reason){if(!frequency_stop(&synth))return false;
 if(reason)say(reason);
 return true;
 }
static bool scene_result(int32_t rc){if(!alive(NULL))return false;
 if(rc==RISC_SCENE_RETAINED){retain(NULL);
 return false;
 }return true;
 }
static bool close_scene(void){
 if(!alive(NULL))return false;
 if(!session)return true;
 
 int32_t rc=scene->close(scene->context,session);
 if(!scene_result(rc))return false;
 
 if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY)return false;
 
 if(rc!=RISC_SCENE_OK){retain(NULL);
 return false;
 }session=0;
 return true;
 
}
static risc_scene_node_v1* node(unsigned kind,const char*label,const char*text,unsigned action){
 unsigned i=candidate.node_count++;
 risc_scene_node_v1*n=&candidate.nodes[i];
 n->id=i+1;
 n->route=1;
 n->kind=kind;
 n->action=action;
 snprintf(n->label,sizeof(n->label),"%s",label);
 snprintf(n->text,sizeof(n->text),"%s",text);
 return n;
 
}
static void declare(void){
 candidate=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=document.revision+1,.root=1,.route_count=1,.screen_key=alarm_modal?2:1,.flags=RISC_COMPONENTS_ROOT_HEADER};
 
 candidate.routes[0]=(risc_scene_route_v1){.id=1,.back_action=FG_BACK};
 snprintf(candidate.routes[0].title,sizeof(candidate.routes[0].title),"%s",alarm_modal?"ALARM":"FREQUENCY");
 
 if(alarm_modal){
  node(RISC_SCENE_TEXT_NODE,alarm_status.state==ALARM_STATE_BLOCKED?"ALARM BLOCKED":"ALARM",alarm_status.label,0);
 
  node(RISC_SCENE_TEXT_NODE,"PLAYBACK","Stopped; Start is required after dismissal",0);
 
  node(RISC_SCENE_ACTION,alarm_status.state==ALARM_STATE_BLOCKED?"RETRY":"DISMISS","",alarm_status.state==ALARM_STATE_BLOCKED?FG_RETRY:FG_ACK);
 return;
 
 }
 char value[64];
 snprintf(value,sizeof(value),"%u Hz / %u%% level",synth.hz,synth.level);
 node(RISC_SCENE_TEXT_NODE,"SINE GENERATOR",value,0);
 
 node(RISC_SCENE_TEXT_NODE,"RANGE","20-7000 Hz / 16 kHz mono PCM",0);
 
 node(RISC_COMPONENT_HEADER_ACTION,synth.playing?"STOP":"START","",FG_TOGGLE)->flags=audio?0:RISC_SCENE_DISABLED;
 
 node(RISC_COMPONENT_HEADER_ACTION,"BACK","",FG_BACK);
 
 snprintf(value,sizeof(value),"%u Hz step",synth.step);
 node(RISC_SCENE_ACTION,"LESS",value,FG_LESS);
 node(RISC_SCENE_ACTION,"MORE",value,FG_MORE);
 
 node(RISC_SCENE_ACTION,"10 Hz step",synth.step==10?"Selected":"",FG_STEP10);
 node(RISC_SCENE_ACTION,"100 Hz step",synth.step==100?"Selected":"",FG_STEP100);
 node(RISC_SCENE_ACTION,"1000 Hz step",synth.step==1000?"Selected":"",FG_STEP1000);
 
 node(RISC_SCENE_ACTION,"LEVEL -","Digital amplitude, not acoustic loudness",FG_QUIETER);
 node(RISC_SCENE_ACTION,"LEVEL +","Keep speaker away from ears",FG_LOUDER);
 
 node(RISC_SCENE_TEXT_NODE,"STATUS",notice,0);
 
 if(!audio)node(RISC_SCENE_TEXT_NODE,"AUDIO","Audio output unavailable",0);
 
 if(!alarm)node(RISC_SCENE_TEXT_NODE,"ALARMS","Alarm service unavailable",0);
 
 if(!resident.enabled)node(RISC_SCENE_TEXT_NODE,"SHARED CONTROLS","Shared physical controls unavailable",0);
 if(!idle_supported)node(RISC_SCENE_TEXT_NODE,"IDLE SLEEP","Automatic physical idle policy unavailable",0);
 
 if(!lifecycle)node(RISC_SCENE_TEXT_NODE,"REFRESH","Automatic refresh unavailable; controls remain explicit",0);
 if(!power_key)node(RISC_SCENE_TEXT_NODE,"POWER KEY","Physical power-key navigation unavailable",0);
 uint32_t background=frequency_background_available();
 if(!(background&1u))node(RISC_SCENE_TEXT_NODE,"CONTEXTS","Background context service unavailable",0);
 if((background&1u)&&!frequency_background_utc_available())node(RISC_SCENE_TEXT_NODE,"MODEL AGING","UTC model aging unavailable",0);
 if(!(background&2u))node(RISC_SCENE_TEXT_NODE,"BROADCAST","Bluetooth telemetry service unavailable",0);
 
}
static bool open_scene(void){
 if(!alive(NULL))return false;
 declare();
 
 if(candidate.screen_key!=document.screen_key)navigation.focus[0]=0;
 
 uint64_t next=0;
 int32_t rc=components->open(scene->context,&candidate,&navigation,&next);
 
 if(!scene_result(rc))return false;
 
 if(rc!=RISC_SCENE_OK){if(next)retain(NULL);
 return false;
 }if(!next){retain(NULL);
 return false;
 }
 session=next;
 sequence=0;
 document=candidate;
 lifecycle=risc_scene_lifecycle_get_v1(scene);
 
 if(lifecycle){rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);
 if(!scene_result(rc))return false;
 if(rc!=RISC_SCENE_OK){lifecycle=NULL;
 say("Automatic controls unavailable");
 }}
 explicit_refresh=dirty=true;
 return true;
 
}
static bool present(void){
 if(!dirty)return true;
 declare();
 candidate.revision=document.revision;
 
 if(!memcmp(&candidate,&document,sizeof(document))){dirty=false;
 return true;
 }candidate.revision=document.revision+1;
 
 int32_t rc=components->update(scene->context,session,&candidate);
 if(!scene_result(rc))return false;
 if(rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN)return true;
 if(rc!=RISC_SCENE_OK){retain(NULL);
 return false;
 }document=candidate;
 dirty=false;
 return true;
 
}
static bool valid_navigation(void){
 if(navigation.api_version!=1||navigation.struct_size!=sizeof(navigation)||navigation.depth!=1||navigation.reserved||navigation.routes[0]!=1)return false;
 for(unsigned i=1;i<RISC_SCENE_MAX_DEPTH;i++)if(navigation.routes[i]||navigation.focus[i])return false;
 if(navigation.focus[0]){
  bool found=false;
  for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].id==navigation.focus[0]&&document.nodes[i].route==1)found=true;
  if(!found)return false;
 }
 return !(flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY));
}
static bool query_idle(void){
 idle_supported=false;
 if(!alive(NULL))return false;
 if(!resident.enabled)return true;
 if(!bg_pause())return false;
 risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_QUERY_IDLE_ONLY,0,0};
 risc_resident_reply_v1 reply={sizeof(reply),0};
 int32_t rc=resident.client.checkpoint(resident.client.invocation,&request,&reply);
 if(!alive(NULL))return false;
 if(reply.struct_size!=sizeof(reply)||reply.flags){retain(NULL);return false;}
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){leave=true;return true;}
 if(rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_DENIED)return true;
 if(rc!=RISC_RESIDENT_OK){retain(NULL);return false;}
 idle_supported=true;return true;
}
static bool checkpoint(unsigned reason){
 if(!alive(NULL))return false;
 if(!pending_checkpoint){
  if(!stop_tone("Stopped - choose Start")||!bg_pause())return false;
  pending_checkpoint=reason;
 }
 reason=pending_checkpoint;
 
 if(!close_scene())return !terminal;
 
 if(resident.enabled){int32_t rc=risc_scene_resident_dispatch_v1(&resident,reason);
 if(!alive(NULL))return false;
 if(rc==RISC_RESIDENT_RETAINED){retain(NULL);
 return false;
 }if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){leave=true;
 return true;
 }if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_DENIED){retain(NULL);
 return false;
 }}
 pending_checkpoint=0;
 if(!load_idle()||!now_ms(&activity))return false;
 manual_neutral=false;
 return open_scene();
 
}
static bool alarm_read(void){
 if(!alive(NULL))return false;
 if(!alarm)return true;
 
 alarm_status_v1 q={.struct_size=sizeof(q)};
 int32_t rc=alarm->status(alarm->context,&q);
 if(!alive(NULL))return false;
 
 if(rc!=ALARM_OK||q.api_version!=1||q.struct_size!=sizeof(q)||q.state>ALARM_STATE_CUE||!memchr(q.label,0,sizeof(q.label))){retain(NULL);
 return false;
 }
 if(q.output_uncertain){retain(NULL);return false;}
 bool reserved=q.state==ALARM_STATE_CUE||q.state==ALARM_STATE_BLOCKED||q.state==ALARM_STATE_ALERT||q.state==ALARM_STATE_DISMISSING||q.occurrence.generation||q.output_uncertain;
 
 bool modal=q.state!=ALARM_STATE_CUE&&(q.state==ALARM_STATE_BLOCKED||q.state==ALARM_STATE_ALERT||q.state==ALARM_STATE_DISMISSING||q.occurrence.generation);
 
 if(modal!=alarm_modal||memcmp(&q,&alarm_status,sizeof(q)))dirty=true;
 
 alarm_status=q;
 alarm_modal=modal;
 alarm_reserved=reserved;
 
 return !reserved||(stop_tone("Stopped for alarm - choose Start")&&bg_pause());
 
}
static bool alarm_step(void){
 if(!alarm||!alarm_read())return !terminal;
 
 int32_t rc=alarm->step(alarm->context);
 if(!alive(NULL))return false;
 
 if(rc==ALARM_OUTPUT||rc>ALARM_PENDING||rc<ALARM_FOREGROUND){retain(NULL);
 return false;
 }
 return alarm_read();
 
}
/* Stop only this scheduler's outputs before releasing its grant. This never
 * acknowledges an occurrence and cannot start an output or perform storage. */
static bool alarm_quiet(void){
 if(!alive(NULL))return false;
 if(!alarm)return true;
 for(unsigned i=0;i<250;i++){
  int32_t rc=alarm->stop_only(alarm->context);
  if(!alive(NULL))return false;
  if(rc==ALARM_OK)return true;
  if(rc!=ALARM_PENDING&&rc!=ALARM_BUSY){retain(NULL);return false;}
  rt->yield_ms(20);
  if(!alive(NULL))return false;
 }
 retain(NULL);return false;
}
static bool action(unsigned a,uint32_t now){
 if(!alive(NULL))return false;
 activity=now;
 explicit_refresh=true;
 
 if(alarm_modal){int32_t rc;
 if(a==FG_ACK&&alarm_status.occurrence.generation)rc=alarm->acknowledge(alarm->context,&alarm_status.occurrence);
 else if(a==FG_RETRY)rc=alarm->refresh(alarm->context);
 else return true;
 if(!alive(NULL))return false;
 if(rc==ALARM_OUTPUT||rc>ALARM_PENDING||rc<ALARM_FOREGROUND){retain(NULL);
 return false;
 }return true;
 }
 switch(a){case FG_TOGGLE:if(synth.playing)return stop_tone("Stopped - choose Start");
 if(alarm_reserved){say("Alarm output reserved");
 return true;
 }if(!bg_pause())return false;
 if(!frequency_start(&synth,audio,now)){if(terminal||synth.terminal)return false;
 say("Audio start unavailable or failed");
 }else say("Sine - stops after 60 seconds");
 break;
 
 case FG_LESS:frequency_edit(&synth,-(int)synth.step,0);
 break;
 case FG_MORE:frequency_edit(&synth,(int)synth.step,0);
 break;
 
 case FG_STEP10:synth.step=10;
 break;
 case FG_STEP100:synth.step=100;
 break;
 case FG_STEP1000:synth.step=1000;
 break;
 
 case FG_QUIETER:frequency_edit(&synth,0,-1);
 break;
 case FG_LOUDER:frequency_edit(&synth,0,1);
 break;
 
 case FG_BACK:if(!stop_tone(NULL))return false;
 leave=true;
 break;
 default:return true;
 }dirty=true;
 return alive(NULL);
 
}
static bool manual(uint32_t now){
 if(!alive(NULL))return false;
 if(!power_key)return true;
 uint32_t bits=0;
 bool ok=power_key->events(power_key->context,&bits);
 if(!alive(NULL))return false;
 
 if(!ok){manual_neutral=false;
 retain(NULL);
 return false;
 }
 bool back=manual_neutral&&bits==RISC_POWER_KEY_SHORT_PRESS;
 manual_neutral=bits==0;
 
 return !back||action(FG_BACK,now);
 
}
static bool handle(const risc_scene_event_v1*e,uint32_t now){
 if(e->struct_size!=sizeof(*e)||!e->sequence||e->sequence<=sequence)return true;
 sequence=e->sequence;
 
 if(e->document_revision!=document.revision)return true;
 
 if((e->kind==RISC_SCENE_SUSPEND_EVENT||e->kind==RISC_SCENE_CONTROLS_EVENT)&&alarm_reserved){
  if(lifecycle){int32_t rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);
   if(!scene_result(rc))return false;
   if(rc!=RISC_SCENE_OK){retain(NULL);return false;}
  }
  return true;
 }
 if(e->kind==RISC_SCENE_SUSPEND_EVENT){if(!stop_tone(NULL))return false;
 leave=true;
 return true;
 }
 if(e->kind==RISC_SCENE_CONTROLS_EVENT)return checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS);
 
 if(e->kind!=RISC_SCENE_ACTION_EVENT)return true;
 
 if(!e->node)return e->action==FG_BACK?action(FG_BACK,now):true;
 
 for(unsigned i=0;i<document.node_count;i++){const risc_scene_node_v1*n=&document.nodes[i];
 if(n->id==e->node&&n->action==e->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))){
 if(e->action==FG_TOGGLE){if(toggle_in_batch)return true;toggle_in_batch=true;}
 return action(e->action,now);
 }
 }return true;
 
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
 
__attribute__((visibility("default"))) void app_main(void){
 rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!rt->retain_invocation||!rt->acquire||!rt->release||!rt->monotonic_ms||!rt->yield_ms)return;
 
 terminal=leave=alarm_modal=alarm_reserved=manual_neutral=false;
 dirty=explicit_refresh=true;
 session=sequence=flags=0;
 pending_checkpoint=0;idle_supported=false;
 memset(grants,0,sizeof(grants));
 scene=NULL;
 components=NULL;
 lifecycle=NULL;
 audio=NULL;
 alarm=NULL;
 power_key=NULL;
 settings=NULL;
 document=(risc_components_document_v1){0};
 alarm_status=(alarm_status_v1){0};
 resident=(risc_scene_resident_v1){0};
 frequency_init(&synth,alive,retain,NULL);
 say("Choose Start / 60 second maximum");
 
 if(acquire(0,RISC_SCENE_CAPABILITY,1,0))scene=grants[0].api;
 if(!alive(NULL))return;
 
 if(!scene||scene->api_version!=1||scene->struct_size<sizeof(*scene)||!scene->next||!scene->snapshot||!scene->close||(components=risc_scene_components_get_v1(scene))==NULL){release_all();
 return;
 }
 if(acquire(1,"audio.output",1,0)){const twatch_audio_out_api_v1*p=grants[1].api;
 if(p->api_version==1&&p->struct_size>=sizeof(*p)&&p->open&&p->write&&p->close)audio=p;
 }if(!alive(NULL))return;
 
 if(acquire(2,ALARM_SERVICE_CAPABILITY,1,0)){const alarm_service_v1*p=grants[2].api;
 if(p->api_version==1&&p->struct_size>=sizeof(*p)&&p->status&&p->step&&p->refresh&&p->acknowledge&&p->stop_only)alarm=p;
 }if(!alive(NULL))return;
 
 if(acquire(3,RISC_POWER_KEY_CAPABILITY,1,0)){const risc_power_key_api_v1*p=grants[3].api;
 if(p->api_version==1&&p->struct_size>=sizeof(*p)&&p->events)power_key=p;
 }if(!alive(NULL))return;
 
 if(acquire(4,RISC_KEY_VALUE_CAPABILITY,1,1)){const risc_key_value_v1*p=grants[4].api;
 if(p->api_version==1&&p->struct_size>=sizeof(*p)&&p->get)settings=p;
 }if(!alive(NULL))return;
 
 if(!frequency_background_init(rt,alive,retain,NULL)||!load_idle()||!now_ms(&activity))return;
 
 if(!risc_scene_resident_bind_v1(rt,&resident)){retain(NULL);
 return;
 }if(!alive(NULL)||!query_idle())return;
 if(leave)goto done;
 
 navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={1}};
 
 if(!open_scene())goto done;
 
 while(alive(NULL)&&!leave){
  if(pending_checkpoint){
   if(!checkpoint(pending_checkpoint))break;
   if(pending_checkpoint){rt->yield_ms(20);continue;}
   if(leave)break;
  }
  uint32_t now;
 if(!now_ms(&now)||!manual(now)||leave)break;
 
  int32_t rc=RISC_SCENE_IDLE;
  toggle_in_batch=false;
  for(unsigned drained=0;drained<32;drained++){
   risc_scene_event_v1 e={.struct_size=sizeof(e)};
   rc=scene->next(scene->context,session,&e);
   if(!scene_result(rc))return;
   if(rc==RISC_SCENE_IDLE)break;
   if(rc!=RISC_SCENE_OK){retain(NULL);return;}
   if(!handle(&e,now))goto done;
   if(leave||pending_checkpoint)break;
  }
  if(leave)break;
  if(pending_checkpoint)continue;

  flags=0;
 rc=scene->snapshot(scene->context,session,&navigation,&flags);
 if(!scene_result(rc))return;
 if(rc!=RISC_SCENE_OK){retain(NULL);
 return;
 }
  if(!valid_navigation()){retain(NULL);return;}
  if(flags&RISC_SCENE_ACTIVITY){activity=now;
 resident.activity=true;
 }
  if(!alarm_read())return;
 
  if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))&&!alarm_step())return;
 
  if(idle_supported&&!alarm_reserved&&(uint32_t)(now-activity)>=idle_ms&&!(flags&RISC_SCENE_INPUT_BUSY)){
   if(!stop_tone("Stopped for inactivity")||!bg_pause())return;
 resident.activity=false;
 
   if(!checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY))break;
 continue;
 
  }
  bool was_playing=synth.playing;
 if(!frequency_step(&synth,now))return;
 if(was_playing&&!synth.playing){say("Stopped - choose Start");
 resident.activity=true;
 }
  if(!frequency_background_step(!alarm_reserved))return;
 
  /* Manual navigation belongs to this application until an actual idle or
   * explicit-controls handoff. Do not poll Home merely because tone is off. */
  if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(lifecycle||explicit_refresh)){if(!present())return;
 explicit_refresh=false;
 }
  rt->yield_ms(synth.playing?1:20);
 
 }
 done:if(!alive(NULL)||!stop_tone(NULL)||!bg_pause()||!alarm_quiet())return;
 
 for(unsigned i=0;session&&i<250;i++){if(close_scene())break;
 if(!alive(NULL))return;
 rt->yield_ms(20);
 if(!alive(NULL))return;
 }
 if(session){retain(NULL);
 return;
 }if(!frequency_background_cleanup()||!alive(NULL))return;
 (void)release_all();
 
}
