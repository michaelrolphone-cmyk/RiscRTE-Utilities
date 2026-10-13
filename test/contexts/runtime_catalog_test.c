/* Production standalone controller and dispatcher; only owner transports are fake. */
#define PORTABLE_CONTEXT_FACE_COUNT 3
#define PORTABLE_CONTEXT_FACE_NAMES {"ONE","TWO","THREE"}
#define CONTEXTS_TEST_MAIN contexts_existing_behavior_main
#include "app_test.c"
#undef CONTEXTS_TEST_MAIN
static risc_runtime_api_v1 catalog_runtime;
static contexts_fingerprint_service_v1 catalog_service;
static unsigned inventory=1,catalog_acquires,catalog_releases,catalog_gets,catalog_snapshots;
static unsigned workflow_reports,workflow_result,close_calls,open_calls;
static bool home_get,empty_checkpoint;
static uint64_t publication=1;
static int snapshot_error,get_error,close_error;
static bool missing,partial_grant,release_error,unavailable,duplicate,malformed,mutate_snapshot,mutate_close,remove_close,reject_launch;
static const char *route_override,*id_override;
static char launched_route[RISC_APP_CATALOG_ROUTE_MAX],workflow_key[33];
static int32_t catalog_snapshot(void*c,risc_app_catalog_snapshot_v1*out){
 (void)c;assert(native_alive);catalog_snapshots++;
 if(snapshot_error)return snapshot_error;
 if(mutate_snapshot&&catalog_snapshots==2)publication++;
 *out=(risc_app_catalog_snapshot_v1){sizeof(*out),inventory,publication};return 0;
}
static int32_t catalog_get(void*c,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1*out){
 (void)c;assert(native_alive);catalog_gets++;if(home_get)native_exit_pending=true;
 if(get_error)return get_error;
 if(generation!=publication)return RISC_APP_CATALOG_STALE;
 if(index>=inventory)return RISC_APP_CATALOG_NOT_FOUND;
 *out=(risc_app_catalog_entry_v1){.struct_size=sizeof(*out),.flags=unavailable?0:RISC_APP_CATALOG_AVAILABLE};
 snprintf(out->display_name,sizeof(out->display_name),"Installed %u",index);
 snprintf(out->file_name,sizeof(out->file_name),"app-%u.elf",duplicate?0:index);
 snprintf(out->id,sizeof(out->id),"native-%u",index);
 if(route_override)strcpy(out->file_name,route_override);
 if(id_override)strcpy(out->id,id_override);
 if(malformed)memset(out->display_name,'x',sizeof(out->display_name));
 return 0;
}
static const risc_app_catalog_v1 catalog_api={1,sizeof(catalog_api),NULL,catalog_snapshot,catalog_get};
static bool catalog_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 assert(native_alive);
 if(strcmp(n,RISC_APP_CATALOG_CAPABILITY))return app_acquire(n,v,id,g);
 assert(v==1&&id==0);catalog_acquires++;
 if(missing)return false;
 g->api=&catalog_api;g->slot=94;g->generation=1;return !partial_grant;
}
static bool catalog_release(risc_runtime_capability_v1*g){
 assert(native_alive);if(g->api!=&catalog_api)return app_release(g);catalog_releases++;
 if(release_error)return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static bool catalog_launch(const char*n){assert(native_alive);app_launches++;strcpy(launched_route,n);return !reject_launch;}
static int32_t catalog_close_scene(void*c,uint64_t token){
 (void)c;assert(native_alive&&token==17);close_calls++;
 if(mutate_close)publication++;
 if(remove_close){inventory=0;publication++;}
 return close_error;
}
static int32_t catalog_snapshot_scene(void*c,uint64_t token,risc_scene_navigation_v1*out,uint32_t*flags){(void)c;assert(token==17);*out=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(*out),.depth=1,.routes={1},.focus={50}};*flags=0;return 0;}
static int32_t catalog_open_scene(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){(void)c;(void)d;assert(n&&n->struct_size==sizeof(*n));assert(n->focus[0]==(empty_checkpoint?0u:50u));open_calls++;*t=17;return 0;}
static int32_t catalog_checkpoint(uint64_t token,const risc_resident_request_v1*q,risc_resident_reply_v1*r){assert(token==99&&q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);r->flags=0;if(empty_checkpoint){inventory=0;publication++;}return 0;}
static const risc_scene_api_v1 catalog_scene={.close=catalog_close_scene,.snapshot=catalog_snapshot_scene};
static const risc_scene_components_api_v1 catalog_components={.open=catalog_open_scene};
static bool catalog_fingerprint(void*c,uint32_t op,void*p){
 if(op==CONTEXTS_FP_WORKFLOW){
  assert(native_alive);contexts_workflow_v1*q=p;
  if(q->claim){q->available=workflow_key[0]!=0;q->step.kind=CR_LAUNCH;strcpy(q->step.text,workflow_key);}
  else{workflow_reports++;workflow_result=q->result;}
  return true;
 }
 return fp(c,op,p);
}
static void setup(void){
 catalog_runtime=app_runtime;catalog_runtime.acquire=catalog_acquire;catalog_runtime.release=catalog_release;catalog_runtime.request_launch=catalog_launch;
 ctx_runtime=&catalog_runtime;cs_scene=&catalog_scene;catalog_service=service;catalog_service.fingerprint=catalog_fingerprint;
 ctx_service=&catalog_service.base;cs_client=(portable_contexts_client){.runtime=ctx_runtime,.api=ctx_service,.rules_available=true};
 ctx_message="";cu_loaded=true;cr_init(&cu_store);cu_saved=cu_store;cr_init(&bank);bank.generation=1;assert(cr_encode(&bank,stored,sizeof(stored)));
}
static void catalog_finish(void){
 if(cs_terminal){unsigned callbacks=catalog_releases+catalog_gets+catalog_snapshots;assert(!cs_catalog_close());cs_cleanup();assert(callbacks==catalog_releases+catalog_gets+catalog_snapshots);}
 else{assert(cs_catalog_close());assert(catalog_releases==(missing?0:catalog_acquires));}
}
static void inventory_test(unsigned count){
 inventory=count;cu_action(CU_ROW_LAUNCH);assert(cu_page==CU_APPS&&cs_catalog.count==count);
 cs_declare();assert(cs_document.node_count<=RISC_COMPONENTS_MAX_NODES);
 unsigned visited=0;for(unsigned i=0;i<cu_count;i++)if(cu_rows[i].action>=CU_RUNTIME_APP&&cu_rows[i].action<CU_RUNTIME_APP+RISC_APP_CATALOG_MAX)visited++;
 assert(visited==count);
 if(count){cu_action(CU_RUNTIME_APP+count-1);assert(cu_store.rules[0].steps==1);char expected[33];snprintf(expected,sizeof(expected),"app-%u.elf",count-1);assert(!strcmp(cu_store.rules[0].workflow[0].text,expected));}
}
int main(int argc,char**argv){
 assert(argc==2);setup();const char*mode=argv[1];
 char long_route[256]="Apps/";while(strlen(long_route)<220)strcat(long_route,"package/");strcat(long_route,"run.elf");
 if(!strcmp(mode,"one"))inventory_test(1);
 else if(!strcmp(mode,"many"))inventory_test(95);
 else if(!strcmp(mode,"oversized")||!strcmp(mode,"one-over-limit")){
  inventory=!strcmp(mode,"one-over-limit")?96:128;cu_action(CU_ROW_LAUNCH);cs_declare();assert(cs_catalog.count==inventory&&cs_document.node_count==2);
  assert(!strcmp(cu_rows[0].title,"CATALOG EXCEEDS DISPLAY LIMIT"));cu_action(CU_RUNTIME_APP);assert(!cu_store.rules[0].steps);
 }
 else if(!strcmp(mode,"empty"))inventory_test(0);
 else if(!strcmp(mode,"refresh")){inventory_test(1);inventory=4;publication++;cu_action(CU_ROW_LAUNCH);assert(cs_catalog.count==4);inventory=0;publication++;cu_action(CU_CATALOG_REFRESH);assert(!cs_catalog.count);}
 else if(!strcmp(mode,"long")||!strcmp(mode,"long-id")||!strcmp(mode,"medium")){
  route_override=!strcmp(mode,"medium")?"Apps/a-directory-longer-than-32-characters/run.elf":long_route;
  id_override=!strcmp(mode,"long-id")?"identity-longer-than-the-existing-workflow-schema":"native-workflow";
  cu_action(CU_ROW_LAUNCH);cu_action(CU_RUNTIME_APP);
  if(!strcmp(mode,"long")){assert(cu_store.rules[0].steps==1);assert(!strcmp(cu_store.rules[0].workflow[0].text,"id:native-workflow"));strcpy(workflow_key,cu_store.rules[0].workflow[0].text);cu_edit=false;cu_page=CU_HOME;assert(cs_rules_tick());assert(cs_dispatch_launch());assert(app_launches==1&&!strcmp(launched_route,long_route)&&workflow_result==CR_RAN);}
  else{assert(!cu_store.rules[0].steps);assert(!strcmp(ctx_message,"APP KEY TOO LONG FOR THIS RULE"));}
 }
 else if(!strcmp(mode,"select-stale")){cu_action(CU_ROW_LAUNCH);publication++;cu_action(CU_RUNTIME_APP);assert(!cu_store.rules[0].steps&&!cs_catalog.count&&!cs_terminal);}
 else if(!strcmp(mode,"unavailable")){unavailable=true;cu_action(CU_ROW_LAUNCH);assert(cu_count==0);cu_build();assert(cu_count==2);cu_action(CU_RUNTIME_APP);assert(!cu_store.rules[0].steps);strcpy(workflow_key,"app-0.elf");cu_page=CU_HOME;assert(cs_rules_tick());assert(workflow_reports==1&&workflow_result==CR_FAILED&&!app_launches);}
 else if(!strcmp(mode,"missing")){missing=true;assert(!cs_catalog_refresh()&&!cs_catalog.count&&!cs_terminal);}
 else if(!strcmp(mode,"partial")){partial_grant=true;assert(!cs_catalog_refresh()&&cs_terminal&&!catalog_snapshots);}
 else if(!strcmp(mode,"release")){assert(cs_catalog_refresh());release_error=true;assert(!cs_catalog_close()&&cs_terminal);}
 else if(!strcmp(mode,"malformed")||!strcmp(mode,"duplicate")||!strcmp(mode,"generation")||!strcmp(mode,"snapshot-error")||!strcmp(mode,"snapshot-context")||!strcmp(mode,"snapshot-retained")||!strcmp(mode,"get-context")||!strcmp(mode,"get-retained")||!strcmp(mode,"get-error")){
  assert(cs_catalog_refresh());catalog_snapshots=0;
  malformed=!strcmp(mode,"malformed");duplicate=!strcmp(mode,"duplicate");if(duplicate)inventory=2;
  mutate_snapshot=!strcmp(mode,"generation");
  snapshot_error=!strcmp(mode,"snapshot-error")?RISC_APP_CATALOG_UNAVAILABLE:!strcmp(mode,"snapshot-context")?RISC_APP_CATALOG_CONTEXT:!strcmp(mode,"snapshot-retained")?RISC_APP_CATALOG_RETAINED:0;
  get_error=!strcmp(mode,"get-context")?RISC_APP_CATALOG_CONTEXT:!strcmp(mode,"get-retained")?RISC_APP_CATALOG_RETAINED:!strcmp(mode,"get-error")?RISC_APP_CATALOG_INVALID:0;
  assert(!cs_catalog_refresh());if(cs_terminal)assert(cs_catalog.uncertain);else assert(!cs_catalog.count&&!cs_catalog.entries);
 }
 else if(!strcmp(mode,"picker-checkpoint")||!strcmp(mode,"picker-checkpoint-empty")){
  empty_checkpoint=!strcmp(mode,"picker-checkpoint-empty");
  inventory=64;cu_action(CU_ROW_LAUNCH);cs_declare();uint32_t screen=cs_document.screen_key;
  cs_session=17;cs_components=&catalog_components;cs_resident.enabled=true;cs_resident.client.invocation=99;cs_resident.client.checkpoint=catalog_checkpoint;
  assert(cs_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS));assert(close_calls==1&&open_calls==1&&cs_catalog.count==(empty_checkpoint?0u:64u)&&cu_page==CU_APPS&&cs_document.screen_key==screen);
  cs_declare();assert(cs_document.node_count==(empty_checkpoint?2u:65u));if(!empty_checkpoint){cu_action(CU_RUNTIME_APP+37);assert(!strcmp(cu_store.rules[0].workflow[0].text,"app-37.elf"));}
  assert(portable_contexts_close(&cs_client));
 }
 else if(!strcmp(mode,"faces-actions")){
  assert(portable_contexts_face_count()==3&&!strcmp(portable_contexts_face_name(2),"THREE"));
  assert(cu_field_available(CR_FACE));cu_cycle(CR_FACE);assert(cu_store.rules[0].settings[CR_FACE]==0);cu_cycle(CR_FACE);cu_cycle(CR_FACE);cu_cycle(CR_FACE);assert(cu_store.rules[0].settings[CR_FACE]==CR_KEEP);
  cu_page=CU_ADD_STEP;cu_action(CU_ROW_TIMER);cu_action(CU_ROW_DURATION+2);assert(cu_store.rules[0].workflow[0].value==900);cu_action(CU_ROW_MESSAGE);strcpy(cu_name,"HELLO");cu_name_done();assert(cu_store.rules[0].steps==2&&!strcmp(cu_store.rules[0].workflow[1].text,"HELLO"));
 }
 else{
  strcpy(workflow_key,!strcmp(mode,"absent")?"absent.elf":"app-0.elf");assert(cs_rules_tick());
  if(!strcmp(mode,"absent")){assert(!cs_launch[0]&&workflow_reports==1&&workflow_result==CR_FAILED);}
  else{
   assert(cs_launch[0]&&!workflow_reports);cs_session=17;
   mutate_close=!strcmp(mode,"launch-stale");remove_close=!strcmp(mode,"launch-removed");reject_launch=!strcmp(mode,"launch-rejected");
   if(!strcmp(mode,"launch-retained"))close_error=RISC_SCENE_RETAINED;
   if(!strcmp(mode,"launch-catalog-retained"))get_error=RISC_APP_CATALOG_RETAINED;
   if(!strcmp(mode,"launch-home"))native_exit_pending=true;
   home_get=!strcmp(mode,"launch-home-get");
   bool expected=!strcmp(mode,"launch");assert(cs_dispatch_launch()==expected);
   if(cs_terminal||native_exit_pending)assert(!app_launches&&!workflow_reports);
   else{assert(workflow_reports==1&&workflow_result==(expected?CR_RAN:CR_FAILED));assert(app_launches==(expected||reject_launch?1u:0u));}
  }
 }
 catalog_finish();printf("Standalone Contexts runtime catalog %s PASS\n",mode);return 0;
}
