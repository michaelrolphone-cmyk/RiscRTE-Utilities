/* Actual production root; only published endpoints are doubled. */
#include "ble_scanner_scene.c"
#include <assert.h>
static risc_runtime_api_v1 test_rt,other_rt;static const risc_runtime_api_v1*identity;
static uint32_t test_now,scene_flags,text_flags,last_reason;static int query_rc,apply_rc,pause_rc;static unsigned pause_calls,resume_calls,manual_injected;
static bool checkpoint_policy;static unsigned attention_calls,reject_calls,attention_bits;static int attention_result;
static unsigned loop_mode,loop_tick,loop_quiet_count;static bool dirty_again;
static unsigned io_count,retained_count,next_count,close_count,text_close_count,releases,checkpoint_count,acquires;
static bool sensor_close_ok,scene_close_again,text_close_again,no_ui,pref_available;
static int text_open_result,text_poll_result;static bool commit_success;static unsigned collect_pending;static risc_text_entry_state_v1 result;
static risc_scene_event_v1 queue[4];static unsigned queued,qi;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?identity:NULL;}
static void io(void){assert(!terminal);io_count++;}
static bool retain_endpoint(void){retained_count++;return true;}
static void editor_idle_setup(void);
static void yield_endpoint(uint32_t n);
static bool health_endpoint(risc_runtime_health_v1*h){io();h->uptime_ms=100;return true;}
static bool clock_endpoint(uint32_t*n){io();*n=test_now;return true;}
static bool sensor_close(void*c,uint64_t t){(void)c;assert(t==7);io();close_count++;return sensor_close_ok;}
static bool sensor_status(void*c,risc_ble_sensor_status_v1*s){(void)c;io();s->state=RISC_BLE_SENSOR_COMPLETE;return true;}
static risc_bluetooth_sensors_v1 sensor_api={.api_version=1,.struct_size=sizeof(sensor_api),.close=sensor_close,.status=sensor_status};
static int scene_next(void*c,uint64_t t,risc_scene_event_v1*e){(void)c;(void)t;io();next_count++;if(qi==queued)return RISC_SCENE_IDLE;*e=queue[qi++];return 0;}
static int scene_snapshot(void*c,uint64_t t,risc_scene_navigation_v1*n,uint32_t*f){(void)c;(void)t;io();n->depth=1;n->routes[0]=1;*f=scene_flags;return 0;}
static int scene_close(void*c,uint64_t t){(void)c;(void)t;io();return scene_close_again?RISC_SCENE_AGAIN:0;}
static risc_scene_api_v1 scene_api={.api_version=1,.struct_size=sizeof(scene_api),.next=scene_next,.snapshot=scene_snapshot,.close=scene_close};
static int text_open(void*c,const risc_text_entry_request_v1*q,uint64_t*t){(void)c;assert(q->capacity==25);io();if(text_open_result==0)*t=9;return text_open_result;}
static int component_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){(void)c;(void)d;(void)n;io();*t=1;return 0;}
static int component_update(void*c,uint64_t t,const risc_components_document_v1*d){(void)c;(void)t;(void)d;io();return 0;}
static risc_scene_components_api_v1 component_api;
static int text_poll(void*c,uint64_t t,risc_text_entry_state_v1*v){(void)c;assert(t==9);io();if(text_poll_result==0||dirty_again)*v=result;return text_poll_result;}
static int text_close_endpoint(void*c,uint64_t t){(void)c;assert(t==9);io();text_close_count++;return text_close_again?RISC_TEXT_ENTRY_AGAIN:0;}
static risc_text_entry_api_v1 text_endpoint={1,sizeof(text_endpoint),NULL,text_open,text_poll,text_close_endpoint};
static int pref_state(void*c,risc_key_value_atomic_state_v1*s){(void)c;io();*s=(risc_key_value_atomic_state_v1){sizeof(*s),4,1,2,64,512,192,0,10};return 0;}
static int pref_get(void*c,uint64_t r,const char*k,void*b,uint32_t z,uint32_t*n){(void)c;(void)r;(void)k;(void)b;(void)z;io();*n=0;return RISC_ATOMIC_NOT_FOUND;}
static int pref_submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*i,uint32_t z,uint64_t*t){(void)c;(void)r;(void)m;(void)n;(void)i;(void)z;io();*t=77;return 0;}
static int pref_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){(void)c;assert(t==77);io();if(collect_pending){collect_pending--;return RISC_ATOMIC_PENDING;}r->status=commit_success?RISC_ATOMIC_OK:RISC_ATOMIC_CANCELLED;r->revision=commit_success?11:10;r->operation_id=commit_success?9:0;return 0;}
static int pref_cancel(void*c,uint64_t t){(void)c;assert(t==77);io();return 0;}
static risc_preference_store_v1 preference_api={1,sizeof(preference_api),NULL,pref_state,pref_get,pref_submit,pref_collect,pref_cancel};
static bool acquire_endpoint(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){(void)v;(void)i;io();acquires++;if(no_ui)return false;const void*a=NULL;if(loop_mode&&!strcmp(n,RISC_SCENE_CAPABILITY))a=&component_api;if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY))a=&text_endpoint;if(pref_available&&!strcmp(n,RISC_PREFERENCE_STORE_CAPABILITY))a=&preference_api;if(!a)return false;g->api=a;g->slot=1;g->generation=1;return true;}
static bool release_endpoint(risc_runtime_capability_v1*g){io();releases++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static int outcome_rc;static uint32_t outcome_flags;static unsigned idle_apply_calls;
static int checkpoint_endpoint(uint64_t t,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 (void)t;(void)r;io();checkpoint_count++;last_reason=q->reason;
 assert(q->reason!=RISC_RESIDENT_CHECKPOINT_POLL&&q->reason!=RISC_RESIDENT_CHECKPOINT_POLICY);
 assert(!q->flags&&!q->reserved&&!scan.token&&!prefs.preference.api&&!prefs.alias.token);
 assert(!ui_token&&(!text_token||text_paused));
 /* A physical key arriving after the child's attention sample stays pending.
  * The old POLL endpoint would consume it and select sleep/exit here. */
 if(q->reason==RISC_RESIDENT_QUERY_IDLE_ONLY||q->reason==RISC_RESIDENT_QUERY_ALARM_ONLY)return query_rc;
 if(q->reason==RISC_RESIDENT_QUERY_LAST_IDLE_ALARM){r->flags=outcome_flags;return outcome_rc;}
 if(q->reason==RISC_RESIDENT_APPLY_IDLE_ONLY){idle_apply_calls++;return apply_rc;}
 return 0;
}
static void reset(void){outcome_rc=0;outcome_flags=idle_apply_calls=0;text_idle_applied=false;test_now=100;scene_flags=text_flags=last_reason=0;query_rc=apply_rc=pause_rc=0;pause_calls=resume_calls=manual_injected=0;idle_at=100;idle_ms=60000;idle_pending=idle_queried=host_alarm_supported=false;terminal=leaving=false;loop_mode=loop_tick=loop_quiet_count=0;dirty_again=false;io_count=retained_count=next_count=close_count=text_close_count=releases=checkpoint_count=acquires=0;queued=qi=0;sensor_close_ok=true;scene_close_again=text_close_again=no_ui=false;pref_available=true;commit_success=false;collect_pending=0;cancel_pending=back_after_cancel=selected_valid=false;committed_count=0;text_open_result=text_poll_result=0;test_rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(test_rt),.acquire=acquire_endpoint,.release=release_endpoint,.health=health_endpoint,.monotonic_ms=clock_endpoint,.yield_ms=yield_endpoint,.retain_invocation=retain_endpoint};identity=rt=&test_rt;ui_token=1;text_token=sequence=0;revision=1;dirty=true;pending_name=save_pending=save_retry=false;handoff_reason=attention_pending=0;text_closing=text_paused=text_pausing=false;view=(scanner_presenter_model){.sensor_available=true,.name_available=true};document=(risc_components_document_v1){0};scene=&scene_api;component_api=(risc_scene_components_api_v1){.tag=RISC_COMPONENTS_TAG,.version=1,.open=component_open,.update=component_update};component_api.lifecycle.base=scene_api;component_api.lifecycle.base.struct_size=sizeof(component_api);components=&component_api;text_api=NULL;text_checkpoint=NULL;lifecycle=NULL;ui_grant=(risc_runtime_capability_v1){0};text_grant=(risc_runtime_capability_v1){0};resident=(risc_scene_resident_v1){0};scanner_session_init(&scan,rt,alive,retain,NULL);assert(scanner_preferences_init(&prefs,rt,alive,retain,NULL)==0);assert(scanner_background_init(rt,alive,retain,NULL));result=(risc_text_entry_state_v1){.struct_size=sizeof(result),.state=RISC_TEXT_ENTRY_CANCELLED};}
static void scan_owned(void){scan.token=7;scan.api=&sensor_api;scan.grant=(risc_runtime_capability_v1){.struct_size=sizeof(scan.grant),.api=&sensor_api,.slot=1,.generation=1};scan.status.state=RISC_BLE_SENSOR_SCANNING;}

static void yield_endpoint(uint32_t n){(void)n;assert(!terminal);if(!loop_mode)return;
 if(loop_mode>=3&&loop_mode<=5){
  assert(loop_tick<4);
  if(loop_tick==0){
   assert(close_ui());editor_idle_setup();
   outcome_flags=loop_mode==3?RISC_RESIDENT_REPLY_REDRAW:0;
   apply_rc=loop_mode==4?RISC_RESIDENT_EXIT:loop_mode==5?RISC_RESIDENT_HANDOFF:RISC_RESIDENT_OK;
   text_close_again=true;
  }else if(loop_tick==1){
   assert(text_token==9&&text_paused&&text_closing&&!text_close_count&&!resume_calls);
   assert(idle_apply_calls==1&&!save_pending&&!leaving);
  }else if(loop_tick==2){
   assert(text_token==9&&text_paused&&text_closing&&text_close_count==1&&!resume_calls);
   assert(idle_apply_calls==1&&!save_pending&&!leaving);text_close_again=false;
  }else{
   assert(!text_token&&!text_paused&&!text_closing&&text_close_count==2);
   assert(!resume_calls&&!save_pending&&idle_apply_calls==1);
   assert(leaving==(loop_mode!=3));
   assert(!!(text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)==(loop_mode!=3));
   assert(checkpoint_count==(loop_mode==3?3u:2u));leaving=true;
  }
  ++loop_tick;return;
 }

 if(loop_tick==0){if(loop_mode==1){scan_owned();sensor_close_ok=false;assert(!scanner_session_stop(&scan));}else{assert(scanner_alias_begin(&prefs,1,(uint8_t[6]){0},draft)==0);assert(scanner_alias_save(&prefs,"A")==3);save_pending=true;strcpy(draft,"A");collect_pending=3;}loop_quiet_count=io_count;}
 else if(loop_tick<=3){assert(io_count==loop_quiet_count+(loop_mode==1?1:2));loop_quiet_count=io_count;if(loop_tick==3){sensor_close_ok=true;collect_pending=0;}}
 else leaving=true;
 loop_tick++;
}
static void text_owned(void){text_token=9;text_api=&text_endpoint;text_grant=(risc_runtime_capability_v1){.struct_size=sizeof(text_grant),.api=&text_endpoint,.slot=1,.generation=1};}
static void row(void){scan.status.count=1;scan.devices[0].address_type=1;scan.devices[0].address[0]=3;view.detail=true;view.detail_index=0;selected_valid=true;selected_type=1;memcpy(selected_address,scan.devices[0].address,6);}
static void make_document(void){view.status=scan.status;memcpy(view.devices,scan.devices,sizeof(view.devices));assert(scanner_presenter_build(&document,&view,1));committed_count=scan.status.count;memcpy(committed_devices,scan.devices,sizeof(committed_devices));}
static void enqueue(uint32_t action){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==action){queue[queued]=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=1,.node=document.nodes[i].id,.action=action,.sequence=queued+1};queued++;return;}assert(0);}
static int attention_query(uint64_t token,risc_resident_attention_v1*q){(void)token;io();attention_calls++;if(attention_result!=RISC_RESIDENT_BUSY||attention_bits){q->supported_flags=7;q->pending_flags=attention_bits;q->monotonic_ms=100;}return attention_result;}
static int attention_reject(uint64_t token,uint32_t flags){(void)token;io();reject_calls++;assert(flags&&(flags&~6u)==0);return 0;}
static void attention_setup(void){resident.enabled=true;resident.client.invocation=1;resident.client.checkpoint=checkpoint_endpoint;attention.base=resident.client;attention.attention=attention_query;attention.reject_manual=attention_reject;attention_available=true;host_alarm_supported=true;attention_checked=false;attention_pending=0;attention_calls=reject_calls=attention_bits=0;attention_result=0;checkpoint_policy=true;manual_neutral=false;}
static alarm_status_v1 fallback_status;static unsigned fallback_steps,fallback_acks;
static int fallback_read(void*c,alarm_status_v1*q){(void)c;io();*q=fallback_status;return 0;}
static int fallback_step(void*c){(void)c;io();fallback_steps++;if(fallback_status.state==ALARM_STATE_READY){fallback_status.state=ALARM_STATE_ALERT;fallback_status.occurrence=(alarm_token_v1){1,1,123,7};strcpy(fallback_status.label,"Wake up");}else assert(!scan.token&&!text_token&&!prefs.preference.api);return 0;}
static int fallback_ack(void*c,const alarm_token_v1*t){(void)c;io();assert(!memcmp(t,&fallback_status.occurrence,sizeof(*t)));fallback_acks++;return 0;}
static int fallback_refresh(void*c){(void)c;io();return 0;}
static int fallback_prepare(void*c,alarm_sleep_v1*q){(void)c;(void)q;return 0;}
static alarm_service_v1 fallback_api={1,sizeof(fallback_api),0,fallback_read,fallback_step,fallback_refresh,fallback_ack,fallback_prepare,fallback_refresh};
static void fallback_setup(void){scanner_alarm_init(&alarm_client,rt,alive,retain,0);alarm_client.checked=true;alarm_client.api=&fallback_api;alarm_client.grant=(risc_runtime_capability_v1){.struct_size=sizeof(alarm_client.grant),.api=&fallback_api,.slot=1,.generation=1};alarm_ui=false;attention_available=false;host_alarm_supported=false;fallback_steps=fallback_acks=0;fallback_status=(alarm_status_v1){.api_version=1,.struct_size=sizeof(fallback_status),.state=ALARM_STATE_READY};}

static int text_snapshot_idle(void*c,uint64_t t,risc_text_entry_checkpoint_state_v1*q){(void)c;assert(t==9);io();q->flags=text_flags;return 0;}
static int text_pause_idle(void*c,uint64_t t){(void)c;assert(t==9);io();pause_calls++;assert(!scan.token);return pause_rc;}
static int text_resume_idle(void*c,uint64_t t,uint32_t flags){(void)c;assert(t==9);io();assert(flags==(RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED));resume_calls++;return 0;}
static risc_text_entry_api_v1_checkpoint idle_text={.checkpoint_snapshot=text_snapshot_idle,.pause_for_focus_change=text_pause_idle,.resume=text_resume_idle};
static void idle_setup(void){attention_setup();attention_bits=0;idle_at=100;idle_ms=5000;test_now=5100;}
static void editor_idle_setup(void){idle_setup();text_owned();ui_token=0;text_checkpoint=&idle_text;strcpy(draft,"Unsaved draft");result.state=RISC_TEXT_ENTRY_EDITING;}

int main(int argc,char**argv){assert(argc==2);reset();const char*c=argv[1];
 if(!strcmp(c,"attention-quiet")){attention_setup();scan_owned();assert(attention_step()&&scan.token&&!checkpoint_count&&!close_count);}
 else if(!strcmp(c,"attention-alarm")){attention_setup();scan_owned();attention_bits=1;assert(attention_step()&&!scan.token&&!attention_pending&&checkpoint_count==1&&!leaving);}
 else if(!strcmp(c,"attention-text")){attention_setup();text_owned();attention_bits=1;assert(attention_step()&&!text_token&&!save_pending&&!leaving&&checkpoint_count==1);}
 else if(!strcmp(c,"attention-short")){attention_setup();row();assert(attention_step()&&manual_neutral);attention_checked=false;attention_bits=2;assert(attention_step()&&!view.detail&&reject_calls==1&&!leaving);}
 else if(!strcmp(c,"attention-long")){attention_setup();manual_neutral=true;attention_bits=4;assert(attention_step()&&reject_calls==1&&!leaving&&!manual_neutral);}
 else if(!strcmp(c,"attention-unknown")){attention_setup();attention_result=-99;assert(!attention_step()&&terminal&&!checkpoint_count);}
 else if(!strcmp(c,"attention-dirty-busy")){attention_setup();attention_result=RISC_RESIDENT_BUSY;attention_bits=1;assert(!attention_step()&&terminal&&!checkpoint_count);}
 else if(!strcmp(c,"alarm-editor-due")){fallback_setup();text_owned();ui_token=0;result.state=RISC_TEXT_ENTRY_EDITING;assert(alarm_fallback()&&fallback_steps==1&&alarm_ui&&!text_token&&!save_pending&&!leaving);}
 else if(!strcmp(c,"alarm-editor-presenting")){fallback_setup();text_owned();ui_token=0;result.state=RISC_TEXT_ENTRY_EDITING;result.flags=RISC_TEXT_ENTRY_PRESENTING;assert(alarm_fallback()&&!fallback_steps&&text_token&&!alarm_ui);result.flags=0;assert(alarm_fallback()&&fallback_steps==1&&!text_token&&alarm_ui);}
 else if(!strcmp(c,"alarm-reserve")){fallback_setup();scan_owned();assert(alarm_fallback()&&alarm_ui&&!scan.token&&fallback_steps==1&&!ui_token);assert(publish()&&ui_token);assert(alarm_fallback()&&fallback_steps==2&&!scan.token);}
 else if(!strcmp(c,"alarm-close-pending")){fallback_setup();scan_owned();sensor_close_ok=false;assert(alarm_fallback()&&scan.cleanup_pending&&!alarm_ui&&fallback_steps==1);sensor_close_ok=true;assert(scanner_session_stop(&scan));assert(alarm_fallback()&&alarm_ui&&fallback_steps==2);}
 else if(!strcmp(c,"alarm-text")){fallback_setup();text_owned();fallback_status.state=ALARM_STATE_ALERT;fallback_status.occurrence.generation=1;text_close_again=true;assert(alarm_fallback()&&text_token&&!alarm_ui&&!fallback_steps);text_close_again=false;assert(alarm_fallback()&&!text_token&&alarm_ui&&!save_pending&&!leaving);}
 else if(!strcmp(c,"alarm-exact-ack")){fallback_setup();assert(alarm_fallback()&&publish());action(SCANNER_ALARM_ACK,0);assert(fallback_acks==1);fallback_status.occurrence.generation++;action(SCANNER_ALARM_ACK,0);assert(fallback_acks==1);}
 else if(!strcmp(c,"alarm-return")){fallback_setup();assert(alarm_fallback()&&alarm_ui);fallback_status=(alarm_status_v1){.api_version=1,.struct_size=sizeof(fallback_status),.state=ALARM_STATE_LOADING};assert(alarm_fallback()&&!alarm_ui&&!scan.token);}
 else if(!strcmp(c,"alarm-uncertain")){fallback_setup();fallback_status.output_uncertain=1;assert(!alarm_fallback()&&terminal&&!fallback_steps);}
 else if(!strcmp(c,"no-ui")){no_ui=true;io_count=acquires=0;app_main();assert(acquires==1&&io_count==1&&!terminal);}
 else if(!strcmp(c,"close-cut")){scan_owned();view.active=true;make_document();enqueue(SCANNER_SCAN);sensor_close_ok=false;events();assert(scan.cleanup_pending&&next_count==1&&!terminal);}
 else if(!strcmp(c,"close-continuation")){scan_owned();sensor_close_ok=false;assert(!stop_owners());unsigned n=io_count;assert(!scanner_session_step(&scan,1)&&io_count==n+1);sensor_close_ok=true;assert(stop_owners()&&!scan.token);}
 else if(!strcmp(c,"text-again")){text_owned();text_close_again=true;text_closing=true;unsigned n=releases;text_step();assert(text_token==9&&releases==n&&!terminal);text_close_again=false;text_step();assert(!text_token&&!terminal);}
 else if(!strcmp(c,"text-accept")){text_owned();result.state=RISC_TEXT_ENTRY_ACCEPTED;strcpy(result.text,"New name");text_step();assert(!text_token&&save_pending&&!strcmp(draft,"New name"));}
 else if(!strcmp(c,"text-cancel")){text_owned();text_step();assert(!text_token&&!save_pending&&!leaving);}
 else if(!strcmp(c,"text-home")){text_owned();result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;text_step();assert(!text_token&&leaving&&!save_pending);}
 else if(!strcmp(c,"text-malformed")){text_owned();result.struct_size=0;text_step();assert(terminal&&text_token==9&&!text_close_count);}
 else if(!strcmp(c,"text-unknown-open")){row();text_open_result=-99;begin_name();assert(terminal&&!text_token);}
 else if(!strcmp(c,"text-clean-absent")){row();text_open_result=RISC_TEXT_ENTRY_UNAVAILABLE;begin_name();assert(!terminal&&!text_token&&!text_grant.api);}
 else if(!strcmp(c,"handoff-pref")){uint8_t a[6]={0};assert(scanner_alias_begin(&prefs,1,a,draft)==0);assert(scanner_alias_save(&prefs,"A")==SCANNER_PREF_PENDING);resident.enabled=true;resident.client.invocation=1;resident.client.checkpoint=checkpoint_endpoint;handoff_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;handoff_step();assert(!terminal);}
 else if(!strcmp(c,"selection-changed")){row();view.detail=false;make_document();scan.devices[0].address[0]=8;action(SCANNER_SELECT_BASE,0);assert(!view.detail&&!selected_valid&&!terminal);}
 else if(!strcmp(c,"selection-moved")){row();view.detail=false;make_document();scan.status.count=2;scan.devices[1]=scan.devices[0];scan.devices[0].address[0]=8;action(SCANNER_SELECT_BASE,0);assert(view.detail&&view.detail_index==1&&selected_valid);}
 else if(!strcmp(c,"name-changed")){row();scan.devices[0].address[0]=8;unsigned n=io_count;begin_name();assert(!text_token&&io_count==n);}
 else if(!strcmp(c,"alias-identity")){row();view.alias_state[0]=1;strcpy(view.aliases[0],"Old");alias_types[0]=1;memset(alias_addresses[0],0,6);aliases_load();assert(!view.aliases[0][0]&&view.alias_state[0]==-1);}
 else if(!strcmp(c,"save-retires")){row();assert(scanner_alias_begin(&prefs,1,scan.devices[0].address,draft)==0);draft_type=1;memcpy(draft_address,scan.devices[0].address,6);save_index=0;strcpy(draft," New name ");save_pending=true;commit_success=true;save_step();assert(save_pending);save_step();assert(!save_pending&&!save_retry&&!prefs.alias_active&&!prefs.phase&&!prefs.preference.api&&!strcmp(view.aliases[0],"New name"));}
 else if(!strcmp(c,"cancel-pending")){row();assert(scanner_alias_begin(&prefs,1,scan.devices[0].address,draft)==0);assert(scanner_alias_save(&prefs,"A")==3);collect_pending=1;back();assert(cancel_pending&&view.detail);assert(cancel_alias());assert(!cancel_pending&&!view.detail&&!prefs.alias_active&&!prefs.preference.api);}
 else if(!strcmp(c,"handoff-pending")){assert(scanner_alias_begin(&prefs,1,(uint8_t[6]){0},draft)==0);assert(scanner_alias_save(&prefs,"A")==3);collect_pending=1;resident.enabled=true;resident.client.invocation=1;resident.client.checkpoint=checkpoint_endpoint;handoff_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;assert(handoff_step()&&!checkpoint_count&&handoff_reason);assert(handoff_step()&&checkpoint_count==1&&!handoff_reason);}
 else if(!strcmp(c,"detail-moved")){row();scan.status.count=2;scan.devices[1]=scan.devices[0];scan.devices[0].address[0]=8;assert(publish()&&view.detail_index==1&&document.screen_key==3);}
 else if(!strcmp(c,"detail-gone")){row();scan.devices[0].address[0]=8;assert(publish()&&!view.detail&&!selected_valid);}
 else if(!strcmp(c,"text-clean-again")){text_owned();text_poll_result=RISC_TEXT_ENTRY_AGAIN;text_step();assert(!terminal&&text_token&&!text_closing);}
 else if(!strcmp(c,"text-dirty-again")){text_owned();text_poll_result=RISC_TEXT_ENTRY_AGAIN;dirty_again=true;result.state=RISC_TEXT_ENTRY_ACCEPTED;text_step();assert(terminal&&text_token&&!text_close_count);}
 else if(!strcmp(c,"loop-close")||!strcmp(c,"loop-pref")){assert(scanner_background_close());loop_mode=!strcmp(c,"loop-close")?1:2;app_main();assert(!terminal&&loop_tick>=4);}
 else if(!strcmp(c,"stale-event")){make_document();enqueue(SCANNER_SCAN);queue[0].document_revision=2;events();assert(!scan.token&&!terminal);}
 else if(!strcmp(c,"sequence-regression")){make_document();enqueue(SCANNER_FILTER);sequence=99;events();assert(terminal);}
 else if(!strcmp(c,"identity-close")){identity=&other_rt;assert(!close_ui());assert(terminal);}

 else if(!strcmp(c,"idle-threshold")){idle_setup();test_now=5099;assert(!idle_due());test_now=5100;assert(idle_due());}
 else if(!strcmp(c,"idle-wrap")){idle_setup();idle_at=UINT32_MAX-2500u;test_now=2498;assert(!idle_due());test_now=2499;assert(idle_due());}
 else if(!strcmp(c,"idle-scene-held")){idle_setup();scene_flags=RISC_SCENE_INPUT_BUSY;assert(snapshot());assert(!idle_due()&&idle_at==test_now);}
 else if(!strcmp(c,"idle-active-scan")){idle_setup();scan_owned();assert(idle_due());idle_pending=true;assert(idle_step()&&!scan.token&&!idle_pending&&checkpoint_count==2&&close_count==1&&last_reason==RISC_RESIDENT_APPLY_IDLE_ONLY&&!leaving);}
 else if(!strcmp(c,"idle-close-pending")){idle_setup();scan_owned();sensor_close_ok=false;idle_pending=true;assert(idle_step()&&!checkpoint_count&&scan.token);sensor_close_ok=true;assert(idle_step()&&checkpoint_count==2&&!scan.token&&!idle_pending);}
 else if(!strcmp(c,"idle-ui-pending")){idle_setup();scene_close_again=true;idle_pending=true;assert(idle_step()&&!checkpoint_count&&ui_token);scene_close_again=false;assert(idle_step()&&checkpoint_count==2&&!ui_token);}
 else if(!strcmp(c,"idle-unsupported")){idle_setup();query_rc=RISC_RESIDENT_BUSY;idle_pending=true;assert(idle_step()&&!idle_pending&&checkpoint_count==1&&!leaving&&idle_at==test_now);}
 else if(!strcmp(c,"idle-apply-busy")){idle_setup();apply_rc=RISC_RESIDENT_BUSY;idle_pending=true;assert(idle_step()&&idle_pending&&idle_queried&&checkpoint_count==2);apply_rc=0;assert(idle_step()&&!idle_pending&&checkpoint_count==3);}
 else if(!strcmp(c,"idle-key-race")){idle_setup();manual_injected=2;idle_pending=true;assert(idle_step()&&manual_injected==2&&!leaving&&checkpoint_count==2);row();attention_checked=false;attention_bits=2;manual_neutral=true;assert(attention_step()&&!view.detail&&!leaving&&reject_calls==1);}
 else if(!strcmp(c,"alarm-key-race")){idle_setup();manual_injected=4;attention_bits=1;assert(attention_step()&&manual_injected==4&&!leaving&&last_reason==RISC_RESIDENT_CHECKPOINT_ALARM&&checkpoint_count==1);}
 else if(!strcmp(c,"alarm-query-unsupported")){idle_setup();ui_token=0;query_rc=RISC_RESIDENT_BUSY;assert(negotiate_alarm()&&!host_alarm_supported);attention_bits=1;scan_owned();assert(attention_step()&&scan.token&&!attention_pending&&checkpoint_count==1);}
 else if(!strcmp(c,"alarm-oldhost-fallback")){fallback_setup();attention_setup();host_alarm_supported=false;attention_bits=1;scan_owned();assert(attention_step()&&scan.token&&!attention_pending&&!checkpoint_count);assert(alarm_fallback()&&fallback_steps==1&&!scan.token&&alarm_ui);}
 else if(!strcmp(c,"idle-text")){editor_idle_setup();assert(text_policy()&&text_token&&!text_paused&&pause_calls==1&&resume_calls==1&&checkpoint_count==3&&!save_pending&&!strcmp(draft,"Unsaved draft"));}
 else if(!strcmp(c,"idle-text-held")){editor_idle_setup();text_flags=RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY;assert(text_policy()&&!pause_calls&&!checkpoint_count&&idle_at==test_now);}
 else if(!strcmp(c,"idle-text-presenting")){editor_idle_setup();text_flags=RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING;assert(text_policy()&&!pause_calls&&!checkpoint_count&&idle_at==100);}
 else if(!strcmp(c,"idle-text-again")){editor_idle_setup();pause_rc=RISC_TEXT_ENTRY_AGAIN;assert(text_policy()&&text_pausing&&!checkpoint_count);pause_rc=0;assert(text_policy()&&pause_calls==2&&resume_calls==1&&checkpoint_count==3);}
 else if(!strcmp(c,"idle-text-busy")){editor_idle_setup();pause_rc=RISC_TEXT_ENTRY_BUSY;assert(text_policy()&&!text_pausing&&!checkpoint_count&&idle_at==test_now);}
 else if(!strcmp(c,"idle-text-key-race")){editor_idle_setup();manual_injected=2;assert(text_policy()&&manual_injected==2&&!leaving&&!save_pending&&text_token&&resume_calls==1);}
 else if(!strcmp(c,"idle-text-deep")){editor_idle_setup();apply_rc=RISC_RESIDENT_EXIT;assert(text_policy()&&text_closing&&text_paused&&!resume_calls&&(text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL));}
 else if(!strcmp(c,"idle-unknown")){idle_setup();apply_rc=-99;idle_pending=true;assert(!idle_step()&&terminal);unsigned n=io_count;assert(!idle_step()&&io_count==n);}

 else if(!strcmp(c,"idle-text-alarm-outcome")){editor_idle_setup();outcome_flags=RISC_RESIDENT_REPLY_REDRAW;assert(text_policy()&&text_closing&&text_paused&&!resume_calls&&!save_pending&&!leaving&&!(text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL));text_step();assert(!text_token&&!save_pending&&!leaving);}
 else if(!strcmp(c,"idle-text-outcome-busy")){editor_idle_setup();outcome_rc=RISC_RESIDENT_BUSY;assert(text_policy()&&text_paused&&text_idle_applied&&!resume_calls&&idle_apply_calls==1);outcome_rc=0;assert(text_policy()&&!text_paused&&resume_calls==1&&idle_apply_calls==1);}
 else if(!strcmp(c,"idle-text-outcome-invalid")){editor_idle_setup();outcome_flags=RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;assert(!text_policy()&&terminal&&!resume_calls);unsigned n=io_count;assert(!text_policy()&&io_count==n);}
 else if(!strcmp(c,"idle-text-outcome-retained")){editor_idle_setup();outcome_rc=RISC_RESIDENT_RETAINED;assert(!text_policy()&&terminal&&!resume_calls);}
 else if(!strcmp(c,"idle-text-old-host")){editor_idle_setup();host_alarm_supported=false;assert(text_policy()&&resume_calls==1&&checkpoint_count==2&&!leaving);}
 else if(!strcmp(c,"loop-idle-alarm-close")||!strcmp(c,"loop-idle-exit-close")||!strcmp(c,"loop-idle-handoff-close")){
 assert(scanner_background_close());loop_mode=!strcmp(c,"loop-idle-alarm-close")?3:!strcmp(c,"loop-idle-exit-close")?4:5;
 app_main();assert(!terminal&&loop_tick==4&&!text_token&&!save_pending&&!resume_calls&&text_close_count==2);
 }
 else assert(0);
 printf("%s PASS\n",c);return 0;}
