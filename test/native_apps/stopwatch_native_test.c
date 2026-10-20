/* Actual Stopwatch controller and shared raster/ownership path. Reuse the
 * ordered asynchronous hardware double; time and storage remain real clients. */
#define main capture_fixture_main
#define risc_runtime_get_api fixture_runtime_get_api
#include "frequency_native_peripherals.h"
#undef risc_runtime_get_api
#undef main
#include "PortableNativeCustody.h"
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
#include "RiscRealtimeV1.h"
#include "AlarmServiceV2.h"
#endif
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version);
#include STOPWATCH_APP_SOURCE
static unsigned scene,puts_count,launches,nav_stage,sleep_calls,alarm_steps,rtc_reads;
static unsigned retains,private_store_attempts;
static char private_store,shared_store;
static bool alarm_triggered,alarm_done,retained,reader_live,auto_back;
static unsigned shift_at;static int time_shift;
static bool test_rtc(void*c,twatch_rtc_time_v1*s){(void)c;rtc_reads++;if(scene==7||(scene==10&&script_step()>=50))return false;*s=(twatch_rtc_time_v1){2026,10,4,0,20,34,(uint8_t)(12+ticks/1000%48)};return true;}
static const twatch_rtc_api_v1 test_rtc_api={.api_version=2,.struct_size=sizeof(test_rtc_api),.read=test_rtc,.write=fake_write};
static bool stopwatch_key(const char*k){return !strcmp(k,"stopwatch")||!strcmp(k,"stopwatch_utc");}
static int32_t test_get(void*c,const char*k,void*b,uint32_t n,uint32_t*s){
 assert(c==(stopwatch_key(k)?&private_store:&shared_store));
 if(stopwatch_key(k)&&scene==6)return RISC_KEY_VALUE_IO;
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
 if(stopwatch_key(k)&&scene==20)return RISC_KEY_VALUE_CONTEXT;
#endif
 return fake_get(c,k,b,n,s);
}
static int32_t test_put(void*c,const char*k,const void*b,uint32_t n){
 assert(c==(stopwatch_key(k)?&private_store:&shared_store));
 if(stopwatch_key(k)){puts_count++;if(scene==5&&script_step()<100)return RISC_KEY_VALUE_IO;}
 return fake_put(c,k,b,n);
}
static const risc_key_value_v1 private_kv={1,sizeof(private_kv),&private_store,test_get,test_put};
static const risc_key_value_v1 shared_kv={1,sizeof(shared_kv),&shared_store,test_get,test_put};
static const risc_key_value_v1 incompatible_kv={2,sizeof(incompatible_kv),&private_store,test_get,test_put};
static const risc_key_value_v1 short_kv={1,sizeof(short_kv)-1,&private_store,test_get,test_put};
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
static int32_t test_read(void*c,risc_realtime_snapshot_v1*s){assert(c&&reader_live&&!retained);rtc_reads++;if(scene==7||(scene==10&&script_step()>=50))return RISC_REALTIME_IO;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=INT64_C(1791115200)+ticks/1000+(shift_at&&script_step()>=shift_at?time_shift:0),.monotonic_before_us=(uint64_t)ticks*1000,.monotonic_after_us=(uint64_t)ticks*1000};return RISC_REALTIME_OK;}
static const risc_realtime_api_v1 reader_api={1,sizeof(reader_api),&reader_live,test_read};
static alarm_service_descriptor_v2 tagged_alarm={.base={2,sizeof(tagged_alarm),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
#endif
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 assert(!retained);if(!strcmp(n,"storage.key-value"))assert(v==1&&(id==1||id==2));
 if(!strcmp(n,"storage.key-value")&&id==2){private_store_attempts++;if(scene==24)return false;}
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
 if(!strcmp(n,"rtc.clock")||!strcmp(n,"runtime.realtime.control")){assert(!"UTC profile must never acquire RTC or write-time authority");return false;}
 if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(v==1&&!id&&!reader_live);reader_live=true;g->api=&reader_api;grants++;}
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(v==2&&!id);g->api=&tagged_alarm;grants++;}
 else
#endif
 if(!strcmp(n,"input.navigation")&&v==1){g->api=&nav_api;grants++;}
 else {if(!fake_acquire(n,v,id,g))return false;if(g->api==&kv_api)g->api=id==1?&shared_kv:scene==25?&incompatible_kv:scene==26?&short_kv:&private_kv;if(g->api==&rtc_api)g->api=&test_rtc_api;}
 g->slot=1;g->generation=1;return true;
}
static bool test_release(risc_runtime_capability_v1*g){assert(!retained);
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
 if(g->api==&reader_api){assert(reader_live);reader_live=false;}
#endif
 if(!fake_release(g))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static bool test_launch(const char*n){assert(!retained&&!strcmp(n,"springboard.elf"));launches++;return fake_launch(n);}
static bool test_retain(void){assert(!retained);retained=true;retains++;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){static risc_runtime_api_v1 r;r=runtime_api;r.acquire=test_acquire;r.release=test_release;r.request_launch=test_launch;
 r.retain_invocation=test_retain;
 return v==1?&r:NULL;}
static void fixture_diagnostic(const char*s){fprintf(stderr,"%s\n",s);}
static void fixture_yield(uint32_t n){(void)n;assert(!retained);}
static void fixture_tick(void){assert(!retained);if(scene==18&&!alarm_triggered&&script_step()>=50)alarm_triggered=true;}
static int32_t fixture_alarm_status(alarm_status_v1*s){*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alarm_triggered&&!alarm_done?ALARM_STATE_CUE:ALARM_STATE_READY,.mode=ALARM_MODE_VISUAL};return ALARM_OK;}
static int32_t fixture_alarm_step(void){if(alarm_triggered&&!alarm_done&&++alarm_steps>=4)alarm_done=true;return ALARM_OK;}
static int fixture_sleep(void){assert(!subs&&!frames);sleep_calls++;return -2;}
static bool fixture_nav(risc_input_navigation_frame_v1*s){*s=(risc_input_navigation_frame_v1){0};if(!auto_back&&script_step()>=200){auto_back=true;s->pressed=T5_APP_BUTTON_BACK;return true;}if(scene==19&&nav_stage<2&&script_step()>=20+40*nav_stage){s->pressed=T5_APP_BUTTON_CONFIRM;nav_stage++;}return true;}
static void verify_native_labels(void){
 for(unsigned y=0;y<native_height;y++)for(unsigned x=native_width;x<native_width+4;x++)assert(pixels[y*(native_width+4)+x]==0xa5a5);
 if(presents!=1)return;
 for(int i=0;i<4;i++){stopwatch_native_rect r=stopwatch_native_geometry.controls[i];bool glyph=false;for(int y=r.y+3;y<r.y+r.h-3;y++)for(int x=r.x+5;x<r.x+r.w-5;x++)if(pixels[y*(native_width+4)+x]!=0x0082)glyph=true;assert(glyph);}
}
static void point(unsigned at,int x,int y){assert(action_count<256);actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
static void tap(unsigned at,int target){stopwatch_native_rect r=stopwatch_native_geometry.controls[target];point(at,r.x+r.w/2,r.y+r.h/2);}
static void verify_geometry(void){
 for(int i=0;i<4;i++){stopwatch_native_rect r=stopwatch_native_geometry.controls[i];assert(r.x>=0&&r.y>=0&&r.w>=26&&r.h>=26&&r.x+r.w<=(int)native_width&&r.y+r.h<=(int)native_height);assert(stopwatch_native_target(r.x,r.y)==i&&stopwatch_native_target(r.x+r.w-1,r.y+r.h-1)==i);assert(stopwatch_native_target(r.x+r.w,r.y)!=i&&stopwatch_native_target(r.x,r.y+r.h)!=i);for(int j=i+1;j<4;j++){stopwatch_native_rect q=stopwatch_native_geometry.controls[j];assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}}
 assert(stopwatch_native_target(-1,0)==-1&&stopwatch_native_target(native_width,0)==-1);
}
static void seed_record(unsigned kind){
 uint32_t seconds=0;
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
 seconds=(uint32_t)(INT64_C(1791115200)-INT64_C(946684800));const char *key="stopwatch_utc";
#else
 assert(sw_calendar_seconds(2026,10,4,20,34,12,&seconds));const char *key="stopwatch";
#endif
 sw_record saved={.elapsed_ms=kind==23?SW_MAX_MS:5000,.anchor_seconds=kind==22?seconds+60:seconds-10,.running=kind!=23};
 if(kind==23)saved.anchor_seconds=0;
 uint8_t bytes[SW_RECORD_SIZE];sw_encode(&saved,bytes);assert(fake_put(NULL,key,bytes,sizeof(bytes))==RISC_KEY_VALUE_OK);
}
int main(int argc,char**argv){
 assert(argc==6);directory=argv[1];scene=(unsigned)atoi(argv[2]);native_width=(unsigned)atoi(argv[3]);native_height=(unsigned)atoi(argv[4]);native_latency=(unsigned)atoi(argv[5]);assert(stopwatch_native_open(native_width,native_height));verify_geometry();stop_poll=180;
 switch(scene){
 case 0:case 6:case 20:case 24:case 25:case 26:break;
 case 21:seed_record(scene);tap(100,STOPWATCH_TOGGLE);break;
 case 22:case 23:seed_record(scene);break;
 case 1:tap(20,STOPWATCH_TOGGLE);tap(100,STOPWATCH_TOGGLE);break;
 case 2:tap(20,STOPWATCH_TOGGLE);tap(50,STOPWATCH_TOGGLE);tap(80,STOPWATCH_TOGGLE);tap(120,STOPWATCH_TOGGLE);break;
 case 3:tap(20,STOPWATCH_TOGGLE);tap(60,STOPWATCH_TOGGLE);tap(100,STOPWATCH_RESET);tap(120,STOPWATCH_RESET);break;
 case 4:tap(20,STOPWATCH_TOGGLE);tap(60,STOPWATCH_RETRY);tap(100,STOPWATCH_TOGGLE);break;
 case 5:tap(20,STOPWATCH_TOGGLE);tap(120,STOPWATCH_TOGGLE);tap(160,STOPWATCH_TOGGLE);break;
 case 7:tap(20,STOPWATCH_TOGGLE);break;
 case 8:tap(20,STOPWATCH_TOGGLE);tap(100,STOPWATCH_BACK);break;
 case 9:shift_at=50;time_shift=-120;tap(20,STOPWATCH_TOGGLE);tap(140,STOPWATCH_TOGGLE);break;
 case 10:tap(20,STOPWATCH_TOGGLE);tap(140,STOPWATCH_TOGGLE);break;
 case 11:tap(20,STOPWATCH_RESET);tap(60,STOPWATCH_TOGGLE);tap(100,STOPWATCH_RESET);tap(140,STOPWATCH_TOGGLE);break;
 case 12:tap(0,STOPWATCH_TOGGLE);tap(100,STOPWATCH_RETRY);break;
 case 13:gap_at=20;tap(20,STOPWATCH_TOGGLE);tap(100,STOPWATCH_RETRY);break;
 case 14:{stopwatch_native_rect a=stopwatch_native_geometry.controls[0],b=stopwatch_native_geometry.controls[1];point(20,a.x+a.w-1,a.y+a.h/2);point(21,b.x,b.y+b.h/2);break;}
 case 15:tap(20,STOPWATCH_TOGGLE);point(60,native_width/2,2);point(61,native_width/2,native_height/4);point(62,native_width/2,native_height*3/4);point(110,native_width/2,native_height-8);tap(150,STOPWATCH_TOGGLE);break;
 case 16:tap(20,STOPWATCH_TOGGLE);tap(100,STOPWATCH_TOGGLE);break;
 case 17:tap(1,STOPWATCH_TOGGLE);break;
 case 18:tap(20,STOPWATCH_TOGGLE);tap(140,STOPWATCH_TOGGLE);break;
 case 19:break;
 default:assert(0);
 }
 for(unsigned entry=0;entry<(scene==16?2u:1u);entry++){
  ticks=polls=0;stop_poll=240;auto_back=false;fake_touch_state=(risc_touch_snapshot_v1){0};fake_event_head=fake_event_count=0;memset(pixels,0xa5,sizeof(pixels));assert(app_module_init()==0);app_main();assert(private_store_attempts==entry+1);
#ifdef PORTABLE_STOPWATCH_NATIVE_UTC
  if(scene==20||scene==24){assert(retained&&retains==1&&portable_adapter_retained());unsigned before=grants;app_module_fini();assert(grants==before);continue;}
#endif
  assert(!retained&&!reader_live);printf("scene=%u elapsed=%u running=%u writes=%u pending=%u ticks=%u status=%s\n",scene,clock_state.elapsed_ms,clock_state.running,puts_count,pending_pause,ticks,status);
  switch(scene){
  case 0:case 6:case 7:case 12:case 13:case 14:case 20:case 24:case 25:case 26:assert(!clock_state.running&&!puts_count&&!clock_state.elapsed_ms);break;
  case 1:case 4:case 15:case 18:case 19:assert(!clock_state.running&&puts_count==2&&clock_state.elapsed_ms>500);break;
  case 2:assert(!clock_state.running&&puts_count==4&&clock_state.elapsed_ms>500);break;
  case 3:assert(!clock_state.running&&!clock_state.elapsed_ms&&puts_count==3);break;
  case 5:assert(!clock_state.running&&puts_count==3&&clock_state.elapsed_ms>500);break;
  case 8:assert(clock_state.running&&puts_count==1&&launches==1);break;
  case 9:case 10:assert(!clock_state.running&&puts_count==2);break;
  case 11:assert(!clock_state.running&&puts_count==2&&!reset_armed);break;
  case 16:assert(!clock_state.running&&puts_count==2*(entry+1));break;
  case 17:if(native_latency)assert(!clock_state.running&&!puts_count);break;
  case 21:assert(!clock_state.running&&clock_state.approximate&&clock_state.elapsed_ms>=15000&&puts_count==1);break;
  case 22:assert(!clock_state.running&&clock_state.clock_changed&&clock_state.elapsed_ms==5000&&!puts_count);break;
  case 23:assert(!clock_state.running&&clock_state.elapsed_ms==SW_MAX_MS&&!puts_count);break;
  }
  if(scene==18)assert(alarm_done&&alarm_steps==4);
  app_module_fini();assert(!grants&&!frames&&!subs&&!native_pending);assert(presents);
 }
 printf("PASS Stopwatch size=%ux%u latency=%u scene=%u\n",native_width,native_height,native_latency,scene);return 0;
}
