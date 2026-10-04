#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PORTABLE_ALARM_CLIENT
#include "../../Apps/frequency_generator.c"
static jmp_buf held;
static unsigned scenario,polls,opens,writes,closes,releases,grants,frames;
static bool live,alarm_live,retained;
static uint32_t ms;
static int screen_w=240,screen_h=240;
static twatch_audio_out_api_v1 audio_api;
static t5_app_api_v1 app_api;
static risc_runtime_api_v1 runtime_api;
static unsigned after_preempt_writes;
static bool audio_open(void*c,uint32_t rate,uint8_t channels) {
    (void)c;assert(!live&&!alarm_live&&rate==16000&&channels==1);opens++;live=true;
    return scenario!=2&&scenario!=5;
}
static bool audio_write(void*c,const int16_t*pcm,size_t n) {
    (void)c;assert(live&&!alarm_live&&n==256&&pcm);writes++;
    for(size_t i=0;i<n;i++)assert(pcm[i]>=-8191&&pcm[i]<=8191);
    if(scenario==3||scenario==4)return false;
    return true;
}
static bool audio_close(void*c) {
    (void)c;assert(!alarm_live);closes++;
    if(scenario==4||scenario==5)return false;
    live=false;return true;
}
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g) {
    assert(!strcmp(n,"audio.output")&&v==1&&!i&&g->struct_size==sizeof(*g));
    if(scenario==6)return false;
    ++grants;g->api=&audio_api;return true;
}
static bool release(risc_runtime_capability_v1*g) {
    assert(g->api&&!live);releases++;if(scenario==9)return false;
    --grants;g->api=NULL;return true;
}
static bool diagnostic(const char*s){assert(s);return true;}
static void yield(uint32_t n){assert(n==50);retained=true;longjmp(held,1);}
static uint32_t millis(void){return ms;}
static int32_t width(void){return screen_w;}
static int32_t height(void){return screen_h;}
static void clear(void){}
static void rect(int32_t x,int32_t y,int32_t w,int32_t h,bool black){
    (void)black;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=screen_w&&y+h<=screen_h);
}
static void text(int32_t x,int32_t y,const char*s){assert(x>=0&&y>=0&&s);}
static void label(int32_t x,int32_t y,int32_t w,const char*s){assert(x>=0&&y>=0&&w>0&&x+w<=screen_w&&s);}
static void present(bool full){assert(!full);++frames;}
bool portable_app_sleep_retained(void){return scenario==10&&polls>=2;}
static bool poll(t5_app_input_t *in,uint32_t wait) {
    assert(wait==0||wait==30);assert(++polls<50);memset(in,0,sizeof(*in));ms+=16;
    if(scenario==0){in->buttons=T5_APP_BUTTON_BACK;return true;}
    if(polls==1){in->buttons=T5_APP_BUTTON_CONFIRM;return true;}
    if(scenario==7&&polls==2){assert(portable_audio_suspend());alarm_live=true;after_preempt_writes=writes;return true;}
    if(scenario==10&&polls==2){assert(portable_audio_suspend());return false;}
    if(scenario==14&&polls==2){in->buttons=T5_APP_BUTTON_CONFIRM;in->tapped=true;in->touch_x=120;in->touch_y=220;return true;}
    if(scenario==8&&polls==2){ms+=60000;return true;}
    if(scenario==11&&polls==2){in->tapped=true;in->touch_x=232-1;in->touch_y=138;return true;}
    if(scenario==11&&polls<14){in->buttons=T5_APP_BUTTON_RIGHT;return true;}
    if(scenario==12&&polls<10){in->tapped=true;in->touch_x=30;in->touch_y=170;return true;}
    if(polls<4)return true;
    in->buttons=T5_APP_BUTTON_BACK;return true;
}
const t5_app_api_v1*t5_app_get_api(uint32_t v){assert(v==1);return &app_api;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return &runtime_api;}
int main(int argc,char**argv) {
    scenario=argc>1?(unsigned)atoi(argv[1]):0;
    audio_api=(twatch_audio_out_api_v1){.api_version=1,.struct_size=sizeof(audio_api),.open=audio_open,.write=audio_write,.close=audio_close};
    if(scenario==13)audio_api.struct_size=8;
    runtime_api=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime_api),.acquire=acquire,.release=release,.diagnostic=diagnostic,.yield_ms=yield};
    app_api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(app_api),.poll=poll,.millis=millis,
        .screen_width=width,.screen_height=height,.clear=clear,.fill_rect=rect,.draw_text=text,.draw_label=label,.present=present};
    if(!setjmp(held))app_main();
    if(scenario==4||scenario==5){assert(retained&&live&&grants==1&&!releases&&closes==1);}
    else if(scenario==9){assert(retained&&!live&&grants==1&&releases==1);}
    else if(scenario==10){assert(!retained&&!live&&grants==1&&!releases&&closes==1);}
    else {
        assert(!retained&&!live&&!grants);
        if(scenario==0||scenario==6||scenario==13)assert(!opens&&!closes&&!writes);
        else assert(opens==1&&closes==1&&releases==1);
    }
    if(scenario==2||scenario==5)assert(!writes);
    if(scenario==3||scenario==4)assert(writes==1);
    if(scenario==7)assert(alarm_live&&writes==after_preempt_writes&&!playing);
    if(scenario==8)assert(!playing&&writes==1);
    if(scenario==11)assert(frequency==7000);
    if(scenario==12)assert(volume==0);
    if(scenario==14)assert(writes==1&&!playing);
    assert(frames);printf("frequency generator scenario %u passed\n",scenario);
}
