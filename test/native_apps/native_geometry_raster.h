/* Real selected System glyph/primitives, modeled surface/custody only. */
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "PortableSafeViewport.h"
static portable_safe_viewport content_view;
static struct {void *pixels;unsigned width,height,stride_bytes;} surface;
static bool failed,list_mode,surface_acquired;
static unsigned surface_acquisitions;
static unsigned raster_pixels,surface_format;
#define RISC_DISPLAY_FORMAT_MONO1 1
static int ng_width(void){return (int)content_view.width;}
static int ng_height(void){return (int)content_view.height;}
static bool raster_checkpoint(void){return !failed;}
static bool raster_surface_writable(void){return !failed&&surface_acquired;}
static void np_pixel(int x,int y,uint32_t c,unsigned a){(void)x;(void)y;(void)c;(void)a;assert(0);}
static void native_point(int*x,int*y){*x+=(int)content_view.x;*y+=(int)content_view.y;assert(*x>=0&&*y>=0&&(unsigned)*x<surface.width&&(unsigned)*y<surface.height);}
static void clear_color(uint32_t c){
 if(failed)return;
 assert(!surface_acquired);surface_acquired=true;surface_acquisitions++;
 for(unsigned y=content_view.y;y<content_view.y+content_view.height;y++)
  for(unsigned x=content_view.x;x<content_view.x+content_view.width;x++)
   ((uint16_t*)surface.pixels)[y*(surface.width+3)+x]=(uint16_t)c;
}
#define width() ng_width()
#define height() ng_height()
#define portable_nova_native_text ng_real_native_text
#include NATIVE_RENDERER_SOURCE
#undef portable_nova_native_text
static char rendered_text[128][96];static unsigned rendered_count;
void portable_nova_native_text(unsigned face,int x,int y,int w,const char*text,uint32_t color){
 if(rendered_count<128){strncpy(rendered_text[rendered_count],text,95);rendered_text[rendered_count++][95]=0;}
 ng_real_native_text(face,x,y,w,text,color);
}
static __attribute__((unused)) bool saw_text(const char*s){for(unsigned i=0;i<rendered_count;i++)if(!strcmp(s,rendered_text[i]))return true;return false;}
#undef width
#undef height
static void ng_open(unsigned w,unsigned h,unsigned l,unsigned t,unsigned r,unsigned b){
 assert(portable_safe_viewport_open(&content_view,w,h,l,t,r,b));
 surface.width=w;surface.height=h;surface.stride_bytes=(w+3)*2;
 surface.pixels=malloc((size_t)surface.stride_bytes*h);assert(surface.pixels);
 for(size_t i=0;i<(size_t)(w+3)*h;i++)((uint16_t*)surface.pixels)[i]=0xa55a;
 rendered_count=0;surface_format=2;failed=false;surface_acquired=false;surface_acquisitions=0;
}
static void ng_check(void){
 for(unsigned y=0;y<surface.height;y++)for(unsigned x=0;x<surface.width+3;x++)
 if(x<content_view.x||x>=content_view.x+content_view.width||y<content_view.y||y>=content_view.y+content_view.height)
  assert(((uint16_t*)surface.pixels)[y*(surface.width+3)+x]==0xa55a);
}
