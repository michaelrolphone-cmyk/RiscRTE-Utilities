/* Adapter-boundary fixture: runs prior real-provider/session suite too. */
#define main baseline_main
#define info physical_info
#include "rf_capability_canvas_test.c"
#undef info
#undef main
#include "PortableTouch.h"
#define PORTABLE_RF_SHARED_CANVAS
#define PORTABLE_QUICK_ACTIONS
#define PORTABLE_ALARM_CLIENT
static bool native_custody_retained;
static void portable_adapter_retain_silent(void){native_custody_retained=true;}
static const risc_runtime_api_v1 *rt;
static const risc_display_output_api_v1 *display;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static uint32_t surface_format;
static bool paper_rotated,alarm_pixels_valid,previous_valid,paper_previous_valid;
static bool quick_modal,quick_replay_pending,quick_replay_delivery,quick_launch_pending,alarm_modal;
static uint64_t paper_token;
static uint16_t *alarm_pixels,*previous_pixels;
static void *paper_previous,*quick_background;
#include "rf_capability_canvas_adapter.inc"
int main(void) {
 assert(baseline_main()==0);
 width=240;height=240;format=RISC_DISPLAY_FORMAT_RGB565;stride=480;
 pixels=calloc(1,640*640*2);assert(pixels);physical_state=0;polls=0;
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=physical_info,
 .acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider provider;canvas_provider_init(&provider,&d,NULL,0);
 owned_canvas_api=&provider.api;input_provider_owned=&provider;
 with_input=true;deny_release=false;change_owner=false;rt=&owner;
 assert(rf_adapter_canvas_open());
 portable_touch t={0};assert(rf_adapter_touch_open(&t,rt));
 assert(rf_adapter_navigation_open()->reset(NULL));
 risc_input_navigation_frame_v1 navigation;
 assert(rf_adapter_navigation_poll(NULL,&navigation));
 assert(rf_adapter_navigation_foreground(NULL,NULL,0));
 assert(rf_adapter_touch_close(&t,rt));
 assert(portable_sdr_canvas_refresh_geometry()&&info.width==240);
 alarm_pixels=malloc(240*240*2);quick_background=malloc(240*240*2);
 previous_pixels=malloc(240*240*2);paper_previous=malloc(240*240/8);
 assert(alarm_pixels&&quick_background&&previous_pixels&&paper_previous);
 void *saved_alarm=alarm_pixels,*saved_quick=quick_background;
 width=320;stride=640;
 paper_token=42;
 assert(portable_sdr_canvas_refresh_geometry()&&info.width==240);
 assert(alarm_pixels==saved_alarm&&quick_background==saved_quick);
 paper_token=0;quick_modal=true;
 assert(portable_sdr_canvas_refresh_geometry()&&info.width==240);
 assert(alarm_pixels==saved_alarm&&quick_background==saved_quick);
 quick_modal=false;alarm_modal=true;
 assert(portable_sdr_canvas_refresh_geometry()&&info.width==240);
 assert(alarm_pixels==saved_alarm&&quick_background==saved_quick);
 alarm_modal=false;
 assert(!rf_adapter_canvas_geometry()&&info.width==240);
 assert(portable_sdr_canvas_refresh_geometry()&&info.width==320);
 assert(!quick_background&&!previous_pixels&&!paper_previous&&!alarm_pixels_valid);
 assert(rf_adapter_canvas_geometry());
 assert(rf_canvas_close(&rf_canvas)==RF_CANVAS_READY);
 free(alarm_pixels);free(pixels);
 puts("RF pre-draw geometry and modal buffer custody PASS");
 return 0;
}
