#define main peripheral_main
#define grants fixture_grants
#include "alarms_native_providers.h"
#undef grants
#undef main
#ifdef TEST_COUNTDOWN
#include "../../Apps/countdown.c"
#else
#include "../../Apps/alarms.c"
#endif
static bool write_fail,read_fail,commit_error;
static unsigned writes;
static int32_t error_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size){
 if(read_fail){*size=0;return RISC_KEY_VALUE_IO;}return fake_get(c,key,data,cap,size);
}
static int32_t error_put(void *c,const char *key,const void *data,uint32_t size){
 writes++;if(!write_fail||commit_error)(void)fake_put(c,key,data,size);return write_fail?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 error_kv={1,sizeof(error_kv),NULL,error_get,error_put};
static void paint(void){
 draw();for(unsigned i=0;i<20&&(pending_token||utility_frame_pending);i++){
  assert(t5_app_frame_drain(app));if(utility_frame_pending)draw();
 }
 assert(!pending_token&&!utility_frame_pending);
}
static void tap(alarms_native_rect r){assert(r.w>0&&r.h>0);assert(alarm_tap(r.x+r.w/2,r.y+r.h/2));paint();}
static void start(void){
 assert(app_module_init()==0);app=t5_app_get_api(1);portable_alarms_native_begin();assert(open_dependencies());
 storage=&error_kv;assert(alarm_writer_load(&writer,storage,DAILY_ALARM_KIND)==0);refresh_status();
 alarm_page=alarm_field=0;editing=false;notice="";values[0]=10;values[1]=5;values[2]=1;
#if DAILY_ALARM_KIND == 1
 alarm_preferences=&error_kv;volume_uncertain=false;volume_message="";volume_load();
#endif
 paint();
}
int main(int argc,char **argv){
 assert(argc==3||argc==5);test_width=(unsigned)atoi(argv[1]);test_height=(unsigned)atoi(argv[2]);
 stop_poll=1000000;memset(pixels,0xa5,sizeof(pixels));start();alarms_native_layout l=alarm_native_layout();
 assert(!alarm_tap(-1,-1)&&!alarm_tap((int)test_width,(int)test_height));
 tap(l.primary);assert(alarm_page==1&&editing&&!writer.saved.enabled);
 unsigned count=DAILY_ALARM_KIND==1?2:3;
 for(unsigned i=0;i<count;i++){
  tap(l.fields[i]);assert(alarm_page==2&&alarm_field==i);unsigned before=values[i];
  tap(l.plus);assert(values[i]!=before);tap(l.minus);assert(values[i]==before);
  tap(i==0&&!alarm_24h()?l.footer[1]:l.done);assert(alarm_page==1);
 }
 tap(l.done);assert(!alarm_page&&editing&&!writer.saved.enabled);
#if DAILY_ALARM_KIND == 1
 for(unsigned format=0;format<2;format++)for(unsigned h=0;h<24;h++){
  alarm_time_format=format;values[0]=h;alarm_page=2;alarm_field=0;paint();
  tap(l.plus);assert(values[0]==(format==PORTABLE_TIME_FORMAT_24?(h+1)%24:(h/12)*12+(h%12+1)%12));
  tap(l.minus);assert(values[0]==h);
  if(format==PORTABLE_TIME_FORMAT_12){tap(l.footer[0]);assert(values[0]==(h+12)%24);tap(l.footer[0]);assert(values[0]==h);}
 }
 alarm_page=0;paint();tap(l.secondary);assert(alarm_page==3&&volume_value==50);
 tap(l.plus);assert(volume_choice==60);tap(l.footer[0]);assert(alarm_page==0&&volume_value==50);
 tap(l.secondary);tap(l.plus);tap(l.footer[1]);assert(!alarm_page&&volume_value==60);
 tap(l.secondary);for(unsigned i=0;i<20;i++)tap(l.minus);assert(volume_choice==0);
 tap(l.footer[1]);assert(!alarm_page&&volume_value==0);
 tap(l.secondary);for(unsigned i=0;i<20;i++)tap(l.plus);assert(volume_choice==100);tap(l.footer[1]);assert(volume_value==100);
 tap(l.secondary);tap(l.minus);write_fail=true;tap(l.footer[1]);assert(volume_uncertain&&volume_choice==90&&volume_value==100);
 assert(!alarm_back()&&alarm_page==3);assert(!alarm_tap(l.plus.x+1,l.plus.y+1));assert(!alarm_tap(l.footer[0].x+1,l.footer[0].y+1));
 write_fail=false;tap(l.footer[1]);assert(!volume_uncertain&&!alarm_page&&volume_value==90);
 tap(l.secondary);tap(l.minus);read_fail=true;tap(l.footer[1]);assert(volume_uncertain&&volume_choice==80);
 read_fail=false;tap(l.footer[1]);assert(!volume_uncertain&&volume_value==80&&!alarm_page);
#endif
 alarm_page=0;editing=true;values[0]=10;values[1]=5;values[2]=1;paint();
 write_fail=true;tap(l.footer[0]);assert(writer.uncertain&&!writer.saved.enabled&&writer.pending.revision==1);
 alarm_config pending=writer.pending;tap(l.footer[1]);assert(writer.uncertain&&!memcmp(&pending,&writer.pending,sizeof(pending)));
 write_fail=false;tap(l.footer[0]);assert(!writer.uncertain&&writer.saved.enabled&&writer.saved.revision==1&&!editing);
#if DAILY_ALARM_KIND == 2
 assert(writer.saved.duration==10*3600+5*60+1);assert(writer.saved.deadline-writer.saved.created==writer.saved.duration);
#endif
 tap(l.footer[1]);assert(!writer.saved.enabled&&writer.saved.revision==2);
 write_fail=true;commit_error=true;read_fail=true;tap(l.footer[0]);assert(writer.uncertain&&writer.pending.revision==3);
 write_fail=read_fail=commit_error=false;tap(l.footer[0]);assert(!writer.uncertain&&writer.saved.enabled&&writer.saved.revision==3);
 tap(l.footer[1]);assert(!writer.saved.enabled&&writer.saved.revision==4);
 service_state.occurrence.generation=7;assert(!alarm_tap(l.primary.x+1,l.primary.y+1));tap(l.secondary);assert(!alarm_page);assert(!alarm_back());
 tap(l.footer[0]);assert(acknowledge_count==1);service_state.occurrence.generation=0;
 tap(l.primary);assert(alarm_page==1);tap(l.fields[0]);assert(alarm_page==2);assert(!alarm_back()&&alarm_page==1);assert(!alarm_back()&&alarm_page==0);
 if(argc==5){directory=argv[3];alarm_page=(unsigned)atoi(argv[4]);alarm_field=0;paint();directory=NULL;}
 close_dependencies();app_module_fini();assert(!fixture_grants&&!frames&&!subs&&!pending_token);
 printf("Alarms native kind=%u %ux%u: fields/time/persistence/uncertainty/volume/navigation/stride PASS\n",DAILY_ALARM_KIND,test_width,test_height);
 return 0;
}
