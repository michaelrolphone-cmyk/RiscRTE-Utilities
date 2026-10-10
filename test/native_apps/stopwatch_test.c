/* Exercise the exact production app, including its private state, through ABI fakes.
 * No adapter or firmware code is modified. Separate runtime integration tests
 * validate actual touch/navigation and capability implementations. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/stopwatch.c"

typedef struct {uint32_t advance,buttons;bool tap;int16_t x,y;int32_t rtc_shift;bool lose_rtc;bool fail_get;bool fail_put;bool recover_get;bool recover_rtc;bool recover_write;bool exit_requested;} event;
static event events[4096];static size_t event_count,event_index;
static uint32_t ms_value;static uint64_t awake_ms;static int64_t rtc_offset;
static bool rtc_available,rtc_invalid_calendar,rtc_invalid_weekday;
static uint8_t blob[64];static uint32_t blob_size;static bool blob_present;
static int32_t get_result,put_result;static bool persist_on_error,fail_after_put;static uint32_t put_latency;
static unsigned storage_gets,writes,reads,acquires,releases,frames,polls,live;
static bool provide_app,provide_runtime,deny_storage,deny_rtc,null_storage,null_rtc;
static t5_app_api_v1 fake_app;static risc_runtime_api_v1 fake_runtime;
static risc_key_value_v1 fake_storage;static twatch_rtc_api_v1 fake_rtc;
static int screen_w=240,screen_h=240;static uint8_t pixels[1024*1024];
static char seen_status[96],seen_left[64];static const char*render_path;
static sw_record write_log[32];
static void advance(uint32_t n){ms_value+=n;awake_ms+=n;}
static uint32_t fake_millis(void){return ms_value;}
static int32_t width(void){return screen_w;}static int32_t height(void){return screen_h;}
static void clear_screen(void){memset(pixels,255,sizeof(pixels));}
static void fill_screen(int32_t x,int32_t y,int32_t w,int32_t h,bool black){
 assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=screen_w&&y+h<=screen_h);
 for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixels[j*screen_w+i]=black?0:255;
}
static void draw_text_fake(int32_t x,int32_t y,const char*s){
 assert(s);int n=(screen_w-x)/6;for(int i=0;s[i]&&i<n;i++){
  char one[2]={s[i],0};daily_draw_text(&fake_app,x+i*6,y,one,1);
 }
}
static void draw_label_fake(int32_t x,int32_t y,int32_t w,const char*s){
 assert(s&&x>=0&&w>0&&x+w<=screen_w&&y>=0&&y+7<=screen_h);
 if(y==124)snprintf(seen_status,sizeof(seen_status),"%s",s);
 if(y==screen_h-67&&x==10)snprintf(seen_left,sizeof(seen_left),"%s",s);
 int n=(int)strlen(s);if(n>w/6)n=w/6;char text[128];assert((size_t)n<sizeof(text));memcpy(text,s,(size_t)n);text[n]=0;
 draw_text_fake(x+(w-n*6)/2,y,text);
}
static void present_fake(bool full){
 assert(!full);frames++;
 if(render_path){FILE*f=fopen(render_path,"wb");assert(f);fprintf(f,"P5\n%d %d\n255\n",screen_w,screen_h);assert(fwrite(pixels,1,(size_t)screen_w*screen_h,f)==(size_t)screen_w*screen_h);assert(!fclose(f));}
}
static bool poll_fake(t5_app_input_t*out,uint32_t wait){
 assert(wait==20||wait==50);polls++;assert(polls<10000);*out=(t5_app_input_t){0};
 if(event_index==event_count)return false;
 event e=events[event_index++];advance(e.advance);rtc_offset+=e.rtc_shift;
 if(e.lose_rtc)rtc_available=false;
 if(e.fail_get)get_result=RISC_KEY_VALUE_IO;
 if(e.fail_put)put_result=RISC_KEY_VALUE_IO;
 if(e.recover_get)get_result=RISC_KEY_VALUE_OK;
 if(e.recover_rtc)rtc_available=true;
 if(e.recover_write){put_result=RISC_KEY_VALUE_OK;fail_after_put=false;}
 out->exit_requested=e.exit_requested;out->buttons=e.buttons;out->tapped=e.tap;out->touch_x=e.x;out->touch_y=e.y;
 /* Production shared adapter maps this chrome hit to BACK. */
 if(e.tap&&e.x<56&&e.y<40)out->buttons|=T5_APP_BUTTON_BACK;
 return true;
}
static bool rtc_read_fake(void*context,twatch_rtc_time_v1*out){
 assert(context==&fake_rtc);reads++;if(!rtc_available)return false;
 int64_t total=1000+(int64_t)(awake_ms/1000u)+rtc_offset;assert(total>=0&&total<=SW_RTC_MAX);
 uint32_t days=(uint32_t)total/86400u;uint16_t year=2000;uint8_t month=1;
 while(days>=(uint32_t)(year%4?365:366)){days-=year%4?365:366;year++;}
 static const uint8_t mdays[]={31,28,31,30,31,30,31,31,30,31,30,31};
 while(days>=(uint32_t)(mdays[month-1]+(month==2&&year%4==0))){days-=mdays[month-1]+(month==2&&year%4==0);month++;}
 *out=(twatch_rtc_time_v1){.year=year,.month=month,.day=(uint8_t)(days+1),.weekday=0,.hour=(uint8_t)(total/3600%24),.minute=(uint8_t)(total/60%60),.second=(uint8_t)(total%60)};
 if(rtc_invalid_calendar)out->day=0;
 if(rtc_invalid_weekday)out->weekday=7;
 return true;
}
static int32_t get_fake(void*context,const char*key,void*dest,uint32_t capacity,uint32_t*n){
 assert(context==&fake_storage&&!strcmp(key,"stopwatch")&&capacity==SW_RECORD_SIZE&&dest&&n);storage_gets++;*n=0;
 if(get_result!=RISC_KEY_VALUE_OK)return get_result;
 if(!blob_present)return RISC_KEY_VALUE_NOT_FOUND;
 *n=blob_size;if(blob_size>capacity)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(dest,blob,blob_size);return RISC_KEY_VALUE_OK;
}
static int32_t put_fake(void*context,const char*key,const void*src,uint32_t n){
 assert(context==&fake_storage&&!strcmp(key,"stopwatch")&&n==SW_RECORD_SIZE);assert(writes<32);
 assert(sw_decode(&write_log[writes],src,n));writes++;
 if(put_result==RISC_KEY_VALUE_OK||persist_on_error){memcpy(blob,src,n);blob_size=n;blob_present=true;}
 advance(put_latency);if(fail_after_put)get_result=RISC_KEY_VALUE_IO;return put_result;
}
static bool acquire_fake(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 assert(instance==0&&g&&g->struct_size==sizeof(*g));acquires++;
 if(!strcmp(name,"storage.key-value")){assert(version==1);if(deny_storage)return false;g->slot=1;g->api=null_storage?NULL:&fake_storage;}
 else {assert(!strcmp(name,"rtc.clock")&&version==2);if(deny_rtc)return false;g->slot=2;g->api=null_rtc?NULL:&fake_rtc;}
 g->generation=1;assert(!(live&(1u<<g->slot)));live|=1u<<g->slot;return true;
}
static bool release_fake(risc_runtime_capability_v1*g){assert(g&&g->generation==1&&(live&(1u<<g->slot)));live&=~(1u<<g->slot);releases++;*g=(risc_runtime_capability_v1){0};return true;}
const t5_app_api_v1*t5_app_get_api(uint32_t version){assert(version==1);return provide_app?&fake_app:NULL;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){assert(version==1);return provide_runtime?&fake_runtime:NULL;}
static void setup(bool erase){
 event_count=event_index=0;ms_value=0;awake_ms=0;rtc_offset=0;rtc_available=true;rtc_invalid_calendar=rtc_invalid_weekday=false;
 get_result=put_result=RISC_KEY_VALUE_OK;persist_on_error=fail_after_put=false;put_latency=0;
 storage_gets=writes=reads=acquires=releases=frames=polls=live=0;provide_app=provide_runtime=true;deny_storage=deny_rtc=null_storage=null_rtc=false;
 screen_w=screen_h=240;seen_status[0]=seen_left[0]=0;render_path=NULL;
 if(erase){memset(blob,0,sizeof(blob));blob_size=0;blob_present=false;}
 fake_app=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=width,.screen_height=height,.clear=clear_screen,.draw_text=draw_text_fake,.fill_rect=fill_screen,.present=present_fake,.poll=poll_fake,.millis=fake_millis,.draw_label=draw_label_fake};
 fake_runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(fake_runtime),.acquire=acquire_fake,.release=release_fake};
 fake_storage=(risc_key_value_v1){.api_version=1,.struct_size=sizeof(fake_storage),.context=&fake_storage,.get=get_fake,.put=put_fake};
 fake_rtc=(twatch_rtc_api_v1){.api_version=2,.struct_size=sizeof(fake_rtc),.context=&fake_rtc,.read=rtc_read_fake};
}
static void add(event e){assert(event_count<4096);events[event_count++]=e;}
static void confirm(uint32_t ms){add((event){.advance=ms,.buttons=T5_APP_BUTTON_CONFIRM});}
static void tap(int x,int y){add((event){.tap=true,.x=(int16_t)x,.y=(int16_t)y});}
static void back(void){add((event){.buttons=T5_APP_BUTTON_BACK});}
static void run(void){app_main();assert(!live);assert(!runtime&&!storage&&!rtc);}
static void seed(sw_record r){sw_encode(&r,blob);blob_size=SW_RECORD_SIZE;blob_present=true;}
static sw_record saved(void){sw_record r;assert(sw_decode(&r,blob,blob_size));return r;}
static void normal_tests(void){
 setup(true);run();assert(loaded&&!clock_state.running&&clock_state.elapsed_ms==0&&storage_gets==1&&writes==0&&releases==2);
 setup(true);confirm(0);for(int i=0;i<120;i++)add((event){.advance=50});confirm(123);back();run();
 assert(writes==2&&storage_gets==1&&!clock_state.running&&clock_state.elapsed_ms==6123&&saved().elapsed_ms==6123&&!saved().running&&frames>100);
 setup(false);rtc_offset=500;run();assert(clock_state.elapsed_ms==6123&&!clock_state.running&&writes==0);
 setup(true);confirm(0);add((event){.advance=1234});back();run();assert(writes==1&&saved().running&&clock_state.elapsed_ms==1234);
 /* Fresh invocation / Light/Deep reset: uptime restarts, raw RTC advances. */
 setup(false);rtc_offset=45;run();assert(clock_state.running&&clock_state.elapsed_ms==45000&&clock_state.approximate&&writes==0);
 setup(false);rtc_offset=60;confirm(234);run();assert(!clock_state.running&&clock_state.elapsed_ms==60234&&saved().approximate&&writes==1);
 setup(false);rtc_offset=1000;run();assert(clock_state.elapsed_ms==60234&&!clock_state.running&&writes==0);
 setup(true);ms_value=UINT32_MAX-100;confirm(0);add((event){.advance=250});confirm(0);run();assert(clock_state.elapsed_ms==250&&saved().elapsed_ms==250);
 setup(true);put_latency=375;confirm(0);add((event){.advance=125});confirm(0);run();assert(saved().elapsed_ms==500);
}
static void heartbeat_tests(void){
 setup(true);confirm(0);for(unsigned i=0;i<3600;i++)add((event){.advance=1000});run();
 assert(writes==1&&storage_gets==1&&clock_state.running&&clock_state.elapsed_ms==3600000&&reads==3602);
 setup(true);ms_value=UINT32_MAX-100;confirm(0);add((event){.advance=5123});confirm(0);run();
 assert(writes==2&&!clock_state.clock_changed&&saved().elapsed_ms==5123);
}
static void retry_tests(void){
 setup(true);confirm(0);add((event){.advance=456});tap(100,220);run();
 assert(clock_state.running&&clock_state.elapsed_ms==456&&!clock_state.approximate&&writes==1);
 setup(true);seed((sw_record){12345,0,false,false});get_result=RISC_KEY_VALUE_IO;
 add((event){.recover_get=true,.tap=true,.x=100,.y=220});run();
 assert(loaded&&clock_state.elapsed_ms==12345&&!clock_state.running&&writes==0&&storage_gets==2);
 setup(true);seed((sw_record){100,1000,true,false});rtc_available=false;
 add((event){.advance=3000,.recover_rtc=true,.tap=true,.x=100,.y=220});run();
 assert(loaded&&clock_state.running&&clock_state.elapsed_ms==3100&&clock_state.approximate&&writes==0);
}
static void anomaly_save_tests(void){
 for(unsigned persist=0;persist<2;persist++){
  setup(true);persist_on_error=persist;confirm(0);add((event){.advance=1000,.rtc_shift=20,.fail_put=true});confirm(0);run();
  assert(writes==2&&!clock_state.running&&clock_state.elapsed_ms==1000&&strstr(seen_status,"UNCONFIRMED"));
  assert(saved().running==(persist==0));
 }
 setup(true);confirm(0);add((event){.advance=1000,.rtc_shift=20,.fail_put=true});
 /* Apply failed reread only after the initial successful start via event. */
 add((event){.buttons=T5_APP_BUTTON_CONFIRM,.fail_get=true});run();
 assert(writes==2&&!loaded&&!clock_state.running&&clock_state.elapsed_ms==1000);
 for(unsigned failed_read=0;failed_read<2;failed_read++){
  setup(true);confirm(0);add((event){.advance=1000,.rtc_shift=20,.fail_put=true});
  add((event){.buttons=T5_APP_BUTTON_CONFIRM,.fail_get=failed_read!=0});
  add((event){.recover_get=true,.recover_write=true,.tap=true,.x=100,.y=220});run();
  assert(writes==3&&loaded&&!clock_state.running&&!clock_state.clock_changed&&!pending_pause);
  assert(saved().elapsed_ms==1000&&!saved().running&&!strcmp(seen_status,"PAUSED"));
 }
}
static void reset_tests(void){
 setup(true);seed((sw_record){12345,0,false,false});tap(170,170);back();run();assert(writes==0&&saved().elapsed_ms==12345&&reset_armed);
 setup(false);tap(170,170);tap(170,170);run();assert(writes==1&&saved().elapsed_ms==0&&!saved().running&&!clock_state.approximate&&!reset_armed);
 setup(true);confirm(0);add((event){.advance=500});tap(170,170);tap(170,170);run();assert(writes==2&&clock_state.elapsed_ms==0&&!clock_state.running);
 setup(true);seed((sw_record){12345,0,false,false});tap(170,170);tap(100,220);tap(170,170);run();assert(writes==0&&reset_armed);
 setup(true);seed((sw_record){12345,0,false,false});tap(170,170);confirm(0);tap(170,170);run();assert(writes==1&&reset_armed&&clock_state.running);
 setup(true);tap(0,170);tap(239,170);tap(120,170);tap(10,150);run();assert(writes==0&&!reset_armed&&!clock_state.running);
}
static void record_tests(void){
 for(unsigned kind=0;kind<7;kind++){
  setup(true);seed((sw_record){12345,0,false,false});
  if(kind==0)blob[0]^=1;
  if(kind==1){blob[3]='2';sw_write32(blob+16,sw_checksum(blob));}
  if(kind==2)blob_size=0;
  if(kind==3)blob_size=19;
  if(kind==4)blob_size=64;
  if(kind==5)get_result=RISC_KEY_VALUE_IO;
  if(kind==6){sw_write32(blob+8,SW_MAX_MS+1u);sw_write32(blob+16,sw_checksum(blob));}
  confirm(0);run();assert(!loaded&&!clock_state.running&&writes==0);
  /* Invalid records are never silently replaced; reset requires two taps. */
  get_result=RISC_KEY_VALUE_OK;event_count=event_index=0;tap(170,170);tap(170,170);run();assert(loaded&&writes==1&&saved().elapsed_ms==0);
 }
}
static void uncertain_tests(void){
 for(unsigned persist=0;persist<2;persist++){
  setup(true);put_result=RISC_KEY_VALUE_IO;persist_on_error=persist;confirm(0);run();
  assert(writes==1&&storage_gets==2&&loaded&&clock_state.running==(persist!=0)&&strstr(seen_status,"UNCONFIRMED"));
 }
 setup(true);seed((sw_record){100,1000,true,false});put_result=RISC_KEY_VALUE_IO;persist_on_error=false;confirm(123);run();
 assert(clock_state.running&&clock_state.approximate&&writes==1&&storage_gets==2&&saved().running);
 setup(true);seed((sw_record){100,1000,true,false});put_result=RISC_KEY_VALUE_IO;persist_on_error=true;confirm(123);run();
 assert(!clock_state.running&&clock_state.elapsed_ms==223&&!saved().running&&writes==1&&storage_gets==2);
 setup(true);put_result=RISC_KEY_VALUE_IO;persist_on_error=true;fail_after_put=true;confirm(0);confirm(20);run();
 assert(!loaded&&!clock_state.running&&writes==1&&storage_gets==2);
 setup(true);seed((sw_record){12345,0,false,false});put_result=RISC_KEY_VALUE_IO;persist_on_error=false;tap(170,170);tap(170,170);run();assert(clock_state.elapsed_ms==12345&&writes==1);
 setup(true);seed((sw_record){12345,0,false,false});put_result=RISC_KEY_VALUE_IO;persist_on_error=true;tap(170,170);tap(170,170);run();assert(clock_state.elapsed_ms==0&&writes==1);
}
static void clock_tests(void){
 setup(true);rtc_available=false;confirm(0);run();assert(!clock_state.running&&writes==0&&strstr(seen_status,"RTC UNAVAILABLE"));
 setup(true);rtc_invalid_calendar=true;confirm(0);run();assert(writes==0&&!clock_state.running);
 setup(true);rtc_invalid_weekday=true;confirm(0);run();assert(writes==0&&!clock_state.running);
 setup(true);seed((sw_record){100,1001,true,false});confirm(0);run();assert(clock_state.clock_changed&&!clock_state.running&&writes==0);
 setup(true);seed((sw_record){100,1000,true,false});rtc_available=false;run();assert(clock_state.clock_changed&&!clock_state.running&&writes==0);
 setup(true);confirm(0);add((event){.advance=1000,.rtc_shift=20});run();assert(!clock_state.running&&clock_state.clock_changed&&pending_pause&&writes==1&&!strcmp(seen_left,"SAVE PAUSE"));
 setup(true);confirm(0);add((event){.advance=1000,.rtc_shift=-5});confirm(0);run();assert(!clock_state.running&&!clock_state.clock_changed&&!pending_pause&&writes==2&&!saved().running&&saved().elapsed_ms==1000);
 setup(true);confirm(0);add((event){.advance=1000,.lose_rtc=true});confirm(0);run();assert(!clock_state.running&&writes==2&&!saved().running&&saved().elapsed_ms==1000);
 /* Offline forward edits are necessarily counted as elapsed time. */
 setup(true);seed((sw_record){100,1000,true,false});rtc_offset=3600;run();assert(clock_state.elapsed_ms==3600100&&clock_state.approximate&&clock_state.running&&writes==0);
 setup(true);seed((sw_record){SW_MAX_MS-1000,1000,true,false});add((event){.advance=1000});run();assert(clock_state.elapsed_ms==SW_MAX_MS&&!clock_state.running&&clock_state.limit&&writes==0);
 setup(true);seed((sw_record){SW_MAX_MS-1000,1000,true,false});rtc_offset=1;confirm(0);run();assert(clock_state.elapsed_ms==SW_MAX_MS&&!clock_state.running&&clock_state.limit&&writes==0);
}
static void capability_tests(void){
 setup(true);provide_app=false;run();assert(!acquires&&!frames);
 setup(true);fake_app.abi_version=2;run();assert(!acquires&&!frames);
 setup(true);fake_app.struct_size=offsetof(t5_app_api_v1,draw_label);run();assert(!acquires&&!frames);
 #define MISSING(member) setup(true);fake_app.member=NULL;run();assert(!acquires&&!frames)
 MISSING(poll);MISSING(millis);MISSING(screen_width);MISSING(screen_height);MISSING(clear);MISSING(draw_text);MISSING(draw_label);MISSING(fill_rect);MISSING(present);
 #undef MISSING
 int dims[][2]={{159,240},{240,239},{1025,240},{240,1025}};
 for(unsigned i=0;i<sizeof(dims)/sizeof(dims[0]);i++){setup(true);screen_w=dims[i][0];screen_h=dims[i][1];run();assert(!acquires&&!frames);}
 for(unsigned kind=0;kind<5;kind++){
  setup(true);if(kind==0)provide_runtime=false;if(kind==1)fake_runtime.api_version=2;if(kind==2)fake_runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE-1;if(kind==3)fake_runtime.acquire=NULL;if(kind==4)fake_runtime.release=NULL;
  back();run();assert(!acquires&&!releases&&frames==1);
 }
 setup(true);deny_storage=true;back();run();assert(acquires==1&&!releases&&!storage_gets);
 setup(true);deny_rtc=true;back();run();assert(acquires==2&&releases==1&&!storage_gets);
 for(unsigned kind=0;kind<4;kind++){
  setup(true);if(kind==0)fake_storage.api_version=2;if(kind==1)fake_storage.struct_size=sizeof(fake_storage)-1;if(kind==2)fake_storage.get=NULL;if(kind==3)fake_storage.put=NULL;
  back();run();assert(acquires==1&&releases==1&&!storage_gets);
 }
 for(unsigned kind=0;kind<3;kind++){
  setup(true);if(kind==0)fake_rtc.api_version=1;if(kind==1)fake_rtc.struct_size=sizeof(fake_rtc)-1;if(kind==2)fake_rtc.read=NULL;
  back();run();assert(acquires==2&&releases==2&&!storage_gets);
 }
 setup(true);fake_app.struct_size=offsetof(t5_app_api_v1,draw_label)+sizeof(fake_app.draw_label);run();assert(loaded&&releases==2);
}
static void null_grant_tests(void){
 setup(true);null_storage=true;back();run();assert(releases==1);
 setup(true);null_rtc=true;back();run();assert(releases==2);
}
static void back_tests(void){
 setup(true);confirm(0);back();run();assert(writes==1&&saved().running&&releases==2&&event_index==2);
 setup(true);tap(12,20);confirm(0);run();assert(writes==0&&releases==2&&event_index==1);
 setup(true);deny_rtc=true;tap(12,20);confirm(0);run();assert(writes==0&&releases==1&&event_index==1);
}
static void exit_tests(void){
 setup(true);confirm(0);add((event){.exit_requested=true});confirm(0);run();
 assert(writes==1&&saved().running&&event_index==2&&releases==2);
 setup(true);deny_rtc=true;add((event){.exit_requested=true});confirm(0);run();
 assert(writes==0&&event_index==1&&releases==1);
}
static void geometry_tests(void){
 int dims[][2]={{160,240},{219,240},{220,240},{240,240},{320,480},{1024,1024}};
 for(unsigned i=0;i<sizeof(dims)/sizeof(dims[0]);i++){setup(true);screen_w=dims[i][0];screen_h=dims[i][1];confirm(0);tap(screen_w-40,screen_h-65);run();assert(frames>=3);}
}
static void render_tests(void){
 const char*dir=getenv("STOPWATCH_RENDER_DIR");if(!dir)return;char path[1024];
 const char*names[]={"idle","running","paused","restored","confirm-reset","clock-lost","invalid-record","limit"};
 for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++){
  setup(true);snprintf(path,sizeof(path),"%s/stopwatch-%s.pgm",dir,names[i]);render_path=path;
  if(i==1){confirm(0);add((event){.advance=62345});}
  if(i==2){confirm(0);confirm(62345);}
  if(i==3){seed((sw_record){0,1000,true,false});rtc_offset=3723;}
  if(i==4)tap(170,170);
  if(i==5){confirm(0);add((event){.advance=1000,.lose_rtc=true});}
  if(i==6){seed((sw_record){0});blob[0]='X';}
  if(i==7){seed((sw_record){SW_MAX_MS-1000,1000,true,false});rtc_offset=1;}
  run();
 }
}
int main(int argc,char**argv){const char*name=argc>1?argv[1]:"all";
 #define RUN(n) if(!strcmp(name,"all")||!strcmp(name,#n)){n##_tests();puts("stopwatch app " #n ": passed");}
 RUN(normal);RUN(heartbeat);RUN(retry);RUN(anomaly_save);RUN(reset);RUN(record);RUN(uncertain);RUN(clock);RUN(capability);RUN(null_grant);RUN(back);RUN(exit);RUN(geometry);RUN(render);
 return 0;
}
