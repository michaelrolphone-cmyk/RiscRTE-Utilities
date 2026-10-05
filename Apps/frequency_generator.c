#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "AlarmOutputV1.h"
#include "tone_core.h"
#include "daily_draw.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const twatch_audio_out_api_v1 *output;
static risc_runtime_capability_v1 grant;
static tone_state tone;
static unsigned frequency,volume,step_index;
static const unsigned steps[]={10,100,1000};
static bool playing,owned,uncertain,dirty;
static uint32_t started;
static const char *message;

/* Shared adapter hooks. Own only streams opened by this app. Once suspended
 * for an alarm/sleep, cleanup must never touch the alarm's subsequent stream. */
bool portable_audio_services_safe(void) { return !uncertain; }
bool portable_audio_suspend(void) {
    if(uncertain)return false;
    if(!owned)return true;
    playing=false;dirty=true;
    if(!output->close(output->context)){uncertain=true;return false;}
    owned=false;message="Stopped - tap Start";return true;
}
static void retain(void) {
    runtime->diagnostic("AUDIO cleanup-unconfirmed; invocation retained");
    for(;;)runtime->yield_ms(50);
}
static bool acquire_output(void) {
    if(output)return true;
    if(grant.api)return false;
    grant=(risc_runtime_capability_v1){.struct_size=sizeof(grant)};
    if(!runtime->acquire("audio.output",1,0,&grant))return false;
    const twatch_audio_out_api_v1 *candidate=grant.api;
    if(!candidate||candidate->api_version!=1||candidate->struct_size<sizeof(*candidate)||
       !candidate->open||!candidate->write||!candidate->close)return false;
    output=candidate;return true;
}
static void stop(void) { if(!portable_audio_suspend())retain(); }
static void toggle(void) {
    if(playing){stop();return;}
    if(!acquire_output()){message="Audio unavailable";dirty=true;return;}
    tone=(tone_state){0};
    if(!tone_configure(&tone,frequency,volume))return;
    /* Even a false open can own a retained native cleanup token. */
    owned=true;
    if(!output->open(output->context,TONE_RATE,1)) {
        stop();message="Audio start failed";dirty=true;return;
    }
    playing=true;started=app->millis();message="Sine - stops after 60 s";dirty=true;
}
static void draw(void) {
    const int w=app->screen_width(),h=app->screen_height();char text[40];
    app->clear();app->draw_text(8,16,"BACK");app->draw_label(58,16,w-66,"FREQUENCY");
    snprintf(text,sizeof(text),"%u HZ",frequency);
    daily_draw_text(app,(w-daily_draw_width(text,3))/2,48,text,3);
    app->draw_label(6,83,w-12,"20-7000 HZ  16 KHZ PCM");
    app->fill_rect(8,94,w/2-14,34,true);app->fill_rect(w/2+6,94,w/2-14,34,true);
    app->fill_rect(10,96,w/2-18,30,false);app->fill_rect(w/2+8,96,w/2-18,30,false);
    snprintf(text,sizeof(text),"LESS %u HZ",steps[step_index]);app->draw_label(10,107,w/2-18,text);
    snprintf(text,sizeof(text),"MORE %u HZ",steps[step_index]);app->draw_label(w/2+8,107,w/2-18,text);
    for(unsigned i=0;i<3;i++) {
        int x=8+(w-16)*(int)i/3,cell=(w-16)/3;
        snprintf(text,sizeof(text),"%u",steps[i]);
        app->draw_label(x,144,cell,text);
        if(step_index==i)app->fill_rect(x+8,154,cell-16,2,true);
    }
    snprintf(text,sizeof(text),"LESS  LEVEL %u%%  MORE",volume);app->draw_label(8,165,w-16,text);
    app->draw_label(6,192,w-12,message?message:"");
    app->fill_rect(8,h-36,w-16,32,true);app->fill_rect(10,h-34,w-20,28,false);
    app->draw_label(10,h-24,w-20,playing?"STOP":"START");app->present(false);dirty=false;
}
static void adjust_frequency(int delta) {
    frequency=tone_adjust(frequency,delta,TONE_MIN_HZ,TONE_MAX_HZ);
    (void)tone_configure(&tone,frequency,volume);dirty=true;
}
void app_main(void) {
    app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);
    if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label)||
       !app->poll||!app->millis||!app->screen_width||!app->screen_height||!app->clear||
       !app->draw_text||!app->draw_label||!app->fill_rect||!app->present||
       !runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||
       !runtime->acquire||!runtime->release||!runtime->diagnostic||!runtime->yield_ms)return;
    if(app->screen_width()<240||app->screen_width()>1024||app->screen_height()<240||app->screen_height()>1024)return;
    output=NULL;grant=(risc_runtime_capability_v1){0};tone=(tone_state){0};
    frequency=440;volume=100;step_index=1;playing=owned=uncertain=false;dirty=true;message="100% level - tap Start";
    for(;;) {
        if(dirty)draw();
        t5_app_input_t input={0};
        if(!app->poll(&input,playing?0:30)) {
#ifdef PORTABLE_ALARM_CLIENT
            if(portable_app_sleep_retained())return;
#endif
            break;
        }
        if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK))break;
        bool toggle_requested=!!(input.buttons&T5_APP_BUTTON_CONFIRM);
        if(input.buttons&T5_APP_BUTTON_LEFT)adjust_frequency(-(int)steps[step_index]);
        if(input.buttons&T5_APP_BUTTON_RIGHT)adjust_frequency((int)steps[step_index]);
        if(input.tapped) {
            int x=input.touch_x,y=input.touch_y,w=app->screen_width(),h=app->screen_height();
            if(x>=8&&x<w-8&&y>=h-36&&y<h-4)toggle_requested=true;
            else if(x>=8&&x<w-8&&y>=94&&y<128)adjust_frequency((x<w/2?-1:1)*(int)steps[step_index]);
            else if(x>=8&&x<w-8&&y>=130&&y<155){step_index=(unsigned)((x-8)*3/(w-16));dirty=true;}
            else if(x>=8&&x<w-8&&y>=157&&y<180){volume=tone_adjust(volume,x<w/2?-1:1,0,TONE_MAX_PERCENT);(void)tone_configure(&tone,frequency,volume);dirty=true;}
        }
        /* Touch and navigation may arrive in one frame. Coalesce a paired
         * Stop instead of stopping and immediately starting the speaker. */
        if(toggle_requested)toggle();
        if(playing) {
            if((uint32_t)(app->millis()-started)>=60000u){stop();message="60 s limit - tap Start";continue;}
            int16_t pcm[TONE_FRAMES];
            if(!tone_generate(&tone,pcm,TONE_FRAMES)||!output->write(output->context,pcm,TONE_FRAMES)) {
                stop();message="Audio write failed";dirty=true;
            }
        }
    }
    stop();
    if(grant.api&&!runtime->release(&grant))retain();
    grant=(risc_runtime_capability_v1){0};output=NULL;
}
