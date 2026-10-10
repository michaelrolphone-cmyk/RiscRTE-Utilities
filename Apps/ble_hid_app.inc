#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "RiscBluetoothHidV1.h"
#include "RiscTouchV1.h"
#include "PortableRadioSession.h"
#include "PortableRadioPolicy.h"
#include "PortableNovaUi.h"
#include "PortableAppSleep.h"
#include "ble_hid_model.h"
#ifdef PORTABLE_PAPER_HID
#include "PaperPresentation.h"
static const paper_presentation *paper;
#else
#define paper 0
#endif
#include <stdio.h>
#if defined(PORTABLE_TOUCH_ROTATION) && PORTABLE_TOUCH_ROTATION != 0
#error HID raw touch profile requires PORTABLE_TOUCH_ROTATION=0
#endif
#ifndef PORTABLE_APP_OWNS_TOUCH_CHROME
#error HID apps require application-owned touch chrome
#endif
#ifndef PORTABLE_RETURN_APP
#error HID apps require an explicit launcher return destination
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_bluetooth_hid_v1 *hid;
static const risc_touch_api_v1 *raw;
static risc_runtime_capability_v1 hid_grant,raw_grant,held_grant;
static bool hid_acquired,raw_acquired,held_acquired,dirty,closing,had_ready;
static uint64_t token,subscription,sequence;
static risc_bluetooth_hid_status_v1 status;
static hid_gesture gesture;
static hid_assignment assignments[4],draft;
static int held_button;
static uint8_t held_contact;
static bool button_gate;
static unsigned selected,view;
static uint32_t refreshed,reported,battery_at;
static bool close_error,save_failed,pair_armed,pair_down,pair_sent,paper_pair_release_gate;
static bool pair_trace_down,pair_trace_cancelled,pair_trace_gap;
static int pair_target,pair_x,pair_y;
static uint8_t pair_id;
static uint32_t pair_began,ui_block_until;
static const char *message;
enum { HID_VIEW_MAIN, HID_VIEW_SETTINGS, HID_VIEW_EDIT, HID_VIEW_MODIFIERS, HID_VIEW_FORGET };
static bool close_session(void);
static int hid_width(void){return paper?480:240;}
static int hid_height(void){return paper?800:240;}
static void session_diagnostic(const char *stage){
 if(!hid||closing)return;
 risc_bluetooth_hid_status_v1 current={.struct_size=sizeof(current)};
 bool ok=hid->status(hid->context,token,&current);
 char text[160];snprintf(text,sizeof(text),"HID %s status=%u state=%u flags=%lu error=%ld generation=%lu",stage,ok?1u:0u,(unsigned)current.state,(unsigned long)current.flags,(long)current.error,(unsigned long)current.connection_generation);runtime->diagnostic(text);
 if(hid->struct_size>=sizeof(risc_bluetooth_hid_diagnostics_api_v1)){
  const risc_bluetooth_hid_diagnostics_api_v1*d=(const void*)hid;
  risc_bluetooth_hid_diagnostics_v1 value={.struct_size=sizeof(value)};
  if(d->diagnostics&&d->diagnostics(hid->context,token,&value)){
   snprintf(text,sizeof(text),"HID cause port=%lu disconnect=%lu security=%lu notify=%ld failures=%lu gap_ms=%lu stale=%lu",(unsigned long)value.port_fault,(unsigned long)value.disconnect_reason,(unsigned long)value.security_status,(long)value.notify_error,(unsigned long)value.notify_failures,(unsigned long)value.max_poll_gap_ms,(unsigned long)value.stale_events);runtime->diagnostic(text);
   snprintf(text,sizeof(text),"HID cleanup stop=%ld native=%ld stopped=%lu poisoned=%lu",(long)value.host_stop_error,(long)value.native_close_result,(unsigned long)value.host_stopped,(unsigned long)value.poisoned);runtime->diagnostic(text);
  }
 }
}

static void cleanup_wait(void){
 runtime->diagnostic("HID cleanup pending; retaining invocation");
 while(!close_session())runtime->yield_ms(20);
}
static void retain_grant(risc_runtime_capability_v1*g){held_grant=*g;held_acquired=true;closing=true;cleanup_wait();}
static bool release_input(void){
 hid_gesture_reset(&gesture,true);button_gate=true;held_button=-1;pair_armed=pair_down=false;pair_target=-1;
 if(token&&hid&&!closing)return hid->release_all(hid->context,token);
 return true;
}
/* Failed close keeps both the token and provider grant alive. This is retryable
 * cleanup only; no reports, status reads or unrelated provider I/O may follow. */
static bool close_session(void){
 closing=true;hid_gesture_reset(&gesture,true);held_button=-1;button_gate=true;
 if(token){if(!hid->close(hid->context,token))return false;token=0;
  risc_bluetooth_hid_status_v1 final={.struct_size=sizeof(final)};
  if(!hid->status(hid->context,0,&final)||final.error)close_error=true;
  /* close succeeded: ordinary reads are legal again until the grant releases. */
  closing=false;session_diagnostic("closed");closing=true;
 }
 if(hid_acquired){if(!runtime->release(&hid_grant))return false;hid_acquired=false;hid=NULL;}
 if(subscription){if(!raw->unsubscribe(raw->context,subscription))return false;subscription=0;}
 if(raw_acquired){if(!runtime->release(&raw_grant))return false;raw_acquired=false;raw=NULL;}
 if(held_acquired){if(!runtime->release(&held_grant))return false;held_acquired=false;}
 closing=false;status=(risc_bluetooth_hid_status_v1){.struct_size=sizeof(status)};dirty=true;return true;
}
#ifdef PORTABLE_RADIO_CONTINUOUS_CAPTURE
/* An explicit HID session owns the foreground radio until checked close.
 * Reuse the resident radio-activity gate: ordinary battery/idle policy must
 * never tear down advertising, numeric comparison or a secure live link.
 * Explicit Stop, Back, controls, alarm and sleep still use suspend below. */
bool portable_radio_capture_active(void){return token!=0 || closing;}
#endif
bool portable_radio_services_safe(void){return !closing;}
bool portable_radio_suspend(void){
 bool was_active=token!=0;
 if(was_active&&!closing)session_diagnostic("suspend");
 if(token&&!closing)(void)release_input();
 if(!close_session())cleanup_wait();
 if(was_active)message=close_error?"Bluetooth session ended with error":"Paused - choose Reconnect";
 return true;
}
static void stop_session(const char*s){
 if(token&&!closing){char text[128];snprintf(text,sizeof(text),"HID stop reason=%s",s);runtime->diagnostic(text);}
 if(!portable_radio_suspend())cleanup_wait();
 /* The OFF error may be the original poll/report fault. Keep its reason on
  * screen; diagnostics preserve both that cause and the close outcome. */
 message=s;dirty=true;
}
static bool policy_allowed(void){
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};uint8_t flags=0;
 if(!runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g)){message="Radio settings unavailable";return false;}
 bool ok=portable_radio_load(g.api,&flags);if(!runtime->release(&g))retain_grant(&g);
 if(!ok){message="Radio settings unreadable";return false;}
 if(flags&PORTABLE_RADIO_AIRPLANE){message="Turn off Airplane mode";return false;}
 if(!(flags&PORTABLE_RADIO_BLUETOOTH)){message="Enable Bluetooth in controls";return false;}
 return true;
}
static bool valid_hid(const risc_bluetooth_hid_v1*p){return p&&p->api_version==1&&p->struct_size>=sizeof(*p)&&p->open&&p->poll&&p->status&&p->confirm_pairing&&p->keyboard&&p->mouse&&p->release_all&&p->close&&p->forget_bond&&p->battery;}
static bool acquire_hid(void){
 hid_grant=(risc_runtime_capability_v1){.struct_size=sizeof(hid_grant)};
 if(!runtime->acquire(RISC_BLUETOOTH_HID_CAPABILITY,1,0,&hid_grant)){message="HID driver unavailable";return false;}
 hid_acquired=true;hid=hid_grant.api;
 if(!valid_hid(hid)){message="HID driver update needed";return false;}return true;
}
static bool open_raw(void){
 raw_grant=(risc_runtime_capability_v1){.struct_size=sizeof(raw_grant)};
 if(!runtime->acquire("input.touch.raw",1,paper?0:6,&raw_grant)){message="Raw touch unavailable";return false;}
 raw_acquired=true;raw=raw_grant.api;
 if(!raw||raw->api_version!=1||raw->struct_size<sizeof(*raw)||!raw->subscribe||!raw->unsubscribe||!raw->poll||!raw->next||!raw->snapshot){message="Raw touch update needed";return false;}
 subscription=raw->subscribe(raw->context);risc_touch_snapshot_v1 s={0};
 if(!subscription||!raw->snapshot(raw->context,&s)||s.width!=hid_width()||s.height!=hid_height()||s.contact_count>RISC_TOUCH_MAX_CONTACTS){message="Raw touch start failed";return false;}
 sequence=s.sequence;hid_gesture_reset(&gesture,true);button_gate=true;return true;
}
static void update_battery(void){
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};uint8_t percent=255;
 if(runtime->acquire("board.battery",1,0,&g)){
  const risc_battery_gauge_api_v1*b=g.api;risc_battery_sample_v1 sample={0};
  if(b&&b->api_version==1&&b->struct_size>=sizeof(*b)&&b->read&&b->read(b->context,&sample)&&sample.percent<=100&&!(sample.flags&RISC_BATTERY_PROFILE_MISSING))percent=sample.percent;
  if(!runtime->release(&g)){retain_grant(&g);return;}
 }
 if(token&&!hid->battery(hid->context,token,percent))stop_session("Battery update failed");
 battery_at=app->millis();
}
static void start_session(bool pair){
 if(token||hid_acquired||raw_acquired)stop_session("Stopped");
 dirty=true;close_error=false;if(!policy_allowed())return;
 if(!acquire_hid()||!open_raw()){const char*s=message;stop_session(s);return;}
 token=0;bool ok=hid->open(hid->context,paper?(HID_TOUCHPAD?"Paper Touchpad":"Paper Buttons"):(HID_TOUCHPAD?"Watch Touchpad":"Watch Buttons"),pair,&token);
 if(!ok||!token){session_diagnostic("open-failed");stop_session(pair?"Pair start failed - retry":"Reconnect failed / no bond");return;}
 had_ready=false;status=(risc_bluetooth_hid_status_v1){.struct_size=sizeof(status)};
 message=pair?(paper?"Find Paper in host Bluetooth":"Find Watch in host Bluetooth"):"Waiting for saved host";refreshed=reported=app->millis();battery_at=refreshed-30000u;update_battery();
}
static void forget(void){
 stop_session("Stopped");
 if(!acquire_hid()){const char*s=message;stop_session(s);return;}
 bool ok=hid->status(hid->context,0,&status)&&status.state==RISC_HID_OFF&&hid->forget_bond(hid->context);
 stop_session(ok?"Bond forgotten - choose Pair":"Could not forget bond");view=HID_VIEW_MAIN;
}
static bool send_assignment(const hid_assignment*a,bool pressed){
 if(a->kind==HID_ACTION_KEY){uint8_t keys[6]={0};if(pressed)keys[0]=a->key;return hid->keyboard(hid->context,token,pressed?a->modifiers:0,keys);}
 return hid->mouse(hid->context,token,pressed?a->buttons:0,pressed?a->dx:0,pressed?a->dy:0,pressed?a->wheel:0);
}
static int button_at(int x,int y){if(paper){if(x<32||x>=448||y<224||y>=624||(x>=232&&x<248)||(y>=416&&y<432))return -1;return (y>=432?2:0)+(x>=248?1:0);}if(x<8||x>=232||y<80||y>=190|| (x>=116&&x<124)||(y>=131&&y<139))return -1;return (y>=139?2:0)+(x>=124?1:0);}
#if defined(PORTABLE_PAPER_HID) && defined(PORTABLE_RESIDENT_SHELL_CLIENT) && !HID_TOUCHPAD
/* The adapter and this app independently sample the same raw provider. A
 * complete short contact can therefore be queued while the final snapshot is
 * already neutral. Recognize only one uninterrupted, single-tile contact; the
 * ordinary state/sequence/neutral/timing gates still decide whether to act. */
typedef struct {int target;uint8_t id;bool down,complete,valid;uint64_t began,last;} hid_button_tap;
static void button_tap_event(hid_button_tap*t,const risc_touch_event_v1*e){
 if(!t->valid)return;
 int target=button_at(e->x,e->y);
 if(!t->down){
  if(e->kind!=RISC_TOUCH_EVENT_DOWN||target<0){t->valid=false;return;}
  t->down=true;t->target=target;t->id=e->id;t->began=t->last=e->timestamp_ms;return;
 }
 if(t->complete||e->id!=t->id||target!=t->target||e->timestamp_ms<t->last||
    e->timestamp_ms-t->began>100u){t->valid=false;return;}
 t->last=e->timestamp_ms;
 if(e->kind==RISC_TOUCH_EVENT_UP)t->complete=true;
 else if(e->kind!=RISC_TOUCH_EVENT_MOVE)t->valid=false;
}
#endif
static bool assignment_ready(const hid_assignment*a){
 uint32_t mask=RISC_HID_ENCRYPTED|RISC_HID_AUTHENTICATED|(a->kind==HID_ACTION_KEY?RISC_HID_KEYBOARD_READY:RISC_HID_MOUSE_READY);
 return token&&status.state==RISC_HID_READY&&(status.flags&mask)==mask;
}
static bool touch_buttons(const risc_touch_snapshot_v1*s,uint32_t now){
 if(button_gate){if(!s->contact_count)button_gate=false;return true;}
 int next=s->contact_count==1?button_at(s->contacts[0].x,s->contacts[0].y):-1;
 if(held_button>=0&&(next!=held_button||s->contacts[0].id!=held_contact)){if(!send_assignment(assignments+held_button,false))return false;held_button=-1;button_gate=s->contact_count!=0;dirty=true;return true;}
 if(next>=0&&!assignment_ready(assignments+next)){button_gate=true;return true;}
 if(held_button<0&&next<0&&s->contact_count){button_gate=true;return true;}
 if(held_button<0&&next>=0){held_button=next;held_contact=s->contacts[0].id;if(!send_assignment(assignments+next,true))return false;reported=now;dirty=true;}
 /* Relative wheel/motion are one shot per contact; only held reports refresh. */
 if(held_button>=0&&(uint32_t)(now-reported)>=400&&assignments[held_button].kind<=HID_ACTION_MOUSE){if(!send_assignment(assignments+held_button,true))return false;reported=now;}
 return true;
}
static const risc_bluetooth_hid_scroll_api_v1 *scroll_api(void){
 if(!hid||hid->struct_size<sizeof(risc_bluetooth_hid_scroll_api_v1))return NULL;
 const risc_bluetooth_hid_scroll_api_v1 *v=(const void*)hid;return v->mouse_scroll?v:NULL;
}
static bool touch_pad(const risc_touch_snapshot_v1*s,uint32_t now){
 hid_gesture_output o=paper?hid_gesture_sample_bounds(&gesture,s,now,32,224,448,624):hid_gesture_sample(&gesture,s,now);
 const risc_bluetooth_hid_scroll_api_v1 *scroll=scroll_api();
 /* Refresh a stationary drag well inside the provider's release watchdog. */
 bool report=o.dx||o.dy||o.button_changed||(o.buttons&&(uint32_t)(now-reported)>=400);
 if(report||((o.wheel_x||o.wheel_y)&&scroll)){
  bool ok=scroll?scroll->mouse_scroll(hid->context,token,o.buttons,o.dx,o.dy,o.wheel_x,o.wheel_y):
    hid->mouse(hid->context,token,o.buttons,o.dx,o.dy,0);
  if(!ok)return false;
  reported=now;
 }
 if(o.click){if(!hid->mouse(hid->context,token,o.click,0,0,0))return false;if(!hid->mouse(hid->context,token,0,0,0,0))return false;reported=now;}
 return true;
}
/* Confirmation consumes a fresh raw contact that begins after an observed
 * neutral pairing screen. A legacy pointer tap carried across a modal/start
 * can never approve pairing. Motion, extra contacts and event gaps cancel it. */
static const char *pair_target_name(int target){return target==1?"accept":target==0?"reject":"outside";}
static int pairing_target(int x,int y){if(paper)return portable_nova_hit(x,y,32,688,200,88)?0:portable_nova_hit(x,y,248,688,200,88)?1:-1;return portable_nova_hit(x,y,8,196,108,44)?0:portable_nova_hit(x,y,124,196,108,44)?1:-1;}
static void pairing_ignored(const char *reason){
 if(pair_trace_cancelled)return;
 char text[96];snprintf(text,sizeof(text),"HID pairing touch ignored reason=%s",reason);
 runtime->diagnostic(text);pair_trace_cancelled=true;
}
static void pairing_sample(const risc_touch_snapshot_v1*s,uint32_t now){
 if(pair_sent)return;
 if(!s->contact_count){
  pair_trace_down=false;pair_trace_cancelled=false;
  if(pair_down&&pair_target>=0&&(uint32_t)(now-pair_began)>800)pairing_ignored("held-too-long");
  if(pair_down&&pair_target>=0&&(uint32_t)(now-pair_began)<=800){
   bool accept=pair_target==1;paper_pair_release_gate=true;ui_block_until=now+100;pair_sent=true;pair_down=false;
   char text[96];snprintf(text,sizeof(text),"HID pairing response action=%s requested",pair_target_name(pair_target));runtime->diagnostic(text);
   bool confirmed=hid->confirm_pairing(hid->context,token,accept);
   snprintf(text,sizeof(text),"HID pairing response action=%s result=%s",pair_target_name(pair_target),confirmed?"ok":"failed");runtime->diagnostic(text);
   if(!confirmed){stop_session("Pairing response failed");return;}
   if(!accept){stop_session("Pairing rejected");return;}
   message="Confirming secure connection";dirty=true;
  }
  pair_down=false;pair_armed=true;return;
 }
 if(!pair_trace_down){
  const risc_touch_contact_v1*c=s->contacts;char text[128];
  snprintf(text,sizeof(text),"HID pairing contact target=%s x=%u y=%u contacts=%u armed=%u",pair_target_name(pairing_target(c->x,c->y)),(unsigned)c->x,(unsigned)c->y,(unsigned)s->contact_count,pair_armed?1u:0u);
  runtime->diagnostic(text);pair_trace_down=true;pair_trace_cancelled=false;
 }
 if(!pair_armed){pairing_ignored("not-armed");return;}
 if(s->contact_count!=1){pairing_ignored("multiple-contacts");pair_target=-1;pair_down=true;return;}
 const risc_touch_contact_v1*c=s->contacts;
 if(!pair_down){pair_down=true;pair_id=c->id;pair_x=c->x;pair_y=c->y;pair_began=now;pair_target=pairing_target(c->x,c->y);if(pair_target<0)pairing_ignored("outside-buttons");}
 else if(c->id!=pair_id||hid_abs((int)c->x-pair_x)>8||hid_abs((int)c->y-pair_y)>8){pairing_ignored(c->id!=pair_id?"contact-replaced":"moved");pair_target=-1;}
}
static void pump(void){
 if(!token||closing)return;
 uint32_t now=app->millis();
 if((uint32_t)(now-refreshed)>100){if(status.state==RISC_HID_PAIR_CONFIRM&&pair_down)pairing_ignored("timing-gap");if(!release_input()){stop_session("Input timing interrupted");return;}}refreshed=now;
 if(!hid->poll(hid->context,token,8)){stop_session("Bluetooth error - reconnect");return;}
 risc_bluetooth_hid_status_v1 next={.struct_size=sizeof(next)};
 if(!hid->status(hid->context,token,&next)){runtime->diagnostic("HID state read failed");stop_session("Connection error - reconnect");return;}
 if(next.state!=status.state||next.flags!=status.flags||next.error!=status.error||next.connection_generation!=status.connection_generation){
  char text[128];snprintf(text,sizeof(text),"HID state from=%u to=%u flags=%lu error=%ld generation=%lu",(unsigned)status.state,(unsigned)next.state,(unsigned long)next.flags,(long)next.error,(unsigned long)next.connection_generation);runtime->diagnostic(text);
 }
 status.error=next.error;
 if(next.state==RISC_HID_FAULT){stop_session("Connection error - reconnect");return;}
 if(status.state==RISC_HID_PAIR_CONFIRM&&next.state!=RISC_HID_PAIR_CONFIRM&&!pair_sent){stop_session("Pairing canceled / timed out");return;}
 if(next.state==RISC_HID_PAIR_CONFIRM&&next.pairing_number>999999u){stop_session("Invalid pairing comparison");return;}
 if(next.state!=status.state||next.flags!=status.flags||next.pairing_number!=status.pairing_number||next.connection_generation!=status.connection_generation){
  if(!release_input()){stop_session("Release failed - reconnect");return;}
  dirty=true;pair_sent=false;pair_trace_down=pair_trace_cancelled=pair_trace_gap=false;
  if(next.state==RISC_HID_READY)message=(next.flags&(RISC_HID_KEYBOARD_READY|RISC_HID_MOUSE_READY))?"Connected securely":"Waiting for HID subscription";
  else if(next.state==RISC_HID_PAIR_CONFIRM)message="Compare both devices";
  else if(next.state==RISC_HID_CONNECTED)message="Waiting for secure HID link";
  else if(had_ready)message="Disconnected - waiting for host";
  else if(status.state==RISC_HID_CONNECTED)message="Connection ended - retry Pair";
  status=next;if(next.state==RISC_HID_READY)had_ready=true;
 }
#if defined(PORTABLE_PAPER_HID) && defined(PORTABLE_RESIDENT_SHELL_CLIENT) && !HID_TOUCHPAD
 hid_button_tap queued_tap={.target=-1,.valid=true};
#endif
 bool polled=raw->poll(raw->context,1),gap=false,saw_up=false;int32_t rc=0;risc_touch_event_v1 e;
 for(unsigned n=0;n<RISC_TOUCH_QUEUE_LENGTH+1;n++){
  rc=raw->next(raw->context,subscription,&e);if(rc<=0)break;
  if(e.sequence!=sequence+1)gap=true;
  sequence=e.sequence;
#if defined(PORTABLE_PAPER_HID) && defined(PORTABLE_RESIDENT_SHELL_CLIENT) && !HID_TOUCHPAD
  if(paper)button_tap_event(&queued_tap,&e);
#endif
  if(e.kind==RISC_TOUCH_EVENT_UP)saw_up=true;
  if(e.kind==RISC_TOUCH_EVENT_DOWN&&saw_up)gap=true;
  if(n==RISC_TOUCH_QUEUE_LENGTH)gap=true;
 }
 if(rc==-2){stop_session("Touch error - reconnect");return;}
 if(rc<0||!polled)gap=true;
 risc_touch_snapshot_v1 sample={0};
 if(!raw->snapshot(raw->context,&sample)||sample.width!=hid_width()||sample.height!=hid_height()||sample.contact_count>RISC_TOUCH_MAX_CONTACTS){stop_session("Touch unavailable - reconnect");return;}
 if(sample.sequence!=sequence)gap=true;
 sequence=sample.sequence;
 if(status.state==RISC_HID_PAIR_CONFIRM&&gap&&!pair_trace_gap)runtime->diagnostic("HID pairing touch ignored reason=event-gap");
 pair_trace_gap=gap;
 if(gap&&!release_input()){stop_session("Touch gap - reconnect");return;}
 if(status.state==RISC_HID_PAIR_CONFIRM){pairing_sample(&sample,now);return;}
 uint32_t need=RISC_HID_ENCRYPTED|RISC_HID_AUTHENTICATED|(HID_TOUCHPAD?RISC_HID_MOUSE_READY:0);
 if(status.state==RISC_HID_READY&&(status.flags&need)==need&&view==HID_VIEW_MAIN){
  bool ok;
#if defined(PORTABLE_PAPER_HID) && defined(PORTABLE_RESIDENT_SHELL_CLIENT) && !HID_TOUCHPAD
  if(paper&&!gap&&!button_gate&&held_button<0&&!sample.contact_count&&
     queued_tap.valid&&queued_tap.complete&&(uint32_t)(app->millis()-refreshed)<=100u&&
     assignment_ready(assignments+queued_tap.target)){
   /* No earlier sampled press exists for this contact. Queue the same FIFO
    * press/neutral pair as an ordinary tap; neither path can deliver it twice. */
   ok=send_assignment(assignments+queued_tap.target,true)&&send_assignment(assignments+queued_tap.target,false);
  }else
#endif
   ok=HID_TOUCHPAD?touch_pad(&sample,now):touch_buttons(&sample,now);
  if(!ok){stop_session("Report rejected - reconnect");return;}
 }else{hid_gesture_reset(&gesture,true);button_gate=true;}
 if((uint32_t)(now-battery_at)>=30000)update_battery();
}
static void load_assignments(void){
 hid_defaults(assignments);if(HID_TOUCHPAD)return;
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,11,&g)){message="Assignments unavailable";return;}
 int result=hid_load(g.api,assignments);if(!runtime->release(&g))retain_grant(&g);
 if(result<0)message="Saved assignments unreadable";
}
static bool save_draft(void){
 save_failed=true;
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};hid_assignment next[4];memcpy(next,assignments,sizeof(next));next[selected]=draft;
 if(!runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,11,&g)){message="Save unavailable - retry";return false;}
 bool ok=hid_save(g.api,next);if(!runtime->release(&g))retain_grant(&g);
 if(!ok){message="Not saved - retry or cancel";return false;}
 memcpy(assignments,next,sizeof(next));message="Assignment saved";save_failed=false;return true;
}
static const char *kind_name(unsigned k){return k==0?"Keyboard":k==1?"Mouse button":k==2?"Mouse wheel":"Mouse motion";}
static void assignment_label(const hid_assignment*a,char text[48]){
 if(a->kind==HID_ACTION_KEY)snprintf(text,48,"%s",hid_keys[hid_key_index(a->key)].name);
 else if(a->kind==HID_ACTION_MOUSE)snprintf(text,48,"%s click",a->buttons==1?"Left":a->buttons==2?"Right":"Middle");
 else if(a->kind==HID_ACTION_WHEEL)snprintf(text,48,"%s %u",a->wheel>0?"Up":"Down",(unsigned)hid_abs(a->wheel));
 else snprintf(text,48,"%s %u px",a->dx?(a->dx>0?"Right":"Left"):(a->dy>0?"Down":"Up"),(unsigned)hid_abs(a->dx?a->dx:a->dy));
}
static void change_kind(void){unsigned k=(draft.kind+1)%4;draft=(hid_assignment){.kind=(uint8_t)k};if(k==0)draft.key=4;else if(k==1)draft.buttons=1;else if(k==2)draft.wheel=1;else draft.dx=5;}
static void change_value(int delta){
 if(draft.kind==HID_ACTION_KEY){int n=(int)hid_key_index(draft.key)+delta;if(n<0)n=(int)HID_KEY_COUNT-1;if(n>=(int)HID_KEY_COUNT)n=0;draft.key=hid_keys[n].code;}
 else if(draft.kind==HID_ACTION_MOUSE){unsigned n=draft.buttons==1?0:draft.buttons==2?1:2;n=(unsigned)((int)n+delta+3)%3;draft.buttons=(uint8_t)(1u<<n);}
 else if(draft.kind==HID_ACTION_WHEEL){static const int8_t values[]={1,-1,3,-3};unsigned n=0;while(n<3&&values[n]!=draft.wheel)n++;draft.wheel=values[((int)n+delta+4)%4];}
 else {static const int8_t values[][2]={{5,0},{-5,0},{0,5},{0,-5},{15,0},{-15,0},{0,15},{0,-15},{1,0},{-1,0},{0,1},{0,-1}};unsigned n=0;while(n<11&&(values[n][0]!=draft.dx||values[n][1]!=draft.dy))n++;n=(unsigned)((int)n+delta+12)%12;draft.dx=values[n][0];draft.dy=values[n][1];}
}
#ifdef PORTABLE_PAPER_HID
#include "ble_hid_paper.inc"
#endif
static void draw(void){
#ifdef PORTABLE_PAPER_HID
 if(paper){paper_draw();return;}
#endif
 char edit_title[24];snprintf(edit_title,sizeof(edit_title),"BUTTON %u",selected+1);
 portable_nova_begin();portable_nova_header(view==HID_VIEW_EDIT?(save_failed?"SAVE FAILED":edit_title):view==HID_VIEW_MODIFIERS?"MODIFIERS":view==HID_VIEW_SETTINGS?"HID SETTINGS":HID_TOUCHPAD?"BLE TOUCHPAD":"BLE BUTTONS");
 char text[64];
 if(status.state==RISC_HID_PAIR_CONFIRM&&token){
  portable_nova_center(1,8,61,224,"Compare with your host",NOVA_TEXT);snprintf(text,sizeof(text),"%06lu",(unsigned long)status.pairing_number);portable_nova_center(0,8,97,224,text,NOVA_CYAN);
  portable_nova_center(2,8,148,224,"Accept only if both numbers match",NOVA_CAP);
  portable_nova_button(8,196,108,44,"Reject",false);portable_nova_button(124,196,108,44,"Accept",true);
 }else if(view==HID_VIEW_MODIFIERS){
  static const char*names[]={"L Ctrl","L Shift","L Alt","L GUI","R Ctrl","R Shift","R Alt","R GUI"};
  for(unsigned i=0;i<8;i++)portable_nova_button(8+(int)(i%2)*116,54+(int)(i/2)*46,108,44,names[i],!!(draft.modifiers&(1u<<i)));
 }else if(view==HID_VIEW_EDIT){
  portable_nova_button(8,58,224,44,kind_name(draft.kind),false);
  portable_nova_button(8,104,44,44,"<",false);portable_nova_button(188,104,44,44,">",false);assignment_label(&draft,text);portable_nova_center(1,54,116,132,text,NOVA_TEXT);
  if(draft.kind==HID_ACTION_KEY){snprintf(text,sizeof(text),"Modifiers: 0x%02X",draft.modifiers);portable_nova_button(8,150,224,44,text,false);}
  portable_nova_button(8,196,108,44,"Cancel",false);portable_nova_button(124,196,108,44,save_failed?"Retry save":"Save",true);
 }else if(view==HID_VIEW_FORGET){
  portable_nova_center(1,8,76,224,"Forget the paired host?",NOVA_TEXT);portable_nova_center(2,8,111,224,"Remove Watch in host settings",NOVA_CAP);portable_nova_center(2,8,135,224,"before pairing again.",NOVA_CAP);
  portable_nova_button(8,196,108,44,"Cancel",false);portable_nova_button(124,196,108,44,"Forget",false);
 }else if(view==HID_VIEW_SETTINGS){
  portable_nova_button(8,54,108,44,"Pair",false);portable_nova_button(124,54,108,44,"Reconnect",true);
  portable_nova_button(8,104,224,44,"Forget host",false);
  if(!HID_TOUCHPAD){portable_nova_center(2,8,155,224,"Choose a button to configure",NOVA_CAP);for(unsigned i=0;i<4;i++){snprintf(text,sizeof(text),"%u",i+1);portable_nova_button(8+(int)i*58,184,50,48,text,false);}}
 }else{
  portable_nova_center(2,8,51,224,message?message:"Pair or reconnect in Settings",NOVA_CAP);
  if(HID_TOUCHPAD){portable_nova_round(8,80,224,110,8,NOVA_DIM);portable_nova_center(1,12,88,216,"TOUCHPAD",NOVA_TEXT);portable_nova_center(2,12,111,216,"Tap: left click",NOVA_CAP);portable_nova_center(2,12,131,216,"Tap then drag: hold click",NOVA_CAP);portable_nova_center(2,12,151,216,"Two-finger tap: right click",NOVA_CAP);portable_nova_center(2,12,171,216,hid&&!scroll_api()?"Scroll needs driver update":"Two-finger move: scroll X/Y",NOVA_CAP);}
  else for(unsigned i=0;i<4;i++){
   int x=8+(int)(i%2)*116,y=80+(int)(i/2)*59;portable_nova_round(x,y,108,51,5,held_button==(int)i?NOVA_CYAN:assignment_ready(assignments+i)?NOVA_DIM:NOVA_LINE);assignment_label(assignments+i,text);portable_nova_center(1,x+3,y+4,102,text,assignment_ready(assignments+i)?NOVA_TEXT:NOVA_CAP);if(assignments[i].modifiers){static const char*mods[]={"LC","LS","LA","LG","RC","RS","RA","RG"};unsigned used=0;text[0]=0;for(unsigned bit=0;bit<8;bit++)if(assignments[i].modifiers&(1u<<bit))used+=(unsigned)snprintf(text+used,sizeof(text)-used,"%s%s",used?" ":"",mods[bit]);}else snprintf(text,sizeof(text),"%u / %s",i+1,assignments[i].kind==0?"key":"mouse");portable_nova_center(2,x+3,y+28,102,text,NOVA_CAP);
  }
  portable_nova_button(8,196,108,44,token?"Stop":"Reconnect",false);portable_nova_button(124,196,108,44,"Settings",false);
 }
 app->present(false);dirty=false;
}
static bool back(void){
 if(view==HID_VIEW_MODIFIERS){view=HID_VIEW_EDIT;dirty=true;return true;}
 if(view!=HID_VIEW_MAIN){view=HID_VIEW_MAIN;message="Changes canceled";dirty=true;return true;}
 stop_session("Stopped");
 if(!runtime->request_launch||!runtime->request_launch(PORTABLE_RETURN_APP)){message="Return failed - retry";dirty=true;return true;}return false;
}
static void tap(int x,int y){
#ifdef PORTABLE_PAPER_HID
 if(paper&&!paper_tap_map(&x,&y))return;
#endif
 if((int32_t)(app->millis()-ui_block_until)<0)return;
 if(status.state==RISC_HID_PAIR_CONFIRM&&token)return;
 if(view==HID_VIEW_MODIFIERS){if(x>=8&&x<232&&y>=54&&y<238){unsigned col=(unsigned)(x-8)/116,row=(unsigned)(y-54)/46;if(col<2&&row<4&&x<8+(int)col*116+108&&y<54+(int)row*46+44){draft.modifiers^=(uint8_t)(1u<<(row*2+col));dirty=true;}}return;}
 if(view==HID_VIEW_EDIT){
  if(y>=196){if(x<116){view=HID_VIEW_MAIN;message="Changes canceled";}else if(x>=124&&save_draft())view=HID_VIEW_MAIN;}
  else if(portable_nova_hit(x,y,8,58,224,44))change_kind();
  else if(portable_nova_hit(x,y,8,104,44,44))change_value(-1);
  else if(portable_nova_hit(x,y,188,104,44,44))change_value(1);
  else if(draft.kind==0&&portable_nova_hit(x,y,8,150,224,44))view=HID_VIEW_MODIFIERS;
  dirty=true;return;
 }
 if(view==HID_VIEW_FORGET){if(y>=196){if(x<116){view=HID_VIEW_MAIN;message="Forget canceled";}else if(x>=124)forget();dirty=true;}return;}
 if(view==HID_VIEW_SETTINGS){
  if(portable_nova_hit(x,y,8,54,108,44)){view=HID_VIEW_MAIN;start_session(true);}
  else if(portable_nova_hit(x,y,124,54,108,44)){view=HID_VIEW_MAIN;start_session(false);}
  else if(portable_nova_hit(x,y,8,104,224,44))view=HID_VIEW_FORGET;
  else if(!HID_TOUCHPAD&&y>=184&&y<232&&x>=8&&x<232){selected=(unsigned)(x-8)/58;if(selected<4){draft=assignments[selected];view=HID_VIEW_EDIT;message=NULL;save_failed=false;}}
  dirty=true;return;
 }
 if(portable_nova_hit(x,y,8,196,108,44)){if(token)stop_session("Stopped");else start_session(false);}
 else if(portable_nova_hit(x,y,124,196,108,44)){stop_session("Stopped");view=HID_VIEW_SETTINGS;dirty=true;}
}
void app_main(void){
 app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);
 if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,touch_contact)+sizeof(app->touch_contact)||!app->poll||!app->millis||!app->present||!app->screen_width||!app->screen_height||!app->set_back_exits_app||!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms||!runtime->diagnostic)return;
#ifdef PORTABLE_PAPER_HID
 paper_pair_release_gate=false;hp_contact_down=hp_release_seen=false;paper=paper_presentation_get();
 if(paper&&(paper->struct_size<sizeof(*paper)||!paper->begin||!paper->text||!paper->measure||!paper->contact))return;
#endif
 if(app->screen_width()!=hid_width()||app->screen_height()!=hid_height())return;
 hid=0;raw=0;hid_acquired=raw_acquired=held_acquired=closing=had_ready=close_error=save_failed=pair_armed=pair_down=pair_sent=false;token=subscription=sequence=0;held_button=-1;button_gate=true;view=HID_VIEW_MAIN;selected=0;dirty=true;message="Pair or reconnect in Settings";status=(risc_bluetooth_hid_status_v1){.struct_size=sizeof(status)};hid_gesture_reset(&gesture,true);load_assignments();
 pair_trace_down=pair_trace_cancelled=pair_trace_gap=false;
 ui_block_until=app->millis();app->set_back_exits_app(false);
 for(;;){
  if(dirty&&(!paper||held_button<0))draw();
  t5_app_input_t input={0};if(!app->poll(&input,20)){if(portable_app_sleep_retained())return;break;}
  if(input.exit_requested)break;
#ifdef PORTABLE_PAPER_HID
  if(paper)paper_input(&input);
#endif
  if((input.buttons&T5_APP_BUTTON_BACK)||(input.tapped&&portable_nova_hit(input.touch_x,input.touch_y,paper?32:8,paper?16:4,paper?64:44,paper?88:44))){if(!back())break;continue;}
  if(input.tapped)tap(input.touch_x,input.touch_y);
  pump();
 }
 if(!portable_radio_suspend())cleanup_wait();
 app->set_back_exits_app(true);
}
