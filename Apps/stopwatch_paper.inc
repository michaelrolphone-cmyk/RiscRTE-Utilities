#include "utility_paper.h"
static void stopwatch_paper_draw(void) {
 up_begin("STOPWATCH");char value[12],large[9];sw_format(clock_state.elapsed_ms,value);memcpy(large,value,8);large[8]=0;
 up_center(210,large,true);up_center(310,value+8,false);
 up_text(32,386,416,clock_state.running?"Running":loaded?"Paused":"Storage error",false,true);
 up_wrap(434,status);
 up_text(32,538,416,"Display updates once per second",false,true);
 up_button(32,584,132,88,pending_pause?"Save":clock_state.running?"Pause":"Start",clock_state.running?"solid:f0c8":"solid:f017",false);
 up_button(174,584,132,88,reset_armed?"Confirm":"Reset","solid:f021",reset_armed);
 up_button(316,584,132,88,"Retry","solid:f021",false);
 up_button(32,688,416,88,"Back","solid:f053",false);app->present(false);
}
