/* Full production app loop: stalled presentation cannot drop PCM/FFT/history. */
#define main original_spectrum_test_main
#include "audio_spectrum_test.c"
#undef main
static bool available;
static unsigned checked,history_before,transforms_before;
static bool ready(void){checked++;return available;}
static void check_busy(void){assert(!frames&&dirty&&utility_frame_pending&&running&&reads>90&&spectrum.transforms>0&&history_count>0);history_before=history_count;transforms_before=spectrum.transforms;}
static void check_logical_scroll(void){
 assert(!frames&&page==PAGE_CONTROLS&&controls_scroll==CONTROLS_ROW_HEIGHT*2);
 /* Third row at the current origin selects frequency scaling, even though
  * no controls frame has ever completed. */
 bool toggle=false,freeze=false;tap_action(130,CONTROLS_TOP+20,&toggle,&freeze);
 assert(prefs.log_frequency&&!frames);available=true;
}
static void latest_present(bool full){assert(available&&page==PAGE_CONTROLS&&controls_scroll==CONTROLS_ROW_HEIGHT*2);present(full);available=false;}
static void check_latest(void){assert(frames==1&&page==PAGE_CONTROLS&&utility_frame_pending);assert(spectrum.transforms>=transforms_before&&history_count>=history_before);}
int main(void){
 reset();available=false;api.frame_ready=ready;api.present=latest_present;start();
 for(unsigned i=0;i<110;i++)add(EVENT_INPUT,0,-1);
 check(check_busy);tap(225,26,-1);add(EVENT_INPUT,T5_APP_BUTTON_DOWN,-1);add(EVENT_INPUT,T5_APP_BUTTON_DOWN,-1);check(check_logical_scroll);
 /* The regular visual cadence may defer the newly available lease. */
 for(unsigned i=0;i<100;i++)add(EVENT_INPUT,0,-1);
 check(check_latest);back();back();run();
 assert(!live&&!grant_live&&!store_live&&checked>10);
 puts("PASS Spectrum full loop: busy display preserves PCM/FFT/history; logical scroll; latest-only redraw");return 0;
}
