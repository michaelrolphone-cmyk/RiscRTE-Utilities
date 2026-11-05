#pragma once
/* Utilities-owned session composition. Requires System-Apps
 * 710b200021b983ea55eb86a6fbddbbf5ce10a984 canvas headers.
 * Logical RGB565 content is adaptive on mono providers. No physical geometry,
 * raw-touch grant, brightness policy, RF service, or saved-data policy here. */
#include "RiscRuntimeV1.h"
#include "PortableHomeCanvas.h"
#include "PortableHomeCanvasTouch.h"
#include "RiscCanvasInputV1.h"
enum { RF_CANVAS_UNAVAILABLE=0, RF_CANVAS_READY=1,
       RF_CANVAS_PENDING=-1, RF_CANVAS_RETAINED=-2 };
typedef struct {
 const risc_runtime_api_v1 *owner,*identity;
 home_canvas_surface surface;
 home_canvas_touch touch;
 risc_runtime_capability_v1 canvas_grant,input_grant,control_grant;
 const risc_canvas_input_v1 *input;
 const risc_display_output_api_v1 *control;
 bool canvas_owned,input_owned,control_owned,input_active,input_prepared,input_paused,retained;
 bool (*safe)(void*);
 void (*retain)(void*);
 void *context;
} rf_capability_canvas;
static inline void rf_canvas_retain(rf_capability_canvas *c) {
 if(c->retained)return;
 c->retained=true;
 if(c->retain)c->retain(c->context);
}
static inline bool rf_canvas_safe(void *context) {
 rf_capability_canvas *c=context;
 if(c->retained)return false;
 if(!c->owner || risc_runtime_get_api(1)!=c->identity ||
    (c->safe&&!c->safe(c->context))) {
  rf_canvas_retain(c);return false;
 }
 return true;
}
static inline void rf_canvas_surface_retained(void *context) {
 rf_canvas_retain(context);
}
static inline int rf_canvas_result(rf_capability_canvas *c,int32_t rc) {
 if(!home_canvas_result(&c->surface,rc)) {
  if(c->surface.retained||c->retained)return RF_CANVAS_RETAINED;
  return rc==RISC_CANVAS_AGAIN||rc==RISC_CANVAS_BUSY?
      RF_CANVAS_PENDING:RF_CANVAS_UNAVAILABLE;
 }
 return RF_CANVAS_READY;
}
static inline bool rf_canvas_empty_grant(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation;
}
static inline int rf_canvas_acquire(rf_capability_canvas *c,const char *name,
 risc_runtime_capability_v1 *g,bool *owned) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=c->owner->acquire(name,1,0,g);
 if(ok)*owned=true; /* Account custody before checking post-call authority. */
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(!ok&&!rf_canvas_empty_grant(g)){rf_canvas_retain(c);return RF_CANVAS_RETAINED;}
 return ok?RF_CANVAS_READY:RF_CANVAS_UNAVAILABLE;
}
static inline int rf_canvas_release(rf_capability_canvas *c,
 risc_runtime_capability_v1 *g,bool *owned) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(!*owned)return RF_CANVAS_READY;
 if(!c->owner->release(g)){rf_canvas_retain(c);return RF_CANVAS_RETAINED;}
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 *owned=false;*g=(risc_runtime_capability_v1){0};return RF_CANVAS_READY;
}
/* Zero-initialize a fresh invocation. Never reset an open/retained object. */
static inline int rf_canvas_open(rf_capability_canvas *c,
 const risc_runtime_api_v1 *runtime,bool (*safe)(void*),
 void (*retain)(void*),void *context) {
 if(!c||c->retained||c->canvas_owned||c->input_owned||c->control_owned||c->surface.session)
  return RF_CANVAS_UNAVAILABLE;
 if(!runtime||runtime->api_version!=1||
    runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||
    !runtime->acquire||!runtime->release)return RF_CANVAS_UNAVAILABLE;
 c->owner=runtime;c->identity=risc_runtime_get_api(1);c->safe=safe;c->retain=retain;c->context=context;
 int rc=rf_canvas_acquire(c,RISC_CANVAS_RASTER_CAPABILITY,
                         &c->canvas_grant,&c->canvas_owned);
 if(rc!=RF_CANVAS_READY)return rc;
 const risc_canvas_api_v1 *api=c->canvas_grant.api;
 api=api?risc_canvas_get_v1(api,api->struct_size):NULL;
 if(!api)return RF_CANVAS_UNAVAILABLE; /* close still owns the grant */
 if(!home_canvas_open_content_guarded(&c->surface,api,
     RISC_DISPLAY_FORMAT_RGB565,false,NULL,NULL,
     rf_canvas_surface_retained,c,rf_canvas_safe,c))
  return c->retained?RF_CANVAS_RETAINED:RF_CANVAS_UNAVAILABLE;
 home_canvas_touch_init(&c->touch,&c->surface);
 return RF_CANVAS_READY;
}
/* Optional control-only output. No physical raster methods are invoked, and
 * no physical dimensions/format are consumed. Only declared brightness support
 * is inspected. Logical raster/input geometry stays authoritative. */
static inline bool rf_canvas_control_brightness(void *context,uint16_t value,uint16_t maximum) {
 rf_capability_canvas *c=context;
 if(!rf_canvas_safe(c)||!c->control||!c->control->set_brightness)return false;
 if(!maximum||value>maximum)return false;
 bool ok=c->control->set_brightness(c->control->context,value,maximum);
 if(!rf_canvas_safe(c))return false;
 if(!ok){rf_canvas_retain(c);return false;}
 return true;
}
static inline int rf_canvas_controls_open(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(!c->surface.session)return RF_CANVAS_UNAVAILABLE;
 if(c->surface.brightness)return RF_CANVAS_READY;
 if(!c->control_owned) {
  int rc=rf_canvas_acquire(c,"display.output",&c->control_grant,&c->control_owned);
  if(rc!=RF_CANVAS_READY)return rc;
 }
 const risc_display_output_api_v1 *api=c->control_grant.api;
 if(!api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->get_info) {
  rf_canvas_retain(c);return RF_CANVAS_RETAINED;
 }
 c->control=api;
 if(!api->set_brightness)return RF_CANVAS_UNAVAILABLE;
 risc_display_info_v1 capability={0};
 bool read=api->get_info(api->context,&capability);
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(!read){rf_canvas_retain(c);return RF_CANVAS_RETAINED;}
 if(!(capability.flags&RISC_DISPLAY_INFO_BRIGHTNESS))return RF_CANVAS_UNAVAILABLE;
 c->surface.brightness=rf_canvas_control_brightness;c->surface.brightness_context=c;
 return RF_CANVAS_READY;
}
/* Optional logical input companion. A button-only provider is valid.
 * Consumers use touch.api plus ordered home_canvas_touch_navigation().
 * No app-owned raw touch or independent navigation subscriptions are created. */
static inline int rf_canvas_input_open(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(!c->surface.session)return RF_CANVAS_UNAVAILABLE;
 if(c->input_active)return RF_CANVAS_READY;
 if(!c->input_owned) {
  int rc=rf_canvas_acquire(c,RISC_CANVAS_INPUT_CAPABILITY,
                          &c->input_grant,&c->input_owned);
  if(rc!=RF_CANVAS_READY)return rc;
  const risc_canvas_input_v1 *api=c->input_grant.api;
  c->input=api?risc_canvas_input_get_v1(api,api->struct_size):NULL;
 }
 if(!c->input)return RF_CANVAS_UNAVAILABLE;
 int rc=rf_canvas_result(c,c->input->activate(c->input->context,c->surface.session));
 if(rc==RF_CANVAS_READY)c->input_active=true;
 return rc;
}
/* Resident/modal pause is distinct from power sleep. Caller closes its local
 * touch facade first. Grant/session identity remains pinned; physical input
 * dependencies are deactivated, with no prepare_sleep or native power call. */
static inline int rf_canvas_pause(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(c->surface.lease||c->surface.receipt||c->touch.subscribed||c->input_prepared)
  return RF_CANVAS_PENDING;
 if(!home_canvas_quiesce(&c->surface))
  return c->retained?RF_CANVAS_RETAINED:RF_CANVAS_PENDING;
 if(c->input_active) {
  int rc=rf_canvas_result(c,c->input->deactivate(c->input->context,c->surface.session));
  if(rc!=RF_CANVAS_READY)return rc;
  c->input_active=false;c->input_paused=true;
 }
 return RF_CANVAS_READY;
}
static inline int rf_canvas_resume(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(c->input_prepared)return RF_CANVAS_PENDING; /* use the power-sleep path */
 if(!home_canvas_resume(&c->surface))
  return c->retained?RF_CANVAS_RETAINED:RF_CANVAS_PENDING;
 if(c->input_paused) {
  int rc=rf_canvas_input_open(c);
  if(rc!=RF_CANVAS_READY)return rc;
  c->input_paused=false;
 }
 return RF_CANVAS_READY;
}
static inline int rf_canvas_prepare_sleep(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(c->surface.lease||c->surface.receipt)return RF_CANVAS_PENDING;
 if(c->input_active&&!risc_canvas_input_sleep_supported(c->input))
  return RF_CANVAS_UNAVAILABLE;
 if(!home_canvas_quiesce(&c->surface))
  return c->retained?RF_CANVAS_RETAINED:RF_CANVAS_PENDING;
 if(!c->input_active||c->input_prepared)return RF_CANVAS_READY;
 int rc=rf_canvas_result(c,c->input->prepare_sleep(c->input->context,c->surface.session,1000));
 /* Preserve a resume obligation even on a clean non-unsupported refusal. */
 if(c->surface.last_result!=RISC_CANVAS_UNSUPPORTED)c->input_prepared=true;
 return rc;
}
static inline int rf_canvas_resume_sleep(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(c->input_prepared) {
  int rc=rf_canvas_result(c,c->input->resume_sleep(c->input->context,c->surface.session,1000));
  if(rc!=RF_CANVAS_READY)return rc;
  c->input_prepared=false;
 }
 if(!home_canvas_resume(&c->surface))
  return c->retained?RF_CANVAS_RETAINED:RF_CANVAS_PENDING;
 return RF_CANVAS_READY;
}
/* Caller settles/cancels its frame and stops its local touch facade first.
 * No hidden polling loop, speculative provider release, or forced cleanup. */
static inline int rf_canvas_close(rf_capability_canvas *c) {
 if(!rf_canvas_safe(c))return RF_CANVAS_RETAINED;
 if(c->surface.lease||c->surface.receipt||c->touch.subscribed)
  return RF_CANVAS_PENDING;
 if(c->input_prepared) {
  int rc=rf_canvas_resume_sleep(c);if(rc!=RF_CANVAS_READY)return rc;
 }
 if(c->input_active) {
  int rc=rf_canvas_result(c,c->input->deactivate(c->input->context,c->surface.session));
  if(rc!=RF_CANVAS_READY)return rc;
  c->input_active=false;
 }
 int rc=rf_canvas_release(c,&c->input_grant,&c->input_owned);
 if(rc!=RF_CANVAS_READY)return rc;
 c->input=NULL;
 if(c->surface.session&&!home_canvas_close(&c->surface))
  return c->retained?RF_CANVAS_RETAINED:RF_CANVAS_PENDING;
 rc=rf_canvas_release(c,&c->canvas_grant,&c->canvas_owned);
 if(rc!=RF_CANVAS_READY)return rc;
 rc=rf_canvas_release(c,&c->control_grant,&c->control_owned);
 if(rc==RF_CANVAS_READY)c->control=NULL;
 return rc;
}
