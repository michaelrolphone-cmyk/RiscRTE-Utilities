#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "RiscBluetoothSensorsV1.h"
#include "PortableWatchKeyboard.h"
#include "ble_sensor_names.h"
#include "PortableRadioSession.h"
#include "PortableRadioPolicy.h"
#include "PortableNovaUi.h"
#include "PortableAppSleep.h"
#include "ble_scan_model.h"
#include "PaperPresentation.h"
#include "PortableBluetoothControl.h"
__attribute__((weak)) const paper_presentation *paper_presentation_get(void){return NULL;}
static const paper_presentation *paper;
static bool paper_enable_needed;
#include <stdio.h>
#ifndef PORTABLE_APP_OWNS_TOUCH_CHROME
#error "BLE Scanner requires application-owned touch chrome"
#endif
#ifndef PORTABLE_RETURN_APP
#error "BLE Scanner requires an explicit launcher return destination"
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_bluetooth_sensors_v1 *host;
static risc_runtime_capability_v1 grant;
static uint64_t token;
static ble_scan scan;
static bool acquired,uncertain,dirty,detail,sensors,restore_failed;
static unsigned selected,scroll,detail_scroll;
static bool contact_down,contact_list,contact_moved;
static int contact_y;
static const char *message;
static unsigned detail_index,key_page,key_choice;
static uint32_t copied_at;
static bool naming,name_save_failed;
static char aliases[BLE_MAX_DEVICES][BLE_ALIAS_MAX+1],editing[BLE_ALIAS_MAX+1];
static int8_t alias_state[BLE_MAX_DEVICES]; /* 0 unloaded, 1 valid, -1 failed */
static unsigned indexes(unsigned out[BLE_MAX_DEVICES]);
static unsigned visible_rows(void){return paper?(unsigned)(app->screen_height()-256)/88u:3u;}
static void load_alias(unsigned i){
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g)){alias_state[i]=-1;return;}
 ble_device*d=&scan.devices[i];int rc=ble_alias_load(g.api,d->address_type,d->address,aliases[i]);
 if(!runtime->release(&g)){uncertain=true;runtime->diagnostic("BLE name grant retained");for(;;)runtime->yield_ms(50);}
 alias_state[i]=rc<0?-1:1;
}
static bool active(void){return scan.phase==BLE_STARTING||scan.phase==BLE_SCANNING;}
static void retain(void){runtime->diagnostic("BLE cleanup-unconfirmed; invocation retained");for(;;)runtime->yield_ms(50);}
bool portable_radio_services_safe(void){return !uncertain;}
bool portable_radio_suspend(void){
 if(uncertain)return false;
 if(token){if(!host->close(host->context,token)){uncertain=true;return false;}token=0;risc_ble_sensor_status_v1 state={.struct_size=sizeof(state)};if(host->status(host->context,&state)&&state.restore_failed){restore_failed=true;message="Bluetooth restore failed";}}
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
 if(!(flags&PORTABLE_RADIO_BLUETOOTH)){paper_enable_needed=paper!=NULL;message=paper?"Tap Enable Bluetooth":"Enable Bluetooth in controls";return false;}
 paper_enable_needed=false;
 return true;
}
static void start_scan(void){
 if(active()){stop();if(!restore_failed)message="Stopped";return;}
 if(!policy_allowed()){dirty=true;return;}
 restore_failed=false;
 grant=(risc_runtime_capability_v1){.struct_size=sizeof(grant)};
 if(!runtime->acquire(RISC_BLUETOOTH_SENSORS_CAPABILITY,1,0,&grant)){message="Bluetooth unavailable";dirty=true;return;}
 acquired=true;host=grant.api;
 if(!host||host->api_version!=1||host->struct_size<sizeof(*host)||!host->open||!host->close||!host->poll||!host->status||!host->device){message="Scanner driver update needed";stop();return;}
 if(!host->open(host->context,&token)||!token){message="Bluetooth busy - retry";stop();return;}
 scan=(ble_scan){.phase=BLE_STARTING};memset(aliases,0,sizeof(aliases));memset(alias_state,0,sizeof(alias_state));selected=scroll=detail_scroll=0;detail=naming=false;message="Starting scan";dirty=true;
}
static bool navigate_back(void){
 if(naming){naming=false;dirty=true;return true;}
 if(detail){detail=false;dirty=true;return true;}
 stop();
 if(!runtime->request_launch||!runtime->request_launch(PORTABLE_RETURN_APP)){message="Return failed - retry";dirty=true;return true;}
 return false;
}
static void pump(void){
 if(!active())return;
 unsigned before[BLE_MAX_DEVICES],old_count=indexes(before),old_index=selected<old_count?before[selected]:BLE_MAX_DEVICES;
 bool ok=host->poll(host->context,token,6);risc_ble_sensor_status_v1 state={.struct_size=sizeof(state)};
 if(!host->status(host->context,&state)||state.count>BLE_MAX_DEVICES){scan.phase=BLE_ERROR;strcpy(scan.error,"Sensor state unavailable");}
 else{
  copied_at=app->millis();scan.phase=state.state;scan.count=state.count;scan.dropped=state.dropped;scan.malformed=state.malformed;
  memcpy(scan.error,state.error,sizeof(scan.error));scan.error[sizeof(scan.error)-1]=0;
  for(unsigned i=0;i<scan.count;i++){
   if(!host->device(host->context,i,&scan.devices[i])){scan.phase=BLE_ERROR;strcpy(scan.error,"Sensor result unavailable");break;}
   if(scan.devices[i].reading_count>RISC_BLE_MAX_READINGS||scan.devices[i].service_count>8||scan.devices[i].payload_size>31||scan.devices[i].address_type>3){scan.phase=BLE_ERROR;strcpy(scan.error,"Invalid sensor result");scan.count=i;break;}
   scan.devices[i].name[sizeof(scan.devices[i].name)-1]=0;
   if(!alias_state[i])load_alias(i);
  }
  if(!ok&&scan.phase!=BLE_ERROR){scan.phase=BLE_ERROR;strcpy(scan.error,"Sensor scan failed");}
 }
 unsigned after[BLE_MAX_DEVICES],count=indexes(after);for(unsigned i=0;i<count;i++)if(after[i]==old_index){selected=i;break;}
 if(selected<scroll)scroll=selected;
 if(selected>=scroll+visible_rows())scroll=selected-visible_rows()+1;
 if(scan.phase==BLE_ERROR||scan.phase==BLE_COMPLETE){
  const char*result=scan.phase==BLE_ERROR?scan.error:scan.dropped?"32 saved - result list full":"Scan complete";
  stop();if(!restore_failed)message=result;
 }else if(scan.phase==BLE_SCANNING)message=scan.dropped?"32 saved - more devices seen":"Passive scan - 15 seconds";
 dirty=true;
}
static bool visible(unsigned i){return !sensors||scan.devices[i].bthome;}
static unsigned indexes(unsigned out[BLE_MAX_DEVICES]){
 unsigned n=0;for(unsigned named=1;;named--){for(unsigned i=0;i<scan.count;i++)if(visible(i)&&((aliases[i][0]!=0)==(named!=0)))out[n++]=i;if(!named)break;}return n;
}
static void begin_name(void){
 if(detail_index>=scan.count)return;
 stop();name_save_failed=false;memcpy(editing,aliases[detail_index],sizeof(editing));key_page=PWK_INITIAL_PAGE;key_choice=0;naming=true;dirty=true;
}
static void save_name(void){
 unsigned start=0,end=(unsigned)strlen(editing);while(start<end&&editing[start]==' ')start++;while(end>start&&editing[end-1]==' ')end--;
 for(unsigned i=0;i<end-start;i++)editing[i]=editing[start+i];
 editing[end-start]=0;
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};bool ok=false;
 if(runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g)){
  ble_device*d=&scan.devices[detail_index];ok=ble_alias_save(g.api,d->address_type,d->address,editing);
  if(!runtime->release(&g)){uncertain=true;retain();}
 }
 if(!ok){message="Name save unconfirmed";name_save_failed=true;dirty=true;return;}
 memcpy(aliases[detail_index],editing,sizeof(editing));alias_state[detail_index]=1;naming=false;message=editing[0]?"Sensor name saved":"Sensor name cleared";
 unsigned list[BLE_MAX_DEVICES],n=indexes(list);for(unsigned i=0;i<n;i++)if(list[i]==detail_index){selected=i;break;}
 scroll=selected>=visible_rows()?selected-visible_rows()+1:0;dirty=true;
}
static void name_key(unsigned key){
 if(key>=PWK_COUNT)return;
 size_t n=strlen(editing);
 if(key<PWK_CHARACTERS){unsigned ch=portable_watch_key_character(key_page,key);if(ch&&n<BLE_ALIAS_MAX){editing[n]=(char)ch;editing[n+1]=0;}}
 else if(key==PWK_PAGE)key_page=(key_page+1)%PWK_PAGES;
 else if(key==PWK_DELETE){if(n)editing[n-1]=0;}
 else if(key==PWK_DONE)save_name();
 dirty=true;
}
static void address(const ble_device*d,char out[32]){snprintf(out,32,"%02X:%02X:%02X:%02X:%02X:%02X",d->address[5],d->address[4],d->address[3],d->address[2],d->address[1],d->address[0]);}
static void move(int delta){
 if(detail){int n=(int)detail_scroll+delta;detail_scroll=(unsigned)(n<0?0:n>32?32:n);dirty=true;return;}
 unsigned list[BLE_MAX_DEVICES],n=indexes(list);if(!n)return;
 int next=(int)selected+delta;selected=(unsigned)(next<0?0:next>=(int)n?(int)n-1:next);
 if(selected<scroll)scroll=selected;
 unsigned rows=visible_rows();
 if(selected>=scroll+rows)scroll=selected-rows+1;
 dirty=true;
}
static bool drag(void){
 t5_app_contact_t sample={0};if(!app->touch_contact(&sample))return false;
 if(sample.down){
  if(!contact_down){contact_down=true;contact_list=sample.y>=60&&sample.y<196;contact_moved=false;contact_y=sample.y;}
  else if(contact_list){int dy=sample.y-contact_y;if(dy>=20||dy<=-20){move(dy<0?1:-1);contact_y=sample.y;contact_moved=true;}}
  return contact_list&&contact_moved;
 }
 bool consumed=contact_down&&contact_list&&contact_moved;contact_down=contact_list=contact_moved=false;return consumed;
}
static uint32_t age_seconds(uint32_t age){uint32_t elapsed=app->millis()-copied_at;return (elapsed>UINT32_MAX-age?UINT32_MAX:age+elapsed)/1000;}
static void line(unsigned index,const char *s){
 if(index>=detail_scroll&&index<detail_scroll+6)portable_nova_text(1,12,61+(int)(index-detail_scroll)*21,216,s,NOVA_TEXT);
}
static void scrollbar(unsigned first,unsigned visible_rows,unsigned total,int y,int height){
 if(total<=visible_rows)return;
 int size=height*(int)visible_rows/(int)total;if(size<8)size=8;
 int top=(height-size)*(int)first/(int)(total-visible_rows);
 portable_nova_fill(236,y,2,height,NOVA_LINE);portable_nova_fill(236,y+top,2,size,NOVA_CYAN);
}
static unsigned detail_lines(void (*emit)(unsigned,const char*)){
 char text[96];
  ble_device*d=&scan.devices[detail_index];unsigned row=0;
  emit(row++,aliases[detail_index][0]?aliases[detail_index]:d->name[0]?d->name:"Unnamed device");
  if(alias_state[detail_index]<0)emit(row++,"Saved name unavailable");
  address(d,text);emit(row++,text);
  snprintf(text,sizeof(text),"%s address",(d->address_type&1)?"Random":"Public");emit(row++,text);
  if(d->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");else snprintf(text,sizeof(text),"Signal %d dBm",d->rssi);emit(row++,text);
  snprintf(text,sizeof(text),"Seen %lu s ago / %lu reports",(unsigned long)(age_seconds(d->seen_age_ms)),(unsigned long)d->reports);emit(row++,text);
  for(unsigned i=0;i<d->service_count;i++){snprintf(text,sizeof(text),"Service 0x%04X",d->services[i]);emit(row++,text);}
  if(d->has_company){snprintf(text,sizeof(text),"Manufacturer 0x%04X",d->company);emit(row++,text);}
  if(d->bthome){snprintf(text,sizeof(text),"Sensor sample %lu s ago",(unsigned long)(age_seconds(d->measurement_age_ms)));emit(row++,text);}
  if(d->bthome)emit(row++,d->bthome_version!=2?"BTHome version unsupported":d->encrypted?"BTHome: encrypted":d->measurement_invalid?"Invalid sensor sample":"BTHome v2 open readings");
  if(d->measurement_partial)emit(row++,"Some readings unsupported");
  for(unsigned i=0;i<d->reading_count;i++){
   risc_ble_reading_v1*r=&d->readings[i];
   if(r->metric==RISC_TELEMETRY_BATTERY_PERCENT)snprintf(text,sizeof(text),"Battery %ld %%",(long)r->value);
   else if(r->metric==RISC_TELEMETRY_TEMPERATURE_CENTIC){int v=r->value;snprintf(text,sizeof(text),"Temperature %s%d.%02d C",v<0?"-":"",v<0?-v/100:v/100,v<0?-v%100:v%100);}
   else if(r->metric==RISC_TELEMETRY_HUMIDITY_CENTIPERCENT)snprintf(text,sizeof(text),"Humidity %ld.%02ld %%",(long)(r->value/100),(long)(r->value%100));
   else if(r->metric==RISC_TELEMETRY_PRESSURE_CENTIHPA)snprintf(text,sizeof(text),"Pressure %ld.%02ld hPa",(long)(r->value/100),(long)(r->value%100));
   else if(r->metric==RISC_TELEMETRY_ILLUMINANCE_CENTILUX)snprintf(text,sizeof(text),"Light %ld.%02ld lx",(long)(r->value/100),(long)(r->value%100));
   else if(r->metric==RISC_TELEMETRY_VOLTAGE_MV)snprintf(text,sizeof(text),"Voltage %ld mV",(long)r->value);
   else if(r->metric==RISC_TELEMETRY_CHARGING)snprintf(text,sizeof(text),"Charging: %s",r->value?"Yes":"No");else continue;
   emit(row++,text);
  }
  emit(row++,"Readings are unverified broadcasts");
  if(d->address_type&1)emit(row++,"Name follows this address only");
  emit(row++,"Raw advertising bytes:");
  for(unsigned i=0;i<d->payload_size;i+=8){unsigned at=0;for(unsigned j=i;j<d->payload_size&&j<i+8;j++)at+=(unsigned)snprintf(text+at,sizeof(text)-at,"%02X ",d->payload[j]);emit(row++,text);}
 return row;
}
static void paper_draw(void);
static void draw(void){
 if(paper){paper_draw();return;}
 portable_nova_begin();portable_nova_header(naming?"SENSOR NAME":detail?"SENSOR DETAILS":"BLE SCANNER");
 if(naming){
  portable_nova_text(1,12,49,216,editing[0]?editing:"Type a name",NOVA_CYAN);
  for(unsigned key=0;key<PWK_COUNT;key++){
   portable_watch_key_rect r;(void)portable_watch_key_bounds(key,&r);
   portable_nova_fill(r.x,r.y,r.w,r.h,key_choice==key?NOVA_DIM:NOVA_LINE);
   unsigned ch=portable_watch_key_character(key_page,key);char label[2]={(char)(ch?ch:' '),0};
   const char*caption=key==PWK_PAGE?"ABC/#":key==PWK_DELETE?"DELETE":key==PWK_DONE?"SAVE":ch==32?"_":label;
   portable_nova_text(2,r.x+2,r.y+3,r.w-4,caption,NOVA_CYAN);
  }
  portable_nova_text(2,12,214,216,name_save_failed?message:"Empty name clears it; Back cancels",NOVA_CAP);
  dirty=false;app->present(false);return;
 }
 char text[80];unsigned list[BLE_MAX_DEVICES],n=indexes(list);
 if(detail&&detail_index<scan.count){
  unsigned row=detail_lines(line);
  if(detail_scroll && detail_scroll+6>row){detail_scroll=row>6?row-6:0;dirty=true;}else dirty=false;
  scrollbar(detail_scroll,6,row,61,126);
  portable_nova_button(8,196,108,44,"Results",false);portable_nova_button(124,196,108,44,"Name",false);
 }else{
  portable_nova_text(2,8,48,224,message?message:"Tap Scan to discover",NOVA_CAP);
  for(unsigned row=0;row<3&&row+scroll<n;row++){
   unsigned idx=row+scroll;ble_device*d=&scan.devices[list[idx]];char name[32];address(d,name);
   portable_nova_round(8,64+(int)row*44,224,42,5,idx==selected?NOVA_DIM:NOVA_LINE);
   portable_nova_text(1,14,67+(int)row*44,210,aliases[list[idx]][0]?aliases[list[idx]]:d->name[0]?d->name:name,NOVA_TEXT);
   if(d->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");
   else snprintf(text,sizeof(text),"%s  %d dBm",d->bthome?"BTHome":d->service_count?"Services":"BLE",d->rssi);
   portable_nova_text(2,14,87+(int)row*44,210,text,NOVA_CAP);
  }
  if(!n){portable_nova_center(1,8,96,224,active()?"Listening...":"No devices in results",NOVA_TEXT);portable_nova_center(2,8,123,224,"Swipe down for radio controls",NOVA_CAP);}
  scrollbar(scroll,3,n,64,132);
  portable_nova_button(8,196,108,44,active()?"Stop":"Scan",false);portable_nova_button(124,196,108,44,sensors?"Sensors":"All devices",sensors);dirty=false;
 }
 app->present(false);
}
#include "ble_scanner_paper.inc"
void app_main(void){
 app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);
 if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,touch_contact)+sizeof(app->touch_contact)||!app->poll||!app->millis||!app->present||!app->screen_width||!app->screen_height||!app->set_back_exits_app||!app->touch_contact||!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms||!runtime->diagnostic)return;
 paper=paper_presentation_get();
 if((!paper&&(app->screen_width()!=240||app->screen_height()!=240))||(paper&&(!app->fill_rect||!app->draw_icon)))return;
 paper_enable_needed=false;bp_down=false;
 scan=(ble_scan){0};grant=(risc_runtime_capability_v1){0};host=NULL;token=0;acquired=uncertain=detail=sensors=restore_failed=naming=false;selected=scroll=detail_scroll=detail_index=0;memset(aliases,0,sizeof(aliases));memset(alias_state,0,sizeof(alias_state));dirty=true;message="Tap Scan to discover";
 contact_down=contact_list=contact_moved=false;contact_y=0;
 app->set_back_exits_app(false);uint32_t rendered=0;
 for(;;){
  if(paper?(dirty&&scan.phase!=BLE_STARTING&&(!active()||(uint32_t)(app->millis()-rendered)>=3000)):
     ((dirty&&(!active()||(uint32_t)(app->millis()-rendered)>=100))||(active()&&(uint32_t)(app->millis()-rendered)>=250))){draw();rendered=app->millis();}
  t5_app_input_t input={0};if(!app->poll(&input,20)){if(portable_app_sleep_retained())return;break;}
  if(input.exit_requested)break;
  if(paper){if(!paper_input(&input))break;pump();continue;}
  if((input.buttons&T5_APP_BUTTON_BACK)||(input.tapped&&portable_nova_hit(input.touch_x,input.touch_y,8,4,44,44))){if(!navigate_back())break;continue;}
  if(naming){
   if(input.buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP))key_choice=key_choice?key_choice-1:PWK_COUNT-1;
   if(input.buttons&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_DOWN))key_choice=(key_choice+1)%PWK_COUNT;
   if(input.buttons&T5_APP_BUTTON_CONFIRM)name_key(key_choice);
   if(input.tapped){int key=portable_watch_key_hit(input.touch_x,input.touch_y);if(key>=0){key_choice=(unsigned)key;name_key(key_choice);}}
   if(input.buttons)dirty=true;
   continue;
  }
  bool swiped=drag();
  if(input.buttons&T5_APP_BUTTON_UP)move(-1);
  if(input.buttons&T5_APP_BUTTON_DOWN)move(1);
  bool action=false;
  if(input.buttons&T5_APP_BUTTON_CONFIRM){if(detail)begin_name();else{unsigned list[BLE_MAX_DEVICES];if(indexes(list)){detail_index=list[selected];detail=true;detail_scroll=0;dirty=true;}else action=true;}}
  if(input.tapped&&!swiped){int x=input.touch_x,y=input.touch_y;
   if(portable_nova_hit(x,y,8,196,108,44)){if(detail){detail=false;dirty=true;}else action=true;}
   else if(portable_nova_hit(x,y,124,196,108,44)){if(detail)begin_name();else{sensors=!sensors;scroll=selected=0;dirty=true;}}
   else if(!detail&&portable_nova_hit(x,y,8,64,224,132)){unsigned list[BLE_MAX_DEVICES],n=indexes(list),i=scroll+(unsigned)(y-64)/44;if(i<n){selected=i;detail_index=list[i];detail=true;detail_scroll=0;dirty=true;}}
  }
  if(action)start_scan();
  pump();
 }
 stop();app->set_back_exits_app(true);
}
