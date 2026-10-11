/* OBD Dashboard: intent-driven scene app; no panel, GPIO, BLE-stack or ECU
 * transport internals. ELM protocol state is independent of the presenter.
 * Every live session owns an explicit serial grant and stops before shared UI. */
#include "elm.h"
#include "RiscBluetoothSerialV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include <stdio.h>
#include <string.h>

enum { ACT_BACK=1,ACT_SCAN,ACT_EXIT,ACT_DISCONNECT,ACT_PAUSE,ACT_UNITS,ACT_TAB,
 ACT_MORE,ACT_PREVIOUS,ACT_PEER=100,ACT_CHANNEL=200 };
enum screen {INTRO,PEERS,CHANNELS,DASHBOARD,NOTICE};
typedef struct {
 const risc_runtime_api_v1 *rt;
 const risc_bluetooth_serial_v1 *ble;
 const risc_scene_api_v1 *scene;
 const risc_scene_components_api_v1 *components;
 risc_scene_resident_v1 resident;
 risc_runtime_capability_v1 grants[2];
 unsigned acquired,screen,tab,first;
 uint64_t session,token,sequence;
 uint32_t revision,now,last_paint,next_view,policy_at;
 bool dirty,done,retained,imperial,elm_started;
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
static bool stop_link(obd_app*a,const char*why){
 if(!authority(a))return retain_app(a);
 char reason[80];copy(reason,sizeof(reason),why);obd_stop(&a->elm,reason);a->elm_started=false;
 if(a->token){
  for(unsigned i=0;i<3;i++){
   bool closed=a->ble->close(a->ble->context,a->token);if(!authority(a))return retain_app(a);
   if(closed){a->token=0;break;}a->rt->yield_ms(10);if(!authority(a))return retain_app(a);
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
 if(!authority(a))return retain_app(a);return ok;
}
static void start_scan(obd_app*a){
 if(!a->ble){notice(a,"Install firmware with bluetooth.serial provider");return;}
 if(!stop_link(a,"Starting new scan"))return;
 bool ok=a->ble->open(a->ble->context,&a->token);if(!authority(a)){retain_app(a);return;}
 if(!ok){char error[80]="Bluetooth unavailable. Stop SDR/HID/Scanner.";if(read_status(a))copy(error,sizeof(error),a->status.message);
  if(stop_link(a,error))notice(a,error);return;}
 show(a,PEERS);read_status(a);a->next_view=a->now;
}
static void metric_pair(obd_app*a,unsigned one,unsigned two){
 char left[32],right[32],lu[20],ru[20],captions[72];
 obd_format(&a->elm,one,a->now,a->imperial,left,sizeof(left),lu,sizeof(lu));
 obd_format(&a->elm,two,a->now,a->imperial,right,sizeof(right),ru,sizeof(ru));
 risc_scene_node_v1*n=node(a,RISC_COMPONENT_STATS,left,right,0);
 const char *ln=one==OBD_RPM?"ENGINE":one==OBD_COOLANT?"COOLANT":one==OBD_THROTTLE?"THROTTLE":one==OBD_MAP?"MAP":one==OBD_SHORT_TRIM?"SHORT TRIM":one==OBD_OIL?"OIL":obd_specs[one].name;
 const char *rn=two==OBD_SPEED?"SPEED":two==OBD_VOLTS?"ECU":two==OBD_LOAD?"LOAD":two==OBD_MAF?"AIR FLOW":two==OBD_LONG_TRIM?"LONG TRIM":two==OBD_INTAKE?"INTAKE":obd_specs[two].name;
 snprintf(captions,sizeof(captions),"%.16s %.14s|%.16s %.14s",ln,lu,rn,ru);copy(a->candidate.details[n->id-1].choices,sizeof(a->candidate.details[0].choices),captions);
}
static void declare(obd_app*a){
 memset(&a->candidate,0,sizeof(a->candidate));a->candidate.api_version=1;a->candidate.struct_size=sizeof(a->candidate);a->candidate.revision=a->revision;
 a->candidate.root=1;a->candidate.route_count=1;a->candidate.screen_key=1+a->screen*10+a->tab;a->candidate.flags=RISC_COMPONENTS_DOCUMENT;
 a->candidate.routes[0].id=1;a->candidate.routes[0].back_action=ACT_BACK;copy(a->candidate.routes[0].title,40,"VEHICLE");
 copy(a->candidate.subtitle,72,"OBD-II / READ-ONLY TELEMETRY");
 if(a->screen==INTRO){
  node(a,RISC_COMPONENT_CARD,"OBD DASHBOARD","Live data from your BLE ELM adapter",0);
  row(a,"SCAN FOR ADAPTER","Plug in adapter and switch ignition on",ACT_SCAN);
  segments(a,"km/h + C|mph + F",a->imperial,1,ACT_UNITS);
  node(a,RISC_SCENE_TEXT_NODE,"SET UP WHILE PARKED","Do not interact with this screen while driving.",0);
  node(a,RISC_SCENE_TEXT_NODE,"STANDARD ENGINE DATA","Available sensors depend on your vehicle. No coding or code clearing.",0);
  if(!a->ble)node(a,RISC_COMPONENT_EMPTY,"PROVIDER REQUIRED","This firmware needs the BLE serial provider.",0);
 }else if(a->screen==NOTICE){
  node(a,RISC_COMPONENT_EMPTY,"NOT CONNECTED",a->notice,0);row(a,"SCAN AGAIN","Choose your adapter",ACT_SCAN);row(a,"CLOSE","Return to Home",ACT_EXIT);
 }else if(a->screen==PEERS){
  if(a->status.state==RISC_SERIAL_SCANNING||a->status.state==RISC_SERIAL_STARTING){
   risc_scene_node_v1*n=node(a,RISC_COMPONENT_PROGRESS,"SCANNING",a->status.message,0);n->value=(a->now/1000)%3;
  }else if(a->status.state==RISC_SERIAL_SCAN_DONE){
   node(a,RISC_SCENE_TEXT_NODE,"SELECT YOUR ADAPTER","BLE only. Classic Bluetooth SPP is not supported by X4.",0);
   if(!a->status.peer_count)node(a,RISC_COMPONENT_EMPTY,"NO BLE DEVICES","Adapter may be Classic-only, asleep or used by another app.",0);
  }else {risc_scene_node_v1*n=node(a,RISC_COMPONENT_PROGRESS,"CONNECTING",a->status.message,0);n->value=(a->now/1000)%3;}
  if(a->status.state==RISC_SERIAL_SCANNING||a->status.state==RISC_SERIAL_SCAN_DONE){
   for(unsigned i=0;i<8&&a->first+i<a->status.peer_count;i++){
    risc_serial_peer_v1*p=&a->peer_rows[i];p->struct_size=sizeof(*p);
    if(!a->ble->peer(a->ble->context,a->token,a->first+i,p)||!authority(a))break;
    char label[40],detail[72];snprintf(label,sizeof(label),"%s",p->name[0]?p->name:"Unnamed BLE device");
    snprintf(detail,sizeof(detail),"%02X:%02X:%02X:%02X:%02X:%02X / %d dBm%s",p->address[5],p->address[4],p->address[3],p->address[2],p->address[1],p->address[0],p->rssi,p->connectable?"":" / beacon");
    risc_scene_node_v1*n=node(a,RISC_COMPONENT_ROW,label,detail,ACT_PEER+i);if(!p->connectable)n->flags|=RISC_SCENE_DISABLED;
   }
   if(a->first)row(a,"PREVIOUS DEVICES","",ACT_PREVIOUS);
   if(a->first+8<a->status.peer_count)row(a,"MORE DEVICES","",ACT_MORE);
  }
  if(a->status.dropped_peers)row(a,"SCAN LIMIT","First 24 devices retained; scan closer to adapter",0);
  row(a,"CANCEL","Stop Bluetooth session",ACT_DISCONNECT);
 }else if(a->screen==CHANNELS){
  node(a,RISC_SCENE_TEXT_NODE,"SELECT SERIAL CHANNEL","UUIDs are discovered, not inferred from the adapter name.",0);
  for(unsigned i=0;i<8&&a->first+i<a->status.channel_count;i++){
   risc_serial_channel_v1*p=&a->channel_rows[i];p->struct_size=sizeof(*p);
   if(!a->ble->channel(a->ble->context,a->token,a->first+i,p)||!authority(a))break;
   char name[40],detail[72];snprintf(name,sizeof(name),"%s %.22s",p->flags&RISC_SERIAL_KNOWN_UART?"UART":"SERVICE",p->service);
   snprintf(detail,sizeof(detail),"TX %.18s / RX %.18s (unpaired)",p->tx_uuid,p->rx_uuid);row(a,name,detail,ACT_CHANNEL+i);
  }
  if(a->first)row(a,"PREVIOUS CHANNELS","",ACT_PREVIOUS);
  if(a->first+8<a->status.channel_count)row(a,"MORE CHANNELS","",ACT_MORE);
  row(a,"CANCEL","Stop Bluetooth session",ACT_DISCONNECT);
 }else {
  segments(a,"DRIVE|ENGINE|MORE|LINK",a->tab,3,ACT_TAB);
  if(a->elm.state!=OBD_LIVE){
   risc_scene_node_v1*n=node(a,RISC_COMPONENT_PROGRESS,"ADAPTER CONNECTED",a->elm.message,0);n->value=(a->now/1000)%3;
   node(a,RISC_SCENE_TEXT_NODE,"ECU NOT YET READY","Waiting for verified vehicle replies",0);
  }else {
   if(a->tab==0){metric_pair(a,OBD_RPM,OBD_SPEED);metric_pair(a,OBD_COOLANT,OBD_VOLTS);metric_pair(a,OBD_THROTTLE,OBD_LOAD);}
   else if(a->tab==1){metric_pair(a,OBD_MAP,OBD_MAF);metric_pair(a,OBD_SHORT_TRIM,OBD_LONG_TRIM);metric_pair(a,OBD_OIL,OBD_INTAKE);}
   else if(a->tab==2){
    const unsigned values[]={OBD_FUEL,OBD_RATE,OBD_RUNTIME,OBD_MIL};
    for(unsigned j=0;j<4;j++){char v[32],u[20],s[64];unsigned i=values[j];obd_format(&a->elm,i,a->now,a->imperial,v,sizeof(v),u,sizeof(u));snprintf(s,sizeof(s),"%s %s",v,u);row(a,obd_specs[i].name,s,0);}
   }else {
    char s[72];row(a,"ADAPTER",a->peer_name,0);row(a,"ELM ID",a->elm.identity,0);
    snprintf(s,sizeof(s),"%lX / ELM protocol %lu",(unsigned long)a->elm.ecu,(unsigned long)a->elm.protocol);row(a,"SELECTED ECU",s,0);
    snprintf(s,sizeof(s),"%lu ms / %lu replies / %lu errors",(unsigned long)a->elm.rtt_ms,(unsigned long)a->elm.replies,(unsigned long)a->elm.failed_requests);row(a,"LAST ROUND TRIP",s,0);
    snprintf(s,sizeof(s),"RX %lu / TX %lu",(unsigned long)a->status.rx_bytes,(unsigned long)a->status.tx_bytes);row(a,"BLUETOOTH BYTES",s,0);
   }
   node(a,RISC_COMPONENT_STAT_ROW,a->elm.paused?"PAUSED / NOT LIVE":"LIVE / READ ONLY",a->elm.paused?"Resume to request fresh readings":a->elm.message,0);
   node(a,RISC_COMPONENT_CHIP,a->elm.paused?"RESUME":"PAUSE","",ACT_PAUSE);
  }
  node(a,RISC_COMPONENT_CHIP,"DISCONNECT","",ACT_DISCONNECT);
  if(a->tab==3)segments(a,"km/h + C|mph + F",a->imperial,1,ACT_UNITS);
 }
}
static bool publish(obd_app*a,bool opening){
 declare(a);if(!authority(a))return retain_app(a);
 if(!opening&&!memcmp(&a->candidate,&a->document,sizeof(a->document))){a->dirty=false;return true;}
 if(a->revision==UINT32_MAX)return retain_app(a);a->candidate.revision=++a->revision;
 int rc=opening?a->components->open(a->scene->context,&a->candidate,NULL,&a->session):a->components->update(a->scene->context,a->session,&a->candidate);
 if(!scene_ok(a,rc))return false;
 a->document=a->candidate;a->last_paint=a->now;a->dirty=false;
 if(opening){a->sequence=0;rc=a->components->lifecycle.configure(a->scene->context,a->session,a->resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!scene_ok(a,rc))return false;}
 return true;
}
static bool valid_event(obd_app*a,const risc_scene_event_v1*e){
 if(e->document_revision!=a->revision||e->sequence<=a->sequence)return false;a->sequence=e->sequence;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT||e->kind==RISC_SCENE_CONTROLS_EVENT)return true;
 if(e->action==ACT_BACK&&!e->node)return e->kind==RISC_SCENE_ACTION_EVENT;
 if(!e->node||e->node>a->document.node_count)return false;const risc_scene_node_v1*n=&a->document.nodes[e->node-1];
 if(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)||!n->action||n->action!=e->action)return false;
 return n->kind==RISC_COMPONENT_SEGMENTS?e->kind==RISC_SCENE_VALUE_EVENT&&e->value>=n->minimum&&e->value<=n->maximum:e->kind==RISC_SCENE_ACTION_EVENT;
}
static void action(obd_app*a,unsigned id,int value){
 if(id==ACT_SCAN){start_scan(a);return;}
 if(id==ACT_EXIT){a->done=true;return;}
 if(id==ACT_BACK||id==ACT_DISCONNECT){if(id==ACT_BACK&&a->screen==INTRO){a->done=true;return;}if(stop_link(a,"Disconnected"))show(a,INTRO);return;}
 if(id==ACT_UNITS){a->imperial=value!=0;a->dirty=true;return;}
 if(id==ACT_TAB){a->tab=(unsigned)value;a->dirty=true;return;}
 if(id==ACT_PAUSE){a->elm.paused=!a->elm.paused;a->dirty=true;return;}
 if(id==ACT_MORE){a->first+=8;a->dirty=true;return;}if(id==ACT_PREVIOUS){if(a->first>=8)a->first-=8;a->dirty=true;return;}
 if(id>=ACT_PEER&&id<ACT_PEER+8&&a->screen==PEERS){
  const risc_serial_peer_v1*p=&a->peer_rows[id-ACT_PEER];copy(a->peer_name,sizeof(a->peer_name),p->name[0]?p->name:"BLE adapter");
  bool ok=a->ble->connect(a->ble->context,a->token,p);if(!authority(a)){retain_app(a);return;}
  if(!ok){if(stop_link(a,"Connect failed"))notice(a,"Connection failed. Scan and select adapter again.");return;}read_status(a);a->dirty=true;return;
 }
 if(id>=ACT_CHANNEL&&id<ACT_CHANNEL+8&&a->screen==CHANNELS){
  bool ok=a->ble->select_channel(a->ble->context,a->token,a->first+id-ACT_CHANNEL);if(!authority(a)){retain_app(a);return;}
  if(!ok){if(stop_link(a,"Channel unavailable"))notice(a,"Serial channel selection failed");return;}show(a,PEERS);read_status(a);
 }
}
static bool shared(obd_app*a,unsigned reason){
 if(!stop_link(a,"Disconnected for shared controls")||!close_scene(a))return false;
 int rc=risc_scene_resident_dispatch_v1(&a->resident,reason);if(!authority(a)||rc==RISC_RESIDENT_RETAINED)return retain_app(a);
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;show(a,INTRO);return publish(a,true);
}
static bool service_link(obd_app*a){
 if(!a->token)return true;uint32_t before=a->status.state;
 bool ok=a->ble->poll(a->ble->context,a->token,4);if(!authority(a))return retain_app(a);
 if(!read_status(a))return retain_app(a);
 if(!ok||a->status.state==RISC_SERIAL_FAULT||a->status.state==RISC_SERIAL_DISCONNECTED){
  char error[80];copy(error,sizeof(error),a->status.message);if(!stop_link(a,error))return false;notice(a,error);return true;
 }
 if(before!=a->status.state)a->dirty=true;
 if(a->status.state==RISC_SERIAL_CHANNELS&&a->screen!=CHANNELS){
  unsigned known=0,index=0;for(unsigned i=0;i<a->status.channel_count;i++){
   risc_serial_channel_v1 c={.struct_size=sizeof(c)};bool found=a->ble->channel(a->ble->context,a->token,i,&c);if(!authority(a))return retain_app(a);if(!found)return false;
   if(c.flags&RISC_SERIAL_KNOWN_UART){known++;index=i;}
  }
  if(known==1){if(!a->ble->select_channel(a->ble->context,a->token,index)){if(!authority(a))return retain_app(a);if(stop_link(a,"Subscription failed"))notice(a,"Cannot subscribe to adapter");}if(!authority(a))return retain_app(a);}
  else show(a,CHANNELS);
 }
 if(a->status.state==RISC_SERIAL_READY){
  if(!a->elm_started){obd_init(&a->elm,a->now);a->elm_started=true;show(a,DASHBOARD);}
  uint8_t b[128];for(unsigned i=0;i<4;i++){
   int32_t n=a->ble->read(a->ble->context,a->token,b,sizeof(b));if(!authority(a))return retain_app(a);
   if(n<0||n>(int32_t)sizeof(b)){if(stop_link(a,"Serial receive failed"))notice(a,"Serial receive failed");return !a->retained;}
   if(!n)break;obd_receive(&a->elm,b,(size_t)n,a->now);
  }
  obd_tick(&a->elm,a->now);
  if(a->elm.state==OBD_FAULT){char error[80];copy(error,sizeof(error),a->elm.message);if(!stop_link(a,error))return false;notice(a,error);return true;}
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
 obd_app*a=&app;memset(a,0,sizeof(*a));a->imperial=true;a->dirty=true;a->status.struct_size=sizeof(a->status);a->rt=risc_runtime_get_api(1);
 if(!a->rt||a->rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!a->rt->monotonic_ms||!a->rt->acquire||!a->rt->release||!a->rt->yield_ms||!a->rt->retain_invocation)return;
 a->grants[0].struct_size=sizeof(a->grants[0]);bool found=a->rt->acquire("ui.scene",1,0,&a->grants[0]);if(!authority(a))return;if(!found)return;a->acquired=1;
 a->scene=a->grants[0].api;a->components=risc_scene_components_get_v1(a->scene);
 if(!a->components||!a->components->lifecycle.configure||!a->scene->next||!a->scene->snapshot||!a->scene->close||!risc_scene_resident_bind_v1(a->rt,&a->resident))goto cleanup;
 a->grants[1].struct_size=sizeof(a->grants[1]);found=a->rt->acquire(RISC_BLUETOOTH_SERIAL_CAPABILITY,1,0,&a->grants[1]);if(!authority(a))return;
 if(found){a->acquired=2;if(serial_api_valid(a->grants[1].api))a->ble=a->grants[1].api;}
 if(a->rt->diagnostic)a->rt->diagnostic("OBD/dashboard version=0.1.0 transport=BLE read_only=1");if(!authority(a))return;
 if(!a->rt->monotonic_ms(&a->now)||!authority(a))goto cleanup;
 if(!publish(a,true))goto cleanup;
 while(!a->done&&authority(a)){
  if(!a->rt->monotonic_ms(&a->now)||!authority(a))break;
  if(!service_link(a))break;
  risc_scene_event_v1 event={.struct_size=sizeof(event)};int rc=a->scene->next(a->scene->context,a->session,&event);
  if(!authority(a)||rc==RISC_SCENE_RETAINED){retain_app(a);break;}if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)break;
  if(rc==RISC_SCENE_OK&&valid_event(a,&event)){
   if(event.kind==RISC_SCENE_SUSPEND_EVENT){a->done=true;break;}
   if(event.kind==RISC_SCENE_CONTROLS_EVENT){if(a->resident.enabled&&!shared(a,RISC_RESIDENT_CHECKPOINT_CONTROLS))break;}
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
  if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(a->dirty||(uint32_t)(a->now-a->next_view)>=1000)){
   a->next_view=a->now;if(!publish(a,false))break;
  }
  a->rt->yield_ms(10);
 }
 cleanup:
 if(!authority(a)){if(!a->retained)retain_app(a);return;}
 if(!stop_link(a,"Closed")||!close_scene(a))return;
 while(a->acquired){if(!authority(a))return;if(!a->rt->release(&a->grants[a->acquired-1])){retain_app(a);return;}a->acquired--;}
}
