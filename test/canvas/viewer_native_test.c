/* Actual Viewer + CanvasApp native integration fixture.
 * Every invocation owns one display and at most one pending physical raster.
 * Only three selected normal-UI rasters are optionally written to disk. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscTouchV1.h"
#include "RiscAppDataV1.h"
#include "RiscStorageVolumeV1.h"
#include "T5FileOpenApi.h"
#include "CanvasApp.h"

#define FX_CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); exit(3); } } while (0)
static struct { void *p; size_t n; } fx_blocks[128];
static size_t fx_live, fx_peak;
static bool fx_retained;
static void *fx_malloc(size_t n) {
    FX_CHECK(!fx_retained);
    void *p=malloc(n); if (!p) return NULL;
    for(unsigned i=0;i<128;i++) if(!fx_blocks[i].p) {
        fx_blocks[i].p=p;fx_blocks[i].n=n;fx_live+=n;
        if(fx_live>fx_peak)fx_peak=fx_live;return p;
    }
    FX_CHECK(false);return NULL;
}
static void *fx_calloc(size_t n,size_t s) {
    FX_CHECK(!s||n<=SIZE_MAX/s);void *p=fx_malloc(n*s);
    if(p)memset(p,0,n*s);return p;
}
static void fx_free(void *p) {
    if(!p)return;FX_CHECK(!fx_retained);
    for(unsigned i=0;i<128;i++)if(fx_blocks[i].p==p){
        fx_live-=fx_blocks[i].n;fx_blocks[i].p=NULL;free(p);return;
    }
    FX_CHECK(false);
}
#define malloc fx_malloc
#define calloc fx_calloc
#define free fx_free
#include "../../lib/CanvasApp/canvas.c"
#include "../../Apps/model_viewer_shading.h"
#include "../../Apps/model_viewer_preview.h"
static void fx_app_pixel(uint8_t *,int,int,bool);
static bool fx_preview_draw(const mv_preview_t *,uint8_t *,mv_shade_pixel_fn,int,int,int,int,float,float,float,mv_shade_service_fn);
#define canvas_pixel fx_app_pixel
#define mv_preview_draw fx_preview_draw
#ifndef VIEWER_NATIVE_APP_SOURCE
#define VIEWER_NATIVE_APP_SOURCE "../../Apps/model_viewer.c"
#endif
#include VIEWER_NATIVE_APP_SOURCE
#include "../../lib/CanvasApp/entry.c"
#undef canvas_pixel
#undef mv_preview_draw
#undef malloc
#undef calloc
#undef free

static unsigned fx_w,fx_h,fx_stride,fx_format,fx_ms,fx_submits,fx_releases;
static unsigned fx_clock_step,fx_nav_buttons;
static unsigned fx_cap_acquired,fx_cap_released,fx_touch_subs,fx_touch_unsubs;
static unsigned fx_foreground_claims,fx_foreground_releases,fx_retain_calls;
static unsigned fx_phase,fx_phase_at,fx_yields,fx_checks,fx_samples;
static unsigned fx_sd_opens,fx_sd_closes,fx_sd_at,fx_sd_seeks;
static unsigned fx_queue_checks,fx_queued_seen,fx_active_seen,fx_completed;
static unsigned fx_shaded_submits,fx_obj_submits;
static size_t fx_pixels_size;
static uint8_t *fx_allocation,*fx_pixels;
static uint64_t fx_queue;
static unsigned fx_ready_at,fx_queue_hash,fx_frame_state;
static bool fx_finished,fx_armed,fx_sd,fx_sd_open,fx_legacy,fx_submit_failed;
static bool fx_automatic,fx_fault_seen;
static const char *fx_case,*fx_sample_dir;
static char fx_diagnostic[160];
static risc_touch_snapshot_v1 fx_touch_state;
static risc_touch_event_v1 fx_events[64];
static unsigned fx_event_head,fx_event_count;
static float fx_start_yaw;
static unsigned fx_observe,fx_observed_pixels,fx_bad_pixels,fx_preview_calls;
static unsigned fx_boundary_exact;
static float fx_boundary_scales[4];
static float fx_preview_scale;
static void fx_app_pixel(uint8_t *p,int x,int y,bool black){
    bool observe=false;int left=0,right=(int)width;
    if(fx_observe==1){if(x==12&&y==20)fx_observe=0;else{observe=true;left=30;right=MV_SHADE_LEFT-4;}}
    else if(fx_observe==2&&y>=MV_VIEW_BOTTOM+8&&y<MV_VIEW_BOTTOM+15){observe=true;left=8;right=(int)width-8;}
    else if(fx_observe==3){observe=true;left=12;right=(int)width-12;}
    if(observe){fx_observed_pixels++;if(x<left||x>=right||y<0||y>=(int)height)fx_bad_pixels++;}
    canvas_pixel(p,x,y,black);
}
static bool fx_preview_draw(const mv_preview_t *p,uint8_t *b,mv_shade_pixel_fn pixel,int x,int y,int w,int h,float cx,float cy,float scale,mv_shade_service_fn service){
    fx_preview_calls++;fx_preview_scale=scale;
    return mv_preview_draw(p,b,pixel,x,y,w,h,cx,cy,scale,service);
}
static const char fx_obj[]="v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n";
static const char fx_stl[]=
    "solid fixture\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 2 0 0\nvertex 0 2 0\nendloop\nendfacet\n"
    "facet normal 0 0 1\nouter loop\nvertex 0 0 1\nvertex 2 0 1\nvertex 0 2 1\nendloop\nendfacet\nendsolid fixture\n";
static bool fx_is(const char *s){return !strcmp(fx_case,s);}
static uint32_t fx_hash(const void *v,size_t n){const uint8_t *p=v;uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static void fx_guard(void){
    for(unsigned i=0;i<32;i++)FX_CHECK(fx_allocation[i]==0xa5&&fx_allocation[32+fx_pixels_size+i]==0xa5);
    if(fx_queue){FX_CHECK(fx_hash(fx_pixels,fx_pixels_size)==fx_queue_hash);fx_queue_checks++;}
}
static bool fx_physical_black(unsigned x,unsigned y){
    const uint8_t *p=fx_pixels+(size_t)y*fx_stride;
    if(fx_format==RISC_DISPLAY_FORMAT_RGB565){uint16_t v;memcpy(&v,p+x*2,2);FX_CHECK(v==0||v==0xffff);return v==0;}
    return (p[x/8]&(0x80u>>(x&7)))!=0;
}
static void fx_raster_check(void){
    FX_CHECK(canvas_width()==(fx_w>fx_h?fx_h:fx_w));
    FX_CHECK(canvas_height()==(fx_w>fx_h?fx_w:fx_h));
    FX_CHECK(out_x==0&&out_y==0&&out_w==width&&out_h==height);
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
        bool ink=(snapshot[(size_t)y*stride+x/8]&(0x80u>>(x&7)))!=0;
        unsigned px=fx_w>fx_h?y:x,py=fx_w>fx_h?fx_h-1-x:y;
        FX_CHECK(fx_physical_black(px,py)==ink);fx_checks++;
    }
    /* Canvas may initialize padding, but must not draw beyond active pixels. */
    unsigned row=(fx_w*(fx_format==RISC_DISPLAY_FORMAT_RGB565?16:1)+7)/8;
    unsigned bg=fx_format==RISC_DISPLAY_FORMAT_RGB565?255:0;
    for(unsigned y=0;y<fx_h;y++)for(unsigned x=row;x<fx_stride;x++)FX_CHECK(fx_pixels[(size_t)y*fx_stride+x]==bg);
    fx_guard();
}
static void fx_sample(const char *name){
    if(!fx_sample_dir||!*fx_sample_dir)return;
    FX_CHECK(fx_samples<3);char path[1024];
    FX_CHECK(snprintf(path,sizeof(path),"%s/%s.pgm",fx_sample_dir,name)<(int)sizeof(path));
    FILE *f=fopen(path,"wb");FX_CHECK(f);fprintf(f,"P5\n%u %u\n255\n",fx_w,fx_h);
    for(unsigned y=0;y<fx_h;y++)for(unsigned x=0;x<fx_w;x++)fputc(fx_physical_black(x,y)?0:255,f);
    FX_CHECK(!fclose(f));fx_samples++;
}
static void fx_event(unsigned event_kind,unsigned id,unsigned x,unsigned y){
    FX_CHECK(x<width&&y<height&&fx_event_count<64);
    unsigned px=fx_w>fx_h?y:x,py=fx_w>fx_h?fx_h-1-x:y;
    risc_touch_event_v1 e={++fx_touch_state.sequence,fx_ms,event_kind,id,px,py};
    fx_events[(fx_event_head+fx_event_count++)%64]=e;
    fx_touch_state.timestamp_ms=fx_ms;
    unsigned at=0;while(at<fx_touch_state.contact_count&&fx_touch_state.contacts[at].id!=id)at++;
    if(event_kind==RISC_TOUCH_EVENT_UP){FX_CHECK(at<fx_touch_state.contact_count);for(unsigned i=at+1;i<fx_touch_state.contact_count;i++)fx_touch_state.contacts[i-1]=fx_touch_state.contacts[i];fx_touch_state.contact_count--;}
    else{if(at==fx_touch_state.contact_count){FX_CHECK(event_kind==RISC_TOUCH_EVENT_DOWN&&at<RISC_TOUCH_MAX_CONTACTS);fx_touch_state.contact_count++;}fx_touch_state.contacts[at]=(risc_touch_contact_v1){.id=id,.x=px,.y=py};}
}
static void fx_tap(unsigned x,unsigned y){fx_event(RISC_TOUCH_EVENT_DOWN,7,x,y);fx_event(RISC_TOUCH_EVENT_UP,7,x,y);}
static void fx_schedule(void){
    if(!fx_automatic||fx_finished||fx_retained||blocked||fx_submits<(fx_submit_failed?1u:2u))return;
    if(!fx_is("ui")&&!fx_is("sd")&&!fx_is("sd-legacy")&&!fx_is("submit-retry")){
        if(fx_is("data-retained")){if(!fx_armed){fx_armed=true;fx_tap(width/2,MV_VIEW_TOP-8);}return;}
        fx_armed=true;fx_finished=true;fx_touch_state.buttons=RISC_TOUCH_BUTTON_PRIMARY;return;
    }
    if(fx_ms-fx_phase_at<100)return;
    fx_phase_at=fx_ms;
    unsigned cx=width/2,cy=(MV_VIEW_TOP+MV_VIEW_BOTTOM)/2;
    switch(fx_phase++){
    case 0:FX_CHECK(g_model.triangle_count==(fx_sd?1u:12u));fx_tap(width/2,height-16);break;
    case 1:FX_CHECK(mv_touch_mode==1);fx_event(1,7,cx,cy);break;
    case 2:fx_event(2,7,cx+12,cy+8);break;
    case 3:FX_CHECK(g_view.pan_x==12&&g_view.pan_y==8);fx_event(3,7,cx+12,cy+8);break;
    case 4:fx_tap(width*5/6,height-16);break;
    case 5:FX_CHECK(mv_touch_mode==2);fx_event(1,7,cx,cy);break;
    case 6:fx_event(2,7,cx,cy-12);break;
    case 7:FX_CHECK(g_view.zoom>1.1f);fx_event(3,7,cx,cy-12);break;
    case 8:fx_tap(width/6,height-16);break;
    case 9:FX_CHECK(mv_touch_mode==0);fx_start_yaw=g_view.yaw;fx_event(1,7,cx,cy);break;
    case 10:fx_event(2,7,cx+14,cy+10);break;
    case 11:FX_CHECK(g_view.yaw>fx_start_yaw+0.1f);fx_event(3,7,cx+14,cy+10);break;
    case 12:fx_tap((MV_SHADE_LEFT+MV_SHADE_RIGHT)/2,20);break;
    case 13:FX_CHECK(g_shaded&&g_depth);if(!fx_shaded_submits)fx_phase--;break;
    case 14:fx_tap((MV_SHADE_LEFT+MV_SHADE_RIGHT)/2,20);break;
    case 15:FX_CHECK(!g_shaded&&!g_depth);fx_tap(width/2,MV_VIEW_TOP-8);break;
    case 16:FX_CHECK(g_model.triangle_count==1&&!strcmp(g_path,"model.obj"));if(!fx_obj_submits){fx_phase--;break;}fx_tap(width/2,MV_VIEW_TOP-8);break;
    case 17:FX_CHECK(g_model.triangle_count==2&&!strcmp(g_path,"model.stl"));fx_tap(width/2,MV_VIEW_TOP-8);break;
    case 18:FX_CHECK(g_model.triangle_count==12&&!strcmp(g_path,"builtin.obj"));fx_finished=true;fx_touch_state.buttons=RISC_TOUCH_BUTTON_PRIMARY;break;
    default:FX_CHECK(false);
    }
}
static bool fx_health(risc_runtime_health_v1 *h){FX_CHECK(!fx_retained);h->uptime_ms=fx_ms;return true;}
static bool fx_monotonic(uint32_t *out){FX_CHECK(!fx_retained);fx_ms+=fx_clock_step;*out=fx_ms;return true;}
static void fx_yield(uint32_t n){FX_CHECK(!fx_retained);FX_CHECK(++fx_yields<30000);fx_ms+=n?n:1;fx_guard();fx_schedule();}
static bool fx_diag(const char *s){FX_CHECK(!fx_retained);snprintf(fx_diagnostic,sizeof(fx_diagnostic),"%s",s);return true;}
static bool fx_retain(void){FX_CHECK(!fx_retained);fx_retained=true;fx_retain_calls++;return true;}
static bool fx_info(void *c,risc_display_info_v1 *i){
    (void)c;FX_CHECK(!fx_retained);*i=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*i),.width=fx_w,.height=fx_h,.supported_formats=RISC_DISPLAY_FORMAT_BIT(fx_format),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|(fx_format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0)};return true;
}
static bool fx_frame_get(void *c,uint32_t f,risc_display_surface_v1 *s){
    (void)c;FX_CHECK(!fx_retained&&f==fx_format);if(fx_frame_state)return false;fx_frame_state=1;
    *s=(risc_display_surface_v1){1,fx_pixels,fx_w,fx_h,fx_stride,fx_pixels_size,f};
    if(fx_is("invalid-surface")){s->stride_bytes=1;fx_fault_seen=true;}return true;
}
static void fx_frame_release(void *c,uint64_t f){(void)c;FX_CHECK(!fx_retained&&f==1&&fx_frame_state==1);fx_frame_state=0;fx_releases++;}
static bool fx_submit(void *c,uint64_t f,const risc_display_rect_v1 *d,size_t n,const risc_display_present_options_v1 *o,uint64_t *token){
    (void)c;FX_CHECK(!fx_retained&&f==1&&fx_frame_state==1&&!fx_queue&&!d&&!n&&o);fx_raster_check();
    if(fx_is("submit-retry")&&!fx_submit_failed){fx_submit_failed=true;return false;}
    fx_queue=++fx_submits;fx_frame_state=2;fx_ready_at=fx_ms+240;fx_queue_hash=fx_hash(fx_pixels,fx_pixels_size);*token=fx_queue;
    if(g_shaded)fx_shaded_submits++;
    if(g_model.triangle_count==1&&fx_phase>=16)fx_obj_submits++;
    if(fx_is("missing-token")){*token=0;fx_fault_seen=true;}
    if(fx_is("ui")){
        if(fx_submits==2)fx_sample("wire");
        else if(g_shaded&&fx_samples==1)fx_sample("shaded");
        else if(g_model.triangle_count==1&&fx_phase>=16&&fx_samples==2)fx_sample("saved-obj");
    }
    return true;
}
static bool fx_status(void *c,uint64_t t,risc_display_present_status_v1 *s){
    (void)c;FX_CHECK(!fx_retained&&t==fx_queue&&fx_frame_state==2);fx_guard();
    if(fx_armed&&fx_is("status-unknown")){fx_fault_seen=true;return false;}
    if(fx_armed&&fx_is("present-failed")){fx_fault_seen=true;s->state=RISC_DISPLAY_PRESENT_FAILED;return true;}
    if(fx_armed&&fx_is("present-timeout")){fx_fault_seen=true;s->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
    if(fx_ms+160<fx_ready_at){s->state=RISC_DISPLAY_PRESENT_QUEUED;fx_queued_seen++;}
    else if(fx_ms<fx_ready_at){s->state=RISC_DISPLAY_PRESENT_ACTIVE;fx_active_seen++;}
    else{s->state=RISC_DISPLAY_PRESENT_COMPLETE;fx_completed++;fx_queue=0;fx_frame_state=0;}return true;
}
static const risc_display_output_api_v1 fx_display_api={.api_version=1,.struct_size=sizeof(fx_display_api),.get_info=fx_info,.acquire=fx_frame_get,.release=fx_frame_release,.submit=fx_submit,.present_status=fx_status};
static uint64_t fx_subscribe(void *c){(void)c;FX_CHECK(!fx_retained&&!fx_touch_subs);fx_touch_subs++;return 1;}
static bool fx_unsubscribe(void *c,uint64_t n){(void)c;FX_CHECK(!fx_retained&&n==1&&!fx_touch_unsubs);if(fx_is("unsubscribe-retained")){fx_fault_seen=true;return false;}fx_touch_unsubs++;return true;}
static bool fx_poll(void *c,size_t n){(void)c;(void)n;FX_CHECK(!fx_retained);return true;}
static int32_t fx_next(void *c,uint64_t n,risc_touch_event_v1 *e){(void)c;FX_CHECK(!fx_retained&&n==1);if(!fx_event_count)return 0;*e=fx_events[fx_event_head];fx_event_head=(fx_event_head+1)%64;fx_event_count--;return 1;}
static bool fx_snapshot(void *c,risc_touch_snapshot_v1 *s){(void)c;FX_CHECK(!fx_retained);*s=fx_touch_state;return true;}
static const risc_touch_api_v1 fx_touch_api={1,sizeof(fx_touch_api),NULL,fx_subscribe,fx_unsubscribe,fx_poll,fx_next,fx_snapshot};
static bool fx_nav_poll(void *c,risc_input_navigation_frame_v1 *n){(void)c;FX_CHECK(!fx_retained);memset(n,0,sizeof(*n));n->buttons=fx_nav_buttons;return true;}
static bool fx_foreground(void *c,const risc_input_foreground_v1 *n,size_t count){(void)c;FX_CHECK(!fx_retained&&count<=1);if(count){FX_CHECK(!strcmp(n->capability,"input.touch.raw"));fx_foreground_claims++;}else{if(fx_is("foreground-retained")){fx_fault_seen=true;return false;}fx_foreground_releases++;}return true;}
static bool fx_nav_reset(void *c){(void)c;FX_CHECK(!fx_retained);return true;}
static const risc_input_navigation_api_v1 fx_nav_api={1,sizeof(fx_nav_api),NULL,fx_nav_poll,fx_foreground,fx_nav_reset};
static const char *fx_data_content(const char *n){return !strcmp(n,"model.obj")?fx_obj:!strcmp(n,"model.stl")?fx_stl:NULL;}
static int32_t fx_data_stat(void *c,const char *n,uint32_t *size,uint64_t *rev){(void)c;FX_CHECK(!fx_retained);const char *p=fx_data_content(n);if(!p)return -1;*size=(uint32_t)strlen(p);*rev=4;return 0;}
static int32_t fx_data_read(void *c,const char *n,uint64_t rev,void *out,uint32_t cap,uint32_t *size,uint64_t *result){(void)c;FX_CHECK(!fx_retained&&rev==4);const char *p=fx_data_content(n);FX_CHECK(p&&strlen(p)==cap);if(fx_is("data-retained")){fx_fault_seen=true;return RISC_APP_DATA_RETAINED;}memcpy(out,p,cap);*size=cap;*result=4;return 0;}
static const risc_app_data_v1 fx_data_api={1,sizeof(fx_data_api),NULL,fx_data_stat,fx_data_read,NULL};
static bool fx_sd_ready(void *c){(void)c;FX_CHECK(!fx_retained);return true;}
static uint32_t fx_sd_open_read(void *c,const char *p,uint64_t *n){(void)c;FX_CHECK(!fx_retained&&!fx_sd_open&&!strcmp(p,"/triangle.obj"));fx_sd_open=true;fx_sd_at=0;fx_sd_opens++;*n=sizeof(fx_obj)-1;return 1;}
static size_t fx_sd_read(void *c,uint32_t h,void *p,size_t n){(void)c;FX_CHECK(!fx_retained&&h==1&&fx_sd_open);if(n>sizeof(fx_obj)-1-fx_sd_at)n=sizeof(fx_obj)-1-fx_sd_at;memcpy(p,fx_obj+fx_sd_at,n);fx_sd_at+=(unsigned)n;return n;}
static bool fx_sd_close(void *c,uint32_t h,bool commit){(void)c;FX_CHECK(!fx_retained&&h==1&&fx_sd_open&&!commit);if(fx_is("file-close-retained")){fx_fault_seen=true;return false;}fx_sd_open=false;fx_sd_closes++;return true;}
static bool fx_sd_seek(void *c,uint32_t h,uint64_t n){(void)c;FX_CHECK(!fx_retained&&h==1&&fx_sd_open&&n<=sizeof(fx_obj)-1);fx_sd_at=(unsigned)n;fx_sd_seeks++;return true;}
static risc_storage_volume_api_v1_ext fx_sd_api={.base={.api_version=1,.struct_size=sizeof(fx_sd_api),.refresh=fx_sd_ready,.ready=fx_sd_ready,.file_open_read=fx_sd_open_read,.file_read=fx_sd_read,.file_close=fx_sd_close},.file_seek=fx_sd_seek};
static bool fx_path(char *p,size_t n){FX_CHECK(!fx_retained);snprintf(p,n,"/sd/triangle.obj");return true;}
static const t5_file_open_api_v1 fx_file_api={.api_version=1,.struct_size=sizeof(fx_file_api),.source_path_get=fx_path};
static bool fx_acquire(const char *n,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *g){
    FX_CHECK(!fx_retained&&v==1&&!instance);const void *api=NULL;
    if(!strcmp(n,"display.output"))api=&fx_display_api;else if(!strcmp(n,"input.touch.raw"))api=&fx_touch_api;
    else if(!strcmp(n,"input.navigation"))api=&fx_nav_api;else if(!strcmp(n,"storage.app-data"))api=&fx_data_api;
    else if(fx_sd&&!strcmp(n,"storage.volume"))api=&fx_sd_api;else if(fx_sd&&!strcmp(n,"file.open"))api=&fx_file_api;
    if(!api)return false;g->api=api;g->slot=++fx_cap_acquired;return true;
}
static bool fx_release_cap(risc_runtime_capability_v1 *g){FX_CHECK(!fx_retained&&!fx_queue&&g->api);if(fx_is("release-retained")){fx_fault_seen=true;return false;}g->api=NULL;fx_cap_released++;return true;}
static const risc_runtime_api_v1 fx_runtime_api={.api_version=1,.struct_size=sizeof(fx_runtime_api),.health=fx_health,.yield_ms=fx_yield,.diagnostic=fx_diag,.acquire=fx_acquire,.release=fx_release_cap,.retain_invocation=fx_retain,.monotonic_ms=fx_monotonic};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!fx_retained?&fx_runtime_api:NULL;}

static void fx_drain(void){while(pending()){fx_yield(8);FX_CHECK(fx_ms<15000);}}
static void fx_init_direct(void){
    FX_CHECK(canvas_open(CANVAS_VIEWER));g_app=t5_app_get_api(1);g_storage=t5_storage_get_api(1);
    g_file_open=t5_file_open_get_api(1);g_video=display_client_get_api();g_use_psram=true;
    FX_CHECK(g_video->start(&g_surface));FX_CHECK(mv_load_model("builtin.obj",&g_model));
    strcpy(g_path,"builtin.obj");mv_reset_view();mv_toolbar_capture=-1;
    t5_app_input_t in;FX_CHECK(g_app->poll(&in,1));
}
static void fx_direct_cleanup(void){mv_preview_release();mv_shade_release();mv_model_free(&g_model);canvas_close();}
static void fx_pixels_test(void){
    fx_init_direct();memset(working,0,(size_t)stride*height);
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)canvas_pixel(working,x,y,((x+3*y)%17)==0);
    FX_CHECK(submit(0,height));FX_CHECK(fx_queue);uint32_t frozen=fx_hash(snapshot,(size_t)stride*height);
    memset(working,0xff,(size_t)stride*height);FX_CHECK(fx_hash(snapshot,(size_t)stride*height)==frozen);fx_guard();
    FX_CHECK(submit(0,height));FX_CHECK(dirty);memset(working,0,(size_t)stride*height);fx_guard();fx_drain();
    FX_CHECK(fx_submits==2&&fx_completed==2);FX_CHECK(fx_queued_seen&&fx_active_seen&&fx_queue_checks);
    /* The inverse input transform must preserve exact native coordinates. */
    const unsigned pts[5][2]={{0,0},{1,1},{width/2,height/2},{width-2,height-2},{width-1,height-1}};
    for(unsigned i=0;i<5;i++){
        fx_event(1,7,pts[i][0],pts[i][1]);t5_app_input_t in;FX_CHECK(g_app->poll(&in,1));
        risc_touch_snapshot_v1 s;FX_CHECK(canvas_contacts(&s)&&s.contact_count==1);
        FX_CHECK(s.contacts[0].x==pts[i][0]&&s.contacts[0].y==pts[i][1]);
        fx_event(3,7,pts[i][0],pts[i][1]);FX_CHECK(g_app->poll(&in,1));FX_CHECK(canvas_contacts(&s)&&!s.contact_count);
    }
    fx_direct_cleanup();
}
static mv_contacts_t fx_contact(int x,int y,uint8_t id){mv_contacts_t c={.count=1,.id={id},.x={x},.y={y}};return c;}
static void fx_toolbar_test(void){
    fx_init_direct();mv_contacts_t zero={0};
    FX_CHECK(mv_toolbar_hit(-1,height-1)<0);FX_CHECK(mv_toolbar_hit(width,height-1)<0);
    FX_CHECK(mv_toolbar_hit(0,height)<0);FX_CHECK(mv_toolbar_hit(0,-1)<0);
    const int x=(int)width/2,y=(int)height-16;
    mv_contacts_t a=fx_contact(x,y,3),b=fx_contact(x,y,4);
    mv_touch_mode=0;mv_handle_touch(&zero,&a,10);mv_handle_touch(&a,&b,20);mv_handle_touch(&b,&zero,30);FX_CHECK(mv_touch_mode==0);
    mv_handle_touch(&zero,&a,40);mv_handle_touch(&a,&zero,400);FX_CHECK(mv_touch_mode==0);
    b=fx_contact(x+15,y,3);mv_handle_touch(&zero,&a,410);mv_handle_touch(&a,&b,420);mv_handle_touch(&b,&zero,430);FX_CHECK(mv_touch_mode==0);
    mv_handle_touch(&zero,&a,440);mv_handle_touch(&a,&zero,450);FX_CHECK(mv_touch_mode==1);
    /* Shade capture must cancel replacement identity and multi-contact input. */
    a=fx_contact((MV_SHADE_LEFT+MV_SHADE_RIGHT)/2,20,3);b=a;b.id[0]=4;
    mv_handle_touch(&zero,&a,500);mv_handle_touch(&a,&b,510);mv_handle_touch(&b,&zero,520);FX_CHECK(!g_shaded);
    b=a;b.count=2;b.id[1]=4;b.x[1]=a.x[0]+1;b.y[1]=20;
    mv_handle_touch(&zero,&a,530);mv_handle_touch(&a,&b,540);mv_handle_touch(&b,&zero,550);FX_CHECK(!g_shaded);
    mv_handle_touch(&zero,&a,560);mv_handle_touch(&a,&zero,570);FX_CHECK(g_shaded);fx_direct_cleanup();
}
static void fx_layout_test(void){
    fx_init_direct();
    if(fx_is("layout")||fx_is("header")){
        fx_observe=1;FX_CHECK(mv_render(false));
        FX_CHECK(fx_observed_pixels&&!fx_bad_pixels);fx_drain();
    }
    if(fx_is("layout")||fx_is("status")){
        memset(g_status,'W',sizeof(g_status)-1);g_status[sizeof(g_status)-1]=0;
        fx_observe=2;fx_observed_pixels=fx_bad_pixels=0;FX_CHECK(mv_render(false));
        FX_CHECK(fx_observed_pixels&&!fx_bad_pixels);fx_drain();
    }
    if(fx_is("layout")||fx_is("message")){
        fx_observe=3;fx_observed_pixels=fx_bad_pixels=0;
        mv_message("MODEL LOAD FAILED", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
        FX_CHECK(fx_observed_pixels&&!fx_bad_pixels);fx_observe=0;fx_drain();
    }
    fx_direct_cleanup();
}
static float fx_actual_scale(void){
#ifdef FX_NATIVE_HAS_VIEW_SCALE
    return mv_view_scale();
#else
    return (width-32)*0.8f*g_view.zoom;
#endif
}
static void fx_transform_test(void){
    fx_init_direct();
    for(unsigned reset=0;reset<2;reset++){
        if(reset){g_view=(mv_view_t){2,2,7,50,50};mv_reset_view();}
        mv_rotation_t r={mv_cos(g_view.yaw),mv_sin(g_view.yaw),mv_cos(g_view.pitch),mv_sin(g_view.pitch)};
        for(unsigned i=0;i<g_model.triangle_count;i++){
            mv_vec3_t vertices[3]={g_model.triangles[i].a,g_model.triangles[i].b,g_model.triangles[i].c};
            for(unsigned j=0;j<3;j++){
                mv_shade_vertex_t p=mv_screen_point(mv_camera_point(vertices[j],&r));
                FX_CHECK(p.y>=MV_VIEW_TOP&&p.y<=MV_VIEW_BOTTOM);
                if(width<480)FX_CHECK(p.x>=4&&p.x<width-4);
            }
        }
    }
    /* Capture the actual preview call rather than reproducing its scale math.
     * Small scales outside the atlas contract must succeed via mesh fallback. */
    const float zooms[]={1.0f,0.18f,2.5f,7.0f};
    for(unsigned i=0;i<4;i++){
        g_view.zoom=zooms[i];g_controller.held=MV_PAD_LB|MV_PAD_UP;
        unsigned previous_submits=fx_submits;
        fx_preview_calls=0;FX_CHECK(mv_render(true));
        float expected=mv_screen_point((mv_shade_vertex_t){1,0,0}).x-mv_screen_point((mv_shade_vertex_t){0,0,0}).x;
        if(width>=480)FX_CHECK(mv_shade_abs(expected-(width-32)*0.8f*g_view.zoom)<0.001f);
        if(expected>=64&&expected<=4096){FX_CHECK(fx_preview_calls==1);FX_CHECK(mv_shade_abs(fx_preview_scale-expected)<0.001f);}
        else FX_CHECK(!fx_preview_calls);
        fx_drain();FX_CHECK(fx_submits==previous_submits+1);
    }
    g_view.zoom=1;
    float base=fx_actual_scale();
    const float scales[]={63,64,4096,4097};
    for(unsigned i=0;i<4;i++){
        g_view.zoom=scales[i]/base;g_controller.held=MV_PAD_LB|MV_PAD_UP;
        uint32_t bits;memcpy(&bits,&g_view.zoom,sizeof(bits));
        /* Some native base scales cannot produce an exact boundary product.
         * Prefer exact products, otherwise choose the closest admissible
         * float. Other geometries exercise all four exact thresholds. */
        float best=g_view.zoom,best_error=1.0f;bool found=false;
        for(int d=-3;d<=3;d++){
            uint32_t trial=bits+d;memcpy(&g_view.zoom,&trial,sizeof(trial));
            float got=fx_actual_scale(),error=mv_shade_abs(got-scales[i]);
            bool supported_side=(i!=1||got>=64)&&(i!=2||got<=4096);
            if(supported_side&&error<best_error){best=g_view.zoom;best_error=error;found=true;}
        }
        FX_CHECK(found&&best_error<0.001f);g_view.zoom=best;
        fx_boundary_scales[i]=fx_actual_scale();
        if(fx_boundary_scales[i]==scales[i])fx_boundary_exact|=1u<<i;
        unsigned previous_submits=fx_submits;fx_preview_calls=0;
        FX_CHECK(mv_render(true));
        if(fx_boundary_scales[i]>=64&&fx_boundary_scales[i]<=4096){FX_CHECK(fx_preview_calls==1&&g_preview.ink);FX_CHECK(fx_preview_scale==fx_boundary_scales[i]);}
        else FX_CHECK(!fx_preview_calls);
        fx_drain();FX_CHECK(fx_submits==previous_submits+1);
    }
    /* A timed BACK observed inside exact fallback aborts before submission. */
    g_view.zoom=63/base;g_controller.held=MV_PAD_LB|MV_PAD_UP;
    unsigned previous_submits=fx_submits;fx_preview_calls=0;
    fx_clock_step=17;fx_nav_buttons=RISC_NAV_BACK;
    FX_CHECK(!mv_render(true));
    FX_CHECK(g_render_input_pending&&g_render_input.exit_requested&&!fx_preview_calls);
    FX_CHECK(fx_submits==previous_submits&&!fx_queue);
    fx_clock_step=fx_nav_buttons=0;g_render_input_pending=false;
    g_controller.held=0;fx_direct_cleanup();
}
int main(int argc,char **argv){
    FX_CHECK(argc>=5);fx_w=(unsigned)strtoul(argv[1],NULL,10);fx_h=(unsigned)strtoul(argv[2],NULL,10);
    fx_format=!strcmp(argv[3],"mono")?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565;fx_case=argv[4];
    fx_sample_dir=argc>5?argv[5]:NULL;fx_stride=(fx_w*(fx_format==RISC_DISPLAY_FORMAT_RGB565?16:1)+7)/8+13;
    fx_pixels_size=(size_t)fx_stride*fx_h;fx_allocation=malloc(fx_pixels_size+64);FX_CHECK(fx_allocation);
    memset(fx_allocation,0xa5,fx_pixels_size+64);fx_pixels=fx_allocation+32;
    fx_touch_state.width=fx_w;fx_touch_state.height=fx_h;
    fx_sd=fx_is("sd")||fx_is("sd-legacy")||fx_is("file-close-retained");fx_legacy=fx_is("sd-legacy");
    if(fx_legacy)fx_sd_api.base.struct_size=sizeof(risc_storage_volume_api_v1);
    if(fx_is("pixels"))fx_pixels_test();else if(fx_is("toolbar"))fx_toolbar_test();
    else if(fx_is("layout")||fx_is("header")||fx_is("status")||fx_is("message"))fx_layout_test();else if(fx_is("transform"))fx_transform_test();
    else{fx_automatic=true;app_main();}
    fx_guard();
    bool expected_retained=fx_is("status-unknown")||fx_is("present-failed")||fx_is("present-timeout")||fx_is("invalid-surface")||fx_is("missing-token")||fx_is("unsubscribe-retained")||fx_is("foreground-retained")||fx_is("release-retained")||fx_is("file-close-retained")||fx_is("data-retained");
    if(expected_retained){FX_CHECK(fx_fault_seen&&fx_retained&&fx_retain_calls==1&&fx_live&&retained);FX_CHECK(fx_diagnostic[0]);}
    else{
        FX_CHECK(!fx_retained&&!fx_retain_calls&&!fx_live&&!working&&!snapshot&&!fx_queue&&!fx_frame_state);
        FX_CHECK(fx_cap_acquired==fx_cap_released&&fx_touch_subs==fx_touch_unsubs);
        FX_CHECK(fx_foreground_claims==fx_foreground_releases);
        if(fx_automatic)FX_CHECK(fx_finished&&fx_phase==19&&fx_submits>=3&&fx_shaded_submits&&fx_obj_submits);
        if(fx_is("ui")&&fx_sample_dir)FX_CHECK(fx_samples==3);
        if(fx_is("submit-retry"))FX_CHECK(fx_submit_failed&&fx_releases==1);
        if(fx_sd){FX_CHECK(fx_sd_opens&&fx_sd_opens==fx_sd_closes&&!fx_sd_open);FX_CHECK(fx_legacy?fx_sd_opens>1:fx_sd_seeks>0);}
    }
    printf("PASS case=%s physical=%ux%u logical=%ux%u format=%s stride=%u frames=%u pixels=%u queue_checks=%u queued=%u active=%u samples=%u boundary_exact_mask=%u allocation_peak=%zu retained=%u diagnostic=%s\n",fx_case,fx_w,fx_h,width,height,argv[3],fx_stride,fx_submits,fx_checks,fx_queue_checks,fx_queued_seen,fx_active_seen,fx_samples,fx_boundary_exact,fx_peak,fx_retained,fx_diagnostic);
    if(fx_is("transform"))printf("boundary_actual_scales=%.9g,%.9g,%.9g,%.9g\n",fx_boundary_scales[0],fx_boundary_scales[1],fx_boundary_scales[2],fx_boundary_scales[3]);
    /* A retained runtime invocation owns these blocks. Tear down only here,
     * outside the simulated invocation, so sanitizers can check the harness. */
    for(unsigned i=0;i<128;i++)if(fx_blocks[i].p)free(fx_blocks[i].p);
    free(fx_allocation);return 0;
}
