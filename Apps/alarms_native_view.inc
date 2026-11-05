/* Alarms/Countdown-local native pixels. Controller state and persistence remain
 * in alarm_nova.inc/alarm_app_shared.h; no fixed viewport is rasterized. */
#include "alarms_native_layout.h"
#include "PortableAlarmsNative.h"
#if !defined(PORTABLE_NATIVE_CANVAS) || !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
#error Native Alarms requires native canvas and app-owned touch chrome
#endif
#if defined(PORTABLE_PAPER_UTILITIES) || defined(ALARM_NATIVE_UTC)
#error Native color Alarms is separate from paper and visual-only UTC profiles
#endif
static alarms_native_layout alarm_native_layout(void){
 alarms_native_layout l={0};
 (void)alarms_native_layout_make(&l,app->screen_width(),app->screen_height(),DAILY_ALARM_KIND==1?2:3);
 return l;
}
static void alarm_native_text(unsigned face,alarms_native_rect r,const char *text,uint32_t color){
 if(r.w<1||r.h<12)return;
 if(!text)text="";
 int height=face==4?40:face==1?20:12;
 if(height>r.h){face=2;height=12;}
 int size=portable_nova_measure(face,text);
 if(size>r.w&&face!=2){face=2;height=12;size=portable_nova_measure(face,text);}
 char fit[128];
 if(size>r.w){
  size_t n=strlen(text);if(n>123)n=123;memcpy(fit,text,n);fit[n]=0;
  while(n&&portable_nova_measure(face,fit)+portable_nova_measure(face,"...")>r.w)fit[--n]=0;
  if(portable_nova_measure(face,"...")>r.w)return;
  memcpy(fit+n,"...",4);text=fit;size=portable_nova_measure(face,text);
 }
 int inset=(r.w-size)/2;
 portable_nova_native_text(face,r.x+inset,r.y+(r.h-height)/2-2,r.w-inset,text,color);
}
static void alarm_native_button(alarms_native_rect r,const char *text,bool selected){
 if(r.w<1||r.h<1)return;
 portable_nova_native_round(r.x,r.y,r.w,r.h,6,selected?NOVA_CYAN:NOVA_DIM);
 if(r.w>2&&r.h>2)portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,5,0x031115u);
 r.x+=3;r.w-=6;alarm_native_text(2,r,text,NOVA_CYAN);
}
static void alarm_native_row(alarms_native_rect r,const char *label,const char *value){
 portable_nova_native_round(r.x,r.y,r.w,r.h,6,NOVA_LINE);
 r.x+=4;r.w-=8;
 if(r.h>=40){
  alarms_native_rect caption=r;caption.h=16;alarm_native_text(2,caption,label,NOVA_CAP);
  r.y+=16;r.h-=16;alarm_native_text(1,r,value,NOVA_CYAN);
 }else alarm_native_text(2,r,value,NOVA_CYAN);
}
static void alarm_native_draw(void){
 alarms_native_layout l=alarm_native_layout();
 uint64_t page=1u+alarm_page*4u+(alarm_page==2?alarm_field:0);
 portable_alarms_native_layout((page<<32)|((uint64_t)(unsigned)l.width<<16)|(unsigned)l.height);
 if(!utility_frame_begin(app))return;
 char value[80],caption[80];unsigned shown[3]={values[0],values[1],values[2]};
 portable_nova_begin();
 alarm_native_button(l.back,"<",false);
 alarm_native_text(NOVA_FACE_ORBITRON_10,l.title,DAILY_ALARM_KIND==1?"ALARMS":"COUNTDOWN",NOVA_CAP);
 if(alarm_page==0){
  alarm_native_button(l.retry,l.retry.w>=40?"Retry":"R",false);
  snprintf(caption,sizeof(caption),"%s",DAILY_ALARM_KIND==1?"Once (does not repeat)":"Duration HH:MM:SS");
#if DAILY_ALARM_KIND == 2
  if(!editing&&service_valid&&writer.saved.enabled&&service_state.schedules[1].revision==writer.saved.revision&&service_state.schedules[1].state==ALARM_SCHEDULE_ARMED){
   uint32_t left=writer.saved.deadline>service_state.rtc_seconds?writer.saved.deadline-service_state.rtc_seconds:0;
   shown[0]=left/3600;shown[1]=left/60%60;shown[2]=left%60;
  }
#else
  if(!editing&&!writer.uncertain&&writer.loaded&&writer.saved.enabled){
   twatch_rtc_time_v1 raw,local;
   if(deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)){
    static const char *const months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    shown[0]=local.hour;shown[1]=local.minute;
    snprintf(caption,sizeof(caption),"Once / %u %s %u",local.day,months[local.month-1],local.year);
   }
  }
#endif
  alarm_value_text(value,sizeof(value),shown);alarm_native_row(l.primary,caption,value);
#if DAILY_ALARM_KIND == 1
  if(!volume_valid)snprintf(value,sizeof(value),"Invalid - choose volume");
  else if(!volume_value)snprintf(value,sizeof(value),"Muted (0%%)");
  else snprintf(value,sizeof(value),"%u%%%s volume",volume_value,volume_missing?" default":"");
  alarm_native_row(l.secondary,"Alarm volume",value);
#else
  alarm_native_row(l.secondary,"Refresh status",alarm_state_text());
#endif
  alarm_native_text(2,l.status,alarm_state_text(),NOVA_CAP);
  bool alert=service_valid&&service_state.occurrence.generation;
  alarm_native_button(l.footer[0],alert?"Dismiss":writer.uncertain?"Retry":DAILY_ALARM_KIND==1?"Arm once":"Start",false);
  alarm_native_button(l.footer[1],writer.uncertain?"Wait":"Cancel",false);
 }else if(alarm_page==1){
  unsigned count=DAILY_ALARM_KIND==1?2:3;
  for(unsigned i=0;i<count;i++){
   if(i==0&&!alarm_24h())snprintf(value,sizeof(value),"Hour: %u %s",values[0]%12?values[0]%12:12,values[0]<12?"AM":"PM");
#ifdef PORTABLE_UNPADDED_HOURS
   else snprintf(value,sizeof(value),i==0?"%s: %u":"%s: %02u",alarm_fields[i],values[i]);
#else
   else snprintf(value,sizeof(value),"%s: %02u",alarm_fields[i],values[i]);
#endif
   alarm_native_button(l.fields[i],value,false);
  }
  alarm_native_button(l.done,"Back to draft",false);
#if DAILY_ALARM_KIND == 1
 }else if(alarm_page==3){
  alarm_native_text(2,l.label,"Alarm volume (%)",NOVA_CAP);
  snprintf(value,sizeof(value),"%u",volume_choice);alarm_native_text(4,l.value,volume_valid?value:"--",NOVA_CYAN);
  alarm_native_button(l.minus,"-",false);alarm_native_button(l.plus,"+",false);
  alarm_native_text(2,l.message,volume_message&&volume_message[0]?volume_message:volume_choice?"Vibration unchanged":"Sound muted",NOVA_CAP);
  alarm_native_button(l.footer[0],volume_uncertain?"Wait":"Cancel",false);
  alarm_native_button(l.footer[1],volume_uncertain?"Retry":"Save",false);
#endif
 }else{
  alarm_native_text(2,l.label,alarm_fields[alarm_field],NOVA_CAP);
  unsigned shown_value=values[alarm_field];if(alarm_field==0&&!alarm_24h())shown_value=shown_value%12?shown_value%12:12;
#ifdef PORTABLE_UNPADDED_HOURS
  snprintf(value,sizeof(value),alarm_field==0?"%u":"%02u",shown_value);
#else
  snprintf(value,sizeof(value),"%02u",shown_value);
#endif
  alarm_native_text(4,l.value,value,NOVA_CYAN);alarm_native_button(l.minus,"-",false);alarm_native_button(l.plus,"+",false);
  alarm_native_text(2,l.message,"Draft only",NOVA_CAP);
  if(alarm_field==0&&!alarm_24h()){
   alarm_native_button(l.footer[0],values[0]<12?"AM":"PM",true);alarm_native_button(l.footer[1],"Done",false);
  }else alarm_native_button(l.done,"Done",false);
 }
 portable_alarms_native_present(false);
}
static bool alarm_native_tap(int x,int y){
 alarms_native_layout l=alarm_native_layout();
 bool alert=service_valid&&service_state.occurrence.generation;
 if(alarm_page==0){
  if(alarms_native_inside(l.retry,x,y)){if(!alert)alarm_retry();return true;}
  if(alarms_native_inside(l.footer[0],x,y)){if(alert)service->acknowledge(service->context,&service_state.occurrence);else save_action(false);}
  else if(alarms_native_inside(l.footer[1],x,y)){if(!alert)save_action(true);}
  else if(alarms_native_inside(l.secondary,x,y)){if(!alert){
#if DAILY_ALARM_KIND == 1
   volume_load();alarm_page=3;
#else
   alarm_retry();
#endif
  }}
  else if(alarms_native_inside(l.primary,x,y)&&!alert&&!writer.uncertain){alarm_page=1;editing=true;notice="Draft only - not saved";}
  else return false;
 }else if(alarm_page==1){
  for(unsigned i=0;i<(DAILY_ALARM_KIND==1?2u:3u);i++)if(alarms_native_inside(l.fields[i],x,y)){alarm_field=i;alarm_page=2;return true;}
  if(alarms_native_inside(l.done,x,y)){alarm_page=0;return true;}return false;
#if DAILY_ALARM_KIND == 1
 }else if(alarm_page==3){
  if(!volume_uncertain&&alarms_native_inside(l.minus,x,y)){volume_choice=volume_choice>=10?volume_choice-10:0;volume_valid=true;volume_message="Draft only - use Save";}
  else if(!volume_uncertain&&alarms_native_inside(l.plus,x,y)){volume_choice=volume_choice<=90?volume_choice+10:100;volume_valid=true;volume_message="Draft only - use Save";}
  else if(alarms_native_inside(l.footer[1],x,y))volume_save();
  else if(!volume_uncertain&&alarms_native_inside(l.footer[0],x,y))alarm_page=0;
  else return false;
#endif
 }else{
  bool up=alarms_native_inside(l.plus,x,y),down=alarms_native_inside(l.minus,x,y);
  if(up||down){unsigned max=alarm_field?59:DAILY_ALARM_KIND==1?23:99;
   if(alarm_field==0&&!alarm_24h())values[0]=(values[0]/12)*12+(values[0]%12+(up?1:11))%12;
   else values[alarm_field]=(values[alarm_field]+(up?1:max))%(max+1);
  }else if(alarm_field==0&&!alarm_24h()&&alarms_native_inside(l.footer[0],x,y))values[0]=(values[0]+12)%24;
  else if(alarms_native_inside(alarm_field==0&&!alarm_24h()?l.footer[1]:l.done,x,y))alarm_page=1;
  else return false;
 }
 return true;
}
