#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "AudioInputV1.h"
#include "spectrum_core.h"
#include "daily_draw.h"
#include <stddef.h>
#include <stdio.h>
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif

static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const twatch_audio_in_api_v1 *microphone;
static risc_runtime_capability_v1 grant;
static spectrum_state spectrum;
static bool acquired, owned, uncertain, running, frozen, spectrogram, dirty;
static uint32_t rendered, recorded_transform;
static unsigned empty_reads;
static const char *message;

bool portable_audio_services_safe(void) { return !uncertain; }
bool portable_audio_suspend(void) {
    if (uncertain) return false;
    running=false; spectrum.used=0;
    if (!owned) return true;
    dirty=true;
    if (!microphone->close(microphone->context)) { uncertain=true; return false; }
    owned=false; message="STOPPED / MIC OFF";
    return true;
}
static void retain(void) {
    runtime->diagnostic("AUDIO input cleanup-unconfirmed; invocation retained");
    for (;;) runtime->yield_ms(50);
}
static void stop(void) { if (!portable_audio_suspend()) retain(); }
static bool acquire_microphone(void) {
    if (microphone) return true;
    if (acquired) return false;
    grant=(risc_runtime_capability_v1){.struct_size=sizeof(grant)};
    if (!runtime->acquire("audio.input",1,0,&grant)) return false;
    acquired=true;
    const twatch_audio_in_api_v1 *candidate=grant.api;
    if (!candidate || candidate->api_version!=1 || candidate->struct_size<sizeof(*candidate) ||
        !candidate->open || !candidate->read || !candidate->close) return false;
    microphone=candidate;
    return true;
}
static void toggle(void) {
    if (running) { stop(); frozen=false; return; }
    if (!acquire_microphone()) { message="MIC UNAVAILABLE"; dirty=true; return; }
    /* Only this attempt establishes cleanup ownership. Even a false open can
     * carry a retained native cleanup token, which must be closed or retained. */
    owned=true;
    if (!microphone->open(microphone->context,SPECTRUM_RATE)) {
        stop(); message="MIC START FAILED"; dirty=true; return;
    }
    memset(&spectrum,0,sizeof(spectrum));
    running=true; frozen=false; empty_reads=0; rendered=app->millis();
    recorded_transform=0; message="LIVE / 16 KHZ MONO"; dirty=true;
}
static void freeze(void) {
    stop(); frozen=true; message="FROZEN / MIC OFF"; dirty=true;
}
static void text_center(int x,int y,int width,const char *text) {
    daily_draw_text(app,x+(width-daily_draw_width(text,1))/2,y,text,1);
}
static void shade(int x,int y,uint8_t intensity) {
    /* Three visible shades on the shared RGB565 renderer. The 2x2 fallback
     * remains portable to older adapters without the optional tone primitive. */
    uint8_t tone=intensity ? (intensity<5?1:intensity<9?2:3) : 0;
    if (!tone) return;
    if (app->struct_size>=offsetof(t5_app_api_v1,fill_rounded_rect_tone)+sizeof(app->fill_rounded_rect_tone) &&
        app->fill_rounded_rect_tone) app->fill_rounded_rect_tone(x,y,2,2,0,tone);
    else if (tone==3) app->fill_rect(x,y,2,2,true);
    else {
        app->fill_rect(x,y,1,1,true);
        if (tone==2) app->fill_rect(x+1,y+1,1,1,true);
    }
}
static void draw(void) {
    int w=app->screen_width(),h=app->screen_height(),left=(w-224)/2;
    app->clear(); daily_draw_text(app,8,8,"BACK",1); text_center(52,8,w-60,"AUDIO SPECTRUM");
    /* The shared adapter reserves x<56,y<40 for Back. Keep every tab glyph
     * and its selection marker outside that chrome hit region. */
    text_center(56,29,w/2-60,"SPECTRUM"); text_center(w/2+4,29,w/2-12,"SPECTROGRAM");
    app->fill_rect(spectrogram?w/2+4:56,40,spectrogram?w/2-12:w/2-60,2,true);
    if (spectrogram) {
        for (unsigned age=0; age<SPECTRUM_ROWS; ++age) {
            const uint8_t *row=spectrum_history(&spectrum,age);
            if (!row) break;
            for (unsigned bin=0; bin<SPECTRUM_BINS; ++bin)
                shade(left+(int)bin*2,48+(int)age*2,row[bin]);
        }
    } else {
        for (unsigned bin=0; bin<SPECTRUM_BINS; ++bin) {
            int height=(int)spectrum_intensity(spectrum.magnitude[bin])*8;
            if (height) app->fill_rect(left+(int)bin*2,176-height,1,height,true);
        }
    }
    app->fill_rect(left,176,224,1,true);
    daily_draw_text(app,left,181,"0",1); text_center(left+82,181,60,"4 KHZ");
    daily_draw_text(app,left+195,181,"8 KHZ",1);
    text_center(8,194,w-16,message);
    app->fill_rect(8,h-34,w/2-12,30,true); app->fill_rect(10,h-32,w/2-16,26,false);
    app->fill_rect(w/2+4,h-34,w/2-12,30,true); app->fill_rect(w/2+6,h-32,w/2-16,26,false);
    text_center(10,h-23,w/2-16,running?"STOP":"START");
    text_center(w/2+6,h-23,w/2-16,frozen?"FROZEN":"FREEZE");
    app->present(false); dirty=false;
}
static void capture(void) {
    int16_t pcm[SPECTRUM_FRAMES]; size_t got=0;
    bool ok=microphone->read(microphone->context,pcm,SPECTRUM_FRAMES,&got);
    if (!ok || got>SPECTRUM_FRAMES || !spectrum_feed(&spectrum,pcm,got)) {
        stop(); message="MIC READ FAILED"; dirty=true; return;
    }
    if (!got) {
        if (++empty_reads>=8) { stop(); message="NO MICROPHONE DATA"; dirty=true; }
        return;
    }
    empty_reads=0;
    uint32_t now=app->millis();
    if (spectrum.transforms!=recorded_transform && (uint32_t)(now-rendered)>=SPECTRUM_INTERVAL_MS) {
        spectrum_record(&spectrum); recorded_transform=spectrum.transforms; rendered=now; dirty=true;
    }
}
void app_main(void) {
    app=t5_app_get_api(1); runtime=risc_runtime_get_api(1);
    if (!app || app->abi_version!=1 || app->struct_size<offsetof(t5_app_api_v1,millis)+sizeof(app->millis) ||
        !app->poll || !app->millis || !app->screen_width || !app->screen_height ||
        !app->clear || !app->fill_rect || !app->present || !runtime || runtime->api_version!=1 ||
        runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !runtime->acquire ||
        !runtime->release || !runtime->diagnostic || !runtime->yield_ms) return;
    if (app->screen_width()<240 || app->screen_width()>1024 || app->screen_height()<240 || app->screen_height()>1024) return;
    microphone=NULL; grant=(risc_runtime_capability_v1){0}; memset(&spectrum,0,sizeof(spectrum));
    acquired=owned=uncertain=running=frozen=spectrogram=false; dirty=true;
    message="PRESS START / MIC OFF"; rendered=0; empty_reads=0;
    for (;;) {
        if (dirty) draw();
        t5_app_input_t input={0};
        /* Explicit yield even for a provider that returns an immediate short
         * or empty read. Provider blocks at most 40 ms, then UI is polled. */
        if (!app->poll(&input,running?1:30)) {
#ifdef PORTABLE_ALARM_CLIENT
            if (portable_app_sleep_retained()) return;
#endif
            break;
        }
        if (input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK)) break;
        bool toggle_requested=!!(input.buttons&T5_APP_BUTTON_CONFIRM);
        bool freeze_requested=!!(input.buttons&T5_APP_BUTTON_DOWN);
        if (input.buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT)) { spectrogram=!spectrogram; dirty=true; }
        if (input.tapped) {
            int x=input.touch_x,y=input.touch_y,w=app->screen_width(),h=app->screen_height();
            if (x>=8 && x<50 && y>=4 && y<23) break;
            if (x>=56 && x<w-8 && y>=24 && y<44) { spectrogram=x>=w/2; dirty=true; }
            else if (y>=h-34 && y<h-4) {
                if (x>=8 && x<w/2-4) toggle_requested=true;
                else if (x>=w/2+4 && x<w-8) freeze_requested=true;
            }
        }
        /* Paired navigation/touch events are one action. Freeze always wins
         * over Start so a simultaneous stop cannot re-open the microphone. */
        if (freeze_requested) freeze();
        else if (toggle_requested) toggle();
        if (running) capture();
    }
    /* Must precede returning control to a modal/UI failure/fini path. */
    stop();
    if (acquired && !runtime->release(&grant)) retain();
    acquired=false; grant=(risc_runtime_capability_v1){0}; microphone=NULL;
}
