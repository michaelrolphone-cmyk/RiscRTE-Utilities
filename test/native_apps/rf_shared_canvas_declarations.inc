/* Host-only peripheral state. Included after the RF fixture declarations. */
#include "canvas_provider.h"
static canvas_provider sc_provider;
static risc_canvas_api_v1 sc_canvas_api;
static unsigned sc_w,sc_h,sc_format,sc_rotation,sc_touch_rotation,sc_insets[4];
static unsigned sc_input_mode,sc_brightness,sc_frames_checked,sc_gaps,sc_geometries;
static bool sc_cut;
static unsigned sc_contacts,sc_before_rejection_contacts;
static unsigned sc_before_resize_gaps,sc_before_resize_geometry;
static void sc_dimensions(unsigned *,unsigned *);
static void sc_report(risc_touch_snapshot_v1 *);
static bool sc_command(const char *);
static void sc_verify_raster(void);
static bool sc_info(void *,risc_display_info_v1 *);
static unsigned sc_pixel(unsigned,unsigned);
static void sc_guards(void);
static void sc_corners(void);
static bool sc_frame(void *,uint32_t,risc_display_surface_v1 *);
extern unsigned rf_host_canvas_contract(void);
