/* Production controller with deterministic semantic services; not hardware proof. */
#include "../../Apps/alarms_scene.c"
#include <assert.h>
static risc_runtime_api_v1 rt,other;
static risc_scene_lifecycle_api_v1 ui;
static risc_app_data_v1 data_api;
static alarm_control_api_v1 service;
static unsigned loop_polls,updates,status_reads,releases;static uint32_t busy_flags;static bool loop_mode;
static bool replaced,present,write_fail,dispatch_replaces,close_replaces;
static unsigned closes,opens,configures,dispatches,writes,retains,io;
static int32_t close_rc,dispatch_rc;
static uint8_t bytes[CHECKPOINT_SIZE];static uint32_t length;static uint64_t version;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?(replaced?&other:&rt):NULL;}
static bool hold(void){retains++;return true;}
static void yield_ms(uint32_t n){(void)n;}
static int32_t stat_data(void*c,const char*n,uint32_t*z,uint64_t*v){(void)c;assert(!strcmp(n,CHECKPOINT_NAME));io++;*z=length;*v=version;return present?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t read_data(void*c,const char*n,uint64_t v,void*b,uint32_t cap,uint32_t*z,uint64_t*a){(void)c;(void)n;io++;assert(v==version&&cap>=length);memcpy(b,bytes,length);*z=length;*a=version;return 0;}
static int32_t write_data(void*c,const char*n,uint64_t v,const void*b,uint32_t z){(void)c;(void)n;io++;writes++;assert(v==version&&z==CHECKPOINT_SIZE);if(write_fail)return RISC_APP_DATA_IO;memcpy(bytes,b,z);length=z;present=true;version++;return 0;}
static int32_t snap(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){(void)c;assert(s==7&&n->struct_size==sizeof(*n));io++;*f=busy_flags;return 0;}
static int32_t close_ui(void*c,uint64_t s){(void)c;assert(s==7);io++;closes++;if(close_replaces)replaced=true;return close_rc;}
static int32_t open_ui(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;io++;opens++;assert(!session&&d->root==OVERVIEW&&n->routes[0]==OVERVIEW);*s=7;return 0;}
static int32_t configure(void*c,uint64_t s,uint32_t f){(void)c;io++;configures++;assert(s==7&&f==(resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));return 0;}
static int32_t dispatch(uint64_t inv,const risc_resident_request_v1*q,risc_resident_reply_v1*p){io++;dispatches++;assert(inv==1&&session==0&&closes==1&&q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);p->flags=0;if(dispatch_replaces)replaced=true;return dispatch_rc;}
static int32_t update_ui(void*c,uint64_t s,const risc_scene_document_v1*d){(void)c;assert(s==7&&d->revision>document.revision&&busy_flags==0);updates++;return 0;}
static int32_t navigate_ui(void*c,uint64_t s,uint32_t op,uint32_t route){(void)c;(void)s;(void)op;(void)route;return 0;}
static int32_t next_ui(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;assert(s==7);loop_polls++;if(loop_polls==1){busy_flags=RISC_SCENE_INPUT_BUSY;return RISC_SCENE_IDLE;}if(loop_polls==2){assert(updates==0);busy_flags=RISC_SCENE_PRESENTING;return RISC_SCENE_IDLE;}if(loop_polls==3){assert(updates==0);busy_flags=0;return RISC_SCENE_IDLE;}assert(loop_polls==4&&updates==1);*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=document.revision,.sequence=1};return 0;}
static int32_t read_status(void*c,alarm_control_snapshot_v1*out){(void)c;*out=(alarm_control_snapshot_v1){.api_version=1,.struct_size=sizeof(*out),.minutes=420};strcpy(out->status,status_reads++?"CHANGED":"INITIAL");return 0;}
static int32_t step_status(void*c){(void)c;return 0;}
static int32_t prepare_status(void*c,uint32_t a,uint32_t b,uint32_t d,alarm_control_command_v1*o){(void)c;(void)a;(void)b;(void)d;(void)o;assert(0);return 0;}
static int32_t apply_status(void*c,const alarm_control_command_v1*q){(void)c;(void)q;assert(0);return 0;}
static int32_t dismiss_status(void*c,const alarm_control_occurrence_v1*q){(void)c;(void)q;assert(0);return 0;}
static bool acquire_api(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1&&id==0);if(!strcmp(n,RISC_PREFERENCE_STORE_CAPABILITY))return false;g->slot=1;g->generation=1;g->api=!strcmp(n,RISC_SCENE_CAPABILITY)?(void*)&ui:!strcmp(n,ALARM_CONTROL_CAPABILITY)?(void*)&service:(void*)&data_api;return true;}
static bool release_api(risc_runtime_capability_v1*g){releases++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool replaced_bad_bind(risc_resident_client_v1*c){replaced=true;other=rt;c->api_version=0;return true;}
static void setup(void){
 loop_polls=updates=status_reads=releases=busy_flags=0;loop_mode=false;
 service=(alarm_control_api_v1){1,sizeof(service),NULL,step_status,read_status,prepare_status,apply_status,dismiss_status};
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.retain_invocation=hold,.yield_ms=yield_ms,.acquire=acquire_api,.release=release_api};other=rt;runtime=&rt;
 ui=(risc_scene_lifecycle_api_v1){.base={.api_version=1,.struct_size=sizeof(ui),.open=open_ui,.snapshot=snap,.close=close_ui,.update=update_ui,.navigate=navigate_ui,.next=next_ui},.configure=configure};scene=&ui.base;lifecycle=&ui;
 data_api=(risc_app_data_v1){1,sizeof(data_api),NULL,stat_data,read_data,write_data};storage=&data_api;
 resident=(risc_scene_resident_v1){.enabled=true,.client={.invocation=1,.checkpoint=dispatch}};
 terminal=recovery=storage_present=force_read=command_not_attempted=false;storage_known=true;stored_size=0;session=7;last_event=99;
 status=(alarm_control_snapshot_v1){0};fresh_model();state.minutes=421;state.time_dirty=1;root_navigation();navigation.depth=2;navigation.routes[1]=EDIT_TIME;navigation.focus[1]=10;document=(risc_scene_document_v1){.revision=3};notice[0]=0;
 replaced=present=write_fail=dispatch_replaces=close_replaces=false;closes=opens=configures=dispatches=writes=retains=io=0;close_rc=dispatch_rc=0;length=0;version=0;memset(bytes,0,sizeof(bytes));
}
int main(void){
 setup();assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS));assert(writes==1&&closes==1&&dispatches==1&&opens==1&&configures==1&&last_event==0&&session==7&&force_read);assert(state.minutes==421&&state.time_dirty&&navigation.routes[1]==EDIT_TIME);
 setup();dispatch_rc=RISC_RESIDENT_BUSY;assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&opens==1);
 setup();dispatch_rc=RISC_RESIDENT_RETAINED;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&retains==1&&opens==0);
 setup();dispatch_replaces=true;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&retains==0&&opens==0);
 setup();close_replaces=true;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&dispatches==0&&opens==0&&retains==0);
 setup();close_rc=RISC_SCENE_AGAIN;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&closes==250&&dispatches==0&&retains==1);
 setup();write_fail=true;assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&writes==1&&configures==1&&closes==0&&dispatches==0&&session==7);
 setup();recovery=true;present=true;length=17;memset(bytes,0x5a,length);assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&writes==0&&recovery&&length==17);for(unsigned i=0;i<length;i++)assert(bytes[i]==0x5a);
 setup();state.pending=1;state.pending_result=ALARM_CONTROL_UNCERTAIN;memset(state.command.bytes,0xa7,sizeof(state.command.bytes));assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&state.pending&&state.pending_result==ALARM_CONTROL_UNCERTAIN);uint8_t payload[PAYLOAD_SIZE];size_t n=0;risc_scene_navigation_v1 nav;assert(risc_scene_state_decode(bytes,length,CHECKPOINT_SCHEMA,&nav,payload,sizeof(payload),&n));assert(risc_scene_get32(payload+24)==1);for(unsigned i=36;i<PAYLOAD_SIZE;i++)assert(payload[i]==0xa7);
 setup();declare();assert(candidate.nodes[3].kind==RISC_SCENE_TEXT_NODE&&strstr(candidate.nodes[3].text,"UNAVAILABLE")&&!(candidate.nodes[3].flags&RISC_SCENE_HIDDEN));status.flags=ALARM_CONTROL_HAS_VOLUME;declare();assert(candidate.nodes[3].kind==RISC_SCENE_LINK);
 setup();write_fail=true;declare();document=candidate;risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=document.revision,.sequence=100};assert(!handle_event(&e)&&configures==1&&closes==0&&writes==1&&session==7);
 setup();status.flags=ALARM_CONTROL_VOLUME_VALID;navigation.routes[1]=EDIT_VOLUME;declare();document=candidate;for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].id==13||document.nodes[i].id==14)assert(document.nodes[i].flags&RISC_SCENE_DISABLED);e=(risc_scene_event_v1){.struct_size=sizeof(e),.kind=RISC_SCENE_VALUE_EVENT,.node=13,.action=CHANGE_VOLUME,.value=50,.document_revision=document.revision,.sequence=100};assert(!handle_event(&e)&&state.volume==0&&writes==0);
 setup();app_main();assert(loop_polls==4&&updates==1&&releases==3&&closes==1&&writes==1);
 setup();rt.resident_shell=replaced_bad_bind;app_main();assert(terminal&&retains==0&&opens==0&&writes==0&&releases==0);
 setup();lifecycle=NULL;declare();const risc_scene_node_v1 *cap=NULL;for(unsigned i=0;i<candidate.node_count;i++)if(candidate.nodes[i].id==24)cap=&candidate.nodes[i];assert(cap&&strstr(cap->text,"AUTOMATIC REFRESH")&&!(cap->flags&RISC_SCENE_HIDDEN));lifecycle=&ui;resident.enabled=false;declare();for(unsigned i=0;i<candidate.node_count;i++)if(candidate.nodes[i].id==24)cap=&candidate.nodes[i];assert(!strcmp(cap->text,"SHARED CONTROLS UNAVAILABLE")&&!(cap->flags&RISC_SCENE_HIDDEN));resident.enabled=true;declare();for(unsigned i=0;i<candidate.node_count;i++)if(candidate.nodes[i].id==24)cap=&candidate.nodes[i];assert(cap->flags&RISC_SCENE_HIDDEN);
 puts("Alarms resident: 15 lifecycle/preservation scenarios passed");return 0;
}
