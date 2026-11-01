/* Actual production root and session/background owners; explicit endpoint doubles. */
#include "../../Apps/frequency_scene.c"
#include <assert.h>
#include <stdlib.h>
static risc_runtime_api_v1 test_rt,other_rt;
static const risc_runtime_api_v1*test_current=&test_rt;
static risc_scene_components_api_v1 test_scene;
static twatch_audio_out_api_v1 test_audio;
static alarm_service_v1 test_alarm;
static risc_power_key_api_v1 test_key;
static risc_key_value_v1 test_kv;
static bool test_retained,test_no_ui,test_loop,test_audio_open,test_close_ok=true,test_alarm_on;
static unsigned test_calls,test_retains,test_opens,test_writes,test_closes,test_scene_opens,test_scene_closes,test_updates,test_nexts,test_checkpoints,test_queries,test_polls,test_policies,test_controls,test_configures,test_alarm_steps,test_acks,test_stops;
static unsigned test_close_again,test_checkpoint_busy,test_manual_bits,test_snapshot_flags;
static uint32_t test_time;static int test_snapshot_bad,test_alarm_bad,test_timer_error,test_stop_rc=ALARM_OK;
static bool test_policy_pending,test_set_cue,test_held,test_activity;
static alarm_status_v1 test_status;
static const char*test_mode;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t api){return api==1&&!test_retained?test_current:NULL;}
static void test_call(void){assert(!test_retained);++test_calls;}
static bool test_retain(void){++test_retains;test_retained=true;return true;}
static bool test_clock(uint32_t*out){test_call();if(test_loop)test_time+=1000;*out=test_time;return true;}
static void test_yield(uint32_t ms){test_call();(void)ms;}
static bool test_open_audio(void*c,uint32_t rate,uint8_t channels){(void)c;test_call();assert(!test_audio_open&&rate==16000&&channels==1);test_audio_open=true;++test_opens;return true;}
static bool test_write_audio(void*c,const int16_t*p,size_t n){(void)c;test_call();assert(test_audio_open&&p&&n==256);++test_writes;return true;}
static bool test_close_audio(void*c){(void)c;test_call();assert(test_audio_open);++test_closes;if(test_close_ok)test_audio_open=false;return test_close_ok;}
static int32_t test_open_scene(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*out){
 (void)c;test_call();assert(d->node_count<=RISC_COMPONENTS_MAX_NODES&&d->route_count==1&&d->root==1&&n->depth==1&&n->routes[0]==1);
 ++test_scene_opens;*out=100+test_scene_opens;return RISC_SCENE_OK;
}
static int32_t test_update_scene(void*c,uint64_t id,const risc_components_document_v1*d){(void)c;test_call();assert(id==session&&d->revision>document.revision);++test_updates;return RISC_SCENE_OK;}
static int32_t test_configure(void*c,uint64_t id,uint32_t features){(void)c;test_call();assert(id==session&&features==(resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));++test_configures;return RISC_SCENE_OK;}
static int32_t test_close_scene(void*c,uint64_t id){(void)c;test_call();assert(id==session);++test_scene_closes;if(test_close_again){--test_close_again;return RISC_SCENE_AGAIN;}return RISC_SCENE_OK;}
static unsigned test_node(unsigned act){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==act)return document.nodes[i].id;return 0;}
static int32_t test_next(void*c,uint64_t id,risc_scene_event_v1*out){
 (void)c;test_call();assert(id==session);++test_nexts;
 if(test_loop&&test_nexts==1&&strcmp(test_mode,"loop-quiet-off")&&strcmp(test_mode,"loop-idle-off")){*out=(risc_scene_event_v1){.struct_size=sizeof(*out),.kind=RISC_SCENE_ACTION_EVENT,.sequence=1,.document_revision=document.revision,.node=test_node(FG_TOGGLE),.action=FG_TOGGLE};return RISC_SCENE_OK;}
 if(test_loop&&test_nexts>=12){*out=(risc_scene_event_v1){.struct_size=sizeof(*out),.kind=RISC_SCENE_SUSPEND_EVENT,.sequence=100,.document_revision=document.revision};return RISC_SCENE_OK;}
 return RISC_SCENE_IDLE;
}
static int32_t test_snapshot(void*c,uint64_t id,risc_scene_navigation_v1*n,uint32_t*out){
 (void)c;test_call();assert(id==session);*n=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(*n),.depth=1,.routes={1}};
 *out=test_snapshot_flags|(test_held?RISC_SCENE_INPUT_BUSY:0)|(test_activity?RISC_SCENE_ACTIVITY:0);
 if(test_snapshot_bad==1)n->depth=0;
 if(test_snapshot_bad==2)n->routes[0]=2;
 if(test_snapshot_bad==3)n->focus[0]=999;
 if(test_snapshot_bad==4)*out|=0x8000;
 return RISC_SCENE_OK;
}
static int32_t test_checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 test_call();assert(invocation==123&&!synth.owned&&!test_audio_open&&!frequency_background_capture_active());++test_checkpoints;
 if(q->reason==RISC_RESIDENT_QUERY_IDLE_ONLY){assert(!session);++test_queries;if(test_checkpoint_busy){--test_checkpoint_busy;return RISC_RESIDENT_BUSY;}return RISC_RESIDENT_OK;}
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){++test_polls;if(test_checkpoint_busy){--test_checkpoint_busy;return RISC_RESIDENT_BUSY;}r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;test_policy_pending=true;return RISC_RESIDENT_OK;}
 assert(!session);
 if(q->reason==RISC_RESIDENT_APPLY_IDLE_ONLY){++test_policies;}
 else if(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY){assert(test_policy_pending);++test_policies;test_policy_pending=false;}
 else {assert(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);++test_controls;}
 if(test_checkpoint_busy){--test_checkpoint_busy;return RISC_RESIDENT_BUSY;}
 return RISC_RESIDENT_OK;
}
static bool test_resident(risc_resident_client_v1*c){test_call();*c=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*c),.role=RISC_RESIDENT_ROLE_FOREGROUND,.invocation=123,.checkpoint=test_checkpoint};return true;}
static int32_t test_alarm_status(void*c,alarm_status_v1*out){(void)c;test_call();*out=test_status;if(test_alarm_bad==1)out->output_uncertain=1;if(test_alarm_bad==2)out->state=99;if(test_alarm_bad==3)memset(out->label,'x',sizeof(out->label));return ALARM_OK;}
static int32_t test_alarm_step(void*c){(void)c;test_call();++test_alarm_steps;if(test_status.state==ALARM_STATE_CUE||test_status.state==ALARM_STATE_ALERT||test_status.state==ALARM_STATE_BLOCKED)assert(!test_audio_open);if(test_set_cue){test_set_cue=false;test_status.state=ALARM_STATE_CUE;}return ALARM_PENDING;}
static int32_t test_alarm_refresh(void*c){(void)c;test_call();return ALARM_PENDING;}
static int32_t test_alarm_ack(void*c,const alarm_token_v1*t){(void)c;test_call();assert(!memcmp(t,&test_status.occurrence,sizeof(*t)));++test_acks;return ALARM_PENDING;}
static int32_t test_alarm_stop(void*c){(void)c;test_call();++test_stops;assert(!test_audio_open);return test_stop_rc;}
static bool test_keys(void*c,uint32_t*out){(void)c;test_call();*out=test_manual_bits;return true;}
static int32_t test_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){(void)c;test_call();assert(!strcmp(k,"sleep_idle")&&cap>=5);*n=0;if(test_timer_error)return test_timer_error;const uint8_t bytes[]={0x54,1,5,0,5^0xa5};memcpy(out,bytes,5);*n=5;return 0;}
static bool test_acquire(const char*n,uint32_t api,uint64_t instance,risc_runtime_capability_v1*g){
 test_call();assert(api==1);const void*p=NULL;
 if(!strcmp(n,RISC_SCENE_CAPABILITY)){if(test_no_ui)return false;p=&test_scene;}
 else if(!strcmp(n,"audio.output"))p=&test_audio;
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){if(!test_alarm_on)return false;p=&test_alarm;}
 else if(!strcmp(n,RISC_POWER_KEY_CAPABILITY))p=&test_key;
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);p=&test_kv;}
 else {assert(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)||!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY));return false;}
 g->api=p;g->slot=test_calls;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){test_call();assert(g->api&&g->slot&&g->generation&&!test_audio_open);if(g->api==&test_scene)assert(!session);*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void test_setup(void){
 test_rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(test_rt),.acquire=test_acquire,.release=test_release,.retain_invocation=test_retain,.monotonic_ms=test_clock,.yield_ms=test_yield,.resident_shell=test_resident};
 test_audio=(twatch_audio_out_api_v1){.api_version=1,.struct_size=sizeof(test_audio),.open=test_open_audio,.write=test_write_audio,.close=test_close_audio};
 test_scene=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(test_scene),.next=test_next,.snapshot=test_snapshot,.close=test_close_scene},.configure=test_configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=test_open_scene,.update=test_update_scene};
 test_alarm=(alarm_service_v1){.api_version=1,.struct_size=sizeof(test_alarm),.status=test_alarm_status,.step=test_alarm_step,.refresh=test_alarm_refresh,.acknowledge=test_alarm_ack,.stop_only=test_alarm_stop};
 test_status=(alarm_status_v1){.api_version=1,.struct_size=sizeof(test_status),.state=ALARM_STATE_READY};
 test_key=(risc_power_key_api_v1){1,sizeof(test_key),NULL,test_keys};test_kv=(risc_key_value_v1){1,sizeof(test_kv),NULL,test_get,no_put};
 rt=&test_rt;scene=&test_scene.lifecycle.base;components=&test_scene;lifecycle=&test_scene.lifecycle;audio=&test_audio;alarm=&test_alarm;power_key=&test_key;settings=&test_kv;
 frequency_init(&synth,alive,retain,NULL);assert(frequency_background_init(rt,alive,retain,NULL));
 resident=(risc_scene_resident_v1){.client={.api_version=1,.struct_size=sizeof(risc_resident_client_v1),.role=RISC_RESIDENT_ROLE_FOREGROUND,.invocation=123,.checkpoint=test_checkpoint},.enabled=true};
 navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={1}};assert(open_scene());
}
static void test_start(void){assert(action(FG_TOGGLE,10)&&synth.owned&&test_audio_open);}
int main(int argc,char**argv){
 assert(argc==2);test_mode=argv[1];test_setup();
 if(!strcmp(test_mode,"document")){assert(document.node_count>=12&&test_configures==1);assert(test_node(FG_TOGGLE)&&test_node(FG_STEP1000)&&test_node(FG_LOUDER));}
 else if(!strcmp(test_mode,"identity-before")){test_current=&other_rt;unsigned n=test_calls;assert(!action(FG_TOGGLE,0)&&terminal&&test_calls==n);}
 else if(!strcmp(test_mode,"level-precision")){assert(action(FG_QUIETER,0)&&synth.level==99);assert(action(FG_LOUDER,1)&&synth.level==100);}
 else if(!strcmp(test_mode,"event-fences")){risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.sequence=1,.document_revision=document.revision-1,.node=test_node(FG_TOGGLE),.action=FG_TOGGLE};assert(handle(&e,0)&&!test_opens);e.sequence=2;e.document_revision=document.revision;e.node=999;assert(handle(&e,0)&&!test_opens);e.sequence=3;e.node=test_node(FG_TOGGLE);assert(handle(&e,0)&&test_opens==1);assert(handle(&e,0)&&test_opens==1&&!test_closes);assert(stop_tone(NULL));}
 else if(!strcmp(test_mode,"paired-toggle")){
  risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.sequence=1,.document_revision=document.revision,.node=test_node(FG_TOGGLE),.action=FG_TOGGLE};
  toggle_in_batch=false;assert(handle(&e,0)&&synth.owned&&test_opens==1&&!test_closes);
  e.sequence=2;assert(handle(&e,0)&&synth.owned&&test_opens==1&&!test_closes);
  toggle_in_batch=false;e.sequence=3;assert(handle(&e,1)&&!synth.owned&&test_closes==1);
 }
 else if(!strcmp(test_mode,"manual-neutral")){test_manual_bits=2;assert(manual(0)&&!leave);test_manual_bits=0;assert(manual(0));test_manual_bits=1;assert(manual(0)&&!leave);test_manual_bits=2;assert(manual(0)&&!leave);test_manual_bits=0;assert(manual(0));test_manual_bits=3;assert(manual(0)&&!leave);test_manual_bits=0;assert(manual(0));test_manual_bits=2;assert(manual(0)&&leave);}
 else if(!strcmp(test_mode,"controls")){test_start();assert(checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS));assert(test_closes==1&&test_controls==1&&test_scene_opens==2&&!synth.playing&&!pending_checkpoint);}
 else if(!strcmp(test_mode,"close-again")){test_start();test_close_again=1;assert(checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&pending_checkpoint&&test_closes==1&&!test_controls);assert(checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&!pending_checkpoint&&test_controls==1&&test_closes==1);}
 else if(!strcmp(test_mode,"checkpoint-busy")){test_start();test_checkpoint_busy=1;assert(checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&pending_checkpoint&&!session);assert(checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&!pending_checkpoint&&test_closes==1&&!synth.playing);}
 else if(!strcmp(test_mode,"idle-policy")){test_start();assert(checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY));assert(test_polls==0&&test_policies==1&&!pending_checkpoint&&!synth.playing);}
 else if(!strcmp(test_mode,"idle-poll-busy")){test_start();test_checkpoint_busy=1;assert(checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY)&&pending_checkpoint&&test_policies==1);assert(checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY)&&!pending_checkpoint&&test_polls==0&&test_policies==2&&test_closes==1);}
 else if(!strcmp(test_mode,"failed-close")){test_start();test_close_ok=false;assert(!checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&!test_checkpoints&&test_retains==1);unsigned n=test_calls;assert(!checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&test_calls==n);}
 else if(!strcmp(test_mode,"alarm-cue")||!strcmp(test_mode,"alarm-alert")||!strcmp(test_mode,"alarm-blocked")){test_start();test_status.state=!strcmp(test_mode,"alarm-cue")?ALARM_STATE_CUE:!strcmp(test_mode,"alarm-alert")?ALARM_STATE_ALERT:ALARM_STATE_BLOCKED;assert(alarm_read()&&alarm_reserved&&!test_audio_open&&test_closes==1);assert(alarm_modal==(test_status.state!=ALARM_STATE_CUE));assert(alarm_step()&&test_alarm_steps==1);test_status.state=ALARM_STATE_READY;assert(alarm_read()&&!alarm_reserved&&!synth.playing&&test_opens==1);}
 else if(!strcmp(test_mode,"alarm-emerges")){test_start();test_set_cue=true;assert(alarm_step()&&alarm_reserved&&!test_audio_open&&test_closes==1);assert(alarm_step()&&test_alarm_steps==2);}
 else if(!strcmp(test_mode,"alarm-ack")){test_status.state=ALARM_STATE_ALERT;test_status.occurrence=(alarm_token_v1){1,2,3,4};assert(alarm_read()&&action(FG_ACK,0)&&test_acks==1);}
 else if(!strcmp(test_mode,"alarm-uncertain")||!strcmp(test_mode,"alarm-malformed")||!strcmp(test_mode,"alarm-label")){test_alarm_bad=!strcmp(test_mode,"alarm-uncertain")?1:!strcmp(test_mode,"alarm-malformed")?2:3;assert(!alarm_read()&&terminal&&test_retains==1&&!test_alarm_steps);}
 else if(!strcmp(test_mode,"alarm-stop-failed")){test_stop_rc=ALARM_OUTPUT;assert(!alarm_quiet()&&terminal&&test_stops==1);}
 else if(!strcmp(test_mode,"timer-context")){test_timer_error=RISC_KEY_VALUE_CONTEXT;assert(!load_idle()&&terminal&&test_retains==1);}
 else if(!strcmp(test_mode,"navigation")){assert(valid_navigation());navigation.depth=0;assert(!valid_navigation());navigation.depth=1;navigation.routes[0]=2;assert(!valid_navigation());navigation.routes[0]=1;navigation.focus[0]=999;assert(!valid_navigation());navigation.focus[0]=0;flags=0x1000;assert(!valid_navigation());}
 else if(!strncmp(test_mode,"loop-",5)){
  assert(close_scene()&&frequency_background_cleanup());test_opens=test_closes=test_writes=test_checkpoints=test_queries=test_polls=test_policies=test_controls=test_nexts=0;
  test_loop=true;test_held=!strcmp(test_mode,"loop-held");test_activity=!strcmp(test_mode,"loop-activity")||!strcmp(test_mode,"loop-quiet-off");
  if(!strcmp(test_mode,"loop-navigation"))test_snapshot_bad=1;
  if(!strcmp(test_mode,"loop-flags"))test_snapshot_bad=4;
  app_main();
  if(test_snapshot_bad){assert(terminal&&test_retains==1);}
  else if(!strcmp(test_mode,"loop-quiet-off")){assert(!terminal&&!test_opens&&!test_closes&&test_checkpoints==test_queries&&test_queries==1);}
  else if(!strcmp(test_mode,"loop-idle-off")){assert(!terminal&&!test_opens&&!test_closes&&test_queries==1&&test_polls==0&&test_policies>=1);}
  else {assert(!terminal&&test_opens==1&&test_closes==1&&!test_audio_open);if(test_held||test_activity)assert(test_checkpoints==test_queries&&test_queries==1&&test_writes>=10);else assert(test_policies>=1&&test_writes<10);}
 }
 else assert(false);
 printf("frequency root %s PASS\n",test_mode);return 0;
}
