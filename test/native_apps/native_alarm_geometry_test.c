#include "RiscDisplayOutputV1.h"
#include "native_geometry_raster.h"
#undef ALARM_NATIVE_UTC
#undef ALARM_SERVICE_TAGGED_V2
#undef PORTABLE_ALARM_CLIENT
#undef PORTABLE_NATIVE_CUSTODY_FENCE
#undef PORTABLE_APP_LAUNCH_GUARD
#undef PORTABLE_NATIVE_TIME_TOOLBAR
#define main historical_controller_main
#include "native_utc_alarm_app_test.c"
#undef main
static void native_present(bool full){assert(!full&&!native_retained&&!reader_live&&surface_acquired);surface_acquired=false;ng_check();}
static void native_back_mode(bool exits){assert(!exits);}
static void snap(const char*name){const char*d=getenv("NATIVE_GEOMETRY_RASTERS");if(!d)return;char path[1024];snprintf(path,sizeof(path),"%s/alarm-%u-%ux%u-%s.ppm",d,DAILY_ALARM_KIND,surface.width,surface.height,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",surface.width,surface.height);for(unsigned y=0;y<surface.height;y++)for(unsigned x=0;x<surface.width;x++){uint16_t p=((uint16_t*)surface.pixels)[y*(surface.width+3)+x];unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));}
static alarm_native_layout layout(void){return alarm_native_layout_make(ng_width(),ng_height(),DAILY_ALARM_KIND,alarm_page);}
static bool press(alarm_native_rect r){assert(r.width>0&&r.height>0);return alarm_tap(r.x+r.width/2,r.y+r.height/2);}
static void bounds(alarm_native_rect r,int w,int h){if(!r.width&&!r.height)return;assert(r.width>0&&r.height>0&&r.x>=0&&r.y>=0&&r.x+r.width<=w&&r.y+r.height<=h);assert(alarm_native_hit(r,r.x,r.y)&&alarm_native_hit(r,r.x+r.width-1,r.y+r.height-1));assert(!alarm_native_hit(r,r.x-1,r.y)&&!alarm_native_hit(r,r.x+r.width,r.y)&&!alarm_native_hit(r,r.x,r.y+r.height));}
static void geometry(int w,int h){for(unsigned page=0;page<3;page++){alarm_native_layout l=alarm_native_layout_make(w,h,DAILY_ALARM_KIND,page);assert(l.valid);alarm_native_rect controls[12]={l.back,l.edit,l.primary,l.cancel,l.retry,l.fields[0],l.fields[1],l.fields[2],l.done,l.plus,l.minus,l.ampm};for(unsigned i=0;i<12;i++){bounds(controls[i],w,h);for(unsigned j=0;j<i;j++){alarm_native_rect a=controls[i],b=controls[j];if(!a.width||!b.width)continue;assert(a.x+a.width<=b.x||b.x+b.width<=a.x||a.y+a.height<=b.y||b.y+b.height<=a.y);}}bounds(l.minus,w,h);bounds(l.ampm,w,h);bounds(l.value,w,h);bounds(l.caption,w,h);}}
static int32_t geometry_status(void*c,alarm_status_v1*out){int32_t r=state(c,out);out->rtc_seconds=(uint32_t)(native_sample.epoch_seconds-946684800);return r;}
static void initialize(void){setup();service_api.base.status=geometry_status;width_value=ng_width();height_value=ng_height();fake_app.present=native_present;fake_app.set_back_exits_app=native_back_mode;alarm_page=alarm_field=0;alarm_leave_requested=false;}
int main(void){
 for(unsigned i=0;i<257;i++)geometry(240+(int)(i*193%785),240+(int)(i*317%785));
 for(unsigned p=0;p<3;p++){assert(!alarm_native_layout_make(239,240,1,p).valid);assert(!alarm_native_layout_make(240,1025,2,p).valid);}
 const unsigned shapes[][6]={{240,240,0,0,0,0},{466,466,69,69,69,69},{410,502,17,29,23,11},{640,320,21,7,33,13},{503,601,31,47,18,20},{1024,1024,0,0,0,0}};
 for(unsigned k=0;k<6;k++){
  const unsigned*g=shapes[k];ng_open(g[0],g[1],g[2],g[3],g[4],g[5]);initialize();
  assert(!surface_acquired);app_main();assert(surface_acquisitions&&put_calls==0);snap("cold-entry");
  assert(open_dependencies());assert(alarm_writer_load(&writer,storage,DAILY_ALARM_KIND)==0);refresh_status();
  assert(press(layout().edit)&&alarm_page==1);assert(press(layout().fields[0])&&alarm_page==2);unsigned before=values[0];
  assert(press(layout().plus));assert(values[0]==(DAILY_ALARM_KIND==1?(before/12)*12+(before%12+1)%12:(before+1)%100));draw();snap("hour-editor");
#if DAILY_ALARM_KIND==1
  for(unsigned hour=0;hour<24;hour++){values[0]=hour;alarm_time_format=PORTABLE_TIME_FORMAT_12;char label[40];alarm_value_text(label,sizeof(label),values);assert(strstr(label,hour<12?"AM":"PM"));press(layout().plus);assert(values[0]/12==hour/12);press(layout().minus);assert(values[0]==hour);press(layout().ampm);assert(values[0]==(hour+12)%24);}
  values[0]=23;alarm_time_format=PORTABLE_TIME_FORMAT_24;press(layout().plus);assert(!values[0]);press(layout().minus);assert(values[0]==23);
#else
  values[0]=99;press(layout().plus);assert(!values[0]);press(layout().minus);assert(values[0]==99);
#endif
  assert(!alarm_back()&&alarm_page==1);assert(press(layout().fields[1])&&alarm_page==2);values[1]=59;press(layout().plus);assert(values[1]==0);press(layout().minus);assert(values[1]==59);press(layout().done);assert(alarm_page==1);press(layout().done);assert(alarm_page==0&&!put_calls);
  values[0]=0;values[1]=5;values[2]=1;
#if DAILY_ALARM_KIND==1
  alarm_time_format=PORTABLE_TIME_FORMAT_12;rendered_count=0;draw();assert(saw_text("12:05")&&saw_text("AM"));snap("12h-am-root");
  alarm_native_rect vr=layout().value;unsigned meridiem_ink=0;
  for(int y=vr.y;y<vr.y+vr.height;y++)for(int x=vr.x+vr.width-48;x<vr.x+vr.width;x++)if(((uint16_t*)surface.pixels)[(content_view.y+y)*(surface.width+3)+content_view.x+x])meridiem_ink++;
  assert(meridiem_ink>10);
  values[0]=12;rendered_count=0;draw();assert(saw_text("12:05")&&saw_text("PM"));snap("12h-pm-root");
  values[0]=0;alarm_time_format=PORTABLE_TIME_FORMAT_24;
#endif
  draw();snap("edited-root");
  press(layout().primary);assert(put_calls==1&&writer.saved.enabled&&!writer.uncertain);uint32_t deadline=writer.saved.deadline;refresh_status();draw();snap("saved-root");
#if DAILY_ALARM_KIND==1
  alarm_time_format=PORTABLE_TIME_FORMAT_12;rendered_count=0;draw();
  assert(!editing&&saw_text("12:05")&&saw_text("AM")&&saw_text("2026-10-05"));snap("12h-am-saved-root");
  unsigned am_ink=0;uint32_t am_pixels=2166136261u;vr=layout().value;
  for(int y=vr.y;y<vr.y+vr.height;y++)for(int x=vr.x+vr.width-48;x<vr.x+vr.width;x++){uint16_t pixel=((uint16_t*)surface.pixels)[(content_view.y+y)*(surface.width+3)+content_view.x+x];am_ink+=pixel!=0;am_pixels=(am_pixels^pixel)*16777619u;}
  assert(am_ink>10);
  zone("Asia/Kamchatka");assert(native_load_zone());rendered_count=0;draw();
  assert(!editing&&saw_text("12:05")&&saw_text("PM")&&saw_text("2026-10-05"));assert(writer.saved.deadline==deadline&&put_calls==1);snap("12h-pm-saved-root");
  unsigned pm_ink=0;uint32_t pm_pixels=2166136261u;
  for(int y=vr.y;y<vr.y+vr.height;y++)for(int x=vr.x+vr.width-48;x<vr.x+vr.width;x++){uint16_t pixel=((uint16_t*)surface.pixels)[(content_view.y+y)*(surface.width+3)+content_view.x+x];pm_ink+=pixel!=0;pm_pixels=(pm_pixels^pixel)*16777619u;}
  assert(pm_ink>10&&am_pixels!=pm_pixels);
  alarm_time_format=PORTABLE_TIME_FORMAT_24;
#endif
  for(unsigned i=0;i<portable_timezone_count();i++){zone(portable_timezone_get(i)->id);assert(native_load_zone());draw();assert(writer.saved.deadline==deadline&&put_calls==1);}snap("long-zone");
  press(layout().cancel);assert(put_calls==2&&!writer.saved.enabled);refresh_status();
  put_error=RISC_KEY_VALUE_IO;readback_error=RISC_KEY_VALUE_IO;persist_error=true;press(layout().primary);assert(writer.uncertain&&put_calls==3);alarm_config pending=writer.pending;
  assert(!press(layout().edit)&&alarm_page==0);assert(!alarm_back()&&!portable_app_before_launch("default.elf"));press(layout().cancel);assert(put_calls==3);draw();snap("uncertain");
  put_error=readback_error=0;press(layout().retry);assert(!writer.uncertain&&put_calls==4&&writer.saved.deadline==pending.deadline&&writer.saved.revision==pending.revision);draw();snap("retried");
#if DAILY_ALARM_KIND==2
  unsigned oldputs=put_calls;values[0]=values[1]=values[2]=0;press(layout().primary);assert(put_calls==oldputs&&strstr(notice,"NONZERO"));
  values[0]=99;values[1]=values[2]=59;press(layout().primary);assert(put_calls==oldputs+1&&writer.saved.duration==359999);
  int64_t epoch_before=native_sample.epoch_seconds;native_sample.epoch_seconds=(int64_t)ALARM_RTC_MAX+946684800-10;press(layout().primary);assert(put_calls==oldputs+1&&strstr(notice,"RANGE"));native_sample.epoch_seconds=epoch_before;
#else
  zone("America/Denver");assert(native_load_zone());uint32_t now,deadline_check;twatch_rtc_time_v1 raw;int64_t epoch_before=native_sample.epoch_seconds;
  native_sample.epoch_seconds=1772956800;assert(read_clock(&raw,&now));values[0]=2;values[1]=30;assert(!next_alarm(&raw,now,&deadline_check)&&strstr(notice,"GAP"));
  native_sample.epoch_seconds=1793516400;assert(read_clock(&raw,&now));values[0]=1;values[1]=30;assert(!next_alarm(&raw,now,&deadline_check)&&strstr(notice,"AMBIGUOUS"));native_sample.epoch_seconds=epoch_before;
#endif
  press(layout().edit);press(layout().fields[1]);unsigned draft=values[1];has_alert=true;refresh_status();assert(!press(layout().plus)&&values[1]==draft);has_alert=false;refresh_status();draw();snap("modal-restored-draft");
  size_t bytes=surface.stride_bytes*surface.height;void*copy=malloc(bytes);assert(copy);memcpy(copy,surface.pixels,bytes);unsigned acquisitions=surface_acquisitions;native_retained=true;draw();assert(!press(layout().plus));assert(!memcmp(copy,surface.pixels,bytes)&&surface_acquisitions==acquisitions);free(copy);native_retained=false;
  close_dependencies();ng_check();free(surface.pixels);
 }
 puts("PASS native alarm layout257, six raster capability shapes, actual production controller/renderer, 12/24-hour wrap, nested draft, verified Save/Cancel, uncertainty locks and no callbacks after retention");return 0;
}
