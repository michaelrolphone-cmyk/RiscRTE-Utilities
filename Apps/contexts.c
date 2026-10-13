/* Contexts foreground: semantic NOVA scenes and the shared detection service.
 * The historical portable renderer is never a shipping fallback. */
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscCivilClockV1.h"
#ifndef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
#include "SceneKeyboardV1.h"
#endif
#include "RiscRuntimeV1.h"
#include "RiscRuntimeHostControlV1.h"
#if defined(PORTABLE_NOVA_UI) || defined(PORTABLE_CONTEXTS_PAPER)
#error "Contexts uses ui.scene. Build with scripts/build_contexts_scene.py; do not link the portable display adapter."
#endif
#include <stdio.h>
#include <string.h>
static bool cs_terminal;
#ifndef CONTEXTS_ALARM_API
#define CONTEXTS_ALARM_API 1u
#endif
static void cs_retain(void);
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
static bool cs_alive(void);
#define PORTABLE_CONTEXTS_LIVE() cs_alive()
#endif
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() (!cs_terminal)
#define PORTABLE_CONTEXTS_CLEANUP_FAILURE() cs_retain()
#include "PortableContextsClient.h"
#include "PortableApps.h"
#include "PortableAppCatalog.h"
#define CONTEXTS_RUNTIME_CATALOG 1
#include "RiscBatteryGaugeV1.h"
#include "PortableContextActions.h"
static const risc_runtime_api_v1 *ctx_runtime;
static const contexts_service_v1 *ctx_service;
static const risc_scene_api_v1 *cs_scene;
static const risc_scene_components_api_v1 *cs_components;
static risc_runtime_capability_v1 cs_scene_grant,cs_civil_grant,cs_battery_grant,cs_alarm_grant;
static portable_contexts_client cs_client;
static risc_scene_resident_v1 cs_resident;
static uint64_t cs_session;
static unsigned cs_source_mask=CONTEXTS_ALL,ctx_model_source;
static bool ctx_enabled,ctx_dirty,ctx_exit,ctx_fp_valid;
static bool cs_training_stopped;
static uint32_t cs_learning_at,cs_learning_draw_at;
static contexts_status_v1 ctx_status;
static contexts_fingerprint_status_v1 ctx_fp;
static const char *ctx_message;
static uint16_t cs_active;
static char cs_launch[RISC_APP_CATALOG_ROUTE_MAX];
static portable_app_catalog cs_catalog;
static portable_app_catalog_launch cs_launch_ticket;
static contexts_workflow_v1 cs_launch_report;
/* Native exit is persistent and owned by the host. Normal cleanup may run,
 * but editor state, shared keyboard, or a queued launch cannot defer it. */
static bool cs_host_exit(void){
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
 bool requested=risc_runtime_exit_requested_v1(ctx_runtime);
 if(!cs_alive())return true;
 if(!requested)return false;
#else
 if(!risc_runtime_exit_requested_v1(ctx_runtime))return false;
#endif
 ctx_exit=true;cs_launch[0]=0;return true;
}
static void ctx_note(const char*s){ctx_message=s;ctx_dirty=true;}
static void cs_retain(void){if(!cs_terminal){cs_terminal=true;if(ctx_runtime&&ctx_runtime->retain_invocation)(void)ctx_runtime->retain_invocation();}}
static bool cs_alive(void){if(cs_terminal)return false;if(!risc_runtime_get_api(1)){cs_terminal=true;return false;}return true;}
static bool cs_release(risc_runtime_capability_v1*g){if(!cs_alive())return false;if(g->api&&!ctx_runtime->release(g)){cs_retain();return false;}memset(g,0,sizeof(*g));return cs_alive();}
#include "contexts_catalog.inc"
static bool cs_backend_open(void){
 if(cs_client.api)return true;
 if(!portable_contexts_open(&cs_client,ctx_runtime)){
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
  /* Failed load may retain app-data custody while Runtime remains live.
   * Never wipe that grant through ordinary backend-close failure cleanup. */
  if(cs_client.fingerprint_store.api){cs_retain();ctx_service=NULL;return false;}
#endif
  if(cs_client.grant.api&&!portable_contexts_close(&cs_client))cs_retain();
  ctx_service=NULL;return false;
 }
 ctx_service=cs_client.api;if(!contexts_fingerprint_api(ctx_service)){ctx_note("TEMPORAL CONTEXTS SERVICE REQUIRED");return false;}
 return true;
}
const contexts_service_v1*portable_contexts_service(void){return cs_backend_open()?cs_client.api:NULL;}
bool portable_contexts_training(bool on){
 if(!cs_backend_open()||!portable_contexts_pause(&cs_client))return false;
 cs_client.foreground_learning=on;cs_training_stopped=!on;
 if(on){risc_runtime_health_v1 h={.struct_size=sizeof(h)};if(!ctx_runtime->health(&h))return false;cs_learning_at=h.uptime_ms;cs_learning_draw_at=h.uptime_ms;}
 return true;
}
bool portable_contexts_enable(bool on){return cs_backend_open()&&portable_contexts_set_enabled(&cs_client,on);}
bool portable_contexts_rules_read(cr_store*out){return cs_backend_open()&&portable_context_rules_read(&cs_client,out);}
bool portable_contexts_rules_save(const cr_store*in){return cs_backend_open()&&portable_context_rules_save(&cs_client,in);}
bool portable_contexts_models_save(void){return cs_backend_open()&&portable_fp_checkpoint(&cs_client,false)&&!cs_client.fingerprint_error;}
unsigned portable_contexts_face_count(void){
#ifdef PORTABLE_CONTEXT_FACE_COUNT
 return PORTABLE_CONTEXT_FACE_COUNT;
#else
 return 0;
#endif
}
const char*portable_contexts_face_name(unsigned id){
#ifdef PORTABLE_CONTEXT_FACE_NAMES
 static const char*const names[]=PORTABLE_CONTEXT_FACE_NAMES;return id<sizeof(names)/sizeof(*names)?names[id]:NULL;
#else
 (void)id;return NULL;
#endif
}
static bool cs_face_save(const risc_key_value_v1*kv,unsigned id){
 if(id>=portable_contexts_face_count()||id>255)return false;
 uint8_t wanted[]={0x46,1,(uint8_t)id,(uint8_t)(id^0xa5u)},actual[4];uint32_t size=0;
 int32_t rc=kv->put(kv->context,"watch_face",wanted,4);return (rc==0||rc==RISC_KEY_VALUE_IO)&&!kv->get(kv->context,"watch_face",actual,4,&size)&&size==4&&!memcmp(wanted,actual,4);
}
static bool ctx_read_status(void){
 if(!cs_backend_open())return false;
 contexts_fingerprint_status_v1 next={.struct_size=sizeof(next)};
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(ctx_service);bool valid=f&&f->fingerprint(f->base.context,CONTEXTS_FP_STATUS,&next);
 bool changed=valid!=ctx_fp_valid||(valid&&memcmp(&next,&ctx_fp,sizeof(next)));ctx_fp_valid=valid;if(valid)ctx_fp=next;
 contexts_status_v1 status={.struct_size=sizeof(status)};if(ctx_service->status(ctx_service->context,&status)){if(status.cleanup_pending){cs_retain();return false;}ctx_status=status;cs_source_mask=(status.audio.model_state==CONTEXTS_MODEL_UNAVAILABLE?0:CONTEXTS_AUDIO)|(status.radio.model_state==CONTEXTS_MODEL_UNAVAILABLE?0:CONTEXTS_RADIO);}
 return changed;
}
static void ctx_fingerprint_setting(bool timing){
 if(!ctx_fp_valid)return;
 if(!timing&&!(cs_source_mask&(1u<<ctx_model_source))){ctx_note("SOURCE UNAVAILABLE");return;}
 if(!portable_contexts_pause(&cs_client)){cs_retain();return;}
 uint8_t value=timing?!ctx_fp.temporal_only:(uint8_t)(ctx_fp.sources^(1u<<ctx_model_source));risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 bool ok=ctx_runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g);
 if(ok&&!pqa_preferences_valid(g.api))ok=false;
 if(ok){const risc_key_value_v1*kv=g.api;const char*key=timing?"context_timing":"context_sources";uint8_t got=255;uint32_t size=0;int32_t rc=kv->put(kv->context,key,&value,1);ok=(rc==0||rc==RISC_KEY_VALUE_IO)&&!kv->get(kv->context,key,&got,1,&size)&&size==1&&got==value;}
 if(!cs_release(&g))return;
 if(ok){cs_client.loaded=false;if(timing)ctx_fp.temporal_only=value;else ctx_fp.sources=value;contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=ctx_fp.sources,.temporal_only=ctx_fp.temporal_only};const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(ctx_service);ok=f->fingerprint(f->base.context,CONTEXTS_FP_CONFIG,&cfg);}
 ctx_note(ok?"SAVED / FRESH SAMPLES REQUIRED":"SAVE UNCONFIRMED / RETRY");
}
static void ctx_back(void){ctx_exit=true;}
#include "contexts_controller.inc"
#include "contexts_scene_view.inc"
static bool cs_service_tick(void){
 if(!cs_backend_open())return false;
 if(!portable_contexts_step(&cs_client,true,true)||!portable_contexts_capture(&cs_client)){cs_retain();return false;}
 if(cs_client.settings_valid&&ctx_enabled!=cs_client.policy.enabled){ctx_enabled=cs_client.policy.enabled;ctx_dirty=true;}
 if(ctx_read_status()&&(cu_page==CU_HOME||cu_page==CU_SOURCES))ctx_dirty=true;
 bool learning_page=cu_page==CU_LEARN||(cu_page==CU_CANCEL&&cu_return==CU_LEARN);
 if(learning_page&&!cu_learning_saved&&!cs_training_stopped){
  unsigned progress=cu_learning.progress,state=cu_learning.state,samples=cu_learning.samples;
  cu_learning.operation=CONTEXTS_LEARN_POLL;
  bool polled=cu_call(CONTEXTS_FP_LEARN,&cu_learning);
  risc_runtime_health_v1 h={.struct_size=sizeof(h)};bool clocked=ctx_runtime->health(&h);
  /* Also bound older service versions, which can remain at 100% forever. */
  if(!polled||!clocked||(!cu_learning.state)||
     (cu_learning.state!=CONTEXTS_LEARN_READY&&(uint32_t)(h.uptime_ms-cs_learning_at)>=(cu_kind==CR_EVENT?120000u:45000u)))
   cu_learning.state=CONTEXTS_LEARN_FAILED;
  bool changed=progress!=cu_learning.progress||state!=cu_learning.state||samples!=cu_learning.samples;
  if(clocked&&cu_learning.progress>=100&&cu_learning.state!=CONTEXTS_LEARN_READY&&cu_learning.state!=CONTEXTS_LEARN_FAILED&&(uint32_t)(h.uptime_ms-cs_learning_draw_at)>=1000u){ctx_dirty=true;cs_learning_draw_at=h.uptime_ms;}
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
  if(changed&&(state!=cu_learning.state||samples!=cu_learning.samples||(uint32_t)(h.uptime_ms-cs_learning_draw_at)>=1000u)){ctx_dirty=true;cs_learning_draw_at=h.uptime_ms;}
#else
  if(changed)ctx_dirty=true;
#endif
  if(cu_learning.state==CONTEXTS_LEARN_READY||cu_learning.state==CONTEXTS_LEARN_FAILED){
   if(!portable_contexts_training(false))return false;
   ctx_dirty=true;
  }
 }
 return cs_alive();
}
static bool cs_finish_learning(void){
 if(cu_page!=CU_LEARN||!cu_autosave||cu_learning.state!=CONTEXTS_LEARN_READY||ctx_dirty)return true;
 /* Present SAVING before synchronous, verified persistence. Input remains live. */
 risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;
 if(cs_scene->snapshot(cs_scene->context,cs_session,&nav,&flags)||!cs_alive())return false;
 if(flags&RISC_SCENE_ACTIVITY)cs_resident.activity=true;
 if(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return true;
 cu_action(CU_ROW_SAVE);return cs_alive();
}
static bool cs_rules_tick(void){
 if(!cs_client.rules_available)return true;
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(ctx_service);contexts_evaluate_v1 q={.struct_size=sizeof(q)};q.observation.enabled=cs_client.settings_valid&&ctx_enabled;
 const risc_civil_clock_api_v1*civil=cs_civil_grant.api;
 if(civil&&(civil->api_version!=1||civil->struct_size<sizeof(*civil)||!civil->read))civil=NULL;
 risc_civil_time_v1 time={.struct_size=sizeof(time)};if(civil){int rc=civil->read(civil->context,&time);if(rc==-9){cs_retain();return false;}if(!rc){q.observation.time_valid=true;q.observation.hour=(uint8_t)(time.minute/60);q.observation.time_valid=time.minute<1440;}}
 const risc_battery_gauge_api_v1*gauge=cs_battery_grant.api;
 if(gauge&&(gauge->api_version!=1||gauge->struct_size<sizeof(*gauge)||!gauge->read))gauge=NULL;
 risc_battery_sample_v1 b={0};if(gauge&&gauge->read(gauge->context,&b)&&b.percent<=100&&!(b.flags&RISC_BATTERY_PROFILE_MISSING)){q.observation.battery_valid=true;q.observation.battery=b.percent;q.observation.charging=!!(b.flags&RISC_BATTERY_CHARGING);}
 if(!f->fingerprint(f->base.context,CONTEXTS_FP_EVALUATE,&q))return false;
 if(cs_active!=q.result.active){cs_active=q.result.active;if(cu_page==CU_HOME)ctx_dirty=true;}
 if(q.result.changed){
  if(!portable_contexts_pause(&cs_client)){cs_retain();return false;}
  risc_runtime_capability_v1 g={.struct_size=sizeof(g)};bool opened=ctx_runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g);uint16_t success=0;
  for(unsigned field=0;field<CR_SETTINGS;field++)if(q.result.changed&(1u<<field))if(opened&&pqa_preferences_valid(g.api)&&cu_field_available(field)&&portable_context_setting(g.api,field,q.result.settings[field],cs_client.low_battery,cs_face_save))success|=1u<<field;
  if(!cs_release(&g))return false;
  for(unsigned i=0;i<CR_RULES;i++){bool changed=false,complete=true;for(unsigned field=0;field<CR_SETTINGS;field++)if(q.result.winners[field]==(int)i&&(q.result.changed&(1u<<field))){changed=true;complete&=!!(success&(1u<<field));}if(changed){contexts_workflow_v1 report={.struct_size=sizeof(report),.rule=i,.result=complete?CR_APPLIED:CR_PARTIAL};if(!f->fingerprint(f->base.context,CONTEXTS_FP_WORKFLOW,&report))return false;}}
 }
 /* Edits/training cannot be interrupted by a launch or modal workflow. The
  * service cursor remains pending until the foreground is ready to run it. */
 if(cu_unsaved()
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
    ||cu_page==CU_NAME
#endif
    ||cs_workflow_message[0]||cs_launch[0])return true;
 contexts_workflow_v1 a={.struct_size=sizeof(a),.claim=true};if(!f->fingerprint(f->base.context,CONTEXTS_FP_WORKFLOW,&a))return false;
 if(a.available){bool ok=false;if(!portable_contexts_pause(&cs_client)){cs_retain();return false;}
  if(a.step.kind==CR_MESSAGE){cs_copy(cs_workflow_message,sizeof(cs_workflow_message),a.step.text);ok=true;ctx_dirty=true;}
  else if(a.step.kind==CR_TIMER){risc_runtime_capability_v1 storage={.struct_size=sizeof(storage)};ok=portable_context_timer(ctx_runtime,cs_alarm_grant.api,a.step.value,&storage);if(storage.api){cs_retain();return false;}}
  else if(a.step.kind==CR_LAUNCH){ok=cs_catalog_prepare(a.step.text);if(!cs_alive())return false;}
  if(cs_launch[0]){cs_launch_report=a;return true;}
  a.claim=false;a.result=ok?CR_RAN:CR_FAILED;if(!f->fingerprint(f->base.context,CONTEXTS_FP_WORKFLOW,&a))return false;
 }
 return cs_alive();
}
static bool cs_close_scene(void){
 if(!cs_alive())return false;
 for(unsigned i=0;cs_session&&i<1000;i++){int rc=cs_scene->close(cs_scene->context,cs_session);if(!cs_alive())return false;if(rc==RISC_SCENE_OK){cs_session=0;return true;}if(rc!=RISC_SCENE_AGAIN){cs_retain();return false;}ctx_runtime->yield_ms(5);}
 if(cs_session){cs_retain();return false;}return true;
}
static bool cs_open_scene(const risc_scene_navigation_v1*navigation){
 int rc=cs_components->open(cs_scene->context,&cs_document,navigation,&cs_session);if(!cs_alive()||rc)return false;cs_last_event=0;
 const risc_scene_lifecycle_api_v1*l=risc_scene_lifecycle_get_v1(cs_scene);if(l){rc=l->configure(cs_scene->context,cs_session,cs_resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!cs_alive()||rc)return false;}return true;
}
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
#include "contexts_text_input.inc"
#endif
static bool cs_checkpoint(unsigned reason){
 risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;if(cs_scene->snapshot(cs_scene->context,cs_session,&nav,&flags)||!cs_alive())return false;
 char focused_route[RISC_APP_CATALOG_ROUTE_MAX]={0};bool refresh_focus=false;
 unsigned depth=nav.depth&&nav.depth<=RISC_SCENE_MAX_DEPTH?nav.depth-1:0;
 if(cu_page==CU_APPS&&nav.focus[depth]&&nav.focus[depth]<=cs_document.node_count){
  unsigned action=cs_document.nodes[nav.focus[depth]-1].action;
  refresh_focus=action==CU_CATALOG_REFRESH;
  if(action>=CU_RUNTIME_APP&&action<CU_RUNTIME_APP+cs_catalog.count)
   strcpy(focused_route,cs_catalog.entries[action-CU_RUNTIME_APP].route);
 }
 if(!cs_catalog_close())return false;
 bool training=cs_client.foreground_learning;if(!portable_contexts_pause(&cs_client)||!portable_contexts_close(&cs_client)){cs_retain();return false;}ctx_service=NULL;
 if(!cs_close_scene())return false;
 int rc=risc_scene_resident_dispatch_v1(&cs_resident,reason);if(!cs_alive())return false;if(rc==RISC_RESIDENT_RETAINED){cs_retain();return false;}if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
 if(!cs_backend_open())return false;
 if(cu_page==CU_APPS){
  if(!cs_catalog_refresh()&&!cs_alive())return false;
  /* Generation identities are scoped to a grant. Across reacquisition, retain
   * focus by the copied canonical route or explicit Refresh action only. */
  memset(nav.focus,0,sizeof(nav.focus));cs_declare();
  for(unsigned i=0;i<cs_document.node_count;i++){
   unsigned action=cs_document.nodes[i].action;
   bool same=refresh_focus&&action==CU_CATALOG_REFRESH;
   if(focused_route[0]&&action>=CU_RUNTIME_APP&&action<CU_RUNTIME_APP+cs_catalog.count)
    same=!strcmp(focused_route,cs_catalog.entries[action-CU_RUNTIME_APP].route);
   if(same){nav.focus[depth]=cs_document.nodes[i].id;break;}
  }
 }
 cs_client.foreground_learning=training;ctx_dirty=true;return cs_open_scene(&nav);
}
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
static bool cs_resident_step(void){
 if(cs_text_busy())return cs_text_resident_step();
 if(cs_resident.enabled){risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;if(cs_scene->snapshot(cs_scene->context,cs_session,&nav,&flags)||!cs_alive())return false;if(flags&RISC_SCENE_ACTIVITY)cs_resident.activity=true;bool working=cs_client.foreground_learning||((cu_page==CU_LEARN||(cu_page==CU_CANCEL&&cu_return==CU_LEARN))&&(cu_learning.state==CONTEXTS_LEARN_READY||cu_learning_saved));uint32_t work=working?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0;int result=risc_scene_resident_poll_work_v1(&cs_resident,flags,work);if(!cs_alive())return false;if(result==RISC_RESIDENT_RETAINED){cs_retain();return false;}if(result==RISC_RESIDENT_EXIT)return false;if(cs_resident.policy&&!working&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!cs_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY))return false;}
 return cs_alive();
}
#endif
static void cs_cleanup(void){
 if(!cs_alive())return;
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
 if(!cs_text_cleanup())return;
#endif
 if(!cs_catalog_close())return;
 if(cs_client.api&&(!portable_contexts_pause(&cs_client)||!portable_contexts_close(&cs_client))){cs_retain();return;}
 if(cs_close_scene()){if(!cs_release(&cs_alarm_grant)||!cs_release(&cs_battery_grant)||!cs_release(&cs_civil_grant))return;(void)cs_release(&cs_scene_grant);}
}
/* Revalidate the exact publication after scene teardown. Never dispatch an
 * old route merely because it was available when the workflow was claimed. */
static bool cs_dispatch_launch(void){
 if(!cs_launch[0]||!cs_alive()||cs_host_exit())return false;
 if(!portable_contexts_pause(&cs_client)||!cs_close_scene()){cs_retain();return false;}
 if(!cs_alive()||cs_host_exit())return false;
 bool valid=cs_catalog_checked(portable_catalog_launch_valid(&cs_catalog,&cs_launch_ticket));
 if(!cs_alive())return false;
 if(cs_host_exit()||!cs_alive())return false;
 bool launched=valid&&ctx_runtime->request_launch&&ctx_runtime->request_launch(cs_launch_ticket.route);
 if(!cs_alive())return false;
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(ctx_service);cs_launch_report.claim=false;cs_launch_report.result=launched?CR_RAN:CR_FAILED;
 if(f)(void)f->fingerprint(f->base.context,CONTEXTS_FP_WORKFLOW,&cs_launch_report);
 if(!cs_alive())return false;
 if(!launched&&ctx_runtime->diagnostic)ctx_runtime->diagnostic("Contexts workflow app launch rejected");
 return launched;
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 ctx_runtime=risc_runtime_get_api(1);if(!ctx_runtime||ctx_runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!ctx_runtime->retain_invocation||!ctx_runtime->acquire||!ctx_runtime->release||!ctx_runtime->yield_ms||!ctx_runtime->health)return;
 cs_terminal=ctx_exit=false;cs_training_stopped=true;cu_autosave=false;ctx_dirty=true;ctx_enabled=false;ctx_message="";cs_session=0;cs_revision=cs_toast_token=0;cs_last_event=0;cs_toast[0]=cs_launch[0]=cs_workflow_message[0]=0;cs_active=0;cs_client=(portable_contexts_client){0};cs_catalog=(portable_app_catalog){0};cs_launch_ticket=(portable_app_catalog_launch){0};
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
 cs_text_reset();
#endif
 cs_scene_grant=(risc_runtime_capability_v1){.struct_size=sizeof(cs_scene_grant)};cs_civil_grant=(risc_runtime_capability_v1){.struct_size=sizeof(cs_civil_grant)};cs_battery_grant=(risc_runtime_capability_v1){.struct_size=sizeof(cs_battery_grant)};cs_alarm_grant=(risc_runtime_capability_v1){.struct_size=sizeof(cs_alarm_grant)};
 bool acquired=ctx_runtime->acquire(RISC_SCENE_CAPABILITY,1,0,&cs_scene_grant);if(!cs_alive())return;
 if(!acquired){(void)cs_release(&cs_scene_grant);return;}
 cs_scene=cs_scene_grant.api;cs_components=risc_scene_components_get_v1(cs_scene);
 if(!cs_scene||!cs_scene->next||!cs_scene->close||!cs_scene->snapshot||!cs_components){if(ctx_runtime->diagnostic)ctx_runtime->diagnostic("Contexts 0.4 requires NOVA scene-host 0.3 or later");(void)cs_release(&cs_scene_grant);return;}
 if(!risc_scene_resident_bind_v1(ctx_runtime,&cs_resident)){cs_retain();return;}
 (void)ctx_runtime->acquire(RISC_CIVIL_CLOCK_CAPABILITY,1,0,&cs_civil_grant);if(!cs_alive())return;
 (void)ctx_runtime->acquire(RISC_BATTERY_GAUGE_CAPABILITY,1,0,&cs_battery_grant);if(!cs_alive())return;
 (void)ctx_runtime->acquire("alarm.service",CONTEXTS_ALARM_API,0,&cs_alarm_grant);if(!cs_alive())return;
 cu_page=CU_HOME;cu_item=cu_rule=cu_offset=0;cu_kind=CR_PLACE;cu_edit=cu_pending=cu_learning_saved=cu_contact=cu_moved=false;cu_last_poll=cu_profile_generation=0;
 cu_loaded=portable_contexts_rules_read(&cu_store);if(!cu_loaded){cr_init(&cu_store);ctx_note("CONTEXT STORAGE UNAVAILABLE");}else cu_sync_profiles();cu_saved=cu_store;
 if(cs_client.api){if(!cs_service_tick()){cs_cleanup();return;}}
 cs_declare();if(!cs_open_scene(NULL)){if(ctx_runtime->diagnostic&&cs_alive())ctx_runtime->diagnostic("Contexts could not open NOVA components; update scene-host to 0.3.0");cs_cleanup();return;}
 unsigned ticks=0;
 while(!ctx_exit&&cs_alive()){
  if(cs_host_exit())break;
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
  if(cs_text_busy()||(cu_page==CU_NAME&&!cs_text_retry)){
   if(!cs_text_step()||ctx_exit)break;
   /* The text host advances logical input even while its display is pending.
    * Detection/capture keep their normal cadence without borrowing its scene. */
   if(cs_client.api&&!cs_service_tick())break;
   if(++ticks>=50){ticks=0;if(cs_client.api&&!cs_rules_tick())break;if(!cs_resident_step())break;}
   if(!cs_text_busy()&&cs_session&&ctx_dirty){
    cs_declare();int updated=cs_components->update(cs_scene->context,cs_session,&cs_document);
    if(!cs_alive())return;
    if(updated){if(updated==RISC_SCENE_RETAINED)cs_retain();break;}ctx_dirty=false;
   }
   ctx_runtime->yield_ms(10);continue;
  }
#endif
  risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=cs_scene->next(cs_scene->context,cs_session,&e);if(!cs_alive())return;
  if(cs_host_exit())break;
  if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE){if(rc==RISC_SCENE_RETAINED)cs_retain();break;}
  if(rc==RISC_SCENE_OK){
   if(e.kind==RISC_SCENE_CONTROLS_EVENT&&e.document_revision==cs_document.revision&&cs_resident.enabled){if(!cs_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS))break;continue;}
   if(e.kind==RISC_SCENE_SUSPEND_EVENT){if(cu_unsaved()){cu_return=cu_page;cu_page_set(CU_CANCEL);}else break;}else cs_event(&e);
  }
  if(cs_client.api){if(!cs_service_tick())break;}
  if(!cs_finish_learning())break;
  if(++ticks>=50){ticks=0;if(cs_client.api&&!cs_rules_tick())break;
#ifdef PORTABLE_CONTEXTS_SHARED_TEXT_INPUT
   if(!cs_resident_step())break;
#else
   if(cs_resident.enabled){risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;if(cs_scene->snapshot(cs_scene->context,cs_session,&nav,&flags)||!cs_alive())break;if(flags&RISC_SCENE_ACTIVITY)cs_resident.activity=true;bool working=cs_client.foreground_learning||((cu_page==CU_LEARN||(cu_page==CU_CANCEL&&cu_return==CU_LEARN))&&(cu_learning.state==CONTEXTS_LEARN_READY||cu_learning_saved));uint32_t work=working?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0;int result=risc_scene_resident_poll_work_v1(&cs_resident,flags,work);if(!cs_alive())return;if(result==RISC_RESIDENT_RETAINED){cs_retain();break;}if(result==RISC_RESIDENT_EXIT)break;if(cs_resident.policy&&!working&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!cs_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY))break;}
#endif
  }
  if(ctx_dirty){cs_declare();rc=cs_components->update(cs_scene->context,cs_session,&cs_document);if(!cs_alive())return;if(rc){if(rc==RISC_SCENE_RETAINED)cs_retain();break;}ctx_dirty=false;}
  if(cs_launch[0])break;
  ctx_runtime->yield_ms(10);
 }
 if(cs_launch[0]){(void)cs_dispatch_launch();if(!cs_alive())return;}
 cs_cleanup();
}

