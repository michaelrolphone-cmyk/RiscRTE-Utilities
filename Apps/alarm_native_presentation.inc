/* Native safe-content presentation only. Controllers and UTC writer stay shared. */
#include "utility_native_geometry.h"
#include "alarm_native_layout.h"
static unsigned alarm_page,alarm_field;
static bool alarm_leave_requested;
static const char *const alarm_fields[]={"Hour","Minute","Second"};
static utility_native_rect an_rect(alarm_native_rect r){return (utility_native_rect){r.x,r.y,r.width,r.height};}
static void an_text(alarm_native_rect r,const char*s,bool large,uint32_t color){un_text(an_rect(r),s,large,color);}
static void an_button(alarm_native_rect r,const char*s,bool selected){un_button(an_rect(r),s,selected);}
static bool alarm_24h(void){return DAILY_ALARM_KIND==2||alarm_time_format==PORTABLE_TIME_FORMAT_24;}
static void alarm_value_text(char*out,size_t cap,const unsigned*shown){
 if(DAILY_ALARM_KIND==2)snprintf(out,cap,"%02u:%02u:%02u",shown[0],shown[1],shown[2]);
 else if(alarm_24h())snprintf(out,cap,"%02u:%02u",shown[0],shown[1]);
 else snprintf(out,cap,"%u:%02u %s",shown[0]%12?shown[0]%12:12,shown[1],shown[0]<12?"AM":"PM");
}
static const char*alarm_state_text(void){
 if(writer.uncertain)return "Save unconfirmed - retry";
 if(!writer.loaded)return "Saved data invalid - retry";
 if(service_valid&&service_state.state==ALARM_STATE_BLOCKED)return service_state.error==ALARM_RTC?"Native time unavailable":"Service error - retry";
 if(service_valid&&service_state.state==ALARM_STATE_DISMISSING)return "Stopping / saving dismiss";
 if(service_valid&&service_state.occurrence.generation)return service_state.label;
 if(!editing&&service_valid&&service_state.schedules[DAILY_ALARM_KIND-1].revision==writer.saved.revision){
  switch(service_state.schedules[DAILY_ALARM_KIND-1].state){
   case ALARM_SCHEDULE_ARMED:return DAILY_ALARM_KIND==1?"Once - scheduled":"Countdown running";
   case ALARM_SCHEDULE_EXPIRED:return "Expired - will not repeat";
   case ALARM_SCHEDULE_DISMISSED:return "Dismissed - finished";
   default:break;
  }
 }
 return notice&&notice[0]?notice:"Not scheduled";
}
static void alarm_retry(void){
 if(writer.uncertain){save_action(false);return;}
 (void)alarm_writer_load(&writer,storage,DAILY_ALARM_KIND);if(native_retained)return;
 (void)native_load_zone();if(native_retained)return;
 editing=false;restore_saved_fields();service->refresh(service->context);if(native_retained)return;
 (void)portable_time_format_load(alarm_preferences,&alarm_time_format);notice="Refreshing";
}
static bool alarm_back(void){
 if(!portable_app_before_launch(NULL))return false;
 if(service_valid&&service_state.occurrence.generation){notice="Dismiss alert before Back";return false;}
 if(alarm_page){alarm_page=alarm_page==2?1:0;return false;}
#ifdef PORTABLE_RETURN_APP
 if(!runtime||!runtime->request_launch||!runtime->request_launch(PORTABLE_RETURN_APP)){notice="Return unavailable - retry";return false;}
#endif
 return true;
}
static void draw(void){
 if(native_retained||!utility_frame_begin(app))return;
 alarm_native_layout l=alarm_native_layout_make(app->screen_width(),app->screen_height(),DAILY_ALARM_KIND,alarm_page);if(!l.valid)return;
 un_header(app,DAILY_ALARM_KIND==1?"Alarms":"Countdown");
 char value[64];unsigned shown[3]={values[0],values[1],values[2]};
 if(alarm_page==0){
  if(!editing&&!writer.uncertain&&writer.loaded&&writer.saved.enabled){
#if DAILY_ALARM_KIND==1
   twatch_rtc_time_v1 raw,local;if(deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)){shown[0]=local.hour;shown[1]=local.minute;}
#else
   if(service_valid&&service_state.schedules[1].revision==writer.saved.revision&&service_state.schedules[1].state==ALARM_SCHEDULE_ARMED){uint32_t left=writer.saved.deadline>service_state.rtc_seconds?writer.saved.deadline-service_state.rtc_seconds:0;shown[0]=left/3600;shown[1]=left/60%60;shown[2]=left%60;}
#endif
  }
  alarm_value_text(value,sizeof(value),shown);
  if(DAILY_ALARM_KIND==1&&!alarm_24h()){
   /* Large original numeric faces omit letters. Render meridiem through the
    * complete text face, never silently drop it or scale a bitmap. */
   alarm_native_rect number=l.value,meridiem=l.value;number.width-=48;
   meridiem.x+=number.width;meridiem.width=48;
   snprintf(value,sizeof(value),"%u:%02u",shown[0]%12?shown[0]%12:12,shown[1]);
   an_text(number,value,true,NOVA_TEXT);an_text(meridiem,shown[0]<12?"AM":"PM",false,NOVA_TEXT);
  }else an_text(l.value,value,true,NOVA_TEXT);
  an_text(l.caption,DAILY_ALARM_KIND==1?"Once (does not repeat)":"Duration HH:MM:SS",false,NOVA_TEXT);
  an_button(l.edit,"Edit draft",editing);an_text(l.status,alarm_state_text(),false,NOVA_CYAN);
  an_text(l.zone,DAILY_ALARM_KIND==1?(native_zone_valid?native_zone_name:"Time zone unavailable"):"Visual alert only",false,NOVA_TEXT);
  const char*date="Visual alert only";
#if DAILY_ALARM_KIND==1
  twatch_rtc_time_v1 raw,local;
  if(writer.loaded&&writer.saved.enabled&&deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)){snprintf(value,sizeof(value),"%04u-%02u-%02u",local.year,local.month,local.day);date=value;}
#else
  date=editing?"Draft - not saved":"Absolute UTC deadline";
#endif
  an_text(l.date,date,false,NOVA_TEXT);
  bool alert=service_valid&&service_state.occurrence.generation;
  an_button(l.primary,alert?"Dismiss":writer.uncertain?"Retry save":DAILY_ALARM_KIND==1?"Arm once":"Start",true);
  an_button(l.cancel,writer.uncertain?"Wait":"Cancel",false);an_button(l.retry,"Retry / Refresh",false);
 }else if(alarm_page==1){
  for(unsigned i=0;i<(DAILY_ALARM_KIND==1?2u:3u);i++){unsigned v=values[i];if(!i&&!alarm_24h())v=v%12?v%12:12;snprintf(value,sizeof(value),"%s  %02u%s",alarm_fields[i],v,!i&&!alarm_24h()?(values[0]<12?" AM":" PM"):"");an_button(l.fields[i],value,false);}
  an_button(l.done,"Done - draft only",false);
 }else{
  an_text(l.caption,alarm_fields[alarm_field],false,NOVA_TEXT);unsigned v=values[alarm_field];if(!alarm_field&&!alarm_24h())v=v%12?v%12:12;
  snprintf(value,sizeof(value),"%02u",v);an_text(l.value,value,true,NOVA_CYAN);
  an_button(l.minus,"-",false);an_button(l.plus,"+",false);
  if(!alarm_field&&!alarm_24h())an_button(l.ampm,values[0]<12?"AM selected - change":"PM selected - change",true);
  else an_text(l.ampm,"Draft - not saved",false,NOVA_TEXT);
  an_button(l.done,"Done",false);
 }
 app->present(false);
}
static bool alarm_tap(int x,int y){
 if(native_retained)return false;
 alarm_native_layout l=alarm_native_layout_make(app->screen_width(),app->screen_height(),DAILY_ALARM_KIND,alarm_page);if(!l.valid)return false;
 if(alarm_native_hit(l.back,x,y)){alarm_leave_requested=alarm_back();return true;}
 bool alert=service_valid&&service_state.occurrence.generation;
 if(alarm_page==0){
  if(alarm_native_hit(l.primary,x,y)){if(alert)service->acknowledge(service->context,&service_state.occurrence);else save_action(false);return true;}
  if(alarm_native_hit(l.retry,x,y)){if(!alert)alarm_retry();return true;}
  if(alarm_native_hit(l.cancel,x,y)){if(!alert)save_action(true);return true;}
 }
 if(alert||writer.uncertain)return false;
 if(alarm_page==0&&alarm_native_hit(l.edit,x,y)){alarm_page=1;editing=true;notice="Draft - not saved";return true;}
 if(alarm_page==1){
  if(alarm_native_hit(l.done,x,y)){alarm_page=0;return true;}
  for(unsigned i=0;i<(DAILY_ALARM_KIND==1?2u:3u);i++)if(alarm_native_hit(l.fields[i],x,y)){alarm_field=i;alarm_page=2;return true;}
 }else if(alarm_page==2){
  bool up=alarm_native_hit(l.plus,x,y),down=alarm_native_hit(l.minus,x,y);
  if(up||down){unsigned max=alarm_field?59:DAILY_ALARM_KIND==1?23:99;
   if(!alarm_field&&!alarm_24h())values[0]=(values[0]/12)*12+(values[0]%12+(up?1:11))%12;
   else values[alarm_field]=(values[alarm_field]+(up?1:max))%(max+1);
   editing=true;notice="Draft - not saved";return true;
  }
  if(!alarm_field&&!alarm_24h()&&alarm_native_hit(l.ampm,x,y)){values[0]=(values[0]+12)%24;return true;}
  if(alarm_native_hit(l.done,x,y)){alarm_page=1;return true;}
 }
 return false;
}
