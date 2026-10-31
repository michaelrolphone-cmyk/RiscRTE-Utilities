/* One semantic power application. Providers own pixels, input, PMIC policy and
 * physical lifecycle. Original power samples and preference records are reused. */
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include "PortablePowerStatus.h"
#include "battery_broadcast.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum { OVERVIEW=1,DETAILS=2,BROADCAST=3 };
enum { REFRESH=1,PREVIOUS,NEXT,TOGGLE,ROOT_BACK,LEAVE_APP,OPEN_DETAILS,OPEN_BROADCAST };
static const risc_runtime_api_v1 *runtime;
static const risc_scene_api_v1 *scene;
static const risc_scene_lifecycle_api_v1 *lifecycle;
static const risc_scene_components_api_v1 *components;
static const risc_battery_gauge_api_v1 *gauge;
static risc_scene_resident_v1 resident;
static risc_runtime_capability_v1 grants[2];static bool acquired[2];
static risc_components_document_v1 document,candidate;
static risc_scene_navigation_v1 navigation;
static risc_battery_sample_v1 sample;
static battery_broadcast broadcast;
static telemetry_broadcast_status_v1 broadcast_status;
static uint64_t session,last_event;
static unsigned detail_page;
static bool terminal,read_ok,explicit_refresh,broadcast_ready,requested,save_failed;
static int broadcast_result;
static char notice[72],navigation_notice[72];
static void retain(void){if(!terminal){terminal=true;if(risc_runtime_get_api(1)==runtime)(void)runtime->retain_invocation();}}
static bool context_alive(void){if(terminal)return false;if(risc_runtime_get_api(1)!=runtime){terminal=true;return false;}return true;}
static bool helper_alive(void *p){(void)p;return context_alive();}
static void helper_retain(void *p){(void)p;retain();}
static bool scene_terminal(int32_t rc){if(!context_alive())return true;if(rc==RISC_SCENE_RETAINED){retain();return true;}return false;}
static bool extended(void){return read_ok&&(sample.flags&PORTABLE_POWER_STATUS_VALID);}
static bool flag(unsigned f){return extended()&&(sample.flags&f);}
static bool percent_known(void){return read_ok&&sample.percent<=100&&!(sample.flags&RISC_BATTERY_PROFILE_MISSING);}
static const char *yes_no(unsigned f){return !extended()?"Unknown":flag(f)?"Yes":"No";}
static const char *power_title(void){
 if(!read_ok)return gauge?"READ ERROR":"BATTERY UNAVAILABLE";
 if(extended()&&!flag(PORTABLE_POWER_BATTERY_PRESENT))return "NO BATTERY";
 if(flag(PORTABLE_POWER_THERMAL_LIMIT))return "THERMAL LIMIT";
 if(flag(PORTABLE_POWER_CHARGE_DONE))return "CHARGE COMPLETE";
 if(sample.flags&RISC_BATTERY_CHARGING)return "CHARGING";
 if(flag(PORTABLE_POWER_INPUT_READY))return flag(PORTABLE_POWER_CHARGER_ENABLED)?"USB POWER":"CHARGE DISABLED";
 if(percent_known()&&sample.percent<=10)return "BATTERY CRITICAL";
 if(percent_known()&&sample.percent<=20)return "LOW BATTERY";
 return "BATTERY STATUS";
}
static uint32_t power_tone(void){
 if(!read_ok||(percent_known()&&sample.percent<=10&&!(sample.flags&RISC_BATTERY_CHARGING)&&!flag(PORTABLE_POWER_INPUT_READY)))return RISC_TONE_DANGER;
 if(flag(PORTABLE_POWER_THERMAL_LIMIT)||(percent_known()&&sample.percent<=20&&!(sample.flags&RISC_BATTERY_CHARGING)))return RISC_TONE_WARNING;
 return flag(PORTABLE_POWER_CHARGE_DONE)?RISC_TONE_SUCCESS:RISC_TONE_ACCENT;
}
static const char *broadcast_label(void){
 switch(broadcast_status.state){
 case TELEMETRY_BROADCAST_OFF:return "Disabled";case TELEMETRY_BROADCAST_BLE_OFF:return "Bluetooth is off";
 case TELEMETRY_BROADCAST_PAUSED:return "Paused";case TELEMETRY_BROADCAST_STARTING:return "Starting";
 case TELEMETRY_BROADCAST_LIVE:return "Broadcasting";case TELEMETRY_BROADCAST_RETRY:return "Waiting / retrying";
 case TELEMETRY_BROADCAST_RETAINED:return "Cleanup pending";default:return "Settings unavailable";}
}
static risc_scene_node_v1 *node(unsigned id,unsigned route,unsigned kind,const char *label,const char *text){
 risc_scene_node_v1 *n=&candidate.nodes[candidate.node_count++];*n=(risc_scene_node_v1){.id=id,.route=route,.kind=kind};
 snprintf(n->label,sizeof(n->label),"%s",label);snprintf(n->text,sizeof(n->text),"%s",text?text:"");return n;
}
static void declare(void){
 candidate=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=document.revision?document.revision:1,.root=OVERVIEW,.route_count=3,.screen_key=1,.flags=RISC_COMPONENTS_ROOT_HEADER};
 candidate.routes[0]=(risc_scene_route_v1){OVERVIEW,LEAVE_APP,"POWER"};candidate.routes[1]=(risc_scene_route_v1){DETAILS,ROOT_BACK,"POWER DETAILS"};candidate.routes[2]=(risc_scene_route_v1){BROADCAST,ROOT_BACK,"BLE BROADCAST"};
 char text[72];if(percent_known())snprintf(text,sizeof(text),"%u%%",(unsigned)sample.percent);else snprintf(text,sizeof(text),"ESTIMATE UNAVAILABLE");
 risc_scene_node_v1 *hero=node(1,OVERVIEW,RISC_COMPONENT_RING,"BATTERY ESTIMATE",text);hero->value=percent_known()?sample.percent:0;hero->maximum=percent_known()?100:0;candidate.details[0].symbol=RISC_SYMBOL_GAUGE;candidate.details[0].tone=power_tone();node(2,OVERVIEW,RISC_SCENE_TEXT_NODE,"STATUS",power_title());
 const char *caption=!read_ok?"Retrying every second":!extended()?"Basic telemetry only":flag(PORTABLE_POWER_THERMAL_LIMIT)?"PMIC limiting charge":flag(PORTABLE_POWER_CHARGE_DONE)?"Charger reports done":flag(PORTABLE_POWER_INPUT_READY)?"External power available":"USB not available";
 node(3,OVERVIEW,RISC_SCENE_TEXT_NODE,"POWER SOURCE",caption);
 node(4,OVERVIEW,RISC_SCENE_ACTION,"DETAILS","")->action=OPEN_DETAILS;
 node(5,OVERVIEW,RISC_SCENE_ACTION,read_ok?"REFRESH":"RETRY","")->action=REFRESH;
 node(6,OVERVIEW,RISC_SCENE_ACTION,"BLE BROADCAST","")->action=OPEN_BROADCAST;
 const char *a,*b,*av,*bv;char voltage[32];snprintf(voltage,sizeof(voltage),"%u mV",(unsigned)sample.millivolts);
 if(detail_page==0){a="Battery voltage";av=read_ok?voltage:"Unavailable";b="USB available";bv=yes_no(PORTABLE_POWER_INPUT_READY);}
 else if(detail_page==1){a="Charger enabled";av=yes_no(PORTABLE_POWER_CHARGER_ENABLED);b="Charge complete";bv=yes_no(PORTABLE_POWER_CHARGE_DONE);}
 else if(detail_page==2){a="Battery present";av=yes_no(PORTABLE_POWER_BATTERY_PRESENT);b="PMIC thermal limit";bv=yes_no(PORTABLE_POWER_THERMAL_LIMIT);}
 else {a="Charging is autonomous";av="Sleep does not stop the PMIC charger";b="Protection conditions apply";bv="No cell temperature or time-to-full measurement";}
 node(7,DETAILS,RISC_SCENE_TEXT_NODE,a,av);node(8,DETAILS,RISC_SCENE_TEXT_NODE,b,bv);
 snprintf(text,sizeof(text),"%u / 4%s",detail_page+1,read_ok?"":" - Read error");node(9,DETAILS,RISC_SCENE_TEXT_NODE,"PAGE",text);
 node(10,DETAILS,RISC_SCENE_ACTION,"PREV","")->action=PREVIOUS;node(11,DETAILS,RISC_SCENE_ACTION,"NEXT","")->action=NEXT;node(12,DETAILS,RISC_SCENE_ACTION,read_ok?"REFRESH":"RETRY","")->action=REFRESH;
 node(13,BROADCAST,RISC_SCENE_TEXT_NODE,"SENSOR STATUS",broadcast_ready?broadcast_label():"BROADCAST UNAVAILABLE");
 node(14,BROADCAST,RISC_SCENE_TEXT_NODE,"SENSOR VALUES","Available values only; no estimated temperature");node(15,BROADCAST,RISC_SCENE_TEXT_NODE,"PRIVACY","Open to nearby receivers without pairing");
 node(16,BROADCAST,RISC_SCENE_TEXT_NODE,"BLE LIMIT",broadcast_status.omitted_fields?"Some fields exceed BLE limit":"")->flags=broadcast_status.omitted_fields?0:RISC_SCENE_HIDDEN;
 node(17,BROADCAST,RISC_SCENE_TEXT_NODE,"SAVE STATUS",notice)->flags=notice[0]?0:RISC_SCENE_HIDDEN;
 risc_scene_node_v1 *n=node(18,BROADCAST,RISC_SCENE_ACTION,save_failed?"RETRY":broadcast_status.enabled?"DISABLE":"ENABLE","");n->action=TOGGLE;if(!broadcast_ready||broadcast_result==3)n->flags=RISC_SCENE_DISABLED;
 n=node(19,OVERVIEW,RISC_SCENE_TEXT_NODE,"PRESENTATION CAPABILITIES",!lifecycle?"AUTOMATIC REFRESH AND SHARED CONTROLS UNAVAILABLE":!resident.enabled?"SHARED CONTROLS UNAVAILABLE":"");if(lifecycle&&resident.enabled)n->flags=RISC_SCENE_HIDDEN;
 for(unsigned i=0;i<3;i++){n=node(20+i,i+1,RISC_SCENE_TEXT_NODE,"NAVIGATION",navigation_notice);if(!navigation_notice[0])n->flags=RISC_SCENE_HIDDEN;}
}
static bool refresh_status(void){
 risc_battery_sample_v1 fresh={0,255,RISC_BATTERY_PROFILE_MISSING};bool ok=gauge&&gauge->read(gauge->context,&fresh);if(!context_alive())return false;
 read_ok=ok;sample=ok?fresh:(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};
 if(broadcast_ready){telemetry_broadcast_status_v1 s={.struct_size=sizeof(s)};int r=battery_broadcast_read(&broadcast,&s);if(!context_alive())return false;if(r==4){retain();return false;}if(r==0)broadcast_status=s;else if(r!=3)broadcast_status.state=TELEMETRY_BROADCAST_SETTINGS_ERROR;}
 return true;
}
static bool present_model(void){declare();if(!memcmp(&candidate,&document,sizeof(document)))return true;if(document.revision==UINT32_MAX)return false;candidate.revision=document.revision+1;int32_t rc=components->update(scene->context,session,&candidate);if(scene_terminal(rc)||rc!=0)return false;document=candidate;return true;}
static bool close_scene(void){if(!session)return true;for(unsigned i=0;i<250;i++){int32_t rc=scene->close(scene->context,session);if(scene_terminal(rc))return false;if(rc==0){session=0;return true;}if(rc!=RISC_SCENE_AGAIN){retain();return false;}runtime->yield_ms(20);if(!context_alive())return false;}retain();return false;}
static bool pause_broadcast(void){
 if(!broadcast_ready)return true;
 for(unsigned i=0;i<250;i++){
  int r=battery_broadcast_pause(&broadcast);if(!context_alive())return false;
  if(r==4){retain();return false;}if(r==0||r==1)return true;
  if(r!=3)return false;
  r=battery_broadcast_step(&broadcast,false);if(!context_alive())return false;if(r==4){retain();return false;}
  if(r!=3){broadcast_result=r;save_failed=r!=0;snprintf(notice,sizeof(notice),"%s",save_failed?"NOT SAVED - RETRY SAME REQUEST":"");if(r==0)(void)refresh_status();}
  runtime->yield_ms(20);if(!context_alive())return false;
 }
 retain();return false;
}
static bool configure_scene(void){lifecycle=risc_scene_lifecycle_get_v1(scene);explicit_refresh=true;if(lifecycle){int32_t rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(scene_terminal(rc))return false;if(rc!=0)lifecycle=NULL;}return true;}
static bool handoff(unsigned reason){
 if(!pause_broadcast())return terminal;
 if(!close_scene())return true;
 int32_t rc=risc_scene_resident_dispatch_v1(&resident,reason);if(!context_alive())return true;if(rc==RISC_RESIDENT_RETAINED){retain();return true;}if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return true;
 declare();document=candidate;rc=components->open(scene->context,&document,&navigation,&session);if(scene_terminal(rc)||rc!=0)return true;last_event=0;return !configure_scene();
}
static bool handle_event(const risc_scene_event_v1 *e){
 if(e->struct_size<sizeof(*e)||!e->sequence||e->sequence<=last_event||e->document_revision!=document.revision)return false;
 last_event=e->sequence;
 if(e->kind==RISC_SCENE_CONTROLS_EVENT&&!e->node&&!e->action)return resident.enabled?handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS):false;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT&&!e->node&&!e->action)return pause_broadcast();
 if(e->kind!=RISC_SCENE_ACTION_EVENT)return false;
 unsigned route=navigation.depth?navigation.routes[navigation.depth-1]:OVERVIEW;
 if(!e->node){if(e->action==LEAVE_APP&&route==OVERVIEW)return pause_broadcast();if(e->action==ROOT_BACK&&route!=OVERVIEW){int32_t r=scene->navigate(scene->context,session,RISC_SCENE_ROOT,0);if(!scene_terminal(r))snprintf(navigation_notice,sizeof(navigation_notice),"%s",r?"BACK FAILED - TRY AGAIN":"");explicit_refresh=true;}return false;}
 const risc_scene_node_v1 *n=NULL;for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].id==e->node)n=&document.nodes[i];
 if(!n||n->kind!=RISC_SCENE_ACTION||n->route!=route||n->action!=e->action||(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)))return false;
 explicit_refresh=true;
 if(e->action==OPEN_DETAILS||e->action==OPEN_BROADCAST){if(e->action==OPEN_DETAILS)detail_page=0;int32_t r=scene->navigate(scene->context,session,RISC_SCENE_PUSH,e->action==OPEN_DETAILS?DETAILS:BROADCAST);if(!scene_terminal(r))snprintf(navigation_notice,sizeof(navigation_notice),"%s",r?"NAVIGATION FAILED - TRY AGAIN":"");}
 else if(e->action==REFRESH)(void)refresh_status();else if(e->action==PREVIOUS)detail_page=(detail_page+3)%4;else if(e->action==NEXT)detail_page=(detail_page+1)%4;
 else if(e->action==TOGGLE){if(!save_failed)requested=!broadcast_status.enabled;broadcast_result=battery_broadcast_set_enabled(&broadcast,requested);if(!context_alive())return true;if(broadcast_result==4){retain();return true;}if(broadcast_result==0)(void)refresh_status();save_failed=broadcast_result!=0&&broadcast_result!=3;snprintf(notice,sizeof(notice),"%s",broadcast_result==3?"SAVING - CHECKING":broadcast_result==0?"":broadcast_result==1?"PREFERENCE STORAGE UNAVAILABLE":"NOT SAVED - RETRY SAME REQUEST");}
 return false;
}
static void close_dependencies(void){
 if(!context_alive())return;
 if(broadcast_ready){
  unsigned i=0;for(;i<250;i++){int r=battery_broadcast_cleanup(&broadcast);if(!context_alive())return;if(r==0||r==1)break;if(r!=3){retain();return;}runtime->yield_ms(20);if(!context_alive())return;}
  if(i==250){retain();return;}broadcast_ready=false;
 }
 for(unsigned i=2;i>0;i--)if(acquired[i-1]){acquired[i-1]=false;bool ok=runtime->release(&grants[i-1]);if(!context_alive())return;if(!ok){retain();return;}}
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 runtime=risc_runtime_get_api(1);if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release||!runtime->yield_ms)return;
 terminal=false;session=last_event=0;read_ok=false;detail_page=0;explicit_refresh=true;broadcast_ready=false;save_failed=false;requested=false;broadcast_result=0;notice[0]=navigation_notice[0]=0;scene=NULL;gauge=NULL;lifecycle=NULL;components=NULL;resident=(risc_scene_resident_v1){0};memset(acquired,0,sizeof(acquired));document=(risc_components_document_v1){0};sample=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};broadcast_status=(telemetry_broadcast_status_v1){.struct_size=sizeof(broadcast_status)};
 navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={OVERVIEW}};
 const char *names[]={RISC_SCENE_CAPABILITY,RISC_BATTERY_GAUGE_CAPABILITY};
 for(unsigned i=0;i<2;i++){grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};bool ok=runtime->acquire(names[i],1,0,&grants[i]);if(!context_alive())return;if(!ok&&(grants[i].api||grants[i].slot||grants[i].generation)){retain();return;}if(ok&&(!grants[i].api||!grants[i].slot||!grants[i].generation||grants[i].struct_size!=sizeof(grants[i]))){retain();return;}acquired[i]=ok;if(i==0&&!ok)return;}
 scene=grants[0].api;if(acquired[1])gauge=grants[1].api;
 if(!scene||scene->api_version!=1||scene->struct_size<sizeof(*scene)||!scene->open||!scene->update||!scene->next||!scene->snapshot||!scene->close||!scene->navigate){close_dependencies();return;}
 components=risc_scene_components_get_v1(scene);if(!components){if(runtime->diagnostic)runtime->diagnostic("Battery semantic components unavailable");if(context_alive())close_dependencies();return;}
 if(gauge&&(gauge->api_version!=1||gauge->struct_size<sizeof(*gauge)||!gauge->read))gauge=NULL;
 if(!risc_scene_resident_bind_v1(runtime,&resident)){retain();return;}if(!context_alive())return;
 int br=battery_broadcast_init(&broadcast,runtime,helper_alive,helper_retain,NULL);if(!context_alive())return;if(br==4){retain();return;}broadcast_ready=br==0;
 if(broadcast_ready){br=battery_broadcast_step(&broadcast,true);if(!context_alive())return;if(br==4){retain();return;}}
 if(!refresh_status()){close_dependencies();return;}declare();document=candidate;
 int32_t rc=components->open(scene->context,&document,&navigation,&session);if(scene_terminal(rc))return;if(rc!=0){close_dependencies();return;}if(!configure_scene()){if(pause_broadcast()&&close_scene())close_dependencies();return;}
 unsigned ticks=0;
 for(;;){risc_scene_event_v1 e={.struct_size=sizeof(e)};rc=scene->next(scene->context,session,&e);if(scene_terminal(rc))return;if(rc!=0&&rc!=RISC_SCENE_IDLE)break;if(rc==0&&handle_event(&e))break;if(terminal)return;
  uint32_t flags=0;rc=scene->snapshot(scene->context,session,&navigation,&flags);if(scene_terminal(rc))return;if(rc!=0)break;
  if(resident.enabled){uint32_t work=broadcast_result==3?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0;int32_t rr=risc_scene_resident_poll_work_v1(&resident,flags,work);if(!context_alive())return;if(rr==RISC_RESIDENT_RETAINED){retain();return;}if(rr==RISC_RESIDENT_EXIT)break;if(resident.policy&&!work&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))){if(handoff(RISC_RESIDENT_CHECKPOINT_POLICY))break;continue;}}
  if(!(flags&RISC_SCENE_PRESENTING)){if(broadcast_ready){int r=battery_broadcast_step(&broadcast,broadcast_result!=3);if(!context_alive())return;if(r==4){retain();return;}if(broadcast_result==3&&r!=3){broadcast_result=r;if(r==0)(void)refresh_status();if(!context_alive())return;save_failed=r!=0;snprintf(notice,sizeof(notice),"%s",save_failed?"NOT SAVED - RETRY SAME REQUEST":"");explicit_refresh=true;}}if(++ticks>=50){ticks=0;if(!refresh_status())return;}}
  if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(lifecycle||explicit_refresh)){if(!present_model())break;explicit_refresh=false;}
  runtime->yield_ms(20);if(!context_alive())return;
 }
 if(pause_broadcast()&&close_scene())close_dependencies();
}
