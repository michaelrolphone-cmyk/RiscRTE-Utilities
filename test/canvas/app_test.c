#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscTouchV1.h"
#include "RiscAppDataV1.h"
#include "CanvasApp.h"
#include "RiscStorageVolumeV1.h"
#include "T5FileOpenApi.h"
#if CANVAS_KIND==1
#include "../../Apps/model_viewer.c"
#else
#include "../../Apps/hollow_trail.c"
#endif
#include "../../lib/CanvasApp/entry.c"
static unsigned ms,submits,releases,steps,acquired,freed,mode,phase,phase_at;
static bool hold_test,held_invocation,finished,subscribed,pressed,changed,did_shade,did_saved,did_move,notes_seen;
static uint8_t *pixels,*captured;static size_t pixels_size;static uint64_t queued;static unsigned ready_at,physical_w,physical_h;
static risc_touch_snapshot_v1 snap;static risc_touch_event_v1 events[32];static unsigned event_head,event_count;
static uint32_t frame_hash;
static int game_start_x;
static bool sd_test,sd_open,sd_legacy;static unsigned sd_at,sd_opens,sd_closes,sd_seeks;
static uint32_t hash(const uint8_t *p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static void event(unsigned k,unsigned x,unsigned y){assert(event_count<32);risc_touch_event_v1 e={++snap.sequence,ms,k,0,x,y};events[(event_head+event_count++)%32]=e;snap.timestamp_ms=ms;snap.contact_count=k==3?0:1;snap.contacts[0]=(risc_touch_contact_v1){.id=0,.x=x,.y=y};}
static void tap(unsigned x,unsigned y){event(1,x,y);event(3,x,y);}
static void game_tap(unsigned x,unsigned y){/* Landscape logical input, portrait raw. */tap(physical_h-1-y,x);}
static void schedule(void){
 if(finished)return;
 if(++steps>20000){fprintf(stderr,"test timeout phase %u frames %u ms %u\n",phase,submits,ms);abort();}
#if CANVAS_KIND==1
 unsigned vw=canvas_width(),vh=canvas_height();
 if(!phase&&submits>=2){phase=1;phase_at=ms;tap(vw/2,vh-16);}
 else if(phase==1&&ms-phase_at>80){phase=2;phase_at=ms;event(1,vw/2,vh/2);}
 else if(phase==2&&ms-phase_at>40){phase=3;phase_at=ms;event(2,vw/2+24,vh/2+12);}
 else if(phase==3&&ms-phase_at>40){phase=4;phase_at=ms;event(3,vw/2+24,vh/2+12);}
 else if(phase==4&&ms-phase_at>100){changed=g_view.pan_x!=0;assert(changed);phase=5;phase_at=ms;tap(vw-42,20);}
 else if(phase==5&&ms-phase_at>150){did_shade=g_shaded;assert(did_shade);phase=6;phase_at=ms;tap(vw/2,vw<=320?44:78);}
 else if(phase==6&&ms-phase_at>180){did_saved=g_model.triangle_count==1;assert(did_saved);phase=7;phase_at=ms;}
 else if(phase==7&&ms-phase_at>300){snap.buttons=1;finished=true;}
#else
 if(!phase&&!ht_cutscene.active&&submits>=3){phase=1;phase_at=ms;game_start_x=ht.x;event(1,30,physical_w/10+20);}
 else if(phase==1&&ms-phase_at>500){did_move=ht.x!=game_start_x;phase=2;phase_at=ms;event(3,30,physical_w/10+20);}
 else if(phase==2&&ms-phase_at>100){phase=3;phase_at=ms;game_tap(physical_w-physical_w/10,30);}
 else if(phase==3&&ms-phase_at>200){notes_seen=reading;phase=4;phase_at=ms;snap.buttons=1;finished=true;}
#endif
}
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=ms;return !held_invocation;}
static bool monotonic(uint32_t *out){*out=ms;return !held_invocation;}
static void yield_ms(uint32_t n){ms+=(n?n:1)*(CANVAS_KIND==2?8:1);if(queued)assert(hash(pixels,pixels_size)==frame_hash);if(!held_invocation)schedule();}
static bool diagnostic(const char *s){fprintf(stderr,"%s\n",s);return true;}
static bool retain(void){held_invocation=true;return true;}
static bool get_info(void *c,risc_display_info_v1 *i){(void)c;*i=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*i),.width=physical_w,.height=physical_h,.supported_formats=RISC_DISPLAY_FORMAT_BIT(mode?1:5),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT| (mode?RISC_DISPLAY_INFO_RETAINS_IMAGE:0)};return true;}
static bool frame_get(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;if(queued)return false;assert(f==(mode?1u:5u));*s=(risc_display_surface_v1){1,pixels,physical_w,physical_h,(physical_w*(mode?1:16)+7)/8,pixels_size,f};return true;}
static void frame_release(void *c,uint64_t f){(void)c;assert(f==1);freed++;}
static bool frame_submit(void *c,uint64_t f,const risc_display_rect_v1 *d,size_t n,const risc_display_present_options_v1 *o,uint64_t *out){(void)c;(void)d;(void)n;assert(f==1&&!queued&&o);queued=++submits;*out=queued;ready_at=ms+160;frame_hash=hash(pixels,pixels_size);memcpy(captured,pixels,pixels_size);return true;}
static bool frame_status(void *c,uint64_t t,risc_display_present_status_v1 *s){(void)c;assert(t==queued);if(hold_test&&finished)return false;if(ms<ready_at)s->state=RISC_DISPLAY_PRESENT_ACTIVE;else{s->state=RISC_DISPLAY_PRESENT_COMPLETE;queued=0;}return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=get_info,.acquire=frame_get,.release=frame_release,.submit=frame_submit,.present_status=frame_status};
static uint64_t subscribe(void *c){(void)c;assert(!subscribed);subscribed=true;return 1;}
static bool unsubscribe(void *c,uint64_t n){(void)c;assert(n==1&&subscribed);subscribed=false;return true;}
static bool touch_poll(void *c,size_t n){(void)c;(void)n;return true;}
static int32_t touch_next(void *c,uint64_t sub,risc_touch_event_v1 *e){(void)c;assert(sub==1);if(!event_count)return 0;*e=events[event_head];event_head=(event_head+1)%32;event_count--;return 1;}
static bool snapshot(void *c,risc_touch_snapshot_v1 *s){(void)c;*s=snap;return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *n){(void)c;memset(n,0,sizeof(*n));return true;}
static bool foreground(void *c,const risc_input_foreground_v1 *n,size_t count){(void)c;assert(count<=1);if(count)assert(!strcmp(n->capability,"input.touch.raw"));return true;}
static bool nav_reset(void *c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,nav_poll,foreground,nav_reset};
static const char triangle[]="v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n";
static int32_t stat_data(void *c,const char *n,uint32_t *size,uint64_t *rev){(void)c;if(strcmp(n,"model.obj"))return -1;*size=sizeof(triangle)-1;*rev=1;return 0;}
static int32_t read_data(void *c,const char *n,uint64_t rev,void *out,uint32_t cap,uint32_t *size,uint64_t *result){assert(!c&&!strcmp(n,"model.obj")&&rev==1&&cap==sizeof(triangle)-1);memcpy(out,triangle,cap);*size=cap;*result=1;return 0;}
static const risc_app_data_v1 data_api={1,sizeof(data_api),NULL,stat_data,read_data,NULL};
static bool sd_ready(void *c){(void)c;return true;}
static uint32_t sd_open_read(void *c,const char *p,uint64_t *n){assert(!c&&!sd_open&&!strcmp(p,"/triangle.obj"));sd_open=true;sd_at=0;sd_opens++;*n=sizeof(triangle)-1;return 1;}
static size_t sd_read(void *c,uint32_t h,void *p,size_t n){assert(!c&&h==1&&sd_open);if(n>sizeof(triangle)-1-sd_at)n=sizeof(triangle)-1-sd_at;memcpy(p,triangle+sd_at,n);sd_at+=n;return n;}
static bool sd_close(void *c,uint32_t h,bool commit){assert(!c&&h==1&&sd_open&&!commit);sd_open=false;sd_closes++;return true;}
static bool sd_seek(void *c,uint32_t h,uint64_t n){assert(!c&&h==1&&sd_open&&n<=sizeof(triangle)-1);sd_at=n;sd_seeks++;return true;}
static risc_storage_volume_api_v1_ext sd_api={.base={.api_version=1,.struct_size=sizeof(sd_api),.refresh=sd_ready,.ready=sd_ready,.file_open_read=sd_open_read,.file_read=sd_read,.file_close=sd_close},.file_seek=sd_seek};
static bool file_path(char *p,size_t n){snprintf(p,n,"/sd/triangle.obj");return true;}
static const t5_file_open_api_v1 file_api={.api_version=1,.struct_size=sizeof(file_api),.source_path_get=file_path};
static bool acquire_cap(const char *n,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *g){assert(v==1&&!instance&&!held_invocation);const void *api=sd_test&&!strcmp(n,"storage.volume")?(const void*)&sd_api:sd_test&&!strcmp(n,"file.open")?(const void*)&file_api:!strcmp(n,"display.output")?(const void*)&display_api:!strcmp(n,"input.touch.raw")?(const void*)&touch_api:!strcmp(n,"input.navigation")&&mode?(const void*)&nav_api:!strcmp(n,"storage.app-data")?(const void*)&data_api:NULL;if(!api)return false;g->api=api;g->slot=++acquired;return true;}
static bool release_cap(risc_runtime_capability_v1 *g){assert(!queued&&!held_invocation&&g->api);if(hold_test)return false;g->api=NULL;releases++;return true;}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.acquire=acquire_cap,.release=release_cap,.retain_invocation=retain,.monotonic_ms=monotonic};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!held_invocation?&runtime_api:NULL;}
int main(int argc,char **argv){
 mode=argc>1&&strcmp(argv[1],"watch")?(!strcmp(argv[1],"x4-native")?2:1):0;sd_test=argc>2&&!strncmp(argv[2],"sd",2);sd_legacy=sd_test&&!strcmp(argv[2],"sd-legacy");if(sd_legacy)sd_api.base.struct_size=sizeof(risc_storage_volume_api_v1);hold_test=argc>2&&!strcmp(argv[2],"retain");
#if CANVAS_KIND==1
 physical_w=mode==2?800:mode?480:240;physical_h=mode==2?480:mode?800:240;
#else
 physical_w=800;physical_h=480;
#endif
 snap.width=mode?480:240;snap.height=mode?800:240;
 pixels_size=(size_t)((physical_w*(mode?1:16)+7)/8)*physical_h;pixels=calloc(1,pixels_size);captured=calloc(1,pixels_size);assert(pixels&&captured);
 app_main();assert(finished&&submits>=3);
 if(hold_test)assert(held_invocation&&releases==0);else assert(!held_invocation&&releases==acquired&&!subscribed&&!queued);
#if CANVAS_KIND==1
 assert(changed&&did_shade&&did_saved);if(sd_test)assert(sd_opens&&sd_opens==sd_closes&&!sd_open);
#else
 assert(did_move&&notes_seen);
#endif
 if(argc>3){FILE *f=fopen(argv[3],"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",physical_w,physical_h);for(unsigned y=0;y<physical_h;y++)for(unsigned x=0;x<physical_w;x++){uint8_t v;if(mode)v=(captured[y*((physical_w+7)/8)+x/8]&(0x80u>>(x&7)))?0:255;else{uint16_t p;memcpy(&p,captured+((size_t)y*physical_w+x)*2,2);v=p?255:0;}uint8_t rgb[3]={v,v,v};fwrite(rgb,1,3,f);}fclose(f);}
 free(pixels);free(captured);printf("real entrypoint kind=%u target=%s frames=%u elapsed=%u retention=%u PASS\n",CANVAS_KIND,mode?"x4":"watch",submits,ms,held_invocation);return 0;
}
