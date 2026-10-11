#include "utility_paper.h"
static bool alarms_paper_sound(void){
#ifdef ALARM_NATIVE_UTC
return false;
#else
return (alarm_service_output_modes(service)&ALARM_MODE_SOUND)!=0;
#endif
}
static void alarms_paper_draw(void){
 char value[64];up_begin("ALARMS");
 if(!alarm_page){
  unsigned shown[3]={values[0],values[1],0};
  strcpy(value,"Once (does not repeat)");
  if(!editing&&!writer.uncertain&&writer.loaded&&writer.saved.enabled){
   twatch_rtc_time_v1 raw,local;
   if(deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)){
    shown[0]=local.hour;shown[1]=local.minute;snprintf(value,sizeof(value),"Once / %04u-%02u-%02u",local.year,local.month,local.day);
   }
  }
  up_text(32,130,416,value,false,true);alarm_value_text(value,sizeof(value),shown);
#ifdef ALARM_NATIVE_UTC
  if(!native_zone_valid&&!editing)snprintf(value,sizeof(value),"--:--");
#endif
  up_center(204,value,true);
  up_button(32,304,416,88,"Edit time","solid:f017",false);
  if(alarms_paper_sound())up_button(32,408,416,88,"Alarm volume","solid:f028",false);
  else up_text(32,432,416,service?"Visual alert only":"Service unavailable",false,true);
#ifdef ALARM_NATIVE_UTC
  up_text(32,472,416,native_zone_valid?native_zone_name:"Time zone unavailable",false,true);
#endif
  up_wrap(510,(!storage
#ifndef ALARM_NATIVE_UTC
||!rtc
#endif
||!service)?"ALARM SERVICE UNAVAILABLE":alarm_state_text());
  up_button(32,584,200,88,service_valid&&service_state.occurrence.generation?"Dismiss":writer.uncertain?"Retry save":"Arm once","solid:f0f3",false);
  up_button(248,584,200,88,writer.uncertain?"Wait":"Cancel","solid:f0c8",false);
 }else if(alarm_page==1){
  for(unsigned i=0;i<2;i++){
   if(!i&&!alarm_24h())snprintf(value,sizeof(value),"Hour: %u %s",values[0]%12?values[0]%12:12,values[0]<12?"AM":"PM");
#ifdef PORTABLE_UNPADDED_HOURS
   else snprintf(value,sizeof(value),i==0?"%s: %u":"%s: %02u",alarm_fields[i],values[i]);
#else
   else snprintf(value,sizeof(value),"%s: %02u",alarm_fields[i],values[i]);
#endif
   up_button(32,168+(int)i*112,416,88,value,NULL,false);
  }
  up_wrap(440,"Draft only. Back, then Arm once.");
 }else if(alarm_page==3){
  up_text(32,136,416,"Alarm volume (%)",true,true);snprintf(value,sizeof(value),"%u",volume_choice);up_center(248,volume_valid?value:"--",true);
  up_button(32,360,200,88,"-",NULL,false);up_button(248,360,200,88,"+",NULL,false);
  up_wrap(470,volume_message&&volume_message[0]?volume_message:"Draft only - use Save");
  up_button(32,584,416,88,volume_uncertain?"Retry save":"Save","solid:f054",false);
 }else{
  up_text(32,136,416,alarm_fields[alarm_field],true,true);
  unsigned shown=values[alarm_field];if(!alarm_field&&!alarm_24h())shown=shown%12?shown%12:12;
#ifdef PORTABLE_UNPADDED_HOURS
  snprintf(value,sizeof(value),alarm_field==0?"%u":"%02u",shown);up_center(248,value,true);
#else
  snprintf(value,sizeof(value),"%02u",shown);up_center(248,value,true);
#endif
  up_button(32,408,200,88,"-",NULL,false);up_button(248,408,200,88,"+",NULL,false);
  if(!alarm_field&&!alarm_24h())up_button(32,520,200,56,values[0]<12?"AM":"PM",NULL,true);
  up_button(32,584,416,88,"Done","solid:f054",false);
 }
 up_button(32,688,alarm_page?416:200,88,"Back","solid:f053",false);
 if(!alarm_page)up_button(248,688,200,88,"Refresh","solid:f021",false);
 app->present(false);
}
static bool alarms_paper_tap(int x,int y){
 bool alert=service_valid&&service_state.occurrence.generation;
 if(!alarm_page){
  if(up_hit(x,y,32,584,200,88)){if(alert)service->acknowledge(service->context,&service_state.occurrence);else save_action(false);}
  else if(up_hit(x,y,248,584,200,88)){if(!alert)save_action(true);}
  else if(up_hit(x,y,248,688,200,88)){if(!alert)alarm_retry();}
  else if(up_hit(x,y,32,304,416,88)&&!alert&&!writer.uncertain){alarm_page=1;editing=true;notice="Draft only - not saved";}
  else if(up_hit(x,y,32,408,416,88)&&!alert&&alarms_paper_sound()){volume_load();alarm_page=3;}
  else return false;
 }else if(alarm_page==1){
  for(unsigned i=0;i<2;i++)if(up_hit(x,y,32,168+(int)i*112,416,88)){alarm_field=i;alarm_page=2;return true;}
  return false;
 }else if(alarm_page==3){
  if(!volume_uncertain&&(up_hit(x,y,32,360,200,88)||up_hit(x,y,248,360,200,88))){volume_choice=x>=up_x(248)?(volume_choice<=90?volume_choice+10:100):(volume_choice>=10?volume_choice-10:0);volume_valid=true;volume_message="Draft only - use Save";}
  else if(up_hit(x,y,32,584,416,88))volume_save();else return false;
 }else{
  if(up_hit(x,y,32,408,200,88)||up_hit(x,y,248,408,200,88)){
   bool plus=x>=up_x(248);unsigned max=alarm_field?59:23;
   if(!alarm_field&&!alarm_24h())values[0]=(values[0]/12)*12+(values[0]%12+(plus?1:11))%12;
   else values[alarm_field]=(values[alarm_field]+(plus?1:max))%(max+1);
  }else if(!alarm_field&&!alarm_24h()&&up_hit(x,y,32,520,200,56))values[0]=(values[0]+12)%24;
  else if(up_hit(x,y,32,584,416,88))alarm_page=1;else return false;
 }
 return true;
}
