#include "utility_paper.h"
#include "PaperBattery.h"
static void battery_paper_draw(void) {
 char value[48];
#ifdef PORTABLE_BLE_BROADCAST
 if(power_broadcast){
  up_begin("BLE BROADCAST");up_text(32,150,416,power_broadcast_label(),true,true);
  up_text(32,230,416,"Battery percent, voltage and charging.",false,true);
  up_text(32,270,416,"Temperature is not available.",false,true);
  up_text(32,350,416,"Open to nearby receivers without pairing.",false,true);
  up_text(32,390,416,"Off by default. Your choice is saved.",false,true);
  if(power_broadcast_state.state==TELEMETRY_BROADCAST_BLE_OFF)up_text(32,452,416,"Enable Bluetooth in Quick Controls.",false,true);
  if(power_broadcast_error)up_text(32,520,416,"Not saved. Try again.",false,true);
  up_button(32,584,416,88,power_broadcast_error?"Retry":power_broadcast_state.enabled?"Disable":"Enable","solid:f293",false);
  up_button(32,688,200,88,"Back","solid:f053",false);
  up_button(248,688,200,88,"Refresh","solid:f021",false);
  power_app->present(false);return;
 }
#endif
 up_begin(power_details?"POWER DETAILS":"BATTERY");
 paper_battery b={.valid=power_percent_known(),.charging=power_read_ok&&(power_sample.flags&RISC_BATTERY_CHARGING),.percent=power_sample.percent};
 paper_battery_draw(power_app,utility_paper,332,120,b);
 if(!power_details){
  if(b.valid)snprintf(value,sizeof(value),"%u%%",b.percent);else strcpy(value,"--%");
  up_center(220,value,true);up_text(32,336,416,power_title(),true,true);up_wrap(400,power_caption());
#ifdef PORTABLE_BLE_BROADCAST
  up_button(32,584,200,88,"Details","solid:f013",false);
  up_button(248,584,200,88,"Broadcast","solid:f293",false);
#else
  up_button(32,584,416,88,"Details","solid:f013",false);
#endif
 }
#ifdef PORTABLE_BLE_BROADCAST
 else if(!power_extended()) {
  if(power_read_ok)snprintf(value,sizeof(value),"%u mV",power_sample.millivolts);else strcpy(value,"Unavailable");
  up_text(32,200,416,"Battery voltage",true,true);up_text(32,248,416,value,false,true);
  up_text(32,340,416,"Charging",true,true);up_text(32,388,416,!power_read_ok?"Unavailable":b.charging?"Yes":"No",false,true);
  up_wrap(480,"Basic gauge telemetry only. No temperature or charger-controller details.");
 }
#endif
 else {
  if(power_detail_page==0){
   if(power_read_ok)snprintf(value,sizeof(value),"%u mV",power_sample.millivolts);else strcpy(value,"Unavailable");
   up_text(32,200,416,"Battery voltage",true,true);up_text(32,248,416,value,false,true);
   up_text(32,340,416,"USB available",true,true);up_text(32,388,416,power_yes_no(PORTABLE_POWER_INPUT_READY),false,true);
  }else if(power_detail_page<3){
   bool charger=power_detail_page==1;
   up_text(32,200,416,charger?"Charger enabled":"Battery present",true,true);
   up_text(32,248,416,power_yes_no(charger?PORTABLE_POWER_CHARGER_ENABLED:PORTABLE_POWER_BATTERY_PRESENT),false,true);
   up_text(32,340,416,charger?"Charge complete":"PMIC thermal limit",true,true);
   up_text(32,388,416,power_yes_no(charger?PORTABLE_POWER_CHARGE_DONE:PORTABLE_POWER_THERMAL_LIMIT),false,true);
  }else{
   up_text(32,200,416,"Charging is autonomous",true,true);
   up_wrap(260,"Sleep does not stop the PMIC charger. USB and protection conditions still apply.");
   up_wrap(400,"No cell temperature or time-to-full measurement.");
  }
  snprintf(value,sizeof(value),"%u / 4%s",power_detail_page+1,power_read_ok?"":" - Read error");up_text(32,520,416,value,false,true);
  up_button(32,584,200,88,"Previous","solid:f053",false);up_button(248,584,200,88,"Next","solid:f054",false);
 }
 if(power_navigation_failed)up_text(32,552,416,"Back failed. Try again.",false,true);
 up_button(32,688,200,88,"Back","solid:f053",false);up_button(248,688,200,88,power_read_ok?"Refresh":"Retry","solid:f021",false);
 power_app->present(false);
}
static void battery_paper_tap(int x,int y,bool *refresh){
 if(up_hit(x,y,248,688,200,88)){*refresh=true;return;}
#ifdef PORTABLE_BLE_BROADCAST
 if(power_broadcast)return;
 if(!power_details&&up_hit(x,y,248,584,200,88)){power_broadcast=true;return;}
#endif
 if(!power_details&&up_hit(x,y,32,584,416,88)){power_details=true;power_detail_page=0;}
 else if(power_details&&up_hit(x,y,32,584,200,88))power_detail_page=(power_detail_page+3)%4;
 else if(power_details&&up_hit(x,y,248,584,200,88))power_detail_page=(power_detail_page+1)%4;
}
