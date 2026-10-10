#pragma once
#include "T5VideoApi.h"
#define DISPLAY_CLIENT_MONO1 T5_VIDEO_PIXEL_MONO_1BPP_MSB
#define DISPLAY_CLIENT_ONE_IS_BLACK T5_VIDEO_FLAG_ONE_IS_BLACK
typedef t5_video_surface_v1 display_client_surface;
typedef t5_video_api_v1 display_client_api;
static inline const display_client_api *display_client_get_api(void){return t5_video_get_api(1);}
