/* Provider-owned rendering and input; no product, geometry or backend branch. */
#include "ble_buttons_owner.h"
#include "ble_touchpad_presenter.h"
#include "ble_buttons_alarm.h"
#include "ble_buttons_policy.h"
#include "scanner_background.h"
#include "RiscSceneResidentV1.h"
#include "PortableRadioPolicy.h"
#include "RiscBatteryGaugeV1.h"
#include <stdio.h>
static ble_buttons_owner owner;
static ble_hid_session hid;
static ble_touchpad_view view;
static risc_runtime_capability_v1 grants[5];
static ble_buttons_alarm alarm;static ble_buttons_policy policy;
static const risc_power_key_api_v1 *power_key;
static alarm_token_v1 displayed_alarm;
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static const risc_scene_hold_api_v1 *hold;
static const risc_scene_pointer_api_v1 *pointer;
static const risc_scene_lifecycle_api_v1 *lifecycle;
static risc_scene_resident_v1 resident;
static risc_components_document_v1 document,candidate;
static risc_scene_navigation_v1 navigation;
static uint64_t session,last_event,hold_generation,last_battery;
static uint32_t pair_generation,pair_number,pending_checkpoint;
static risc_scene_event_v1 paired_identity;
static bool dirty,leave,paired_press,local_fault,background_ready,reset_view;
static char notice[96];
static bool alive(void){return bbo_live(&owner);}
static bool endpoint(int32_t rc){
 if(!alive())return false;
 if(rc==RISC_SCENE_RETAINED||(rc!=0&&rc!=RISC_SCENE_IDLE&&rc!=RISC_SCENE_AGAIN&&rc!=RISC_SCENE_BUSY&&rc!=RISC_SCENE_STALE&&rc!=RISC_SCENE_INVALID&&rc!=RISC_SCENE_UNAVAILABLE)){bbo_retain(&owner);return false;}
 return true;
}
static void message(const char *s){snprintf(notice,sizeof(notice),"%s",s);dirty=true;}
/* Bounded state/failure diagnostics never include pairing numbers, bond data or
 * HID reports. Cached status avoids an extra status/pump during cleanup. */
static void diagnostic(const char *stage){
 if(!alive()||hid.terminal||!owner.runtime->diagnostic)return;
 char text[192];
 snprintf(text,sizeof(text),"HID %s state=%u flags=%lu error=%ld generation=%lu closing=%u",stage,(unsigned)hid.status.state,(unsigned long)hid.status.flags,(long)hid.status.error,(unsigned long)hid.status.connection_generation,hid.closing?1u:0u);
 owner.runtime->diagnostic(text);if(!alive())return;
 if(!hid.token||hid.closing||!hid.api||hid.api->struct_size<sizeof(risc_bluetooth_hid_diagnostics_api_v1))return;
 const risc_bluetooth_hid_diagnostics_api_v1 *api=(const void*)hid.api;
 if(!api->diagnostics)return;
 risc_bluetooth_hid_diagnostics_v1 d={.struct_size=sizeof(d)};
 bool ok=api->diagnostics(api->base.context,hid.token,&d);
 if(!alive())return;
 if(!ok||d.struct_size!=sizeof(d))return;
 snprintf(text,sizeof(text),"HID cause port=%lu disconnect=%lu security=%lu notify=%ld failures=%lu gap_ms=%lu stale=%lu",(unsigned long)d.port_fault,(unsigned long)d.disconnect_reason,(unsigned long)d.security_status,(long)d.notify_error,(unsigned long)d.notify_failures,(unsigned long)d.max_poll_gap_ms,(unsigned long)d.stale_events);
 owner.runtime->diagnostic(text);if(!alive())return;
 snprintf(text,sizeof(text),"HID cleanup stop=%ld native=%ld stopped=%lu poisoned=%lu",(long)d.host_stop_error,(long)d.native_close_result,(unsigned long)d.host_stopped,(unsigned long)d.poisoned);
 owner.runtime->diagnostic(text);(void)alive();
}
static bool pause_background(void){return !background_ready||scanner_background_pause();}
static void background_retain(void *p){(void)p;bbo_retain(&owner);}
static bool radio_allowed(void){
 if(!pause_background())return false;
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!bbo_acquire(&owner,&g,RISC_KEY_VALUE_CAPABILITY,1,1)){if(alive())message("RADIO SETTINGS UNAVAILABLE");return false;}
 const risc_key_value_v1 *kv=g.api;bool allowed=false;
 if(kv&&kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get){
  uint8_t b[4],flags=0;uint32_t n=0;int rc=kv->get(kv->context,PORTABLE_RADIO_KEY,b,sizeof(b),&n);
  if(!bbo_kv_status(&owner,rc))return false;
  if(rc==RISC_KEY_VALUE_OK&&portable_radio_decode(b,n,&flags))allowed=(flags&PORTABLE_RADIO_BLUETOOTH)&&!(flags&PORTABLE_RADIO_AIRPLANE);
 }
 if(!bbo_release(&owner,&g))return false;
 if(!allowed)message("BLUETOOTH OFF OR SETTINGS UNAVAILABLE - use shared controls");
 return allowed;
}
static bool battery_update(uint64_t now){
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};uint8_t percent=255;
 bool found=bbo_acquire(&owner,&g,"board.battery",1,0);if(!alive())return false;
 if(found){const risc_battery_gauge_api_v1 *b=g.api;risc_battery_sample_v1 sample={0};
  if(b&&b->api_version==1&&b->struct_size>=sizeof(*b)&&b->read){bool ok=b->read(b->context,&sample);if(!alive())return false;if(ok&&sample.percent<=100&&!(sample.flags&RISC_BATTERY_PROFILE_MISSING))percent=sample.percent;}
  if(!bbo_release(&owner,&g))return false;
 }
 last_battery=now;return bhs_battery(&hid,percent);
}
static bool stop(void){
 if(!view.hid_available)return true;
 bool owned=hid.token!=0||hid.closing;
 if(owned&&!hid.closing&&!local_fault){diagnostic("stop");if(!alive())return false;}
 bool ok=bhs_close(&hid);if(!alive())return false;
 if(hid.terminal){bbo_retain(&owner);return false;}
 if(ok){paired_press=false;hold_generation=0;if(owned)message("Paused - choose Reconnect");}
 return ok;
}
static void back(void){
 if(view.page==BB_ALERT){message("Dismiss the alarm first");return;}
 if(view.page==BB_PAIR){leave=true;dirty=true;return;}
 if(view.page==BB_MAIN)leave=true;
 else {view.page=BB_MAIN;message("Changes canceled");}
 dirty=true;
}
static const risc_scene_node_v1 *event_node(const risc_scene_event_v1 *e){
 for(unsigned i=0;i<document.node_count;i++){const risc_scene_node_v1*n=&document.nodes[i];if(n->id==e->node&&n->action==e->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)))return n;}return NULL;
}
static void action(unsigned a,uint64_t now){
 if(a==BB_BACK){back();return;}
 if(a==BB_ALARM_ACK){(void)bba_ack(&alarm,&displayed_alarm);dirty=true;return;}
 if(a==BB_STOP){(void)stop();return;}
 if(a==BB_SETTINGS_OPEN){if(!stop())return;view.page=BB_SETTINGS;dirty=true;return;}
 if(a==BB_RECONNECT||a==BB_PAIR_START){if(alarm.reserved){message("ALARM OUTPUT RESERVED - WAIT");return;}if(!view.hid_available){message("BLUETOOTH HID UNAVAILABLE");return;}if(!stop()||!radio_allowed())return;if(bhs_open(&hid,"BLE Touchpad",a==BB_PAIR_START,now)){view.page=BB_MAIN;message(a==BB_PAIR_START?"Find BLE Touchpad on your host":"Waiting for saved host");(void)battery_update(now);}else message("Session start failed - cleanup required");return;}
 if(a==BB_FORGET_OPEN){if(!view.hid_available){message("BLUETOOTH HID UNAVAILABLE");return;}if(!stop())return;view.page=BB_FORGET;dirty=true;return;}
 if(a==BB_FORGET_CONFIRM){message(bhs_forget(&hid)?"Bond forgotten":"Forget failed");view.page=BB_SETTINGS;return;}
 if(a==BB_CANCEL){view.page=BB_SETTINGS;dirty=true;return;}
 if(a==BB_PAIR_REJECT){(void)bhs_confirm(&hid,pair_generation,pair_number,false);(void)stop();view.page=BB_MAIN;}
 dirty=true;
}
static void input(const risc_scene_hold_event_v1 *h,uint64_t now){
 const risc_scene_event_v1 *e=&h->event;
 if(h->phase==RISC_HOLD_CANCEL||h->phase==RISC_HOLD_RELEASE){
  if(hid.holding)(void)bhs_release(&hid);
  if(!alive()||hid.terminal)return;
  if(h->phase==RISC_HOLD_RELEASE&&paired_press&&!leave&&!pending_checkpoint&&!reset_view&&h->reason==RISC_HOLD_REASON_NONE&&!h->flags&&!memcmp(e,&paired_identity,sizeof(*e))&&h->generation==hold_generation&&h->held_ms<=800&&document.screen_key==BB_PAIR+1&&view.page==BB_PAIR&&e->document_revision==document.revision&&e->action==BB_PAIR_ACCEPT){(void)bhs_confirm(&hid,pair_generation,pair_number,true);dirty=true;}
  paired_press=false;hold_generation=0;
  if(h->flags&RISC_HOLD_LOCAL_FAULT_PENDING){local_fault=true;(void)stop();}
  return;
 }
 if(leave||pending_checkpoint)return;
 if(e->struct_size!=sizeof(*e)||!e->sequence||e->sequence<=last_event||e->document_revision!=document.revision)return;
 last_event=e->sequence;
 /* Lifecycle intents are provider-owned, even while a new page is pending. */
 if(h->phase==RISC_HOLD_NONE&&!h->reason&&!h->flags&&!h->generation&&!h->held_ms&&e->kind==RISC_SCENE_CONTROLS_EVENT&&!e->node&&!e->action){if(resident.enabled&&!alarm.reserved)pending_checkpoint=RISC_RESIDENT_CHECKPOINT_CONTROLS;else reset_view=true;return;}
 if(h->phase==RISC_HOLD_NONE&&!h->reason&&!h->flags&&!h->generation&&!h->held_ms&&e->kind==RISC_SCENE_SUSPEND_EVENT&&!e->node&&!e->action){leave=true;return;}
 if(document.screen_key!=view.page+1)return;
 if(h->phase==RISC_HOLD_PRESS){
  const risc_scene_node_v1 *n=event_node(e);if(!n||n->kind!=RISC_COMPONENT_HOLD_KEY||!h->generation)return;
  hold_generation=h->generation;
  if(e->action==BB_PAIR_ACCEPT&&e->kind==RISC_SCENE_ACTION_EVENT&&n->step==RISC_HOLD_CONFIRM&&h->reason==RISC_HOLD_REASON_NONE&&!h->held_ms&&!h->flags){paired_press=true;paired_identity=*e;}
  return;
 }
 if(e->kind!=RISC_SCENE_ACTION_EVENT)return;
 if(!e->node){if(e->action==BB_BACK)back();return;}
 const risc_scene_node_v1*n=event_node(e);if(!n||(n->kind!=RISC_SCENE_ACTION&&n->kind!=RISC_COMPONENT_HEADER_ACTION))return;
 action(e->action,now);
}
static bool close_view(void){
 if(!session)return true;
 int rc=scene->close(scene->context,session);if(!endpoint(rc))return false;
 if(rc==0){session=0;return true;}
 if(rc!=RISC_SCENE_BUSY&&rc!=RISC_SCENE_AGAIN){bbo_retain(&owner);return false;}return false;
}
static bool build_view(void){
 view.notice=notice;view.session=&hid;view.alarm=&alarm.status;view.alarm_available=alarm.available;view.key_available=power_key!=NULL;view.idle_available=resident.enabled&&policy.idle_known;view.background_available=background_ready&&scanner_background_status_copy().contexts_available;view.controls_available=resident.enabled&&risc_scene_lifecycle_get_v1(scene)!=NULL;
 btp_build(&candidate,&view,document.revision?document.revision+1:1);
 if(!candidate.revision){bbo_retain(&owner);return false;}return true;
}
static bool open_view(void){
 if(!build_view())return false;
 navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={1}};
 uint64_t opened=0;int rc=pointer?pointer->open(scene->context,&candidate,&navigation,&opened):hold?hold->open(scene->context,&candidate,&navigation,&opened):components->open(scene->context,&candidate,&navigation,&opened);
 if(!endpoint(rc))return false;
 if(rc!=0){
  if(!opened&&(rc==RISC_SCENE_UNAVAILABLE||rc==RISC_SCENE_INVALID)){leave=true;return false;}
  if(opened||(rc!=RISC_SCENE_BUSY&&rc!=RISC_SCENE_AGAIN))bbo_retain(&owner);
  return false;
 }
 if(!opened){bbo_retain(&owner);return false;}
 session=opened;document=candidate;if(view.page==BB_PAIR){pair_generation=hid.status.connection_generation;pair_number=hid.status.pairing_number;}if(view.page==BB_ALERT)displayed_alarm=alarm.status.occurrence;last_event=0;dirty=false;
 lifecycle=risc_scene_lifecycle_get_v1(scene);
 if(lifecycle){rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!endpoint(rc))return false;if(rc!=0){bbo_retain(&owner);return false;}}
 return true;
}
static bool snapshot(uint32_t *flags){
 int rc=scene->snapshot(scene->context,session,&navigation,flags);if(!endpoint(rc))return false;
 if(rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN)return false;
 if(rc!=0||navigation.api_version!=1||navigation.struct_size!=sizeof(navigation)||navigation.depth!=1||navigation.routes[0]!=1||(*flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_ACTIVITY))){bbo_retain(&owner);return false;}
 for(unsigned i=1;i<RISC_SCENE_MAX_DEPTH;i++)if(navigation.routes[i]||navigation.focus[i]){bbo_retain(&owner);return false;}
 return true;
}
static bool update_view(uint32_t flags){
 if(reset_view||!dirty||flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))return true;
 if(hid.holding||paired_press)return true;
 if(!build_view())return false;
 int rc=pointer?pointer->update(scene->context,session,&candidate):hold?hold->update(scene->context,session,&candidate):components->update(scene->context,session,&candidate);
 if(!endpoint(rc))return false;
 if(rc==0){document=candidate;if(view.page==BB_PAIR){pair_generation=hid.status.connection_generation;pair_number=hid.status.pairing_number;}if(view.page==BB_ALERT)displayed_alarm=alarm.status.occurrence;dirty=false;last_event=0;return true;}
 if(rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN)return true;
 message("VIEW UPDATE FAILED - RETRY");return true;
}
static bool alarm_stop(void *p){(void)p;return stop()&&pause_background();}
static bool handoff(void){
 if(!stop()||!pause_background()||!close_view())return false;
 int rc;
 if(pending_checkpoint==RISC_RESIDENT_APPLY_IDLE_ONLY&&!policy.idle_supported){
  risc_resident_request_v1 q={sizeof(q),RISC_RESIDENT_QUERY_IDLE_ONLY,0,0};risc_resident_reply_v1 out={sizeof(out),0};
  rc=resident.client.checkpoint(resident.client.invocation,&q,&out);
  if(!alive())return false;
  if(rc==RISC_RESIDENT_RETAINED||out.struct_size!=sizeof(out)||out.flags){bbo_retain(&owner);return false;}
  if(rc!=RISC_RESIDENT_OK){pending_checkpoint=0;policy.idle_known=false;message("IDLE POLICY UNAVAILABLE");return true;}
  policy.idle_supported=true;
 }
 rc=risc_scene_resident_dispatch_v1(&resident,pending_checkpoint);
 if(!alive())return false;
 if(rc==RISC_RESIDENT_RETAINED){bbo_retain(&owner);return false;}
 if(rc==RISC_RESIDENT_BUSY)return false;
 if(rc==RISC_RESIDENT_EXIT){leave=true;pending_checkpoint=0;return true;}
 if(rc!=RISC_RESIDENT_OK){message("SHARED CONTROLS UNAVAILABLE");}
 pending_checkpoint=0;view.page=BB_MAIN;dirty=true;uint64_t now;if(!bbo_now(&owner,&now)||!bbpolicy_load(&policy,&owner,now))return false;return true;
}
static void cleanup(void){
 if(!alive())return;
 if(hid.token||session){bbo_retain(&owner);return;}
 if(background_ready&&!scanner_background_close())return;
 for(unsigned i=5;i;i--)if(!bbo_release(&owner,&grants[i-1]))return;
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 if(!bbo_init(&owner))return;
 session=last_event=hold_generation=last_battery=0;pair_generation=pair_number=pending_checkpoint=0;paired_identity=(risc_scene_event_v1){0};dirty=true;leave=paired_press=local_fault=background_ready=false;notice[0]=0;
 view=(ble_touchpad_view){0};document=(risc_components_document_v1){0};resident=(risc_scene_resident_v1){0};scene=NULL;components=NULL;hold=NULL;pointer=NULL;lifecycle=NULL;reset_view=false;
 for(unsigned i=0;i<5;i++)grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
 if(!bbo_acquire(&owner,&grants[0],RISC_SCENE_CAPABILITY,1,0))return;
 scene=grants[0].api;
 if(scene->api_version!=1||scene->struct_size<sizeof(*scene)||!scene->next||!scene->snapshot||!scene->close){cleanup();return;}
 components=risc_scene_components_get_v1(scene);if(!components){cleanup();return;}
 hold=risc_scene_hold_get_v1(scene);view.hold_available=hold!=NULL;
 pointer=risc_scene_pointer_get_v1(scene);view.pointer_available=pointer!=NULL;
 /* Prove an available UI session before borrowing domain or storage services. */
 (void)bhs_init(&hid,NULL,bbo_live,&owner);alarm=(ble_buttons_alarm){0};power_key=NULL;policy=(ble_buttons_policy){0};
 if(!open_view()){cleanup();return;}
 bool got=bbo_acquire(&owner,&grants[1],RISC_BLUETOOTH_HID_CAPABILITY,1,0);if(!alive())return;
 view.hid_available=bhs_init(&hid,got?grants[1].api:NULL,bbo_live,&owner);
 if(!risc_scene_resident_bind_v1(owner.runtime,&resident)||!alive()){if(alive())bbo_retain(&owner);return;}
 view.controls_available=resident.enabled;
 got=bbo_acquire(&owner,&grants[3],"alarm.service.facade",2,0);if(!alive())return;
 if(!got){got=bbo_acquire(&owner,&grants[3],ALARM_SERVICE_CAPABILITY,1,0);if(!alive())return;}
 (void)bba_init(&alarm,got?grants[3].api:NULL,bbo_live,alarm_stop,&owner);
 got=bbo_acquire(&owner,&grants[4],RISC_POWER_KEY_CAPABILITY,1,0);if(!alive())return;power_key=got?grants[4].api:NULL;
 if(power_key&&(power_key->api_version!=1||power_key->struct_size<sizeof(*power_key)||!power_key->events))power_key=NULL;
 policy=(ble_buttons_policy){0};uint64_t initial;if(!bbo_now(&owner,&initial)||!bbpolicy_load(&policy,&owner,initial))return;
 if(!scanner_background_init(owner.runtime,bbo_live,background_retain,&owner))return;
 background_ready=true;reset_view=true;
 unsigned cleanup_wait=0;
 for(;;){
  uint64_t now;if(!bbo_now(&owner,&now))return;
  if(background_ready&&!local_fault&&!hid.closing&&!alarm.reserved&&!pending_checkpoint&&!leave&&!scanner_background_capture())return;
  if(hid.terminal){bbo_retain(&owner);return;}
  if(view.hid_available&&!bhs_poll(&hid,now)){if(hid.terminal){bbo_retain(&owner);return;}diagnostic("failure");if(!alive())return;message("HID ERROR - STOPPING");}
  if(hid.closing){if(++cleanup_wait>250){bbo_retain(&owner);return;}owner.runtime->yield_ms(10);if(!alive())return;continue;}else cleanup_wait=0;
  if(local_fault){bbo_retain(&owner);return;}
  bool manual_back=false;if(!bbpolicy_key(&policy,&owner,power_key,&manual_back,now))return;if(manual_back){if(hid.holding&&!bhs_release(&hid))continue;back();}
  if(hid.dirty){hid.dirty=false;dirty=true;reset_view=true;diagnostic("state");if(!alive())return;}
  if(hid.token&&!hid.closing&&now-last_battery>=30000)(void)battery_update(now);
  if(!alive())return;
  if(hid.status.state==RISC_HID_PAIR_CONFIRM&&hid.token&&!hid.closing&&view.page!=BB_PAIR){view.page=BB_PAIR;dirty=true;}
  if(view.page==BB_PAIR&&hid.status.state!=RISC_HID_PAIR_CONFIRM){view.page=BB_MAIN;dirty=true;}
  if(leave){if(stop()&&pause_background()&&bba_pause(&alarm)&&close_view())break;}
  else if(pending_checkpoint){(void)handoff();}
  if(!alive())return;
  if(reset_view&&session&&close_view())reset_view=false;
  if(!session&&!leave&&!pending_checkpoint){reset_view=false;(void)open_view();}
  if(!alive())return;
  if(session){
   risc_scene_pointer_event_v1 p={.struct_size=sizeof(p),.intent={.struct_size=sizeof(p.intent),.event={.struct_size=sizeof(p.intent.event)}}};
   int rc=pointer?pointer->next(scene->context,session,&p):hold?hold->next(scene->context,session,&p.intent):scene->next(scene->context,session,&p.intent.event);
   if(!endpoint(rc))return;
   if(rc==0){
    if(p.struct_size!=sizeof(p)||p.kind>RISC_POINTER_RESET){bbo_retain(&owner);return;}
    if(p.kind==RISC_POINTER_RESET){
     if(p.intent.struct_size!=sizeof(p.intent)||(p.intent.flags&~RISC_HOLD_LOCAL_FAULT_PENDING)||p.intent.phase||p.intent.reason||p.intent.generation||p.intent.held_ms||p.dx||p.dy||p.wheel_x||p.wheel_y||p.buttons||p.click){bbo_retain(&owner);return;}
     if(hid.token&&!bhs_release(&hid)&&!alive())return;
     if(p.intent.flags&RISC_HOLD_LOCAL_FAULT_PENDING){local_fault=true;(void)stop();}
    }else if(p.kind==RISC_POINTER_REPORT){
     const risc_scene_event_v1*e=&p.intent.event;
     if(e->struct_size!=sizeof(*e)||!e->sequence||e->sequence<=last_event||e->kind||p.intent.flags||p.intent.phase||p.intent.reason||p.intent.generation||p.intent.held_ms){bbo_retain(&owner);return;}
     last_event=e->sequence;
     const risc_scene_node_v1*n=event_node(e);
     if(!reset_view&&!leave&&!pending_checkpoint&&view.page==BB_MAIN&&e->document_revision==document.revision&&n&&n->kind==RISC_COMPONENT_POINTER&&n->action==BB_POINTER&&btp_ready(&hid)){
      if(!btp_report(&hid,&p,now)&&!hid.closing&&!hid.terminal){bbo_retain(&owner);return;}
     }
    }else {
     const risc_scene_hold_event_v1*h=&p.intent;
     if(hold&&(h->struct_size!=sizeof(*h)||h->phase>RISC_HOLD_CANCEL||h->reason>RISC_HOLD_REASON_LOCAL_FAULT||(h->flags&~RISC_HOLD_LOCAL_FAULT_PENDING)||(h->flags&&h->phase!=RISC_HOLD_CANCEL)||(h->phase!=RISC_HOLD_NONE&&!h->generation)||(h->phase==RISC_HOLD_PRESS&&h->held_ms))){bbo_retain(&owner);return;}
     input(h,now);
    }
   }else if(rc!=RISC_SCENE_IDLE&&rc!=RISC_SCENE_BUSY&&rc!=RISC_SCENE_AGAIN){bbo_retain(&owner);return;}
   if(!alive())return;
   if(hid.closing){owner.runtime->yield_ms(10);if(!alive())return;continue;}
   if(local_fault){if(!stop()){if(!alive())return;}else {bbo_retain(&owner);return;}}
   uint32_t flags=0;if(snapshot(&flags)){
    if(flags&RISC_SCENE_ACTIVITY){resident.activity=true;policy.activity=now;}
    (void)bba_step(&alarm,!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING)));
    if(!alive())return;
    if(alarm.terminal){bbo_retain(&owner);return;}
    if(hid.closing)continue;
    if(!alarm.reserved&&!leave&&!pending_checkpoint){if(!scanner_background_step(true)||!scanner_background_capture())return;}
    if(alarm.status.state==ALARM_STATE_ALERT||alarm.status.state==ALARM_STATE_DISMISSING){if(view.page!=BB_ALERT){view.page=BB_ALERT;dirty=true;reset_view=true;}}else if(view.page==BB_ALERT&&!alarm.reserved){view.page=BB_MAIN;dirty=true;reset_view=true;}
    /* Active HID owns native radio. No ordinary resident checkpoint can cross
     * its exit barrier. Explicit controls close it first, without auto-resume. */
    if(!hid.token&&!hid.closing&&!alarm.reserved&&!leave&&!pending_checkpoint&&resident.enabled&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))&&bbpolicy_due(&policy,now))pending_checkpoint=RISC_RESIDENT_APPLY_IDLE_ONLY;
    if(!leave&&!pending_checkpoint)(void)update_view(flags);
   }
  }
  if(!alive())return;
  owner.runtime->yield_ms(10);if(!alive())return;
 }
 cleanup();
}
