/* One semantic application. Providers own layout, input and physical lifecycle.
 * Legacy raw-calendar and UTC records remain separate, never reinterpreted. */
#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "RiscSceneLifecycleV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscKeyValueV1.h"
#include "RiscRealtimeV1.h"
#include "PortableRtcClock.h"
#include "stopwatch_native_utc_core.h"
#undef sw_decode
#undef sw_encode
#include <stdio.h>
#include <string.h>
enum { TOGGLE=1,RESET,RETRY,DOMAIN,LEAVE };
static const risc_runtime_api_v1 *rt;
static const risc_scene_api_v1 *ui;
static const risc_scene_lifecycle_api_v1 *lifecycle;
static bool explicit_refresh;
static risc_scene_resident_v1 resident;
static bool close_scene(void);
static bool open_scene(void);
static const risc_key_value_v1 *kv;
static const risc_realtime_api_v1 *utc;
static const twatch_rtc_api_v1 *raw;
static risc_runtime_capability_v1 grants[4];
static bool held[4],terminal,loaded,uncertain,pending_pause,reset_armed,utc_domain;
static sw_clock clock_state;
static sw_record pending;
static risc_scene_document_v1 document;
static risc_scene_navigation_v1 navigation;
static uint64_t session,sequence;
static uint32_t checked;
static char notice[72];
static void say(const char *s){snprintf(notice,sizeof(notice),"%s",s);}
static bool alive(void){if(terminal)return false;if(risc_runtime_get_api(1)!=rt){terminal=true;return false;}return true;}
static void retain(void){if(!alive())return;terminal=true;(void)rt->retain_invocation();}
static bool result_ok(int32_t rc){if(!alive())return false;switch(rc){case RISC_KEY_VALUE_OK:case RISC_KEY_VALUE_NOT_FOUND:case RISC_KEY_VALUE_BUFFER_SMALL:case RISC_KEY_VALUE_INVALID:case RISC_KEY_VALUE_IO:case RISC_KEY_VALUE_BUSY:return true;default:retain();return false;}}
static bool scene_ok(int32_t rc){if(!alive())return false;if(rc==RISC_SCENE_RETAINED){retain();return false;}return true;}
static bool millis(uint32_t *out){if(!alive())return false;bool ok=rt->monotonic_ms(out);return alive()&&ok;}
static const char *key(void){return utc_domain?"stopwatch_utc":"stopwatch";}
static bool seconds(uint32_t *out){
 if(!alive())return false;
 if(utc_domain){
  if(!utc)return false;
  risc_realtime_snapshot_v1 s={.struct_size=sizeof(s)};
  int32_t rc=utc->read(utc->context,&s);if(!alive())return false;
  if(rc==RISC_REALTIME_CONTEXT){retain();return false;}
  if(rc!=RISC_REALTIME_OK||s.struct_size!=sizeof(s)||s.validity!=RISC_REALTIME_VALID||s.nanoseconds>=1000000000u||s.epoch_seconds<946684800||s.epoch_seconds>2147483647)return false;
  *out=(uint32_t)(s.epoch_seconds-946684800);return true;
 }
 if(!raw)return false;
 twatch_rtc_time_v1 t;bool ok=raw->read(raw->context,&t);if(!alive())return false;
 return ok&&t.weekday<=6&&sw_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,out);
}
/* Only NOT_FOUND establishes absence. A malformed or unreadable existing
 * domain stays selected so recovery is visible rather than an apparent reset. */
static void select_initial_domain(void){
 utc_domain=utc!=NULL;if(!kv)return;
 uint8_t bytes[SW_RECORD_SIZE];uint32_t n=0;
 int32_t rc=kv->get(kv->context,key(),bytes,sizeof(bytes),&n);if(!result_ok(rc)||rc!=RISC_KEY_VALUE_NOT_FOUND)return;
 utc_domain=!utc_domain;n=0;
 rc=kv->get(kv->context,key(),bytes,sizeof(bytes),&n);if(!result_ok(rc))return;
 if(rc==RISC_KEY_VALUE_NOT_FOUND)utc_domain=!utc_domain;
}
static bool read_record(sw_record *record){
 if(!kv){say("STORAGE UNAVAILABLE");return false;}
 uint8_t bytes[SW_RECORD_SIZE];uint32_t n=0;
 int32_t rc=kv->get(kv->context,key(),bytes,sizeof(bytes),&n);if(!result_ok(rc))return false;
 if(rc==RISC_KEY_VALUE_NOT_FOUND){*record=(sw_record){0};return true;}
 if(rc!=RISC_KEY_VALUE_OK){say("STORAGE READ FAILED - RETRY");return false;}
 bool ok=utc_domain?sw_utc_decode(record,bytes,n):sw_decode(record,bytes,n);
 if(!ok)say("SAVED DATA INVALID - RETRY OR RESET");
 return ok;
}
static void restore(void){
 sw_record r;loaded=read_record(&r);if(terminal)return;
 if(!loaded){clock_state.running=false;return;}
 uint32_t sec=0,ms=0;bool ok=seconds(&sec);if(terminal)return;
 if(!millis(&ms)){say("MONOTONIC CLOCK UNAVAILABLE - RETRY");clock_state=(sw_clock){.saved=r,.elapsed_ms=r.elapsed_ms,.approximate=r.approximate,.clock_changed=true};loaded=false;return;}
 sw_restore(&clock_state,&r,ok,sec,ms);pending_pause=false;
 say(clock_state.clock_changed?"CLOCK UNAVAILABLE OR MOVED BACK":clock_state.limit?"99 HOUR LIMIT - RESET":clock_state.approximate?"RECOVERED - APPROXIMATE":"PAUSED");
 checked=ms;
}
static bool save(sw_record r){
 if(!kv){say("STORAGE UNAVAILABLE");return false;}
 uint8_t bytes[SW_RECORD_SIZE],verify[SW_RECORD_SIZE];uint32_t n=0;
 if(utc_domain)sw_utc_encode(&r,bytes);else sw_encode(&r,bytes);
 pending=r;uncertain=true;
 int32_t rc=kv->put(kv->context,key(),bytes,sizeof(bytes));if(!result_ok(rc))return false;
 /* IO is ambiguous. Only exact readback acknowledges the intended record. */
 rc=kv->get(kv->context,key(),verify,sizeof(verify),&n);if(!result_ok(rc))return false;
 if(rc!=RISC_KEY_VALUE_OK||n!=sizeof(bytes)||memcmp(bytes,verify,n)){clock_state.running=false;pending_pause=true;say("SAVE UNCONFIRMED - RETRY");return false;}
 uncertain=false;loaded=true;clock_state.saved=r;return true;
}
static void retry_save(void){if(save(pending)&&!terminal){restore();pending_pause=false;}}
static void toggle(void){
 reset_armed=false;if(uncertain){retry_save();return;}
 if((!loaded||clock_state.clock_changed)&&!pending_pause){say("RETRY OR RESET REQUIRED");return;}
 uint32_t ms;if(!millis(&ms)){say("MONOTONIC CLOCK UNAVAILABLE");return;}sw_tick(&clock_state,ms);
 if(clock_state.running||pending_pause){
  sw_record r={clock_state.elapsed_ms,0,false,clock_state.approximate};
  if(save(r)){clock_state.running=false;clock_state.clock_changed=false;pending_pause=false;say(r.approximate?"PAUSED - APPROXIMATE":"PAUSED");}
 }else{
  uint32_t sec=0;if(clock_state.elapsed_ms==SW_MAX_MS){say("99 HOUR LIMIT - RESET");return;}
  if(!seconds(&sec)){if(!terminal)say("CLOCK UNAVAILABLE - RETRY");return;}
  if(!millis(&ms))return;
  sw_record r={clock_state.elapsed_ms,sec,true,clock_state.approximate};
  if(save(r)){clock_state.running=true;clock_state.last_ms=clock_state.ms_anchor=ms;clock_state.rtc_anchor=sec;checked=ms;say(r.approximate?"RUNNING - APPROXIMATE":"RUNNING");}
 }
}
static void reset(void){
 if(uncertain){say("SAVE UNCONFIRMED - RETRY");return;}
 if(!reset_armed){reset_armed=true;say("SELECT RESET AGAIN TO CLEAR");return;}
 reset_armed=false;
 if(save((sw_record){0})){pending_pause=false;restore();if(!terminal)say("RESET");}
}
static void tick(void){
 uint32_t ms;if(!millis(&ms)){if(clock_state.running&&loaded){clock_state.running=false;clock_state.clock_changed=true;pending_pause=true;say("MONOTONIC CLOCK LOST - SAVE PAUSE");}return;}
 bool running=clock_state.running;sw_tick(&clock_state,ms);
 if(running&&!clock_state.running)say("99 HOUR LIMIT - RESET");
 if(clock_state.running&&(uint32_t)(ms-checked)>=1000){
  checked=ms;uint32_t sec=0;bool ok=seconds(&sec);if(terminal)return;
  if(!sw_clock_consistent(&clock_state,ok,sec,ms)){clock_state.running=false;clock_state.clock_changed=true;pending_pause=true;say(ok?"CLOCK CHANGED - SAVE PAUSE":"CLOCK LOST - SAVE PAUSE");}
 }
}
static void node(risc_scene_document_v1 *d,uint32_t id,uint32_t kind,uint32_t action,const char *label,const char *text,uint32_t flags){
 risc_scene_node_v1 *n=&d->nodes[d->node_count++];*n=(risc_scene_node_v1){.id=id,.route=1,.kind=kind,.action=action,.flags=flags};
 snprintf(n->label,sizeof(n->label),"%s",label);snprintf(n->text,sizeof(n->text),"%s",text);
}
static void declare(risc_scene_document_v1 *d){
 *d=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(*d),.revision=document.revision,.root=1,.route_count=1};
 d->routes[0]=(risc_scene_route_v1){.id=1,.back_action=LEAVE};snprintf(d->routes[0].title,sizeof(d->routes[0].title),"STOPWATCH");
 char value[12];sw_format(clock_state.elapsed_ms,value);
 node(d,1,RISC_SCENE_TEXT_NODE,0,utc_domain?"UTC TIMER":"RAW CLOCK TIMER",value,RISC_SCENE_PRIMARY);
 node(d,2,RISC_SCENE_TEXT_NODE,0,clock_state.running?"RUNNING":loaded?"PAUSED":"STORAGE ERROR",notice,0);
 node(d,3,RISC_SCENE_ACTION,TOGGLE,uncertain?"RETRY SAVE":pending_pause?"SAVE PAUSE":clock_state.running?"PAUSE":"START","",kv?RISC_SCENE_PRIMARY:RISC_SCENE_DISABLED);
 node(d,4,RISC_SCENE_ACTION,RESET,reset_armed?"CONFIRM RESET":"RESET","",RISC_SCENE_DESTRUCTIVE|(!kv||uncertain?RISC_SCENE_DISABLED:0));
 node(d,5,RISC_SCENE_ACTION,RETRY,"RETRY","",0);
 node(d,6,RISC_SCENE_ACTION,DOMAIN,utc_domain?"OPEN RAW CLOCK TIMER":"OPEN UTC TIMER","",uncertain||pending_pause?RISC_SCENE_DISABLED:0);
 if(!(utc_domain?utc!=NULL:raw!=NULL))node(d,7,RISC_SCENE_TEXT_NODE,0,"CLOCK UNAVAILABLE","Saved paused data remains readable",0);
}
static bool present(void){
 risc_scene_document_v1 next;declare(&next);if(!memcmp(&next,&document,sizeof(next)))return true;
 if(document.revision==UINT32_MAX)return false;
 next.revision=document.revision+1;
 int32_t rc=ui->update(ui->context,session,&next);if(!scene_ok(rc)||rc!=RISC_SCENE_OK)return false;document=next;return true;
}
static bool event_valid(const risc_scene_event_v1 *e){
 if(e->struct_size!=sizeof(*e)||e->document_revision!=document.revision||!e->sequence||e->sequence<=sequence)return false;
 sequence=e->sequence;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT||e->kind==RISC_SCENE_CONTROLS_EVENT)return !e->node&&!e->action;
 if(e->kind!=RISC_SCENE_ACTION_EVENT)return false;
 if(!e->node)return e->action==LEAVE;
 for(unsigned i=0;i<document.node_count;i++){const risc_scene_node_v1 *n=&document.nodes[i];if(n->id==e->node)return n->kind==RISC_SCENE_ACTION&&n->action==e->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN));}
 return false;
}
static void reject_handoff(void){
 say("SAVE UNCONFIRMED - RETRY");explicit_refresh=true;
 if(lifecycle){int32_t rc=lifecycle->configure(ui->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!scene_ok(rc))return;if(rc!=RISC_SCENE_OK)retain();}
}
static bool checkpoint(uint32_t reason){
 if(uncertain||pending_pause){reject_handoff();return false;}
 uint32_t flags=0;int32_t rc=ui->snapshot(ui->context,session,&navigation,&flags);if(!scene_ok(rc)||rc!=RISC_SCENE_OK)return true;
 if(!close_scene())return true;
 rc=risc_scene_resident_dispatch_v1(&resident,reason);if(!alive())return true;if(rc==RISC_RESIDENT_RETAINED){retain();return true;}
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return true;
 tick();if(terminal)return true;declare(&document);return !open_scene();
}
static bool handle(const risc_scene_event_v1 *e){
 if(!event_valid(e))return false;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT||e->action==LEAVE){if(uncertain||pending_pause){reject_handoff();return false;}return true;}
 if(e->kind==RISC_SCENE_CONTROLS_EVENT){if(resident.enabled)return checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS);return false;}
 explicit_refresh=true;
 switch(e->action){
 case TOGGLE:toggle();break;
 case RESET:reset();break;
 case RETRY:reset_armed=false;if(uncertain)retry_save();else if(pending_pause)toggle();else if(!clock_state.running)restore();break;
 case DOMAIN:reset_armed=false;utc_domain=!utc_domain;clock_state=(sw_clock){0};restore();break;
 default:break;
 }
 return false;
}
static bool acquire(unsigned index,const char *name,uint32_t api){
 grants[index]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[index])};
 bool ok=rt->acquire(name,api,index==1?2:0,&grants[index]);if(!alive())return false;if(ok&&(grants[index].struct_size!=sizeof(grants[index])||!grants[index].api||!grants[index].slot||!grants[index].generation)){retain();return false;}held[index]=ok;return ok;
}
static void release_all(void){for(unsigned i=4;i>0&&alive();i--)if(held[i-1]){bool ok=rt->release(&grants[i-1]);if(!alive())return;if(!ok||grants[i-1].struct_size!=sizeof(grants[i-1])||grants[i-1].api||grants[i-1].slot||grants[i-1].generation){retain();return;}held[i-1]=false;}}
static bool close_scene(void){
 if(!session)return true;
 for(unsigned i=0;i<250;i++){int32_t rc=ui->close(ui->context,session);if(!scene_ok(rc))return false;if(rc==RISC_SCENE_OK){session=0;return true;}if(rc!=RISC_SCENE_AGAIN){retain();return false;}rt->yield_ms(20);if(!alive())return false;}
 retain();return false;
}
static bool open_scene(void){
 int32_t rc=ui->open(ui->context,&document,&navigation,&session);if(!scene_ok(rc))return false;if(rc!=RISC_SCENE_OK)return false;
 sequence=0;lifecycle=risc_scene_lifecycle_get_v1(ui);explicit_refresh=false;
 if(lifecycle){rc=lifecycle->configure(ui->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!scene_ok(rc))return false;if(rc!=RISC_SCENE_OK)lifecycle=NULL;}
 if(!lifecycle){say("LIVE DISPLAY UNAVAILABLE - SELECT RETRY");explicit_refresh=true;}
 return true;
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!rt->monotonic_ms||!rt->retain_invocation||!rt->acquire||!rt->release||!rt->yield_ms)return;
 terminal=loaded=uncertain=pending_pause=reset_armed=false;memset(held,0,sizeof(held));ui=NULL;kv=NULL;utc=NULL;raw=NULL;session=sequence=0;notice[0]=0;clock_state=(sw_clock){0};document=(risc_scene_document_v1){0};
 if(acquire(0,RISC_SCENE_CAPABILITY,1))ui=grants[0].api;
 if(!alive())return;
 if(!ui||ui->api_version!=1||ui->struct_size<sizeof(*ui)||!ui->open||!ui->update||!ui->next||!ui->close||!ui->snapshot){release_all();return;}
 if(acquire(1,RISC_KEY_VALUE_CAPABILITY,1)){const risc_key_value_v1 *p=grants[1].api;if(p&&p->api_version==1&&p->struct_size>=sizeof(*p)&&p->get&&p->put)kv=p;}
 if(!alive())return;
 if(acquire(2,RISC_REALTIME_CAPABILITY,1)){const risc_realtime_api_v1 *p=grants[2].api;if(p&&p->api_version==1&&p->struct_size>=sizeof(*p)&&p->read)utc=p;}
 if(!alive())return;
 if(acquire(3,TWATCH_RTC_CAPABILITY,2)){const twatch_rtc_api_v1 *p=grants[3].api;if(p&&p->api_version==2&&p->struct_size>=sizeof(*p)&&p->read)raw=p;}
 if(!alive())return;
 if(!risc_scene_resident_bind_v1(rt,&resident)){retain();return;}
 if(!alive())return;
 select_initial_domain();if(terminal)return;restore();if(terminal)return;
 navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={1}};
 declare(&document);document.revision=1;
 if(!open_scene()){if(!terminal&&close_scene())release_all();return;}
 int32_t rc;
 for(;;){
  risc_scene_event_v1 e={.struct_size=sizeof(e)};rc=ui->next(ui->context,session,&e);if(!scene_ok(rc))return;
  if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)break;
  if(rc==RISC_SCENE_OK&&handle(&e))break;
  if(terminal)return;
  tick();if(terminal)return;
  uint32_t flags=0;rc=ui->snapshot(ui->context,session,&navigation,&flags);if(!scene_ok(rc))return;if(rc!=RISC_SCENE_OK)break;
  if(resident.enabled){
   uint32_t work=(uncertain||pending_pause)?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0;
   int32_t rr=risc_scene_resident_poll_work_v1(&resident,flags,work);if(!alive())return;if(rr==RISC_RESIDENT_RETAINED){retain();return;}if(rr==RISC_RESIDENT_EXIT)break;
   if(resident.policy&&!work&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))){if(checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY))break;continue;}
  }
  /* Never invalidate an in-flight raster or held/queued contact. The provider
   * owns input continuity; absent tracking permits explicit refresh only. */
  if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(lifecycle||explicit_refresh)){
   if(!present()){if(terminal)return;break;}explicit_refresh=false;
  }
  rt->yield_ms(50);if(!alive())return;
 }
 if(close_scene())release_all();
}
