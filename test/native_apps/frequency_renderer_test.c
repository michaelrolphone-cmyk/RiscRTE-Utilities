/* Production app + production adapter/rasterizer. Only peripherals are fake. */
#define main unused_capture_main
#define portable_input_navigation_open fixture_navigation_open
#define risc_runtime_get_api fixture_runtime_get_api
#include "nova_peripherals.h"
#undef risc_runtime_get_api
#undef main
#undef portable_input_navigation_open
#include "AlarmOutputV1.h"
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version);
#include FREQUENCY_APP_SOURCE
static bool audio_live;
static unsigned audio_opens,audio_writes,audio_closes,launch_count;
static unsigned fixture_scene;
static bool audio_open(void *c,uint32_t rate,uint8_t channels){(void)c;assert(!audio_live&&rate==16000&&channels==1);audio_live=true;++audio_opens;return true;}
static bool audio_write(void *c,const int16_t *pcm,size_t n){(void)c;assert(audio_live&&pcm&&n==256);++audio_writes;ticks+=16;return true;}
static bool audio_close(void *c){(void)c;assert(audio_live);audio_live=false;++audio_closes;if(fixture_scene==9)stop_poll=polls+1;return true;}
static const twatch_audio_out_api_v1 audio_api={.api_version=1,.struct_size=sizeof(audio_api),.open=audio_open,.write=audio_write,.close=audio_close};
static bool acquire_audio(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g){
 if(!strcmp(name,"audio.output")&&version==1){g->api=&audio_api;++grants;return true;}
 return fake_acquire(name,version,id,g);
}
static bool launch_audio(const char *name){assert(!audio_live&&!strcmp(name,"springboard.elf"));++launch_count;return fake_launch(name);}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){
 static risc_runtime_api_v1 api;if(version!=1)return NULL;api=runtime_api;api.acquire=acquire_audio;api.request_launch=launch_audio;return &api;
}
static bool nav_audio(void *c,risc_input_navigation_frame_v1 *out){
    (void)c;*out=(risc_input_navigation_frame_v1){0};
    if(fixture_scene==6&&polls==30)out->pressed=T5_APP_BUTTON_BACK;
    if(fixture_scene==10&&polls==31)out->pressed=T5_APP_BUTTON_CONFIRM;
    return true;
}
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1 *rt){
    (void)rt;static risc_input_navigation_api_v1 api;api=nav_api;api.poll=nav_audio;return &api;
}
#ifdef PORTABLE_NOVA_UI
static void tap(unsigned at,int x,int y){assert(action_count<256);actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
#endif
int main(int argc,char **argv){
 assert(argc==3);directory=argv[1];unsigned scene=(unsigned)atoi(argv[2]);fixture_scene=scene;stop_poll=220;memset(pixels,0xa5,sizeof(pixels));
#ifdef PORTABLE_NOVA_UI
 if(scene==1){tap(10,200,120);tap(20,45,170);tap(30,45,120);tap(40,195,170);tap(50,200,120);}
 if(scene==2){tap(10,30,217);tap(20,30,217);tap(30,210,217);}
 if(scene==3){for(unsigned i=0;i<101;i++)tap(10+i*2,30,217);stop_poll=250;}
 if(scene==4){tap(10,120,217);tap(50,120,217);}
 if(scene==5){tap(10,120,217);tap(30,20,20);}
 if(scene==6){tap(10,120,217);}
 if(scene==7){tap(10,195,170);for(unsigned i=0;i<9;i++)tap(20+i*8,200,120);}
 if(scene==8){tap(10,195,170);for(unsigned i=0;i<9;i++)tap(20+i*8,40,120);}
 if(scene==9){tap(10,120,217);for(unsigned i=500;i<5000;i+=500)tap(i,120,90);stop_poll=5000;}
 if(scene==10){tap(10,120,217);tap(30,120,217);}
 if(scene==11){tap(10,210,217);}
 if(scene==12||scene==13){tap(10,120,217);if(scene==13)tap(20,30,217);tap(30,120,5);tap(31,120,60);tap(32,120,140);tap(90,120,222);}
#endif
 assert(app_module_init()==0);app_main();app_module_fini();assert(!grants&&!frames&&!subs&&!audio_live);assert(presents);
#ifdef PORTABLE_NOVA_UI
 if(scene==1)assert(frequency==1530&&step_index==2);
 if(scene==2)assert(volume==99);
 if(scene==3)assert(volume==0);
 if(scene==4)assert(audio_opens==1&&audio_closes==1&&audio_writes>0&&!playing);
 if(scene==5||scene==6)assert(launch_count==1&&polls>=30&&audio_opens==1&&audio_closes==1);
 if(scene==7)assert(frequency==TONE_MAX_HZ);
 if(scene==8)assert(frequency==TONE_MIN_HZ);
 if(scene==9)assert(!playing&&audio_closes==1&&ticks>=60000&&!strcmp(message,"60 s limit - tap Start"));
 if(scene==10)assert(audio_opens==1&&audio_closes==1&&!playing&&audio_writes==20);
 if(scene==11)assert(volume==100);
 if(scene==12||scene==13)assert(audio_opens==1&&audio_closes==1&&!playing&&volume==(scene==13?99u:100u)&&presents>50);
#endif
 printf("Frequency production renderer scene %u: %u frames, %u audio writes, bounds and cleanup passed\n",scene,presents,audio_writes);
 return 0;
}
