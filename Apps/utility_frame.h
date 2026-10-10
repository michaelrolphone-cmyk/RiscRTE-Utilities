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
