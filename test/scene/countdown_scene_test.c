/* Production controller with deterministic semantic services; not hardware proof. */
#include "../../Apps/countdown_scene.c"
#include <assert.h>
static risc_runtime_api_v1 rt,other;
static risc_scene_lifecycle_api_v1 ui;
static risc_app_data_v1 data_api;
static countdown_control_api_v1 service;
static unsigned loop_polls,updates,status_reads,releases;static uint32_t busy_flags;static bool loop_mode;
static bool replaced,present,write_fail,write_commits,apply_replaces,dispatch_replaces,close_replaces;
static unsigned closes,opens,configures,dispatches,writes,retains,io;
static int32_t close_rc,dispatch_rc;
static uint8_t bytes[CHECKPOINT_SIZE];static uint32_t length;static uint64_t version;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?(replaced?&other:&rt):NULL;}
static bool hold(void){retains++;return true;}
static void yield_ms(uint32_t n){(void)n;}
static int32_t stat_data(void*c,const char*n,uint32_t*z,uint64_t*v){(void)c;assert(!strcmp(n,CHECKPOINT_NAME));io++;*z=length;*v=version;return present?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t read_data(void*c,const char*n,uint64_t v,void*b,uint32_t cap,uint32_t*z,uint64_t*a){(void)c;(void)n;io++;assert(v==version&&cap>=length);memcpy(b,bytes,length);*z=length;*a=version;return 0;}
static int32_t write_data(void*c,const char*n,uint64_t v,const void*b,uint32_t z){(void)c;(void)n;io++;writes++;assert(v==version&&z==CHECKPOINT_SIZE);if(write_fail&&!write_commits)return RISC_APP_DATA_IO;memcpy(bytes,b,z);length=z;present=true;version++;return write_fail?RISC_APP_DATA_IO:0;}
static int32_t snap(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){(void)c;assert(s==7&&n->struct_size==sizeof(*n));io++;*f=busy_flags;return 0;}
static int32_t close_ui(void*c,uint64_t s){(void)c;assert(s==7);io++;closes++;if(close_replaces)replaced=true;return close_rc;}
static int32_t open_ui(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;io++;opens++;assert(!session&&d->root==OVERVIEW&&n->routes[0]==OVERVIEW);*s=7;return 0;}
static int32_t configure(void*c,uint64_t s,uint32_t f){(void)c;io++;configures++;assert(s==7&&f==(resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));return 0;}
static int32_t dispatch(uint64_t inv,const risc_resident_request_v1*q,risc_resident_reply_v1*p){io++;dispatches++;assert(inv==1&&session==0&&closes==1&&q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);p->flags=0;if(dispatch_replaces)replaced=true;return dispatch_rc;}
static int32_t update_ui(void*c,uint64_t s,const risc_scene_document_v1*d){(void)c;assert(s==7&&d->revision>document.revision&&busy_flags==0);updates++;return 0;}
static int32_t navigate_ui(void*c,uint64_t s,uint32_t op,uint32_t route){(void)c;(void)s;(void)op;(void)route;return 0;}
static int32_t next_ui(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;assert(s==7);loop_polls++;if(loop_polls==1){busy_flags=RISC_SCENE_INPUT_BUSY;return RISC_SCENE_IDLE;}if(loop_polls==2){assert(updates==0);busy_flags=RISC_SCENE_PRESENTING;return RISC_SCENE_IDLE;}if(loop_polls==3){assert(updates==0);busy_flags=0;return RISC_SCENE_IDLE;}assert(loop_polls==4&&updates==1);*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=document.revision,.sequence=1};return 0;}
static int32_t read_status(void*c,countdown_control_snapshot_v1*out){(void)c;*out=(countdown_control_snapshot_v1){.api_version=1,.struct_size=sizeof(*out),.duration_seconds=300};strcpy(out->status,status_reads++?"CHANGED":"INITIAL");return 0;}
static int32_t step_status(void*c){(void)c;return 0;}
static unsigned preparations,applies,dismissals;static int32_t apply_rc;static uint32_t prepared_op,prepared_value,prepared_expected;static countdown_control_command_v1 applied_command;
static int32_t prepare_status(void*c,uint32_t a,uint32_t b,uint32_t d,countdown_control_command_v1*o){(void)c;preparations++;prepared_op=a;prepared_value=b;prepared_expected=d;*o=(countdown_control_command_v1){.struct_size=sizeof(*o)};for(unsigned i=0;i<sizeof(o->bytes);i++)o->bytes[i]=(uint8_t)(i+a);return 0;}
static int32_t apply_status(void*c,const countdown_control_command_v1*q){(void)c;assert(state.pending&&present&&writes);applies++;applied_command=*q;if(apply_replaces)replaced=true;return apply_rc;}
static int32_t dismiss_status(void*c,const countdown_control_occurrence_v1*q){(void)c;assert(q->generation);dismissals++;return 0;}
static bool acquire_api(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1&&id==0);g->slot=1;g->generation=1;g->api=!strcmp(n,RISC_SCENE_CAPABILITY)?(void*)&ui:!strcmp(n,COUNTDOWN_CONTROL_CAPABILITY)?(void*)&service:(void*)&data_api;return true;}
static bool release_api(risc_runtime_capability_v1*g){releases++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool replaced_bad_bind(risc_resident_client_v1*c){replaced=true;other=rt;c->api_version=0;return true;}
static void setup(void){
 preparations=applies=dismissals=0;apply_rc=0;prepared_op=prepared_value=prepared_expected=0;memset(&applied_command,0,sizeof(applied_command));loop_polls=updates=status_reads=releases=busy_flags=0;loop_mode=false;
 service=(countdown_control_api_v1){1,sizeof(service),NULL,step_status,read_status,prepare_status,apply_status,dismiss_status};
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.retain_invocation=hold,.yield_ms=yield_ms,.acquire=acquire_api,.release=release_api};other=rt;runtime=&rt;
 ui=(risc_scene_lifecycle_api_v1){.base={.api_version=1,.struct_size=sizeof(ui),.open=open_ui,.snapshot=snap,.close=close_ui,.update=update_ui,.navigate=navigate_ui,.next=next_ui},.configure=configure};scene=&ui.base;lifecycle=&ui;
 data_api=(risc_app_data_v1){1,sizeof(data_api),NULL,stat_data,read_data,write_data};storage=&data_api;
 resident=(risc_scene_resident_v1){.enabled=true,.client={.invocation=1,.checkpoint=dispatch}};
 terminal=recovery=storage_present=force_read=command_not_attempted=false;storage_known=true;stored_size=0;session=7;last_event=99;
 control=&service;status=(countdown_control_snapshot_v1){0};fresh_model();state.duration_seconds=421;state.duration_dirty=1;root_navigation();navigation.depth=2;navigation.routes[1]=EDIT_DURATION;navigation.focus[1]=10;document=(risc_scene_document_v1){.revision=3};notice[0]=0;
 replaced=present=write_fail=write_commits=apply_replaces=dispatch_replaces=close_replaces=false;closes=opens=configures=dispatches=writes=retains=io=0;close_rc=dispatch_rc=0;length=0;version=0;memset(bytes,0,sizeof(bytes));
}

static unsigned cases;
static const risc_scene_node_v1 *getnode(unsigned id){for(unsigned i=0;i<candidate.node_count;i++)if(candidate.nodes[i].id==id)return &candidate.nodes[i];assert(0);return NULL;}
static void ready_route(unsigned route){root_navigation();if(route!=OVERVIEW){navigation.depth=2;navigation.routes[1]=route;}declare();document=candidate;}
static void event(unsigned nodeid,unsigned action,int value,uint32_t kind){risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=kind,.node=nodeid,.action=action,.value=value,.document_revision=document.revision,.sequence=++last_event};last_event--;assert(!handle_event(&e));}
#define CASE(...) do{setup();__VA_ARGS__;cases++;}while(0)
int main(void){
 CASE(assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS));assert(writes==1&&closes==1&&dispatches==1&&opens==1&&configures==1&&last_event==0&&session==7&&force_read);assert(state.duration_seconds==421&&state.duration_dirty&&navigation.routes[1]==EDIT_DURATION));
 CASE(dispatch_rc=RISC_RESIDENT_BUSY;assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&opens==1));
 CASE(dispatch_rc=RISC_RESIDENT_RETAINED;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&retains==1&&opens==0));
 CASE(dispatch_replaces=true;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&retains==0&&opens==0));
 CASE(close_replaces=true;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&!dispatches&&!opens&&!retains));
 CASE(close_rc=RISC_SCENE_AGAIN;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&closes==250&&!dispatches&&retains==1));
 CASE(write_fail=true;assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&writes==1&&configures==1&&!closes&&!dispatches&&session==7));
 CASE(recovery=true;present=true;length=17;memset(bytes,0x5a,length);assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&!writes&&recovery&&length==17);for(unsigned i=0;i<length;i++)assert(bytes[i]==0x5a));
 CASE(state.pending=1;state.pending_result=COUNTDOWN_CONTROL_UNCERTAIN;memset(state.command.bytes,0xa7,sizeof(state.command.bytes));assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS));uint8_t payload[PAYLOAD_SIZE];size_t n=0;risc_scene_navigation_v1 nav;assert(risc_scene_state_decode(bytes,length,CHECKPOINT_SCHEMA,&nav,payload,sizeof(payload),&n));assert(risc_scene_get32(payload+12)==1);for(unsigned i=24;i<PAYLOAD_SIZE;i++)assert(payload[i]==0xa7);fresh_model();assert(load_checkpoint()&&state.pending&&state.pending_result==COUNTDOWN_CONTROL_UNCERTAIN);for(unsigned i=0;i<sizeof(state.command.bytes);i++)assert(state.command.bytes[i]==0xa7));
 CASE(write_fail=true;ready_route(OVERVIEW);event(0,0,0,RISC_SCENE_SUSPEND_EVENT);assert(configures==1&&!closes&&writes==1&&session==7));
 CASE(app_main();assert(loop_polls==4&&updates==1&&releases==3&&closes==1&&writes==1));
 CASE(rt.resident_shell=replaced_bad_bind;app_main();assert(terminal&&!retains&&!opens&&!writes&&!releases));
 CASE(fresh_model();assert(state.duration_seconds==300&&!state.duration_dirty&&!state.pending));
 CASE(status.duration_seconds=600;status.remaining_seconds=42;status.flags=COUNTDOWN_CONTROL_ENABLED|COUNTDOWN_CONTROL_CLOCK_VALID;state.duration_dirty=0;declare();assert(!strcmp(getnode(1)->text,"00:00:42"));state.duration_dirty=1;state.duration_seconds=359999;declare();assert(!strcmp(getnode(1)->text,"99:59:59")&&!preparations&&!applies);status.flags&=~COUNTDOWN_CONTROL_CLOCK_VALID;state.duration_dirty=0;declare();assert(!strcmp(getnode(1)->text,"99:59:59")));
 CASE(state.duration_seconds=0;status.flags=COUNTDOWN_CONTROL_CONFIG_VALID|COUNTDOWN_CONTROL_CLOCK_VALID|COUNTDOWN_CONTROL_SERVICE_READY;ready_route(EDIT_DURATION);assert(getnode(4)->flags&RISC_SCENE_DISABLED);event(10,CHANGE_HOURS,99,RISC_SCENE_VALUE_EVENT);ready_route(EDIT_DURATION);event(11,CHANGE_MINUTES,59,RISC_SCENE_VALUE_EVENT);ready_route(EDIT_DURATION);event(12,CHANGE_SECONDS,59,RISC_SCENE_VALUE_EVENT);assert(state.duration_seconds==359999&&state.duration_dirty&&!preparations&&!applies);ready_route(EDIT_DURATION);event(10,CHANGE_HOURS,100,RISC_SCENE_VALUE_EVENT);assert(state.duration_seconds==359999));
 CASE(status.flags=COUNTDOWN_CONTROL_CONFIG_VALID|COUNTDOWN_CONTROL_CLOCK_VALID|COUNTDOWN_CONTROL_SERVICE_READY;state.duration_seconds=1;state.base_revision=17;ready_route(OVERVIEW);event(4,START_COUNTDOWN,0,RISC_SCENE_ACTION_EVENT);assert(preparations==1&&prepared_op==COUNTDOWN_CONTROL_ARM&&prepared_value==1&&prepared_expected==17&&applies==1&&!state.pending&&writes==2));
 CASE(ready_route(CONFIRM_CANCEL);event(17,CANCEL_COUNTDOWN,0,RISC_SCENE_ACTION_EVENT);assert(prepared_op==COUNTDOWN_CONTROL_CANCEL&&prepared_value==0&&applies==1&&!state.pending));
 CASE(apply_rc=COUNTDOWN_CONTROL_UNCERTAIN;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(state.pending&&applies==1&&writes==1&&!reset_allowed());countdown_control_command_v1 exact=state.command;ready_route(CONFIRM_CANCEL);event(17,CANCEL_COUNTDOWN,0,RISC_SCENE_ACTION_EVENT);assert(preparations==1&&applies==1);fresh_model();assert(load_checkpoint()&&state.pending&&!memcmp(&state.command,&exact,sizeof(exact)));apply_rc=0;assert(apply_pending()&&!state.pending&&applies==2&&preparations==1&&!memcmp(&applied_command,&exact,sizeof(exact))));
 CASE(write_fail=true;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(state.pending&&command_not_attempted&&reset_allowed()&&!applies));
 CASE(apply_rc=COUNTDOWN_CONTROL_STALE;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(state.pending&&reset_allowed()&&state.pending_result==COUNTDOWN_CONTROL_STALE));
 CASE(apply_rc=COUNTDOWN_CONTROL_RETAINED;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(terminal&&retains==1&&state.pending&&writes==1));
 CASE(assert(checkpoint(false));bytes[0]^=1;unsigned oldwrites=writes;assert(!load_checkpoint()&&recovery);assert(!checkpoint(false)&&writes==oldwrites));
 CASE(assert(checkpoint(false));bytes[length-1]^=1;state.duration_seconds++;assert(!checkpoint(false)&&writes==1&&strstr(notice,"CHANGED ELSEWHERE")));
 CASE(uint8_t p[PAYLOAD_SIZE];encode_payload(p);risc_scene_put32(p,360000);assert(!decode_payload(p,sizeof(p)));encode_payload(p);risc_scene_put32(p+8,2);assert(!decode_payload(p,sizeof(p)));encode_payload(p);p[24]=1;assert(!decode_payload(p,sizeof(p))));
 CASE(ready_route(OVERVIEW);risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_VALUE_EVENT,.node=10,.action=CHANGE_HOURS,.value=5,.document_revision=document.revision,.sequence=100};assert(!handle_event(&e)&&state.duration_seconds==421);e.document_revision++;e.sequence++;assert(!handle_event(&e)&&state.duration_seconds==421);ready_route(EDIT_DURATION);e.document_revision=document.revision;e.sequence=last_event;assert(!handle_event(&e)&&state.duration_seconds==421));
 CASE(lifecycle=NULL;declare();assert(strstr(getnode(22)->text,"AUTOMATIC REFRESH")&&candidate.node_count==23);lifecycle=&ui;resident.enabled=false;declare();assert(!strcmp(getnode(22)->text,"SHARED CONTROLS UNAVAILABLE"));resident.enabled=true;declare();assert(getnode(22)->flags&RISC_SCENE_HIDDEN));
 CASE(message("SAVE UNCONFIRMED");declare();assert(strstr(getnode(23)->text,"VISUAL ONLY")&&!strcmp(getnode(2)->text,"SAVE UNCONFIRMED"));status.flags=COUNTDOWN_CONTROL_HAS_SOUND;declare();assert(!strcmp(getnode(23)->text,"HAPTIC UNAVAILABLE"));status.flags=COUNTDOWN_CONTROL_HAS_HAPTIC;declare();assert(!strcmp(getnode(23)->text,"SOUND UNAVAILABLE"));status.flags|=COUNTDOWN_CONTROL_HAS_SOUND;declare();assert(getnode(23)->flags&RISC_SCENE_HIDDEN));
 CASE(status.flags=COUNTDOWN_CONTROL_ALERT|COUNTDOWN_CONTROL_ENABLED|COUNTDOWN_CONTROL_CONFIG_VALID|COUNTDOWN_CONTROL_CLOCK_VALID|COUNTDOWN_CONTROL_SERVICE_READY;status.occurrence=(countdown_control_occurrence_v1){1,2,3,4};ready_route(OVERVIEW);assert(getnode(3)->flags&RISC_SCENE_DISABLED);assert(getnode(4)->flags&RISC_SCENE_DISABLED);assert(getnode(5)->flags&RISC_SCENE_DISABLED);event(6,DISMISS,0,RISC_SCENE_ACTION_EVENT);assert(dismissals==1&&!applies));
 CASE(write_fail=write_commits=true;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(applies==1&&!state.pending&&writes==2));
 CASE(apply_replaces=true;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(terminal&&state.pending&&applies==1&&!retains&&!releases&&writes==1));
 CASE(status.flags=COUNTDOWN_CONTROL_ALERT;prepare_command(COUNTDOWN_CONTROL_ARM,421,0);assert(!preparations&&!applies&&!writes);state.pending=1;state.pending_result=COUNTDOWN_CONTROL_UNCERTAIN;assert(!apply_pending()&&!applies&&!writes));
 CASE(status.duration_seconds=600;status.revision=7;state.duration_seconds=421;state.duration_dirty=1;ready_route(EDIT_DURATION);event(15,DISCARD_DURATION,0,RISC_SCENE_ACTION_EVENT);assert(state.duration_seconds==600&&!state.duration_dirty&&state.base_revision==7&&!writes&&!applies));
 CASE(state.pending=1;state.pending_result=COUNTDOWN_CONTROL_UNCERTAIN;declare();assert(!reset_allowed()&&(getnode(20)->flags&RISC_SCENE_DISABLED));state.pending_result=COUNTDOWN_CONTROL_EXPIRED;assert(reset_allowed()));
 CASE(status.flags=COUNTDOWN_CONTROL_ALERT;ready_route(EDIT_DURATION);assert(getnode(10)->flags&RISC_SCENE_DISABLED);event(10,CHANGE_HOURS,10,RISC_SCENE_VALUE_EVENT);assert(state.duration_seconds==421);ready_route(CONFIRM_CANCEL);event(17,CANCEL_COUNTDOWN,0,RISC_SCENE_ACTION_EVENT);assert(!preparations&&!applies&&!writes));
 CASE(status.flags=COUNTDOWN_CONTROL_ALERT;ready_route(OVERVIEW);event(0,LEAVE_APP,0,RISC_SCENE_ACTION_EVENT);assert(!writes&&!closes&&strstr(notice,"DISMISS ALERT BEFORE BACK"))); 
 CASE(status.flags=COUNTDOWN_CONTROL_ALERT;ready_route(OVERVIEW);risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=document.revision,.sequence=100};assert(handle_event(&e)&&writes==1&&!applies));
 CASE(ready_route(OVERVIEW);risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.action=LEAVE_APP,.document_revision=document.revision,.sequence=100};assert(handle_event(&e)&&writes==1&&!applies));
 printf("Countdown scene: %u lifecycle/data/intent/identity/capability scenarios passed (semantic doubles, not hardware)\n",cases);return 0;
}
