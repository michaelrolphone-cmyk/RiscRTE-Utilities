/* The production app, adapter and rasterizer, with asynchronous peripheral
 * doubles. Scripts use physical coordinates and monotonic time, never polls. */
#include <setjmp.h>
#define main capture_fixture_main
#define risc_runtime_get_api fixture_runtime_get_api
#include "frequency_native_peripherals.h"
#undef risc_runtime_get_api
#undef main
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version);
#include FREQUENCY_APP_SOURCE
#include "PortableBluetoothControl.h"
static unsigned scene,audio_opens,audio_writes,audio_closes,audio_releases,launches,nav_stage,sleep_calls,alarm_steps;
static bool audio_live,retaining,retained,alarm_triggered,alarm_done,alarm_live;
static unsigned alarm_after_writes;
static jmp_buf retention;
static bool audio_open(void*c,uint32_t rate,uint8_t channels){(void)c;assert(!audio_live&&!alarm_live&&rate==16000&&channels==1);audio_live=true;audio_opens++;return scene!=19&&scene!=25;}
static bool audio_write(void*c,const int16_t*pcm,size_t n){(void)c;assert(audio_live&&!alarm_live&&pcm&&n==256);audio_writes++;ticks+=16;return scene!=20&&scene!=21;}
static bool audio_close(void*c){(void)c;assert(audio_live&&!alarm_live);audio_closes++;if(scene==21||scene==25)return false;audio_live=false;return true;}
static const twatch_audio_out_api_v1 audio_api={.api_version=1,.struct_size=sizeof(audio_api),.open=audio_open,.write=audio_write,.close=audio_close};
static twatch_audio_out_api_v1 acquired_audio_api;
static bool ble_off(void*c,bool on){(void)c;assert(!on&&!audio_live);return true;}
static bool ble_status(void*c,uint8_t*s){(void)c;*s=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 ble_api={.api_version=1,.struct_size=sizeof(ble_api),.set_enabled=ble_off,.status=ble_status};
static bool acquire_audio(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!strcmp(n,"audio.output")&&v==1){if(scene==26)return false;acquired_audio_api=audio_api;if(scene==27)acquired_audio_api.struct_size=8;g->api=&acquired_audio_api;grants++;return true;}
 if(!strcmp(n,"bluetooth.hci")){g->api=&ble_api;grants++;return true;}
 return fake_acquire(n,v,id,g);
}
static bool release_audio(risc_runtime_capability_v1*g){
 if(g->api==&acquired_audio_api){assert(!audio_live);audio_releases++;if(scene==22)return false;}
 return fake_release(g);
}
static bool launch_audio(const char*n){assert(!audio_live&&!strcmp(n,"springboard.elf"));launches++;return fake_launch(n);}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){static risc_runtime_api_v1 a;a=runtime_api;a.acquire=acquire_audio;a.release=release_audio;a.request_launch=launch_audio;return v==1?&a:NULL;}
static void fixture_diagnostic(const char*s){if(strstr(s,"cleanup-unconfirmed"))retaining=true;}
static void fixture_yield(uint32_t n){(void)n;if(retaining){retained=true;longjmp(retention,1);}}
static void fixture_tick(void){
 if(scene==24&&!nav_stage&&script_step()>=50){nav_stage++;ticks+=60000;}
 if((scene==23||scene==29)&&!alarm_triggered&&script_step()>=50){alarm_triggered=true;alarm_after_writes=audio_writes;}
}
static int32_t fixture_alarm_status(alarm_status_v1*s){
 *s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alarm_triggered&&!alarm_done?ALARM_STATE_CUE:ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;
}
static int32_t fixture_alarm_step(void){
 if(alarm_triggered&&!alarm_done){assert(!audio_live);alarm_live=true;alarm_steps++;if(alarm_steps==4){alarm_done=true;alarm_live=false;}}
 return ALARM_OK;
}
static int fixture_sleep(void){assert(!audio_live&&!subs&&!frames);sleep_calls++;return -2;}
static bool fixture_nav(risc_input_navigation_frame_v1*s){
 *s=(risc_input_navigation_frame_v1){0};unsigned now=script_step();
 if(scene==6&&nav_stage==0&&now>=50){s->pressed=T5_APP_BUTTON_BACK;nav_stage++;}
 if(scene==10&&nav_stage==0&&now>=61){s->pressed=T5_APP_BUTTON_CONFIRM;nav_stage++;}
 if(scene==17&&nav_stage<3&&now>=20+nav_stage*20){s->pressed=nav_stage==0?T5_APP_BUTTON_RIGHT:nav_stage==1?T5_APP_BUTTON_LEFT:T5_APP_BUTTON_CONFIRM;nav_stage++;}
 return true;
}
static void verify_native_labels(void){
 /* Quick/alarm images are foreign frames, so only initial app frames prove
  * captions. Every submission still checks stride and immutable pending bytes. */
 for(unsigned y=0;y<native_height;y++)for(unsigned x=native_width;x<native_width+4;x++)assert(pixels[y*(native_width+4)+x]==0xa5a5);
 if(presents!=1)return;
 for(int i=0;i<9;i++){
  frequency_native_rect r=frequency_native_geometry.controls[i];bool glyph=false;
  for(int y=r.y+3;y<r.y+r.h-3;y++)for(int x=r.x+5;x<r.x+r.w-5;x++)if(pixels[y*(native_width+4)+x]!=0x0082)glyph=true;
  assert(glyph&&"Every native control retains a visible label");
 }
}
static void point(unsigned at,int x,int y){assert(action_count<256);actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
static void tap(unsigned at,int target){frequency_native_rect r=frequency_native_geometry.controls[target];point(at,r.x+r.w/2,r.y+r.h/2);}
static void verify_geometry(void){
 for(int i=0;i<9;i++){
  frequency_native_rect r=frequency_native_geometry.controls[i];assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>=18&&r.x+r.w<=(int)native_width&&r.y+r.h<=(int)native_height);
  assert(frequency_native_target(r.x,r.y)==i&&frequency_native_target(r.x+r.w-1,r.y+r.h-1)==i);
  assert(frequency_native_target(r.x+r.w,r.y)!=i&&frequency_native_target(r.x,r.y+r.h)!=i);
  for(int j=i+1;j<9;j++){frequency_native_rect q=frequency_native_geometry.controls[j];assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}
 }
 assert(frequency_native_target(-1,0)==-1&&frequency_native_target(native_width,0)==-1);
}
int main(int argc,char**argv){
 assert(argc==6);directory=argv[1];scene=(unsigned)atoi(argv[2]);native_width=(unsigned)atoi(argv[3]);native_height=(unsigned)atoi(argv[4]);native_latency=(unsigned)atoi(argv[5]);
 assert(frequency_native_open(native_width,native_height));verify_geometry();stop_poll=180;
 switch(scene){
 case 0:break;
 case 1:tap(20,FREQUENCY_MORE);tap(40,FREQUENCY_STEP_10);tap(60,FREQUENCY_LESS);tap(80,FREQUENCY_STEP_1000);tap(100,FREQUENCY_MORE);break;
 case 2:tap(20,FREQUENCY_LEVEL_LESS);tap(40,FREQUENCY_LEVEL_LESS);tap(60,FREQUENCY_LEVEL_MORE);break;
 case 3:for(unsigned i=0;i<101;i++)tap(20+i*8,FREQUENCY_LEVEL_LESS);stop_poll=850;break;
 case 4:tap(20,FREQUENCY_TOGGLE);tap(60,FREQUENCY_TOGGLE);break;
 case 5:tap(20,FREQUENCY_TOGGLE);tap(60,FREQUENCY_BACK);break;
 case 6:tap(20,FREQUENCY_TOGGLE);break;
 case 7:case 8:tap(20,FREQUENCY_STEP_1000);for(unsigned i=0;i<9;i++)tap(40+i*10,scene==7?FREQUENCY_MORE:FREQUENCY_LESS);break;
 case 9:tap(20,FREQUENCY_TOGGLE);for(unsigned i=400;i<2400;i+=400)point(i,native_width/2,frequency_native_geometry.specification.y+2);stop_poll=2500;break;
 case 10:tap(20,FREQUENCY_TOGGLE);tap(60,FREQUENCY_TOGGLE);break;
 case 11:tap(20,FREQUENCY_LEVEL_MORE);break;
 case 12:case 13:tap(20,FREQUENCY_TOGGLE);if(scene==13)tap(40,FREQUENCY_LEVEL_LESS);point(60,native_width/2,2);point(61,native_width/2,native_height/4);point(62,native_width/2,native_height*3/4);point(110,native_width/2,native_height-8);tap(150,FREQUENCY_MORE);break;
 case 14:tap(0,FREQUENCY_TOGGLE);tap(40,FREQUENCY_MORE);break;
 case 15:{frequency_native_rect a=frequency_native_geometry.controls[FREQUENCY_LESS],b=frequency_native_geometry.controls[FREQUENCY_MORE];point(20,a.x+a.w-1,a.y+a.h/2);point(21,b.x,b.y+b.h/2);tap(40,FREQUENCY_MORE);break;}
 case 16:gap_at=20;tap(20,FREQUENCY_TOGGLE);tap(40,FREQUENCY_MORE);break;
 case 17:break;
 case 18:tap(20,FREQUENCY_MORE);break;
 case 24:stop_poll=4000;tap(20,FREQUENCY_TOGGLE);break;
 case 19:case 20:case 21:case 22:case 23:case 25:case 26:case 27:tap(20,FREQUENCY_TOGGLE);break;
 case 28:tap(1,FREQUENCY_TOGGLE);tap(40,FREQUENCY_MORE);break;
 case 29:tap(20,FREQUENCY_TOGGLE);tap(100,FREQUENCY_TOGGLE);tap(140,FREQUENCY_TOGGLE);break;
 default:assert(!"Unknown scene");
 }
 for(unsigned entry=0;entry<(scene==18?2u:1u);entry++){
  ticks=0;fake_touch_state=(risc_touch_snapshot_v1){0};fake_event_head=fake_event_count=0;memset(pixels,0xa5,sizeof(pixels));
  assert(app_module_init()==0);if(!setjmp(retention))app_main();
  if(scene==21||scene==25){assert(retained&&audio_live&&audio_closes==1&&!audio_releases&&grants);continue;}
  if(scene==22){assert(retained&&!audio_live&&audio_releases==1&&grants);continue;}
  if(scene==24){fprintf(stderr,"sleep=%u retained=%u native=%u grants=%u subs=%u ticks=%u live=%u\n",sleep_calls,retained,portable_app_sleep_retained(),grants,subs,ticks,audio_live);assert(!retained&&sleep_calls==1&&portable_app_sleep_retained()&&!audio_live&&grants&&!subs);continue;}
  fprintf(stderr,"state scene=%u freq=%u volume=%u opens=%u closes=%u launch=%u ticks=%u\n",scene,frequency,volume,audio_opens,audio_closes,launches,ticks);
  switch(scene){
  case 0:assert(!audio_opens&&frequency==440&&volume==100);break;
  case 1:assert(frequency==1530&&step_index==2);break;
  case 2:assert(volume==99);break;
  case 3:assert(volume==0);break;
  case 4:case 10:assert(audio_opens==1&&audio_closes==1&&audio_writes&&!playing);break;
  case 5:case 6:assert(launches==1&&audio_opens==1&&audio_closes==1);break;
  case 7:assert(frequency==TONE_MAX_HZ);break;
  case 8:assert(frequency==TONE_MIN_HZ);break;
  case 9:assert(!playing&&audio_closes==1&&ticks>=60000&&!strcmp(message,"60 s limit - tap Start"));break;
  case 11:assert(volume==100);break;
  case 12:case 13:assert(audio_opens==1&&audio_closes==1&&!playing&&volume==(scene==13?99u:100u)&&presents>2&&frequency==540);break;
  case 14:case 16:assert(!audio_opens&&frequency==540);break;
  case 15:case 18:assert(frequency==540);break;
  case 17:assert(frequency==440&&audio_opens==1);break;
  case 19:assert(audio_opens==1&&!audio_writes&&audio_closes==1&&!strcmp(message,"Audio start failed"));break;
  case 20:assert(audio_opens==1&&audio_writes==1&&audio_closes==1&&!strcmp(message,"Audio write failed"));break;
  case 23:assert(alarm_done&&alarm_steps==4&&audio_writes==alarm_after_writes&&!playing&&audio_closes==1);break;
  case 26:case 27:assert(!audio_opens&&!strcmp(message,"Audio unavailable"));break;
  case 28:assert(frequency==540);if(native_latency)assert(!audio_opens);break;
  case 29:assert(alarm_done&&audio_opens==2&&audio_closes==2&&!playing);break;
  }
  app_module_fini();assert(!grants&&!frames&&!subs&&!native_pending&&!audio_live&&!alarm_live);assert(presents);
 }
 printf("Frequency native scene=%u size=%ux%u latency=%u frames=%u writes=%u PASS\n",scene,native_width,native_height,native_latency,presents,audio_writes);return 0;
}
