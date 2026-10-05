#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "PortableBluetoothHost.h"
#include "PortableRadioSession.h"
#include "PortableRadioPolicy.h"
#include "PortableNovaUi.h"
#include "PortableAppSleep.h"
#include "ble_scan_core.h"
#include <stdio.h>
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const portable_bluetooth_host_v1 *host;
static risc_runtime_capability_v1 grant;
static uint64_t token;
static ble_scan scan;
static bool acquired,uncertain,dirty,detail,sensors,restore_failed;
static unsigned selected,scroll,detail_scroll;
static const char *message;
static bool active(void){return scan.phase==BLE_STARTING||scan.phase==BLE_SCANNING;}
static void retain(void){runtime->diagnostic("BLE cleanup-unconfirmed; invocation retained");for(;;)runtime->yield_ms(50);}
bool portable_radio_services_safe(void){return !uncertain;}
bool portable_radio_suspend(void){
 if(uncertain)return false;
 if(token){int32_t rc=host->release(host->controls.context,token);if(rc<0){uncertain=true;return false;}token=0;if(rc==0){restore_failed=true;message="Bluetooth restore failed";}}
 if(acquired){if(!runtime->release(&grant)){uncertain=true;return false;}acquired=false;grant=(risc_runtime_capability_v1){0};host=NULL;}
 if(active()){scan.phase=BLE_COMPLETE;if(!restore_failed)message="Paused - tap Scan";}
 dirty=true;return true;
}
static void stop(void){if(!portable_radio_suspend())retain();}
static bool policy_allowed(void){
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};uint8_t flags=0;
 if(!runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g)){message="Radio settings unavailable";return false;}
 bool ok=portable_radio_load(g.api,&flags);
 if(!runtime->release(&g)){uncertain=true;retain();}
 if(!ok){message="Radio settings unreadable";return false;}
 if(flags&PORTABLE_RADIO_AIRPLANE){message="Turn off Airplane mode";return false;}
 if(!(flags&PORTABLE_RADIO_BLUETOOTH)){message="Enable Bluetooth in controls";return false;}
 return true;
}
static void start_scan(void){
 if(active()){stop();if(!restore_failed)message="Stopped";return;}
 if(!policy_allowed()){dirty=true;return;}
 restore_failed=false;
 grant=(risc_runtime_capability_v1){.struct_size=sizeof(grant)};
 if(!runtime->acquire("bluetooth.hci",1,16,&grant)){message="Bluetooth unavailable";dirty=true;return;}
 acquired=true;host=grant.api;
 if(!host||host->controls.api_version!=1||host->controls.struct_size<sizeof(*host)||!host->claim||!host->release||!host->send_owned||!host->next_owned){message="Scanner driver update needed";stop();return;}
 if(!host->claim(host->controls.context,&token)||!token){message="Bluetooth busy - retry";stop();return;}
 ble_begin(&scan);selected=scroll=detail_scroll=0;detail=false;message="Starting scan";dirty=true;
}
static void pump(void){
 if(!active())return;
 uint32_t now=app->millis();uint8_t command[16];size_t size=ble_command(&scan,command,now);
 if(size&&!host->send_owned(host->controls.context,token,1,command,size))ble_fail(&scan,"Send failed - retry");
 for(unsigned i=0;i<6&&active();i++){
  uint8_t packet[1028],type=0;size_t count=0;
  int32_t rc=host->next_owned(host->controls.context,token,&type,packet,sizeof(packet),&count);
  if(rc<0){ble_fail(&scan,"Receive failed - retry");break;}if(!rc)break;
  ble_event(&scan,type,packet,count,now);dirty=true;
 }
 if(ble_expired(&scan,now)){
  const char *result=scan.phase==BLE_ERROR?scan.error:scan.dropped?"32 saved - result list full":"Scan complete";
  stop();if(!restore_failed)message=result;dirty=true;
 }else if(scan.phase==BLE_SCANNING)message=scan.dropped?"32 saved - more devices seen":"Passive scan - 15 seconds";
}
static bool visible(unsigned i){return !sensors||scan.devices[i].service_count||scan.devices[i].has_company;}
static unsigned indexes(unsigned out[BLE_MAX_DEVICES]){unsigned n=0;for(unsigned i=0;i<scan.count;i++)if(visible(i))out[n++]=i;return n;}
static void address(const ble_device*d,char out[32]){snprintf(out,32,"%02X:%02X:%02X:%02X:%02X:%02X",d->address[5],d->address[4],d->address[3],d->address[2],d->address[1],d->address[0]);}
static void move(int delta){
 if(detail){int n=(int)detail_scroll+delta;detail_scroll=(unsigned)(n<0?0:n>32?32:n);dirty=true;return;}
 unsigned list[BLE_MAX_DEVICES],n=indexes(list);if(!n)return;
 int next=(int)selected+delta;selected=(unsigned)(next<0?0:next>=(int)n?(int)n-1:next);
 if(selected<scroll)scroll=selected;
 if(selected>=scroll+3)scroll=selected-2;
 dirty=true;
}
static void line(unsigned index,const char *s){
 if(index>=detail_scroll&&index<detail_scroll+6)portable_nova_text(1,12,61+(int)(index-detail_scroll)*21,216,s,NOVA_TEXT);
}
static void scrollbar(unsigned first,unsigned visible_rows,unsigned total,int y,int height){
 if(total<=visible_rows)return;
 int size=height*(int)visible_rows/(int)total;if(size<8)size=8;
 int top=(height-size)*(int)first/(int)(total-visible_rows);
 portable_nova_fill(236,y,2,height,NOVA_LINE);portable_nova_fill(236,y+top,2,size,NOVA_CYAN);
}
static void draw(void){
 portable_nova_begin();portable_nova_header(detail?"SENSOR DETAILS":"BLE SCANNER");
 char text[80];unsigned list[BLE_MAX_DEVICES],n=indexes(list);
 if(detail&&selected<n){
  ble_device*d=&scan.devices[list[selected]];unsigned row=0;
  line(row++,d->name[0]?d->name:"Unnamed device");address(d,text);line(row++,text);
  snprintf(text,sizeof(text),"%s address",(d->address_type&1)?"Random":"Public");line(row++,text);
  if(d->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");else snprintf(text,sizeof(text),"Signal %d dBm",d->rssi);line(row++,text);
  snprintf(text,sizeof(text),"Seen %lu s ago / %lu reports",(unsigned long)((app->millis()-d->seen)/1000),(unsigned long)d->reports);line(row++,text);
  for(unsigned i=0;i<d->service_count;i++){snprintf(text,sizeof(text),"Service 0x%04X",d->services[i]);line(row++,text);}
  if(d->has_company){snprintf(text,sizeof(text),"Manufacturer 0x%04X",d->company);line(row++,text);}
  if(d->bthome){snprintf(text,sizeof(text),"Sensor sample %lu s ago",(unsigned long)((app->millis()-d->measurement_seen)/1000));line(row++,text);}
  if(d->bthome)line(row++,d->bthome_version!=2?"BTHome version unsupported":d->encrypted?"BTHome: encrypted":"BTHome v2 advertised data");
  if(d->has_temperature){int v=d->temperature;snprintf(text,sizeof(text),"Temperature %s%d.%02d C",v<0?"-":"",v<0?-v/100:v/100,v<0?-v%100:v%100);line(row++,text);}
  if(d->has_humidity){snprintf(text,sizeof(text),"Humidity %u.%02u %%",d->humidity/100,d->humidity%100);line(row++,text);}
  if(d->has_battery){snprintf(text,sizeof(text),"Battery %u %%",d->battery);line(row++,text);}
  line(row++,"Raw advertising bytes:");
  for(unsigned i=0;i<d->payload_size;i+=8){unsigned at=0;for(unsigned j=i;j<d->payload_size&&j<i+8;j++)at+=(unsigned)snprintf(text+at,sizeof(text)-at,"%02X ",d->payload[j]);line(row++,text);}
  if(detail_scroll && detail_scroll+6>row){detail_scroll=row>6?row-6:0;dirty=true;}else dirty=false;
  scrollbar(detail_scroll,6,row,61,126);
  portable_nova_button(8,196,108,44,"Results",false);portable_nova_button(124,196,108,44,active()?"Stop":"Scan",false);
 }else{
  portable_nova_text(2,8,48,224,message?message:"Tap Scan to discover",NOVA_CAP);
  for(unsigned row=0;row<3&&row+scroll<n;row++){
   unsigned idx=row+scroll;ble_device*d=&scan.devices[list[idx]];char name[32];address(d,name);
   portable_nova_round(8,64+(int)row*44,224,42,5,idx==selected?NOVA_DIM:NOVA_LINE);
   portable_nova_text(1,14,67+(int)row*44,210,d->name[0]?d->name:name,NOVA_TEXT);
   if(d->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");
   else snprintf(text,sizeof(text),"%s  %d dBm",d->bthome?"BTHome":d->service_count?"Services":"BLE",d->rssi);
   portable_nova_text(2,14,87+(int)row*44,210,text,NOVA_CAP);
  }
  if(!n){portable_nova_center(1,8,96,224,active()?"Listening...":"No devices in results",NOVA_TEXT);portable_nova_center(2,8,123,224,"Swipe down for radio controls",NOVA_CAP);}
  scrollbar(scroll,3,n,64,132);
  portable_nova_button(8,196,108,44,active()?"Stop":"Scan",false);portable_nova_button(124,196,108,44,sensors?"Services":"All devices",sensors);dirty=false;
 }
 app->present(false);
}
void app_main(void){
 app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);
 if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,take_touch_swipe)+sizeof(app->take_touch_swipe)||!app->poll||!app->millis||!app->present||!app->screen_width||!app->screen_height||!app->set_back_exits_app||!app->take_touch_swipe||app->screen_width()!=240||app->screen_height()!=240||!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms||!runtime->diagnostic)return;
 scan=(ble_scan){0};grant=(risc_runtime_capability_v1){0};host=NULL;token=0;acquired=uncertain=detail=sensors=restore_failed=false;selected=scroll=detail_scroll=0;dirty=true;message="Tap Scan to discover";
 app->set_back_exits_app(true);uint32_t rendered=0;
 for(;;){
  if((dirty&&(!active()||(uint32_t)(app->millis()-rendered)>=100))||(active()&&(uint32_t)(app->millis()-rendered)>=250)){draw();rendered=app->millis();}
  app->set_back_exits_app(!detail);
  t5_app_input_t input={0};if(!app->poll(&input,20)){if(portable_app_sleep_retained())return;break;}
  if(input.exit_requested)break;
  if(input.buttons&T5_APP_BUTTON_BACK){if(detail){detail=false;dirty=true;continue;}break;}
  t5_app_swipe_t swipe={0};bool swiped=app->take_touch_swipe(&swipe);
  if(swiped){int dy=swipe.end_y-swipe.start_y;if(dy>20)move(-1);else if(dy<-20)move(1);}
  if(input.buttons&T5_APP_BUTTON_UP)move(-1);
  if(input.buttons&T5_APP_BUTTON_DOWN)move(1);
  bool action=false;
  if(input.buttons&T5_APP_BUTTON_CONFIRM){if(detail)action=true;else{unsigned list[BLE_MAX_DEVICES];if(indexes(list)){detail=true;detail_scroll=0;dirty=true;}else action=true;}}
  if(input.tapped&&!swiped){int x=input.touch_x,y=input.touch_y;
   if(portable_nova_hit(x,y,8,196,108,44)){if(detail){detail=false;dirty=true;}else action=true;}
   else if(portable_nova_hit(x,y,124,196,108,44)){if(detail)action=true;else{sensors=!sensors;scroll=selected=0;dirty=true;}}
   else if(!detail&&portable_nova_hit(x,y,8,64,224,132)){unsigned list[BLE_MAX_DEVICES],n=indexes(list),i=scroll+(unsigned)(y-64)/44;if(i<n){selected=i;detail=true;detail_scroll=0;dirty=true;}}
  }
  if(action)start_scan();
  pump();
 }
 stop();app->set_back_exits_app(true);
}
