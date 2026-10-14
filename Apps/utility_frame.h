#pragma once
#include "T5AppApi.h"
/* One latest-state request per controller, never a queue of raster snapshots.
 * New SDKs size-check the append-only callbacks; old SDKs remain synchronous. */
#if defined(__has_include)
# if __has_include("T5AppFrame.h")
#  include "T5AppFrame.h"
#  define UTILITY_FRAME_API 1
# endif
#endif
static bool utility_frame_pending;
static inline void utility_frame_reset(void) { utility_frame_pending=false; }
static inline bool utility_frame_begin(const t5_app_api_v1 *api) {
#ifdef UTILITY_FRAME_API
    if(!t5_app_frame_ready(api)){utility_frame_pending=true;return false;}
#else
    (void)api;
#endif
    utility_frame_pending=false;
    return true;
}
/* Ownership drains belong to the shared adapter's exit/sleep/modal boundary.
 * Do not call a drain in a render retry or delay input waiting for this flag. */

/* Match the Nova renderer using capability-reported physical dimensions.
 * A width-scaled native-height build uses the same scale on both axes. */
static inline bool utility_color_dimensions(const t5_app_api_v1 *api) {
    return api->screen_width()>=240 && api->screen_width()<=1024 &&
           api->screen_height()>=240 && api->screen_height()<=1024;
}
static inline int utility_color_x(const t5_app_api_v1 *api,int x) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    return x<0?-1:x*240/api->screen_width();
#else
    return x-(api->screen_width()-240)/2;
#endif
}
static inline int utility_color_y(const t5_app_api_v1 *api,int y) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    return y<0?-1:y*240/api->screen_width();
#else
    return y-(api->screen_height()-240)/2;
#endif
}
static inline void utility_color_input(const t5_app_api_v1 *api,t5_app_input_t *in) {
    if(!in->tapped)return;
    int x=utility_color_x(api,in->touch_x),y=utility_color_y(api,in->touch_y);
    if(x<0||y<0||x>=240||in->touch_x>=api->screen_width()||in->touch_y>=api->screen_height()){in->tapped=false;return;}
    in->touch_x=(uint16_t)x;in->touch_y=(uint16_t)y;
}
