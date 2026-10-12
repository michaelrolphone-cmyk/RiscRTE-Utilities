/* OBD Dashboard: intent-driven scene app; no panel, GPIO, BLE-stack or ECU
 * transport internals. ELM protocol state is independent of the presenter.
 * Every live session owns an explicit serial grant and stops before shared UI. */
#include "dashboard.h"
#include "RiscSceneTelemetryV1.h"
#include "RiscStorageVolumeFsV1.h"
#include "RiscBluetoothSerialV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include <stdio.h>
#include <string.h>

enum { ACT_BACK=1,ACT_SCAN,ACT_EXIT,ACT_DISCONNECT,ACT_PAUSE,ACT_UNITS,ACT_TAB,
 ACT_MORE,ACT_PREVIOUS,ACT_CONNECT,ACT_SETTINGS,ACT_CUSTOM,ACT_GRAPH_ADD,ACT_GRAPH_PAUSE,
 ACT_GRAPH_SPAN,ACT_DETAIL_SPAN,ACT_THRESHOLD,ACT_LIMIT_HIGH,ACT_LIMIT_LOW,ACT_RESCAN,
 ACT_READINESS,ACT_LOG,ACT_ALERTS,ACT_UPDATE,ACT_PROTOCOL,ACT_ALERT_SWITCH,ACT_LOG_TRIP,
 ACT_CLEAR_ASK,ACT_CLEAR,ACT_KEEP,ACT_DISCONNECT_ASK,ACT_FORGET,ACT_ALERT_OK,ACT_MUTE,
 ACT_ALERT_GRAPH,ACT_POLL_PAUSE,ACT_PEER=100,ACT_CHANNEL=200,ACT_PID=300,
 ACT_ON_DASH=400,ACT_GRAPH_SENSOR=500,ACT_DTC=600,ACT_ALERT_ITEM=800 };
enum screen {INTRO,PEERS,CHANNELS,DASHBOARD,NOTICE,GRAPH,DETAIL,CUSTOM,FAULTS,DTC_DETAIL,
 READINESS,INFO,ADLOG,SETTINGS,ALERTS,GRAPH_PICK,CONFIRM,ALERT_MODAL,SELECTED,CONNECTING};
typedef struct {
 const risc_runtime_api_v1 *rt;
 const risc_bluetooth_serial_v1 *ble;
 const risc_scene_api_v1 *scene;
 const risc_scene_components_api_v1 *components;
 risc_scene_resident_v1 resident;
 risc_runtime_capability_v1 grants[3];
 const risc_scene_telemetry_api_v1 *telemetry_api;
 const risc_storage_volume_api_v1_ext *volume;
 const risc_storage_volume_api_v1_fs *fs;
 risc_storage_file_t log_file;char log_path[96];uint32_t log_bytes,last_log_sync,log_seen[OBD_METRICS];
 bool log_seen_once[OBD_METRICS],save_config,storage_error;
 db_model data;risc_scene_telemetry_v1 plot_candidate,plot_document;
 unsigned stack[8],stack_count,selected_metric,codes_tab,graph_span,detail_span,confirm_action;
 uint16_t selected_code;
 bool initial_queries,have_peer,reconnecting;
 unsigned reconnect_attempts;uint32_t reconnect_at,stable_since;bool stable_tracking;
 risc_serial_peer_v1 selected_peer;
 char toast[40];uint32_t toast_token;
 unsigned acquired,screen,tab,first,seen_clear_status;
 uint64_t session,token,sequence;
 uint32_t revision,now,last_paint,next_view,policy_at;
 bool dirty,done,retained,imperial,elm_started,view_settled,upgrade_required,reload_required;
 char notice[80],peer_name[40];
 obd_client elm;
 risc_serial_status_v1 status;
 risc_serial_peer_v1 peer_rows[8];
 risc_serial_channel_v1 channel_rows[8];
 risc_components_document_v1 document,candidate;
} obd_app;
static obd_app app;
static bool authority(obd_app*a){return !a->retained&&risc_runtime_get_api(1)!=NULL;}
static bool retain_app(obd_app*a){
 if(!a->retained){bool owned=risc_runtime_get_api(1)!=NULL;a->retained=true;if(owned&&a->rt&&a->rt->retain_invocation)a->rt->retain_invocation();}return false;
}
static bool scene_ok(obd_app*a,int rc){return rc==RISC_SCENE_RETAINED||!authority(a)?retain_app(a):rc==RISC_SCENE_OK;}
static void copy(char*d,size_t n,const char*s){if(n)snprintf(d,n,"%s",s?s:"");}
static void show(obd_app*a,unsigned s){a->screen=s;a->first=0;a->dirty=true;}
static void notice(obd_app*a,const char*s){copy(a->notice,sizeof(a->notice),s);show(a,NOTICE);}
static risc_scene_node_v1 *node(obd_app*a,unsigned kind,const char*label,const char*text,unsigned action){
 unsigned i=a->candidate.node_count++;risc_scene_node_v1*n=&a->candidate.nodes[i];n->id=i+1;n->route=1;n->kind=kind;n->action=action;
 copy(n->label,sizeof(n->label),label);copy(n->text,sizeof(n->text),text);a->candidate.details[i].marker=6;return n;
}
static void row(obd_app*a,const char*l,const char*t,unsigned action){node(a,action?RISC_COMPONENT_ROW:RISC_COMPONENT_STAT_ROW,l,t,action);}
static void segments(obd_app*a,const char*choices,unsigned value,unsigned maximum,unsigned action){
 risc_scene_node_v1*n=node(a,RISC_COMPONENT_SEGMENTS,"","",action);n->maximum=(int)maximum;n->value=(int)value;n->step=1;
 copy(a->candidate.details[n->id-1].choices,sizeof(a->candidate.details[0].choices),choices);
}
#include "storage.inc"
static bool read_status(obd_app*);
static bool stop_link(obd_app*a,const char*why){
 if(!authority(a))return retain_app(a);
 if(!log_close(a))return false;
 char reason[80];copy(reason,sizeof(reason),why);obd_stop(&a->elm,reason);a->elm_started=false;a->stable_tracking=false;
 if(a->token){
  for(unsigned i=0;i<3;i++){
   bool closed=a->ble->close(a->ble->context,a->token);if(!authority(a))return retain_app(a);
   if(closed){a->token=0;if(!read_status(a))return retain_app(a);break;}a->rt->yield_ms(10);if(!authority(a))return retain_app(a);
  }
  if(a->token)return retain_app(a);
 }
 a->status.state=RISC_SERIAL_OFF;return true;
}
static bool close_scene(obd_app*a){
 if(!a->session)return true;
 for(unsigned i=0;i<1000;i++){
  int rc=a->scene->close(a->scene->context,a->session);if(!authority(a)||rc==RISC_SCENE_RETAINED)return retain_app(a);
  if(rc==RISC_SCENE_OK){a->session=0;return true;}if(rc!=RISC_SCENE_AGAIN)return retain_app(a);
  a->rt->yield_ms(5);if(!authority(a))return retain_app(a);
 }return retain_app(a);
}
static bool read_status(obd_app*a){
 a->status.struct_size=sizeof(a->status);bool ok=a->ble->status(a->ble->context,a->token,&a->status);
 if(!authority(a))return retain_app(a);
 if(ok&&(a->status.flags&RISC_SERIAL_RELOAD_REQUIRED)){a->reload_required=true;a->reconnecting=false;a->dirty=true;}
 return ok;
}
static void start_scan(obd_app*a){
 if(a->reload_required){a->dirty=true;return;}
 if(!a->ble){notice(a,"Install firmware with bluetooth.serial provider");return;}
 if(!stop_link(a,"Starting new scan"))return;
 if(a->reload_required)return;
 bool ok=a->ble->open(a->ble->context,&a->token);if(!authority(a)){retain_app(a);return;}
 if(!ok){char error[80]="Bluetooth unavailable. Stop SDR/HID/Scanner.";if(read_status(a))copy(error,sizeof(error),a->status.message);
  if(stop_link(a,error))notice(a,error);return;}
 show(a,PEERS);read_status(a);a->next_view=a->now;
}
#include "ui.inc"
static bool publish(obd_app*a,bool opening){
 declare(a);if(!authority(a))return retain_app(a);
 if(!opening&&!memcmp(&a->candidate,&a->document,sizeof(a->document))&&!memcmp(&a->plot_candidate,&a->plot_document,sizeof(a->plot_document))){a->dirty=false;return true;}
 if(a->revision==UINT32_MAX)return retain_app(a);a->candidate.revision=++a->revision;
 int rc=opening?a->components->open(a->scene->context,&a->candidate,NULL,&a->session):a->telemetry_api?a->telemetry_api->present(a->scene->context,a->session,&a->candidate,&a->plot_candidate):a->components->update(a->scene->context,a->session,&a->candidate);
 if(!scene_ok(a,rc))return false;
 a->view_settled=false;a->document=a->candidate;a->plot_document=a->plot_candidate;a->last_paint=a->now;a->dirty=false;
 if(opening){a->sequence=0;rc=a->components->lifecycle.configure(a->scene->context,a->session,a->resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!scene_ok(a,rc))return false;}
 return true;
}
static bool valid_event(obd_app*a,const risc_scene_event_v1*e){
 if(e->document_revision!=a->revision||e->sequence<=a->sequence)return false;a->sequence=e->sequence;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT||e->kind==RISC_SCENE_CONTROLS_EVENT)return true;
 if(e->action==ACT_BACK&&!e->node)return e->kind==RISC_SCENE_ACTION_EVENT;
 if(!e->node||e->node>a->document.node_count)return false;const risc_scene_node_v1*n=&a->document.nodes[e->node-1];
 if(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)||!n->action||n->action!=e->action)return false;
 return n->kind==RISC_COMPONENT_SEGMENTS||n->kind==RISC_COMPONENT_TAB_BAR||n->kind==RISC_COMPONENT_STEPPER||n->kind==RISC_COMPONENT_SWITCH||n->kind==RISC_COMPONENT_CHECK_ROW?e->kind==RISC_SCENE_VALUE_EVENT&&e->value>=n->minimum&&e->value<=n->maximum:e->kind==RISC_SCENE_ACTION_EVENT;
}
#include "actions.inc"
static bool shared(obd_app*a,unsigned reason){
 a->reconnecting=false;if(!save_settings(a)||!stop_link(a,"Disconnected for shared controls")||!close_scene(a))return false;
 int rc=risc_scene_resident_dispatch_v1(&a->resident,reason);if(!authority(a)||rc==RISC_RESIDENT_RETAINED)return retain_app(a);
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;show(a,INTRO);return publish(a,true);
}
static bool service_link(obd_app*a){
 if(a->reconnecting&&!a->reload_required&&!a->token&&(int32_t)(a->now-a->reconnect_at)>=0){
  if(a->reconnect_attempts>=3){a->reconnecting=false;notice(a,"Reconnect stopped after 3 tries. Search again.");return true;}
  a->reconnect_attempts++;a->reconnect_at=a->now+5000;
  bool opened=a->ble->open(a->ble->context,&a->token);if(!authority(a))return retain_app(a);
  if(!opened){if(!stop_link(a,"Reconnect open failed"))return false;return true;}
 }
 if(!a->token)return true;uint32_t before=a->status.state;
 bool ok=a->ble->poll(a->ble->context,a->token,4);if(!authority(a))return retain_app(a);
 if(!read_status(a))return retain_app(a);
 if(!ok||a->reload_required||a->status.state==RISC_SERIAL_FAULT||a->status.state==RISC_SERIAL_DISCONNECTED){
  char error[80];copy(error,sizeof(error),a->status.message);bool retry=a->have_peer&&(a->reconnecting||a->elm_started);
  if(!stop_link(a,error))return false;
  if(retry&&!a->reload_required){a->reconnecting=true;a->reconnect_at=a->now+3000;show(a,DASHBOARD);}else notice(a,error);return true;
 }
 if(before!=a->status.state)a->dirty=true;
 if(a->reconnecting&&(a->status.state==RISC_SERIAL_SCANNING||a->status.state==RISC_SERIAL_SCAN_DONE)){
  for(unsigned i=0;i<a->status.peer_count;i++){
   risc_serial_peer_v1 p={.struct_size=sizeof(p)};bool found=a->ble->peer(a->ble->context,a->token,i,&p);if(!authority(a))return retain_app(a);
   if(found&&p.address_type==a->selected_peer.address_type&&!memcmp(p.address,a->selected_peer.address,6)){
    bool connected=a->ble->connect(a->ble->context,a->token,&p);if(!authority(a))return retain_app(a);if(!connected){if(!stop_link(a,"Reconnect failed"))return false;}
    /* The status snapshot belongs to the scan; do not expire a new connection. */
    return true;
   }
  }
  if(a->status.state==RISC_SERIAL_SCAN_DONE&&(int32_t)(a->now-a->reconnect_at)>=0){if(!stop_link(a,"Adapter not seen"))return false;}
 }
 if(a->status.state==RISC_SERIAL_CHANNELS&&a->screen!=CHANNELS){
  unsigned known=0,index=0;for(unsigned i=0;i<a->status.channel_count;i++){
   risc_serial_channel_v1 c={.struct_size=sizeof(c)};bool found=a->ble->channel(a->ble->context,a->token,i,&c);if(!authority(a))return retain_app(a);if(!found)return false;
   if(c.flags&RISC_SERIAL_KNOWN_UART){known++;index=i;}
  }
  if(known==1){if(!a->ble->select_channel(a->ble->context,a->token,index)){if(!authority(a))return retain_app(a);if(stop_link(a,"Subscription failed"))notice(a,"Cannot subscribe to adapter");}if(!authority(a))return retain_app(a);}
  else show(a,CHANNELS);
 }
 if(a->status.state==RISC_SERIAL_READY){
  if(!a->elm_started){obd_init(&a->elm,a->now);a->elm.requested_protocol=a->data.cfg.protocol;
   a->elm_started=true;a->initial_queries=false;a->reconnecting=false;a->stable_tracking=false;
   memset(a->data.seen_once,0,sizeof(a->data.seen_once));show(a,CONNECTING);}
  uint8_t b[128];for(unsigned i=0;i<4;i++){
   int32_t n=a->ble->read(a->ble->context,a->token,b,sizeof(b));if(!authority(a))return retain_app(a);
   if(n<0||n>(int32_t)sizeof(b)){if(stop_link(a,"Serial receive failed"))notice(a,"Serial receive failed");return !a->retained;}
   if(!n)break;obd_receive(&a->elm,b,(size_t)n,a->now);
  }
  obd_tick(&a->elm,a->now);
  if(a->seen_clear_status!=a->elm.clear_status){a->seen_clear_status=a->elm.clear_status;
   if(a->seen_clear_status==OBD_DIAG_VALID)feedback(a,"ECU acknowledged clear; rescanning");
   else if(a->seen_clear_status==OBD_DIAG_ERROR||a->seen_clear_status==OBD_DIAG_UNAVAILABLE)feedback(a,"Clear failed or cancelled; inspect log");}
  if(a->elm.state==OBD_FAULT){char error[80];copy(error,sizeof(error),a->elm.message);if(!stop_link(a,error))return false;notice(a,error);return true;}
  if(a->elm.state==OBD_LIVE&&obd_fresh(&a->elm,OBD_RPM,a->now)){
   if(!a->stable_tracking){a->stable_tracking=true;a->stable_since=a->now;}
   else if((uint32_t)(a->now-a->stable_since)>=10000)a->reconnect_attempts=0;
  }else a->stable_tracking=false;
  if(a->screen==CONNECTING&&a->elm.state==OBD_LIVE&&a->elm.vin_status>=OBD_DIAG_VALID)show(a,DASHBOARD);
  if(a->elm.state==OBD_LIVE&&!a->initial_queries){obd_request_info(&a->elm);obd_request_faults(&a->elm);a->initial_queries=true;}
  a->elm.poll_mask=db_poll_mask(&a->data,a->screen==DETAIL?(int)a->selected_metric:-1);
  if(a->screen==INFO)a->elm.poll_mask|=(1u<<OBD_STANDARD)|(1u<<OBD_DISTANCE_CLEAR)|(1u<<OBD_DISTANCE_MIL)|(1u<<OBD_RUNTIME);
  const char*command;size_t n=obd_next(&a->elm,a->now,&command);
  if(n){int32_t sent=a->ble->write(a->ble->context,a->token,(const uint8_t*)command,(uint32_t)n);if(!authority(a))return retain_app(a);
   if(sent==(int32_t)n)obd_sent(&a->elm,a->now);
   else if(sent!=0){if(stop_link(a,"Serial send failed"))notice(a,"Serial send failed");return !a->retained;}}
 }
 return true;
}
static bool serial_api_valid(const risc_bluetooth_serial_v1*b){return b&&b->api_version==1&&b->struct_size>=sizeof(*b)&&b->open&&b->poll&&b->status&&b->peer&&b->connect&&b->channel&&b->select_channel&&b->write&&b->read&&b->close;}
extern __attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1;
const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 obd_app*a=&app;memset(a,0,sizeof(*a));a->imperial=false;a->dirty=true;a->status.struct_size=sizeof(a->status);a->rt=risc_runtime_get_api(1);
 if(!a->rt||a->rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!a->rt->monotonic_ms||!a->rt->acquire||!a->rt->release||!a->rt->yield_ms||!a->rt->retain_invocation)return;
 a->grants[0].struct_size=sizeof(a->grants[0]);bool found=a->rt->acquire("ui.scene",1,0,&a->grants[0]);if(!authority(a))return;if(!found)return;a->acquired=1;
 a->scene=a->grants[0].api;a->components=risc_scene_components_get_v1(a->scene);a->telemetry_api=risc_scene_telemetry_get_v1(a->scene);
 if(!a->components||!a->components->lifecycle.configure||!a->scene->next||!a->scene->snapshot||!a->scene->close||!risc_scene_resident_bind_v1(a->rt,&a->resident))goto cleanup;
 if(!a->telemetry_api){a->upgrade_required=true;goto ui_ready;}
 a->grants[1].struct_size=sizeof(a->grants[1]);found=a->rt->acquire(RISC_BLUETOOTH_SERIAL_CAPABILITY,1,0,&a->grants[1]);if(!authority(a))return;
 if(found){a->acquired=2;if(serial_api_valid(a->grants[1].api))a->ble=a->grants[1].api;}
 a->grants[a->acquired].struct_size=sizeof(a->grants[0]);
 found=a->rt->acquire("storage.volume",1,0,&a->grants[a->acquired]);if(!authority(a))return;
 if(found){const risc_storage_volume_api_v1*b=a->grants[a->acquired].api;a->acquired++;a->volume=risc_storage_volume_extension(b);a->fs=risc_storage_volume_fs(b);if(!storage_api_valid(a->volume,a->fs)){a->volume=NULL;a->fs=NULL;}}
 ui_ready:
 if(a->rt->diagnostic)a->rt->diagnostic("OBD/dashboard version=0.2.0 transport=BLE clear=confirmed-only");if(!authority(a))return;
 if(!a->rt->monotonic_ms(&a->now)||!authority(a))goto cleanup;
 db_init(&a->data,a->now);if(!load_settings(a))goto cleanup;a->imperial=a->data.cfg.imperial!=0;
 if(!publish(a,true))goto cleanup;
 while(!a->done&&authority(a)){
  if(!a->rt->monotonic_ms(&a->now)||!authority(a))break;
  if(!service_link(a))break;
  db_tick(&a->data,&a->elm,a->now);
  if(a->data.alert_visible&&a->screen!=ALERT_MODAL&&a->screen!=CONFIRM){go(a,ALERT_MODAL);}
  if(!log_samples(a))break;
  risc_scene_event_v1 event={.struct_size=sizeof(event)};int rc=a->scene->next(a->scene->context,a->session,&event);
  if(!authority(a)||rc==RISC_SCENE_RETAINED){retain_app(a);break;}if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)break;
  if(rc==RISC_SCENE_OK&&valid_event(a,&event)){
   if(event.kind==RISC_SCENE_SUSPEND_EVENT){a->done=true;break;}
   if(event.kind==RISC_SCENE_CONTROLS_EVENT){if(a->resident.enabled&&!shared(a,RISC_RESIDENT_CHECKPOINT_CONTROLS))break;}
   else if(a->upgrade_required)a->done=true;
   else if(a->reload_required){if(event.action==ACT_EXIT||event.action==ACT_BACK)a->done=true;}
   else action(a,event.action,event.value);
  }
  if(a->done||!authority(a))break;
  risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;
  if(!scene_ok(a,a->scene->snapshot(a->scene->context,a->session,&nav,&flags)))break;
  if((uint32_t)(a->now-a->policy_at)>=100){
   a->policy_at=a->now;int status=risc_scene_resident_poll_work_v1(&a->resident,flags,a->token?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0);
   if(status==RISC_RESIDENT_RETAINED||!authority(a)){retain_app(a);break;}if(status==RISC_RESIDENT_EXIT)break;
   if(a->resident.policy&&!a->token&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))){if(!shared(a,RISC_RESIDENT_CHECKPOINT_POLICY))break;continue;}
  }
  /* Schedule data repaint from completed presentation. Starting another frame
   * immediately on completion can otherwise make every visible hit map stale. */
  if(!(flags&RISC_SCENE_PRESENTING)&&!a->view_settled){a->view_settled=true;a->next_view=a->now;}
  if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(a->dirty||(uint32_t)(a->now-a->next_view)>=a->data.cfg.update_ms)){
   a->next_view=a->now;if(!publish(a,false))break;
  }
  if(a->save_config&&a->view_settled&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!save_settings(a))break;
  a->rt->yield_ms(10);
 }
 cleanup:
 if(!authority(a)){if(!a->retained)retain_app(a);return;}
 if(!save_settings(a)||!stop_link(a,"Closed")||!close_scene(a))return;
 while(a->acquired){if(!authority(a))return;if(!a->rt->release(&a->grants[a->acquired-1])){retain_app(a);return;}a->acquired--;}
}
