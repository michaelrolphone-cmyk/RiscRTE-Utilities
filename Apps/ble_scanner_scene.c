/* One semantic Scanner. Providers own geometry, input and physical custody. */
#include "scanner_session.h"
#include "scanner_alarm.h"
#include "scanner_preferences.h"
#include "scanner_presenter.h"
#include "scanner_background.h"
#include "RiscSceneResidentV1.h"
#include "RiscResidentAttentionV1.h"
#include "RiscResidentIdleV1.h"
#include "RiscResidentAlarmV1.h"
#include "RiscTextEntryV1.h"
#include "PortableTextInputResidentCheckpoint.h"
#include <stdio.h>
static const risc_runtime_api_v1 *rt;
static bool terminal,leaving,dirty,explicit_refresh,pending_name;
static uint32_t handoff_reason,idle_at,idle_ms;
static bool idle_pending,idle_queried,host_alarm_supported,text_idle_applied;
static scanner_session scan;
static scanner_alarm alarm_client;
static bool alarm_ui;
static alarm_token_v1 committed_alarm;
enum { SCANNER_ALARM_ACK=16,SCANNER_ALARM_RETRY=17 };
static scanner_preferences prefs;
static scanner_presenter_model view;
static risc_runtime_capability_v1 ui_grant,text_grant;
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static const risc_scene_lifecycle_api_v1 *lifecycle;
static const risc_text_entry_api_v1 *text_api;
static const risc_text_entry_api_v1_checkpoint *text_checkpoint;
static risc_components_document_v1 document,candidate;
static risc_scene_navigation_v1 navigation;
static uint64_t ui_token,text_token,sequence;
static uint32_t ui_flags,revision,attention_at,attention_pending;
static bool text_closing,text_paused,text_pausing,text_continuity,text_presenting;
static risc_text_entry_state_v1 text_state;
static char draft[25];static uint8_t draft_type,draft_address[6];
static bool cancel_pending,back_after_cancel;
static risc_ble_sensor_device_v1 committed_devices[32];
static uint32_t committed_count;
static uint8_t selected_type,selected_address[6];
static bool selected_valid;
static uint8_t alias_types[32],alias_addresses[32][6];
static unsigned save_index;static bool save_pending,save_retry,attention_available,attention_checked,manual_neutral;
static risc_scene_resident_v1 resident;
static risc_resident_attention_client_v1 attention;
static bool alive(void*p){(void)p;if(terminal)return false;
 if(risc_runtime_get_api(1)!=rt){terminal=true;return false;}return true;}
static void retain(void*p){(void)p;if(terminal)return;
 terminal=true;if(risc_runtime_get_api(1)==rt&&rt->retain_invocation)(void)rt->retain_invocation();}
static bool fail(void){retain(NULL);return false;}
static uint32_t millis(void){uint32_t n=0;if(!alive(NULL))return 0;bool ok=rt->monotonic_ms(&n);if(!alive(NULL))return 0;if(!ok)retain(NULL);return n;}
static void notice(const char*s){snprintf(view.notice,sizeof(view.notice),"%s",s);dirty=true;}
static bool empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation&&(g->struct_size==0||g->struct_size==sizeof(*g));}
static bool release(risc_runtime_capability_v1*g){if(!alive(NULL))return false;
 if(empty(g))return true;
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation)return fail();
 risc_runtime_capability_v1 copy=*g;bool ok=rt->release(&copy);if(!alive(NULL))return false;
 if(!ok||copy.struct_size!=sizeof(copy)||!empty(&copy))return fail();
 *g=copy;return true;}
/* 0 absent, 1 admitted, -1 terminal */
static int acquire(const char*name,risc_runtime_capability_v1*g){if(!alive(NULL))return -1;if(!empty(g)){fail();return -1;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};bool ok=rt->acquire(name,1,0,g);if(!alive(NULL))return -1;if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):!empty(g))){fail();return -1;}return ok?1:0;}
static bool snapshot(void){if(!ui_token)return true;
 uint32_t flags=0;risc_scene_navigation_v1 n={.api_version=1,.struct_size=sizeof(n)};int rc=scene->snapshot(scene->context,ui_token,&n,&flags);if(!alive(NULL))return false;
 if(rc!=RISC_SCENE_OK||n.api_version!=1||n.struct_size!=sizeof(n)||n.reserved||n.depth!=1||n.routes[0]!=1||(flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_ACTIVITY)))return fail();
 navigation=n;ui_flags=flags;if(flags&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))resident.activity=true;return true;}
static bool close_ui(void){if(!alive(NULL))return false;
 if(!ui_token)return true;
 int rc=scene->close(scene->context,ui_token);if(!alive(NULL))return false;
 if(rc==RISC_SCENE_AGAIN)return false;
 if(rc!=RISC_SCENE_OK)return fail();
 ui_token=0;ui_flags=0;return true;}
static bool selected_index(unsigned *out);
static bool publish(void){
 if(!alive(NULL)||text_token)return false;
 if(ui_token&&(!snapshot()||(ui_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))))return false;
 if(!dirty&&ui_token)return true;
 if(revision==UINT32_MAX)return fail();
 if(view.detail){unsigned current=0;if(selected_index(&current))view.detail_index=current;else{view.detail=false;selected_valid=false;}}
 view.now_ms=millis();if(!alive(NULL))return false;
 view.status=scan.status;memcpy(view.devices,scan.devices,sizeof(view.devices));view.copied_at=scan.copied_at;view.active=scan.token!=0;view.name_retry=save_retry;
 if(alarm_ui){
 candidate=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=revision+1,.root=1,.route_count=1,.screen_key=100,.flags=RISC_COMPONENTS_ALERT};
 bool alert=alarm_client.status.state==ALARM_STATE_ALERT;
 uint32_t action=alert?SCANNER_ALARM_ACK:SCANNER_ALARM_RETRY;
 candidate.cancel_action=action;candidate.routes[0]=(risc_scene_route_v1){.id=1,.back_action=action};
 snprintf(candidate.routes[0].title,sizeof(candidate.routes[0].title),"%s",alert?"ALARM":"ALARM SERVICE");
 scanner_presenter_node(&candidate,RISC_SCENE_TEXT_NODE,0,alert?"ALARM":"STATUS",alert?alarm_client.status.label:"Alarm reconciliation pending. Retry safely.");
 scanner_presenter_node(&candidate,RISC_SCENE_ACTION,action,alert?"Dismiss":"Retry","");
 }else if(!scanner_presenter_build(&candidate,&view,revision+1))return fail();
 if(ui_token){candidate.revision=document.revision;if(!memcmp(&candidate,&document,sizeof(candidate))){dirty=false;return true;}candidate.revision=revision+1;}
 bool new_session=!ui_token;
 uint64_t opened=0;int rc=ui_token?components->update(scene->context,ui_token,&candidate):components->open(scene->context,&candidate,NULL,&opened);
 if(!alive(NULL))return false;
 if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY){if(opened)return fail();
 return false;}
 if(rc!=RISC_SCENE_OK)return fail();
 if(!ui_token){if(!opened)return fail();
 ui_token=opened;sequence=0;}
 document=candidate;revision=document.revision;dirty=false;explicit_refresh=false;if(alarm_ui)committed_alarm=alarm_client.status.occurrence;
 memcpy(committed_devices,view.devices,sizeof(committed_devices));committed_count=view.status.count;
 /* Configuring establishes input ownership and consumes inherited input.
  * Ordinary updates must preserve events arriving after the snapshot. */
 if(new_session&&lifecycle){rc=lifecycle->configure(scene->context,ui_token,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!alive(NULL))return false;
 if(rc!=RISC_SCENE_OK)return fail();}
 return true;
}
static bool stop_owners(void){
 if(!scanner_session_stop(&scan)||!scanner_background_pause())return false;
 int rc=scanner_preferences_cleanup(&prefs);
 if(!alive(NULL))return false;
 if(rc==SCANNER_PREF_RETAINED)return fail();
 if(rc==SCANNER_PREF_PENDING)return false;
 return true;
}
static bool check_pref(int rc){if(!alive(NULL))return false;
 if(rc==SCANNER_PREF_RETAINED)return fail();
 return true;}
static bool cancel_alias(void){
 cancel_pending=true;save_pending=save_retry=false;
 if(!scanner_background_pause())return false;
 int rc=scanner_alias_cancel(&prefs);if(!check_pref(rc))return false;
 if(rc==SCANNER_PREF_PENDING)return false;
 if(rc!=SCANNER_PREF_OK)return fail();
 cancel_pending=false;
 if(back_after_cancel){view.detail=false;selected_valid=false;back_after_cancel=false;dirty=true;}
 return true;
}
static bool selected_index(unsigned *out){
 if(!selected_valid)return false;
 for(unsigned i=0;i<scan.status.count;i++)if(scan.devices[i].address_type==selected_type&&!memcmp(scan.devices[i].address,selected_address,6)){*out=i;return true;}
 return false;
}
static void policy_read(void){if(!scanner_background_pause())return;
 uint32_t timeout=60000u;int timer=scanner_idle_timeout(&prefs,&timeout);if(!check_pref(timer))return;idle_ms=timeout;
 uint8_t flags=0;int rc=scanner_radio_allowed(&prefs,&flags);if(!check_pref(rc))return;
 view.radio_enabled=rc==SCANNER_PREF_OK&&(flags&PORTABLE_RADIO_BLUETOOTH)&&!(flags&PORTABLE_RADIO_AIRPLANE);view.enable_available=rc==SCANNER_PREF_OK&&!(flags&PORTABLE_RADIO_AIRPLANE);if(rc!=SCANNER_PREF_OK)notice("Radio settings unavailable");else if(flags&PORTABLE_RADIO_AIRPLANE)notice("Turn off Airplane mode");}
static void aliases_load(void){
 bool needed=false;for(unsigned i=0;i<scan.status.count;i++){
 if(alias_types[i]!=scan.devices[i].address_type||memcmp(alias_addresses[i],scan.devices[i].address,6)){view.alias_state[i]=0;view.aliases[i][0]=0;alias_types[i]=scan.devices[i].address_type;memcpy(alias_addresses[i],scan.devices[i].address,6);}
 needed|=!view.alias_state[i];}
 if(!needed)return;
 if(!scanner_background_pause())return;
 for(unsigned i=0;i<scan.status.count&&alive(NULL);i++)if(!view.alias_state[i]){
  int rc=scanner_alias_read(&prefs,scan.devices[i].address_type,scan.devices[i].address,view.aliases[i]);if(!check_pref(rc))return;
 if(rc==SCANNER_PREF_PENDING)return;
 view.alias_state[i]=rc==SCANNER_PREF_OK?1:-1;
 }
}
static bool bind_resident(void){attention=(risc_resident_attention_client_v1){0};resident=(risc_scene_resident_v1){0};if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!rt->resident_shell)return true;
 attention.base.struct_size=sizeof(attention);bool ok=rt->resident_shell(&attention.base);if(!alive(NULL))return false;
 if(!ok)return true;
 risc_resident_client_v1*c=&attention.base;if(c->api_version!=1||c->struct_size<sizeof(*c)||!c->invocation||c->role!=RISC_RESIDENT_ROLE_FOREGROUND||!c->checkpoint)return fail();
 resident.client=*c;resident.enabled=true;attention_available=risc_resident_attention_available_v1(&attention);return true;}
static int checkpoint(uint32_t reason,uint32_t flags,uint32_t*out){if(!alive(NULL)||!resident.enabled||!resident.client.checkpoint)return RISC_RESIDENT_RETAINED;risc_resident_request_v1 q={sizeof(q),reason,flags,0};risc_resident_reply_v1 r={sizeof(r),0};int rc=resident.client.checkpoint(resident.client.invocation,&q,&r);if(!alive(NULL))return RISC_RESIDENT_RETAINED;if(r.struct_size!=sizeof(r)||(r.flags&~(RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED|RISC_RESIDENT_REPLY_POLICY_REQUEST))||(rc!=RISC_RESIDENT_OK&&r.flags)){fail();return RISC_RESIDENT_RETAINED;}if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF){fail();return RISC_RESIDENT_RETAINED;}if(out)*out=r.flags;return rc;}
static void idle_activity(void){idle_at=millis();resident.activity=false;}
static bool idle_due(void){
 if(resident.activity)idle_activity();
 return alive(NULL)&&idle_ms&&(uint32_t)(millis()-idle_at)>=idle_ms;
}
static bool negotiate_alarm(void){
 host_alarm_supported=false;if(!resident.enabled)return true;
 uint32_t flags=0;int rc=checkpoint(RISC_RESIDENT_QUERY_ALARM_ONLY,0,&flags);
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK||flags)return fail();
 host_alarm_supported=true;return true;
}
static void back(void);
static bool text_close(void){if(!text_token)return true;
 int rc=text_api->close(text_api->context,text_token);if(!alive(NULL))return false;
 if(rc==RISC_TEXT_ENTRY_AGAIN)return false;
 if(rc!=RISC_TEXT_ENTRY_OK)return fail();
 text_token=0;text_api=NULL;text_checkpoint=NULL;text_paused=text_pausing=false;idle_pending=idle_queried=false;return release(&text_grant);}
static void text_step(void){
 if(!text_token||!alive(NULL))return;
 if(!text_closing){risc_text_entry_state_v1 v={.struct_size=sizeof(v)};int rc=text_api->poll(text_api->context,text_token,&v);if(!alive(NULL))return;
 if(rc==RISC_TEXT_ENTRY_AGAIN){risc_text_entry_state_v1 empty_state={.struct_size=sizeof(empty_state)};if(memcmp(&v,&empty_state,sizeof(v)))fail();return;}
 if(rc!=RISC_TEXT_ENTRY_OK||v.struct_size!=sizeof(v)||v.state>RISC_TEXT_ENTRY_CANCELLED||(v.flags&~(RISC_TEXT_ENTRY_HARDWARE|RISC_TEXT_ENTRY_PRESENTING|RISC_TEXT_ENTRY_HOME_CANCEL))||!memchr(v.text,0,sizeof(v.text))){fail();return;}text_state=v;text_presenting=!!(v.flags&RISC_TEXT_ENTRY_PRESENTING);if(v.state==RISC_TEXT_ENTRY_EDITING)return;
 text_closing=true;}
 if(!text_close())return;
 text_closing=false;dirty=explicit_refresh=true;
 if(text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL){leaving=true;return;}
 if(text_state.state==RISC_TEXT_ENTRY_ACCEPTED){size_t n=strlen(text_state.text);if(n>24){notice("Invalid sensor name");(void)cancel_alias();return;}memcpy(draft,text_state.text,n+1);save_pending=true;}
 else{(void)cancel_alias();notice("Name cancelled");save_retry=false;}
}
static void begin_name(void){
 unsigned selected=0;if(!selected_index(&selected)||text_token){pending_name=false;return;}
 view.detail_index=selected;
 if(!stop_owners()||!close_ui())return;
 pending_name=false;
 const risc_ble_sensor_device_v1*d=&scan.devices[view.detail_index];save_index=view.detail_index;draft_type=d->address_type;memcpy(draft_address,d->address,6);
 int rc=scanner_alias_begin(&prefs,draft_type,draft_address,draft);if(!check_pref(rc))return;
 if(rc==SCANNER_PREF_PENDING){pending_name=true;return;}
 if(rc!=SCANNER_PREF_OK){notice("Name settings unavailable");return;}
 int a=acquire(RISC_TEXT_ENTRY_CAPABILITY,&text_grant);if(a<=0){if(!terminal){(void)cancel_alias();notice("Text input unavailable");}return;}
 text_api=text_grant.api;if(text_api->api_version!=1||text_api->struct_size<sizeof(*text_api)||!text_api->open||!text_api->poll||!text_api->close){release(&text_grant);(void)cancel_alias();text_api=NULL;notice("Text input unavailable");return;}
 risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=25};snprintf(q.label,sizeof(q.label),"Sensor name");snprintf(q.text,sizeof(q.text),"%s",draft);if(risc_text_entry_home_reason(text_api))q.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON;
 rc=text_api->open(text_api->context,&q,&text_token);if(!alive(NULL))return;
 if(rc!=RISC_TEXT_ENTRY_OK){if(text_token||(rc!=RISC_TEXT_ENTRY_AGAIN&&rc!=RISC_TEXT_ENTRY_BUSY&&rc!=RISC_TEXT_ENTRY_UNAVAILABLE)){fail();return;}release(&text_grant);(void)cancel_alias();text_api=NULL;notice("Text input unavailable");return;}if(!text_token){fail();return;}
 text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state)};text_closing=text_presenting=false;text_checkpoint=risc_text_entry_checkpoint(text_api);
 if(text_checkpoint){rc=text_checkpoint->checkpoint_enable(text_api->context,text_token);if(!alive(NULL))return;
 if(rc!=RISC_TEXT_ENTRY_OK){fail();return;}}
}
static void save_step(void){if(!save_pending||text_token||!ui_token)return;
 if(!scanner_background_pause())return;
 int rc=scanner_alias_save(&prefs,draft);if(!check_pref(rc))return;
 if(rc==SCANNER_PREF_PENDING)return;
 save_pending=false;save_retry=rc!=SCANNER_PREF_OK;
 if(rc==SCANNER_PREF_OK){if(save_index<scan.status.count&&scan.devices[save_index].address_type==draft_type&&!memcmp(scan.devices[save_index].address,draft_address,6)){/* Helper normalized immutable value; format normalization is identical to original. */unsigned a=0,b=(unsigned)strlen(draft);while(a<b&&draft[a]==' ')a++;while(b>a&&draft[b-1]==' ')b--;memcpy(view.aliases[save_index],draft+a,b-a);view.aliases[save_index][b-a]=0;view.alias_state[save_index]=1;}notice("Sensor name saved");(void)cancel_alias();}else notice("Name save unconfirmed; retry same request");}
static void back(void){if(alarm_ui){if(alarm_client.status.state==ALARM_STATE_ALERT)(void)scanner_alarm_ack(&alarm_client,&committed_alarm);return;}if(text_token){text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};text_closing=true;return;}if(view.detail){back_after_cancel=true;(void)cancel_alias();return;}leaving=true;}
static void action(uint32_t a,uint32_t index){explicit_refresh=true;resident.activity=true;
 if(alarm_ui){
 if(a==SCANNER_ALARM_ACK)(void)scanner_alarm_ack(&alarm_client,&committed_alarm);
 else if(a==SCANNER_ALARM_RETRY)(void)scanner_alarm_refresh(&alarm_client);
 return;}
 if(a==SCANNER_BACK){back();return;}
 if(prefs.phase||cancel_pending)return;
 if(a==SCANNER_FILTER){view.sensors=!view.sensors;dirty=true;return;}
 if(a>=SCANNER_SELECT_BASE&&index<committed_count){
 selected_type=committed_devices[index].address_type;memcpy(selected_address,committed_devices[index].address,6);selected_valid=true;
 unsigned current=0;if(!selected_index(&current)){selected_valid=false;notice("Device changed; refresh list");return;}
 view.detail=true;view.detail_index=current;dirty=true;return;}
 if(a==SCANNER_NAME){begin_name();return;}
 if(a==SCANNER_RETRY_NAME){save_pending=true;return;}
 if(a==SCANNER_ENABLE){if(!stop_owners())return;
 int rc=scanner_enable_bluetooth(&prefs);if(!check_pref(rc))return;
 notice(rc==SCANNER_PREF_OK?"Bluetooth enabled":rc==SCANNER_PREF_PENDING?"Enabling Bluetooth":"Bluetooth enable failed");if(rc!=SCANNER_PREF_PENDING)policy_read();return;}
 if(a==SCANNER_SCAN){if(save_pending||save_retry||cancel_pending)return;if(scan.token){if(scanner_session_stop(&scan))notice(scan.restore_failed?"Bluetooth restore failed":"Stopped");return;}policy_read();if(!alive(NULL)||!view.radio_enabled)return;
 if(scanner_session_start(&scan)){memset(view.aliases,0,sizeof(view.aliases));memset(view.alias_state,0,sizeof(view.alias_state));notice("Starting passive scan");}else if(alive(NULL)){view.sensor_available=scan.available;notice("Bluetooth unavailable or busy; retry");}}
}

/* Automatic idle never uses POLL: that lane is allowed to consume Home keys.
 * Full text detachment is required because no input-continuity promise exists. */
static bool text_policy(void){
 if(!alive(NULL))return false;
 if(!text_token||!text_checkpoint||text_closing||attention_pending)return true;
 int rc;
 if(!text_pausing&&!text_paused){
  risc_text_entry_checkpoint_state_v1 q={.struct_size=sizeof(q)};
  rc=text_checkpoint->checkpoint_snapshot(text_api->context,text_token,&q);
  if(!alive(NULL))return false;
  if(rc!=RISC_TEXT_ENTRY_OK||q.struct_size!=sizeof(q)||(q.flags&~15u))return fail();
  text_presenting=!!(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING);
  if(q.flags&(RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY))resident.activity=true;
  if(resident.activity)idle_activity();
  if(text_presenting)return true;
  if(!scanner_background_step(true)||!scanner_background_capture())return false;
  if(!resident.enabled||(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)||!idle_due())return true;
  idle_pending=true;idle_queried=false;text_idle_applied=false;text_pausing=true;
 }
 if(text_pausing){
  if(!stop_owners())return !terminal;
  rc=text_checkpoint->pause_for_focus_change(text_api->context,text_token);if(!alive(NULL))return false;
  if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
  if(rc==RISC_TEXT_ENTRY_BUSY){text_pausing=false;idle_pending=false;idle_activity();return true;}
  if(rc!=RISC_TEXT_ENTRY_OK)return fail();
  text_pausing=false;text_paused=true;
 }
 uint32_t flags=0;
 if(!idle_queried){
  rc=checkpoint(RISC_RESIDENT_QUERY_IDLE_ONLY,0,&flags);
  if(rc==RISC_RESIDENT_BUSY)goto resume_editor;
  if(rc!=RISC_RESIDENT_OK||flags)return fail();
  idle_queried=true;
 }
 if(!text_idle_applied) {
  rc=checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY,0,&flags);
  if(rc==RISC_RESIDENT_BUSY)return true;
  if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){
   text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED,.flags=RISC_TEXT_ENTRY_HOME_CANCEL};text_closing=true;return true;
  }
  if(rc!=RISC_RESIDENT_OK||(flags&RISC_RESIDENT_REPLY_POLICY_REQUEST))return fail();
  text_idle_applied=true;
 }
 if(host_alarm_supported) {
  rc=checkpoint(RISC_RESIDENT_QUERY_LAST_IDLE_ALARM,0,&flags);
  if(rc==RISC_RESIDENT_BUSY)return true;
  if(rc!=RISC_RESIDENT_OK||(flags&~RISC_RESIDENT_REPLY_REDRAW))return fail();
  if(flags&RISC_RESIDENT_REPLY_REDRAW) {
   text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
   text_closing=true;idle_pending=idle_queried=text_idle_applied=false;idle_activity();return true;
  }
 }
resume_editor:
 rc=text_checkpoint->resume(text_api->context,text_token,RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
 if(!alive(NULL))return false;
 if(rc!=RISC_TEXT_ENTRY_OK)return fail();
 text_paused=false;idle_pending=idle_queried=text_idle_applied=false;manual_neutral=false;idle_activity();policy_read();return alive(NULL);
}
static bool idle_step(void){
 if(!alive(NULL))return false;
 if(!idle_pending)return true;
 if(!stop_owners()||!close_ui())return !terminal;
 uint32_t flags=0;int rc;
 if(!idle_queried){
  rc=checkpoint(RISC_RESIDENT_QUERY_IDLE_ONLY,0,&flags);
  if(rc==RISC_RESIDENT_BUSY){idle_pending=false;idle_activity();notice("Automatic idle unavailable");return true;}
  if(rc!=RISC_RESIDENT_OK||flags)return fail();
  idle_queried=true;
 }
 rc=checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY,0,&flags);
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)return fail();
 if(flags&RISC_RESIDENT_REPLY_POLICY_REQUEST)return fail();
 idle_pending=idle_queried=false;manual_neutral=false;idle_activity();
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){leaving=true;return true;}
 policy_read();if(!alive(NULL))return false;notice("Paused - tap Scan");explicit_refresh=true;return true;
}
static bool attention_step(void){
 if(!alive(NULL)||!resident.enabled)return alive(NULL);
 if(!attention_pending&&attention_available){uint32_t now=millis();if(!alive(NULL))return false;
 if(attention_checked&&(uint32_t)(now-attention_at)<10)return true;
 attention_at=now;attention_checked=true;
  risc_resident_attention_v1 q={.struct_size=sizeof(q)};int rc=attention.attention(attention.base.invocation,&q);if(!alive(NULL))return false;
  if(rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_INCOMPATIBLE){if(q.struct_size!=sizeof(q)||q.supported_flags||q.pending_flags||q.monotonic_ms)return fail();
 if(rc==RISC_RESIDENT_INCOMPATIBLE){attention_available=false;notice("Capture-safe alarm attention unavailable");}return true;}
  if(rc!=RISC_RESIDENT_OK||q.struct_size!=sizeof(q)||(q.supported_flags&~7u)||(q.pending_flags&~q.supported_flags))return fail();
 attention_pending=q.pending_flags;
 if(!host_alarm_supported)attention_pending&=~RISC_RESIDENT_ATTENTION_ALARM;
  if(!(q.pending_flags&6u))manual_neutral=true;
 }
 if(!attention_pending)return true;
 uint32_t manual=attention_pending&6u;
 if(manual){resident.activity=true;int rc=attention.reject_manual(attention.base.invocation,manual);if(!alive(NULL))return false;
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK)return fail();
 attention_pending&=~manual;bool short_back=manual==RISC_RESIDENT_ATTENTION_CONTROLS&&manual_neutral;manual_neutral=false;if(short_back)back();}
 if(!attention_pending||leaving)return true;
 if(!stop_owners())return !terminal;
 if(text_token){text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};text_closing=true;text_step();if(text_token||terminal)return !terminal;
 save_pending=false;}
 if(!close_ui())return !terminal;
 uint32_t flags=0;int rc=checkpoint(RISC_RESIDENT_CHECKPOINT_ALARM,0,&flags);if(rc==RISC_RESIDENT_BUSY)return true;
 if(flags&RISC_RESIDENT_REPLY_POLICY_REQUEST)return fail();
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)return fail();
 attention_pending=0;idle_pending=idle_queried=false;idle_activity();if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF)leaving=true;else policy_read();if(!alive(NULL))return false;notice("Stopped; tap Scan to start again");explicit_refresh=true;return true;
}
static bool alarm_stop_owners(void){
 if(!stop_owners())return false;
 if(text_token){text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};text_closing=true;text_step();if(text_token||!alive(NULL))return false;}
 if(cancel_pending&&!cancel_alias())return false;
 if(!alarm_ui){if(!close_ui())return false;alarm_ui=true;dirty=explicit_refresh=true;}
 return true;
}
static bool alarm_fallback(void){
 if(host_alarm_supported)return true;
 if(!scanner_alarm_open(&alarm_client))return false;
 if(!alarm_client.api)return true;
 bool settled=!text_token;
 if(text_token&&!text_pausing&&!text_paused&&!text_closing){
  if(text_checkpoint){risc_text_entry_checkpoint_state_v1 q={.struct_size=sizeof(q)};int rc=text_checkpoint->checkpoint_snapshot(text_api->context,text_token,&q);
   if(!alive(NULL))return false;
   if(rc!=RISC_TEXT_ENTRY_OK||q.struct_size!=sizeof(q)||(q.flags&~15u))return fail();
   text_presenting=!!(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING);
  }else{text_step();if(!alive(NULL))return false;}
  settled=!text_presenting&&!text_closing;
 }
 if(ui_token){if(!snapshot())return false;settled=!(ui_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING));}
 alarm_status_v1 prior=alarm_client.status;
 if(!scanner_alarm_step(&alarm_client,settled,alarm_stop_owners))return false;
 if(memcmp(&prior,&alarm_client.status,sizeof(prior)))dirty=true;
 if(!alarm_client.owned&&alarm_ui){alarm_ui=false;dirty=explicit_refresh=true;notice("Stopped; tap Scan to start again");}
 return true;
}
static bool handoff_step(void){
 if(!handoff_reason)return true;
 if(!stop_owners())return !terminal;
 if(!close_ui())return !terminal;
 uint32_t flags=0;int rc=checkpoint(handoff_reason,0,&flags);if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)return fail();
 handoff_reason=0;idle_activity();if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF)leaving=true;explicit_refresh=dirty=true;policy_read();return !terminal;
}
static bool events(void){
 if(!ui_token)return true;
 for(unsigned i=0;i<32;i++){risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=scene->next(scene->context,ui_token,&e);if(!alive(NULL))return false;
 if(rc==RISC_SCENE_IDLE)return true;
 if(rc!=RISC_SCENE_OK)return fail();
 if(e.struct_size!=sizeof(e)||!e.sequence||e.sequence<=sequence)return fail();
 sequence=e.sequence;if(e.document_revision!=document.revision)continue;
  resident.activity=true;
  if(e.kind==RISC_SCENE_CONTROLS_EVENT&&!e.node&&!e.action){if(resident.enabled&&!alarm_client.owned)handoff_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;continue;}
  if(e.kind==RISC_SCENE_SUSPEND_EVENT&&!e.node&&!e.action){if(alarm_client.owned)continue;leaving=true;return true;}
  if(alarm_ui){
   bool allowed=e.kind==RISC_SCENE_ACTION_EVENT&&!e.value&&((!e.node&&e.action==document.cancel_action));
   for(unsigned j=0;j<document.node_count;j++)allowed|=e.kind==RISC_SCENE_ACTION_EVENT&&!e.value&&e.node==document.nodes[j].id&&e.action&&e.action==document.nodes[j].action;
   if(allowed)action(e.action,0);
   continue;
  }
  uint32_t a=0,index=0;if(scanner_presenter_intent(&document,&e,&a,&index)){if(a==SCANNER_NAME)pending_name=true;else action(a,index);}
  if(terminal||leaving||text_token||pending_name||handoff_reason||scan.cleanup_pending||cancel_pending)return !terminal;
 }
 return false; /* queue may still contain Cancel; do not advance domain work */
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 rt=risc_runtime_get_api(1);if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!rt->acquire||!rt->release||!rt->yield_ms||!rt->monotonic_ms||!rt->retain_invocation)return;
 terminal=leaving=cancel_pending=back_after_cancel=selected_valid=false;committed_count=0;memset(alias_types,0,sizeof(alias_types));memset(alias_addresses,0,sizeof(alias_addresses));ui_token=text_token=sequence=0;revision=ui_flags=attention_at=attention_pending=0;attention_available=attention_checked=manual_neutral=false;handoff_reason=0;idle_pending=idle_queried=host_alarm_supported=text_idle_applied=false;idle_ms=60000u;idle_at=0;pending_name=save_pending=save_retry=false;text_closing=text_paused=text_pausing=text_continuity=text_presenting=false;
 ui_grant=text_grant=(risc_runtime_capability_v1){0};scene=NULL;components=NULL;lifecycle=NULL;text_api=NULL;text_checkpoint=NULL;document=(risc_components_document_v1){0};navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation)};view=(scanner_presenter_model){.sensor_available=true,.name_available=true};dirty=explicit_refresh=true;
 int a=acquire(RISC_SCENE_CAPABILITY,&ui_grant);if(a<=0)return;
 scene=ui_grant.api;components=risc_scene_components_get_v1(scene);if(!components||!scene->next||!scene->snapshot||!scene->close){release(&ui_grant);return;}lifecycle=risc_scene_lifecycle_get_v1(scene);
 scanner_alarm_init(&alarm_client,rt,alive,retain,NULL);alarm_ui=false;
 scanner_session_init(&scan,rt,alive,retain,NULL);if(!check_pref(scanner_preferences_init(&prefs,rt,alive,retain,NULL)))return;
 if(!scanner_background_init(rt,alive,retain,NULL)||!bind_resident()||!negotiate_alarm())return;
 idle_activity();
 policy_read();if(!lifecycle)notice("Automatic updates/shared controls unavailable");
 while(alive(NULL)&&!leaving){
  if(scan.cleanup_pending){(void)scanner_session_stop(&scan);if(!alive(NULL))break;rt->yield_ms(10);continue;}
  if(cancel_pending){(void)cancel_alias();if(!alive(NULL))break;rt->yield_ms(10);continue;}
  if(prefs.phase){
   (void)events();if(!alive(NULL)||leaving)break;if(cancel_pending)continue;
   if(prefs.phase==1)save_step();else{int rc=scanner_preferences_step(&prefs);if(!check_pref(rc))break;if(rc!=SCANNER_PREF_PENDING){notice(rc==SCANNER_PREF_OK?"Bluetooth enabled":"Bluetooth enable failed");policy_read();}}
   if(alive(NULL))rt->yield_ms(10);
   continue;
  }
  if(!scanner_background_capture())break;
  if(!attention_step()||!alive(NULL)||leaving)break;
  if(attention_pending||scan.cleanup_pending||cancel_pending){rt->yield_ms(10);continue;}
  if(!alarm_fallback())break;
  if(scan.cleanup_pending||cancel_pending){rt->yield_ms(10);continue;}
  if(alarm_client.owned){if(!alarm_ui){rt->yield_ms(10);continue;}if(!text_token){(void)events();if(alarm_client.status.state!=ALARM_STATE_CUE)(void)publish();}if(alive(NULL))rt->yield_ms(10);continue;}
  if(handoff_reason){if(!handoff_step())break;rt->yield_ms(10);continue;}
  if(idle_pending&&!text_token){if(!idle_step())break;rt->yield_ms(10);continue;}
  if(pending_name){begin_name();if(scan.cleanup_pending)continue;if(pending_name){rt->yield_ms(10);continue;}}
  if(text_token){if(text_closing||(!text_paused&&!text_pausing))text_step();if(!alive(NULL))break;if(text_token&&!text_policy())break;rt->yield_ms(10);continue;}
  bool drained=events();if(!alive(NULL)||leaving)break;if(!drained){rt->yield_ms(10);continue;}if(pending_name||handoff_reason||scan.cleanup_pending||cancel_pending)continue;
  uint32_t previous_state=scan.status.state,previous_count=scan.status.count;
  if(!scanner_session_step(&scan,millis())){if(terminal)break;continue;}
  dirty|=previous_state!=scan.status.state||previous_count!=scan.status.count;
  if(scan.status.count){aliases_load();if(!alive(NULL))break;}
  if(scan.restore_failed)notice("Bluetooth restore failed");
  if(prefs.phase==2||prefs.phase==3){if(!scanner_background_pause())break;int rc=scanner_preferences_step(&prefs);if(!check_pref(rc))break;if(rc!=SCANNER_PREF_PENDING){notice(rc==SCANNER_PREF_OK?"Bluetooth enabled":"Bluetooth enable failed");policy_read();}}
  if(!scanner_background_step(true)||!scanner_background_capture())break;
  dirty|=scan.token!=0;
  save_step();if(!alive(NULL))break;
  if(ui_token&&!snapshot())break;
  /* Observe a settled cut before submitting the next scan frame, otherwise
   * a continuously refreshed receipt can starve configured idle forever. */
  if(resident.enabled&&!save_pending&&!save_retry&&!pending_name&&!(ui_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))&&idle_due()){
   idle_pending=true;idle_queried=false;if(!idle_step())break;
  }else{
   if(resident.activity)idle_activity();
   if(lifecycle||explicit_refresh||!ui_token){(void)publish();}
  }
  if(alive(NULL))rt->yield_ms(10);
 }
 if(!alive(NULL))return;
 while(alive(NULL)&&!stop_owners())rt->yield_ms(10);
 while(alive(NULL)&&!scanner_alarm_close(&alarm_client))rt->yield_ms(10);
 while(alive(NULL)&&text_token&&!text_close())rt->yield_ms(10);
 while(alive(NULL)&&ui_token&&!close_ui())rt->yield_ms(10);
 if(!alive(NULL))return;
 int rc=scanner_preferences_cleanup(&prefs);while(check_pref(rc)&&rc==SCANNER_PREF_PENDING){rt->yield_ms(10);rc=scanner_preferences_cleanup(&prefs);}if(!check_pref(rc)||rc!=SCANNER_PREF_OK)return;
 if(!scanner_background_close())return;
 release(&ui_grant);
}
