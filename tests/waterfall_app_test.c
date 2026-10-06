#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../Apps/waterfall.c"
static t5_app_api_v1 app;
static risc_runtime_api_v1 runtime_api;
static risc_key_value_v1 kv;
static risc_radio_iq_api_v1 radio;
static unsigned frames,polls,bursts,acquires,releases,yields,live,suspend_calls;
static unsigned limit=3,mode;
static bool deny_radio,airplane,deny_policy,retain_once,release_once,legacy,fail_launch;
static uint8_t pixels[240*240];
static int32_t screen(void){return 240;}
static void clear(void){memset(pixels,0xff,sizeof(pixels));}
static void fill(int32_t x,int32_t y,int32_t w,int32_t h,bool black){
 assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=240&&y+h<=240);
 for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)pixels[j*240+i]=black?0:255;
}
static void tone_fill(int32_t x,int32_t y,int32_t w,int32_t h,int32_t radius,uint8_t tone){(void)radius;fill(x,y,w,h,tone>1);}
static void present(bool full){assert(!full);++frames;}
static bool poll(t5_app_input_t *out,uint32_t wait){
 assert(wait==30);memset(out,0,sizeof(*out));++polls;
 if(mode==1&&polls==1){assert(portable_radio_suspend());}
 if(mode==2&&polls==1){deny_radio=false;out->tapped=true;out->touch_x=210;out->touch_y=5;}
 if(mode==3&&polls==2){out->tapped=true;out->touch_x=210;out->touch_y=5;}
 if(mode==4&&polls==1){out->tapped=true;out->touch_x=10;out->touch_y=5;}
 if(mode==5&&polls==1){out->exit_requested=true;}
 return polls<=limit;
}
static int32_t get(void *context,const char *key,void *dst,uint32_t cap,uint32_t *size){
 assert(context==&kv&&!strcmp(key,"quick_radio")&&cap==4);assert(portable_radio_services_safe());
 if(deny_policy)return RISC_KEY_VALUE_IO;
 uint8_t *b=dst;b[0]=0x51;b[1]=1;b[2]=airplane?PORTABLE_RADIO_AIRPLANE:PORTABLE_RADIO_WIFI;b[3]=b[2]^0xa5;*size=4;
 return RISC_KEY_VALUE_OK;
}
static int32_t put(void *c,const char *k,const void *v,uint32_t n){(void)c;(void)k;(void)v;(void)n;assert(0);return -1;}
static bool suspend(void *context){assert(context==&radio);++suspend_calls;if(retain_once){retain_once=false;return false;}return true;}
static int burst(void *context,uint32_t *pairs,uint32_t count){
 assert(context==&radio&&pairs&&count==256&&portable_radio_services_safe());++bursts;
 if(mode==3&&bursts==1)return RISC_RADIO_IQ_PLL_FAILED;
 if(mode==6&&bursts==1){retain_once=true;return RISC_RADIO_IQ_CLEANUP_RETAINED;}
 for(unsigned i=0;i<count;++i){int n=waterfall_sin_q14[(i*bursts*10)&255]/64;pairs[i]=((uint32_t)n&1023)|(((uint32_t)n&1023)<<10);}
 return RISC_RADIO_IQ_OK;
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 assert(version==1&&out&&out->struct_size==sizeof(*out));++acquires;
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);out->api=&kv;}
 else{assert(!strcmp(name,"radio.iq")&&!instance);if(deny_radio)return false;out->api=&radio;}
 ++live;out->slot=live;out->generation=1;return true;
}
static bool release(risc_runtime_capability_v1 *grant){
 assert(grant&&live&&portable_radio_services_safe());++releases;
 if(release_once){release_once=false;return false;}--live;return true;
}
static void yield(uint32_t ms){assert(ms==50);assert(++yields<10);}
static bool launch(const char *file){assert(!strcmp(file,"springboard.elf"));return !fail_launch;}
const t5_app_api_v1 *t5_app_get_api(uint32_t abi){assert(abi==1);return &app;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return &runtime_api;}
static void reset(void){
 frames=polls=bursts=acquires=releases=yields=live=suspend_calls=mode=0;limit=3;
 deny_radio=airplane=deny_policy=retain_once=release_once=legacy=fail_launch=false;
 app=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(app),.screen_width=screen,.screen_height=screen,.clear=clear,.fill_rect=fill,.present=present,.poll=poll,.fill_rounded_rect_tone=tone_fill};
 runtime_api=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime_api),.acquire=acquire,.release=release,.yield_ms=yield,.request_launch=launch};
 kv=(risc_key_value_v1){.api_version=1,.struct_size=sizeof(kv),.context=&kv,.get=get,.put=put};
 radio=(risc_radio_iq_api_v1){.api_version=1,.struct_size=sizeof(radio),.context=&radio,.capture_burst=burst,.suspend=suspend};
}
int main(void){
 reset();app_main();assert(bursts==3&&frames==4&&waterfall_state.rows_pushed==3&&!live);
 reset();mode=1;app_main();assert(!bursts&&!live&&!waterfall_enabled); /* sleep stops, no automatic resume */
 reset();deny_radio=true;mode=2;app_main();assert(bursts==3&&!live); /* missing grant can retry */
 reset();airplane=true;app_main();assert(!bursts&&!live);assert(acquires==1);
 reset();deny_policy=true;app_main();assert(!bursts&&!live);assert(acquires==1);
 reset();radio.struct_size=offsetof(risc_radio_iq_api_v1,suspend);app_main();assert(!bursts&&!live);
 reset();mode=3;app_main();assert(bursts==3&&waterfall_state.rows_pushed==2&&!live); /* explicit retry */
 reset();mode=4;app_main();assert(!bursts&&!live);
 reset();mode=5;app_main();assert(!bursts&&!live);
 reset();mode=6;app_main();assert(bursts==1&&yields==1&&!live&&!waterfall_enabled);
 reset();release_once=true;app_main();assert(yields==1&&!live);
 reset();app_main();assert(bursts==3&&!live); /* reentry resets former grant/status state */
 puts("Waterfall app: repeated frame paint, grant retry, airplane policy, sleep pause, errors, cleanup and reentry passed");
}
