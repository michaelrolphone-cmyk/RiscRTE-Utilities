#include "utility_paper.h"
static void countdown_paper_draw(void) {
 up_begin("COUNTDOWN");char value[40];unsigned shown[3]={values[0],values[1],values[2]};
 if(!alarm_page) {
  if(!editing&&service_valid&&writer.saved.enabled&&service_state.schedules[1].revision==writer.saved.revision&&service_state.schedules[1].state==ALARM_SCHEDULE_ARMED) {
   uint32_t left=writer.saved.deadline>service_state.rtc_seconds?writer.saved.deadline-service_state.rtc_seconds:0;
   shown[0]=left/3600;shown[1]=left/60%60;shown[2]=left%60;
  }
  alarm_value_text(value,sizeof(value),shown);up_text(32,130,416,"Duration  HH:MM:SS",false,true);up_center(204,value,true);
  up_button(32,328,416,88,"Edit duration","solid:f013",false);
  up_wrap(440,(!storage
#ifndef ALARM_NATIVE_UTC
||!rtc
#endif
||!service)?notice:alarm_state_text());
  up_button(32,584,200,88,service_valid&&service_state.occurrence.generation?"Dismiss":writer.uncertain?"Retry save":"Start","solid:f017",false);
  up_button(248,584,200,88,writer.uncertain?"Wait":"Cancel","solid:f0c8",false);
  up_button(32,688,200,88,"Back","solid:f053",false);up_button(248,688,200,88,"Refresh","solid:f021",false);
 } else if(alarm_page==1) {
  for(unsigned i=0;i<3;i++){snprintf(value,sizeof(value),"%s: %02u",alarm_fields[i],values[i]);up_button(32,168+(int)i*112,416,88,value,NULL,false);}
  up_wrap(540,"Draft only. Back, then Start.");up_button(32,688,416,88,"Back","solid:f053",false);
 } else {
  up_text(32,136,416,alarm_fields[alarm_field],true,true);snprintf(value,sizeof(value),"%02u",values[alarm_field]);up_center(248,value,true);
  up_button(32,408,200,88,"-",NULL,false);up_button(248,408,200,88,"+",NULL,false);
  up_button(32,584,416,88,"Done","solid:f054",false);up_button(32,688,416,88,"Back","solid:f053",false);
 }
 app->present(false);
}
static bool countdown_paper_tap(int x,int y) {
 bool alert=service_valid&&service_state.occurrence.generation;
 if(!alarm_page) {
  if(up_hit(x,y,32,584,200,88)){if(alert)service->acknowledge(service->context,&service_state.occurrence);else save_action(false);}
  else if(up_hit(x,y,248,584,200,88)){if(!alert)save_action(true);}
  else if(up_hit(x,y,248,688,200,88)){if(!alert)alarm_retry();}
  else if(up_hit(x,y,32,328,416,88)&&!alert&&!writer.uncertain){alarm_page=1;editing=true;notice="Draft only - not saved";}
  else return false;
 } else if(alarm_page==1) {
  for(unsigned i=0;i<3;i++)if(up_hit(x,y,32,168+(int)i*112,416,88)){alarm_field=i;alarm_page=2;return true;}
  return false;
 } else {
  if(up_hit(x,y,32,408,200,88)||up_hit(x,y,248,408,200,88)){unsigned max=alarm_field?59:99;values[alarm_field]=(values[alarm_field]+(x>=up_x(248)?1:max))%(max+1);}
  else if(up_hit(x,y,32,584,416,88))alarm_page=1;
  else return false;
 }
 return true;
}
