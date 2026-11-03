#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "PortableQuickActions.h"
static struct {pqa_state ui;} quick;
static struct {uint64_t frame;} surface;
static uint64_t paper_token;
static bool rf_canvas_controls_requested,failed,native_custody_retained,display_settled;
static bool alarm_modal,handoff_requested,sdr_resident_active,resident_controls_requested;
static bool quick_modal,quick_launch_pending;
static uint32_t last_activity;
static unsigned neutrals;
static uint32_t millis_now(void){return 123;}
static void resident_neutral(void){neutrals++;}
#include "sdr_controls_dispatch.inc"
static void reset(void){
 pqa_init(&quick.ui);surface.frame=paper_token=0;
 rf_canvas_controls_requested=true;failed=native_custody_retained=false;
 display_settled=true;alarm_modal=handoff_requested=sdr_resident_active=false;
 resident_controls_requested=quick_modal=quick_launch_pending=false;
 neutrals=last_activity=0;
}
int main(void){
 reset();surface.frame=1;sdr_controls_dispatch();assert(rf_canvas_controls_requested&&!neutrals);
 reset();paper_token=1;sdr_controls_dispatch();assert(rf_canvas_controls_requested&&!neutrals);
 reset();display_settled=false;sdr_controls_dispatch();assert(rf_canvas_controls_requested);
 reset();alarm_modal=true;sdr_controls_dispatch();assert(rf_canvas_controls_requested);
 reset();native_custody_retained=true;sdr_controls_dispatch();assert(rf_canvas_controls_requested&&!neutrals);
 reset();handoff_requested=true;sdr_controls_dispatch();assert(rf_canvas_controls_requested);
 reset();sdr_resident_active=true;sdr_controls_dispatch();
 assert(resident_controls_requested&&!rf_canvas_controls_requested&&!quick.ui.target_q8&&!neutrals);
 reset();quick_modal=true;sdr_controls_dispatch();assert(rf_canvas_controls_requested);
 reset();quick_launch_pending=true;sdr_controls_dispatch();assert(rf_canvas_controls_requested);
 reset();assert(pqa_set_height(&quick.ui,502));sdr_controls_dispatch();
 assert(!rf_canvas_controls_requested&&quick.ui.target_q8==502*256&&quick.ui.neutral_gate);
 assert(neutrals==1&&last_activity==123&&!resident_controls_requested);
 sdr_controls_dispatch();assert(neutrals==1);
 puts("SDR logical Controls intent: deferred custody, resident/local routing and one-shot neutralization PASS");
 return 0;
}
