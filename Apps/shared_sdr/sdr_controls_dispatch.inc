/* One copied logical Controls intent, consumed only at a settled safe point.
 * This is an intent transition, never a fabricated touch/contact gesture. */
static void sdr_controls_dispatch(void) {
 if(!rf_canvas_controls_requested || failed || native_custody_retained ||
    surface.frame || paper_token || !display_settled || alarm_modal ||
    handoff_requested)return;
 if(sdr_resident_active) {
  resident_controls_requested=true;rf_canvas_controls_requested=false;return;
 }
 if(quick_modal || quick_launch_pending)return;
 pqa_cancel_input(&quick.ui);
 quick.ui.position_q8=0;quick.ui.target_q8=pqa_open_q8(&quick.ui);quick.ui.velocity_q8=0;
 quick.ui.clock_valid=false;quick.ui.animation_remainder_ms=0;
 resident_neutral();
 rf_canvas_controls_requested=false;last_activity=millis_now();
}
