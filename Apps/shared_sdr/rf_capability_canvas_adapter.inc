/* Include after native custody and adapter state, before input navigation. */
#ifdef PORTABLE_RF_SHARED_CANVAS
#include "rf_capability_canvas.h"
static rf_capability_canvas rf_canvas;
static bool rf_canvas_controls_requested;
static bool rf_adapter_canvas_safe(void *unused) {
 (void)unused;return !native_custody_retained;
}
static void rf_adapter_canvas_retain(void *unused) {
 (void)unused;portable_adapter_retain_silent();
}
static bool rf_adapter_canvas_open(void) {
 if(rf_canvas_open(&rf_canvas,rt,rf_adapter_canvas_safe,
                   rf_adapter_canvas_retain,NULL)!=RF_CANVAS_READY)return false;
 int controls=rf_canvas_controls_open(&rf_canvas);
 if(controls==RF_CANVAS_RETAINED||controls==RF_CANVAS_PENDING)return false;
 display=&rf_canvas.surface.display;
 surface_format=RISC_DISPLAY_FORMAT_RGB565;paper_rotated=false;
 return display->get_info(display->context,&info);
}
static bool rf_adapter_touch_open(portable_touch *t,const risc_runtime_api_v1 *r) {
 (void)r;
 int status=rf_canvas_input_open(&rf_canvas);
 if(status==RF_CANVAS_RETAINED||status==RF_CANVAS_PENDING)return false;
 /* Input unavailable is explicit in input_active, but does not close output.
  * An input-free canvas supplies an empty logical snapshot. */
 memset(t,0,sizeof(*t));t->api=&rf_canvas.touch.api;
 t->subscription=t->api->subscribe(t->api->context);
 return t->subscription&&portable_touch_sync(t,true);
}
static bool rf_adapter_touch_close(portable_touch *t,const risc_runtime_api_v1 *r) {
 return portable_touch_close(t,r); /* local facade has no raw-touch grant */
}
#define portable_touch_open rf_adapter_touch_open
#define portable_touch_close rf_adapter_touch_close
static bool rf_adapter_navigation_poll(void *unused,risc_input_navigation_frame_v1 *out) {
 (void)unused;if(!rf_canvas_safe(&rf_canvas))return false;
 bool controls=false;
 *out=home_canvas_touch_navigation(&rf_canvas.touch,&controls);
 rf_canvas_controls_requested|=controls;return true;
}
static bool rf_adapter_navigation_reset(void *unused) {
 (void)unused;if(!rf_canvas_safe(&rf_canvas))return false;
 home_canvas_touch_discard_navigation(&rf_canvas.touch);
 rf_canvas_controls_requested=false;return true;
}
static bool rf_adapter_navigation_foreground(void *unused,
 const risc_input_foreground_v1 *claims,size_t count) {
 (void)unused;(void)claims;(void)count;return rf_canvas_safe(&rf_canvas);
}
static const risc_input_navigation_api_v1 *rf_adapter_navigation_open(void) {
 static const risc_input_navigation_api_v1 api={
 .api_version=1,.struct_size=sizeof(api),.poll=rf_adapter_navigation_poll,
 .reset=rf_adapter_navigation_reset,.foreground=rf_adapter_navigation_foreground};
 return &api;
}
/* Commit new logical dimensions only before the RF controller lays out a
 * frame. Outstanding provider work and modal copies stay untouched. */
bool portable_sdr_canvas_refresh_geometry(void) {
 if(!rf_canvas_safe(&rf_canvas))return false;
 if(surface.frame||paper_token||rf_canvas.surface.lease||rf_canvas.surface.receipt)
  return true;
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_modal||quick_replay_pending||quick_replay_delivery||quick_launch_pending)
  return true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
 if(alarm_modal)return true;
#endif
 risc_display_info_v1 next={0};
 if(!display->get_info(display->context,&next)||next.width<160||next.height<160||
    next.width>1024||next.height>1024)return false;
 if(next.width==info.width&&next.height==info.height){info=next;return true;}
 size_t bytes=(size_t)next.width*2*next.height;
#ifdef PORTABLE_ALARM_CLIENT
 void *replacement=alarm_pixels?malloc(bytes):NULL;
 if(alarm_pixels&&!replacement)return false;
 free(alarm_pixels);alarm_pixels=replacement;alarm_pixels_valid=false;
#else
 (void)bytes;
#endif
 free(previous_pixels);previous_pixels=NULL;previous_valid=false;
 free(paper_previous);paper_previous=NULL;paper_previous_valid=false;
#ifdef PORTABLE_QUICK_ACTIONS
 free(quick_background);quick_background=NULL;
#endif
 info=next;paper_rotated=false;
 return true;
}
/* Never commit a resize after RF has chosen its layout. A provider may change
 * geometry between the pre-draw check and acquisition; defer that frame until
 * the next pre-draw refresh instead of painting stale coordinates. */
static bool rf_adapter_canvas_geometry(void) {
 if(!rf_canvas_safe(&rf_canvas)||surface.frame||paper_token||
    rf_canvas.surface.lease||rf_canvas.surface.receipt)return false;
 risc_display_info_v1 next={0};
 if(!display->get_info(display->context,&next))return false;
 return next.width==info.width&&next.height==info.height;
}
#endif
