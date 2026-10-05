#define PORTABLE_ALARM_CLIENT
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/audio_spectrum.c"

typedef enum { EVENT_INPUT, EVENT_END, EVENT_SUSPEND, EVENT_RETAIN, EVENT_FRAME } event_kind;
typedef struct { event_kind kind; t5_app_input_t input; int running_before; const char *save; } event;
static event events[1200];
static unsigned count,index_event,opens,reads,closes,acquires,releases,polls,frames,yields;
static int width,height;
static uint32_t now;
static bool deny_acquire,fail_open,fail_read,fail_close,fail_release,retained,present_failure,ui_failed,live,grant_live,escaped;
static size_t read_count;
static int malformed;
static t5_app_api_v1 api;
static risc_runtime_api_v1 rt;
static twatch_audio_in_api_v1 mic;
static jmp_buf retained_jump;
static uint8_t pixels[1024*1024];
static const char *output_directory;
static int32_t screen_width(void) { return width; }
static int32_t screen_height(void) { return height; }
static void clear(void) { memset(pixels,255,sizeof(pixels)); }
static void rect(int32_t x,int32_t y,int32_t w,int32_t h,bool black) {
    assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=width&&y+h<=height);
    for(int row=y;row<y+h;++row)memset(pixels+row*width+x,black?0:255,(size_t)w);
}
static void tone_rect(int32_t x,int32_t y,int32_t w,int32_t h,int32_t radius,uint8_t tone) {
    assert(!radius && tone<=3);rect(x,y,w,h,false);
    for(int row=y;row<y+h;++row)memset(pixels+row*width+x,(3-tone)*85,(size_t)w);
}
static void save_frame(const char *name) {
    if(!output_directory)return;
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s.pgm",output_directory,name)>0);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P5\n%d %d\n255\n",width,height);assert(fwrite(pixels,1,(size_t)width*height,f)==(size_t)width*height);assert(!fclose(f));
}
static void present(bool full) { assert(!full);++frames;if(present_failure)ui_failed=true; }
static uint32_t millis(void) { return now; }
static bool open_mic(void *context,uint32_t rate) {
    assert(context==&mic && rate==16000 && grant_live && !live);++opens;live=true;return !fail_open;
}
static bool read_mic(void *context,int16_t *pcm,size_t capacity,size_t *got) {
    assert(context==&mic && live && grant_live && !uncertain && running && capacity==256 && pcm && got);++reads;now+=16;
    size_t n=read_count>256?256:read_count;
    for(size_t i=0;i<n;++i)pcm[i]=(int16_t)(spectrum_sin((unsigned)(i+reads*256)*16)*3/4);
    *got=read_count;return !fail_read;
}
static bool close_mic(void *context) { assert(context==&mic && live && grant_live && !uncertain);++closes;if(fail_close)return false;live=false;return true; }
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
    assert(!strcmp(name,"audio.input") && version==1 && instance==0 && out->struct_size==sizeof(*out) && !grant_live);++acquires;
    if(deny_acquire)return false;
    grant_live=true;out->api=malformed==4?NULL:&mic;return true;
}
static bool release(risc_runtime_capability_v1 *in) { assert(in==&grant && grant_live && !live && !uncertain);++releases;if(fail_release)return false;grant_live=false;return true; }
static bool diagnostic(const char *s) { assert(strstr(s,"retained")); return true; }
static void yield_ms(uint32_t ms) { assert(ms==50);++yields;assert(fail_close||fail_release);longjmp(retained_jump,1); }
bool portable_app_sleep_retained(void) { return retained; }
static bool poll(t5_app_input_t *out,uint32_t wait) {
    assert(wait==(running?1u:30u));++polls;now+=wait;
    if(ui_failed)return false;
    assert(index_event<count);event next=events[index_event++];
    if(next.running_before>=0)assert(running==(next.running_before!=0));
    if(next.save)save_frame(next.save);
    if(next.kind==EVENT_END)return false;
    if(next.kind==EVENT_SUSPEND||next.kind==EVENT_RETAIN) {
        bool ok=portable_audio_suspend();assert(ok);assert(!running&&!owned);
        if(next.kind==EVENT_RETAIN){retained=true;return false;}
    }
    *out=next.input;return true;
}
const t5_app_api_v1 *t5_app_get_api(uint32_t version) { assert(version==1);return &api; }
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) { assert(version==1);return &rt; }
static void reset(void) {
    count=index_event=opens=reads=closes=acquires=releases=polls=frames=yields=0;width=height=240;now=0;
    deny_acquire=fail_open=fail_read=fail_close=fail_release=retained=present_failure=ui_failed=live=grant_live=escaped=false;
    read_count=256;malformed=0;
    api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(api),.screen_width=screen_width,.screen_height=screen_height,.clear=clear,.fill_rect=rect,.present=present,.poll=poll,.millis=millis,.fill_rounded_rect_tone=tone_rect};
    rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.yield_ms=yield_ms,.diagnostic=diagnostic};
    mic=(twatch_audio_in_api_v1){.api_version=1,.struct_size=sizeof(mic),.context=&mic,.open=open_mic,.read=read_mic,.close=close_mic};
}
static void add(event_kind kind,uint32_t buttons,int running_before) { assert(count<1200);events[count++]=(event){.kind=kind,.input={.buttons=buttons},.running_before=running_before}; }
static void tap(int x,int y,int running_before) { add(EVENT_INPUT,0,running_before);events[count-1].input=(t5_app_input_t){.tapped=true,.touch_x=(int16_t)x,.touch_y=(int16_t)y}; }
static void start(void) { add(EVENT_INPUT,T5_APP_BUTTON_CONFIRM,0); }
static void back(void) { add(EVENT_INPUT,T5_APP_BUTTON_BACK,-1); }
static void run(void) { if(!setjmp(retained_jump))app_main();else escaped=true; }
static void clean(unsigned expected_opens,unsigned expected_reads,unsigned expected_closes) {
    assert(!escaped&&!live&&!grant_live&&!owned&&!running&&!uncertain);
    assert(opens==expected_opens&&reads==expected_reads&&closes==expected_closes);
    assert(acquires==releases);
}
int main(int argc,char **argv) {
    output_directory=argc>1?argv[1]:NULL;
    reset();back();run();clean(0,0,0);assert(acquires==0);save_frame("spectrum-idle");
    reset();start();back();run();clean(1,1,1);
    reset();start();add(EVENT_INPUT,T5_APP_BUTTON_CONFIRM,1);back();run();clean(1,1,1);assert(!strcmp(message,"STOPPED / MIC OFF"));
    reset();start();tap(40,220,1);events[count-1].input.buttons=T5_APP_BUTTON_CONFIRM;back();run();clean(1,1,1); /* Combined Stop executes once. */
    reset();start();tap(180,220,1);events[count-1].input.buttons=T5_APP_BUTTON_CONFIRM;back();run();clean(1,1,1);assert(frozen); /* Freeze wins over Start. */
    reset();start();tap(180,220,1);add(EVENT_INPUT,0,0);back();run();clean(1,1,1);assert(frozen);save_frame("spectrum-frozen");
    reset();start();add(EVENT_INPUT,T5_APP_BUTTON_DOWN,1);start();back();run();clean(2,2,2);
    reset();tap(40,220,0);tap(180,30,1);for(unsigned i=0;i<450;++i)add(EVENT_INPUT,0,1);tap(180,220,1);back();run();clean(1,452,1);assert(spectrogram&&spectrum.rows==64);save_frame("spectrogram-full");
    reset();start();for(unsigned i=0;i<12;++i)add(EVENT_INPUT,0,1);add(EVENT_INPUT,T5_APP_BUTTON_DOWN,1);back();run();clean(1,13,1);save_frame("spectrum-tone");
    reset();api.fill_rounded_rect_tone=NULL;start();tap(180,30,1);for(unsigned i=0;i<12;++i)add(EVENT_INPUT,0,1);back();run();clean(1,14,1);save_frame("spectrogram-fallback");
    reset();read_count=17;start();for(unsigned i=0;i<15;++i)add(EVENT_INPUT,0,1);back();run();clean(1,16,1);assert(spectrum.transforms==1);
    reset();read_count=0;start();for(unsigned i=0;i<7;++i)add(EVENT_INPUT,0,1);add(EVENT_INPUT,0,0);back();run();clean(1,8,1);assert(!strcmp(message,"NO MICROPHONE DATA"));
    reset();read_count=257;start();back();run();clean(1,1,1);assert(spectrum.transforms==0&&!strcmp(message,"MIC READ FAILED"));
    reset();read_count=SIZE_MAX;start();back();run();clean(1,1,1);
    reset();read_count=17;fail_read=true;start();back();run();clean(1,1,1);assert(spectrum.used==0&&spectrum.transforms==0);
    reset();fail_open=true;start();back();run();clean(1,0,1);assert(!strcmp(message,"MIC START FAILED"));
    reset();deny_acquire=true;start();back();run();assert(opens==0&&closes==0&&releases==0&&acquires==1);
    for(int fault=1;fault<=4;++fault) {
        reset();malformed=fault;if(fault==1)mic.api_version=2;if(fault==2)mic.struct_size=4;if(fault==3)mic.read=NULL;
        start();start();back();run();clean(0,0,0);assert(acquires==1&&releases==1);
    }
    reset();start();add(EVENT_END,0,1);run();clean(1,1,1); /* UI failure closes before return. */
    reset();present_failure=true;start();run();clean(0,0,0); /* First frame failure never starts capture. */
    reset();start();add(EVENT_SUSPEND,0,1);add(EVENT_INPUT,0,0);add(EVENT_INPUT,0,0);back();run();clean(1,1,1); /* Alarm/sleep stop, no resume. */
    reset();start();add(EVENT_SUSPEND,0,1);start();back();run();clean(2,2,2); /* Only explicit new Start. */
    reset();start();add(EVENT_RETAIN,0,1);run();assert(!escaped&&!live&&grant_live&&acquires==1&&releases==0&&closes==1&&reads==1); /* Native sleep owns unwind. */
    reset();add(EVENT_RETAIN,0,0);run();assert(!acquires&&!closes&&!releases);
    reset();fail_close=true;start();back();run();assert(escaped&&uncertain&&live&&grant_live&&closes==1&&reads==1&&releases==0&&yields==1);assert(!portable_audio_suspend()&&closes==1&&!portable_audio_services_safe());
    reset();fail_open=fail_close=true;start();run();assert(escaped&&closes==1&&reads==0&&releases==0);
    reset();fail_read=fail_close=true;start();run();assert(escaped&&closes==1&&reads==1&&releases==0);
    reset();fail_release=true;start();back();run();assert(escaped&&!live&&grant_live&&releases==1&&closes==1&&yields==1);
    reset();tap(-1,220,0);tap(240,220,0);tap(60,-1,0);tap(60,240,0);tap(120,220,0);tap(20,43,0);back();run();clean(0,0,0);
    reset();start();tap(20,10,1);run();clean(1,1,1);
    const int sizes[][2]={{240,320},{320,240},{480,480},{1024,1024}};
    for(unsigned i=0;i<4;++i){reset();width=sizes[i][0];height=sizes[i][1];start();tap(width-40,30,1);back();run();clean(1,2,1);}
    reset();width=239;run();assert(!polls&&!acquires);
    reset();height=1025;run();assert(!polls&&!acquires);
    reset();api.poll=NULL;run();assert(!polls&&!acquires);
    reset();rt.release=NULL;run();assert(!polls&&!acquires);
    reset();api.struct_size=4;run();assert(!polls&&!acquires);
    puts("Spectrum app: user-start, modes, frozen state, partial/empty/invalid PCM, API/open/read/close/release faults, alarm/sleep retention and bounded UI passed");
    return 0;
}
