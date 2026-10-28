#pragma once
/* Explicit content-local native pixels. The selected System adapter owns the
 * capability safe rectangle and the sole inverse touch/origin transform. */
#if !defined(PORTABLE_APP_OWNS_TOUCH_CHROME) || !defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_RF_NATIVE_CANVAS) || !defined(PORTABLE_SAFE_VIEWPORT) || !defined(PORTABLE_WATCH_SAFE_INSETS) || !defined(PORTABLE_WATCH_NATIVE_HEIGHT) || !defined(PORTABLE_WATCH_COLOR_ONLY)
#error Native Utilities require the complete native safe-content adapter profile
#endif
#if defined(PORTABLE_PAPER_UTILITIES) || defined(PORTABLE_NOVA_TEXT_FACES_012)
#error Native Utilities use color and the original complete six native font faces
#endif
#include "PortableNovaUi.h"
#include "T5AppApi.h"
#include <string.h>
typedef struct { int x,y,width,height; } utility_native_rect;
static inline bool un_dimensions(const t5_app_api_v1 *api) {
 return api && api->screen_width && api->screen_height && api->screen_width()>=240 &&
  api->screen_height()>=240 && api->screen_width()<=1024 && api->screen_height()<=1024;
}
static inline bool un_hit(utility_native_rect r,int x,int y) {
 return x>=r.x && y>=r.y && x-r.x<r.width && y-r.y<r.height;
}
static inline utility_native_rect un_back(void) {return (utility_native_rect){8,4,52,32};}
static inline utility_native_rect un_footer(int w,int h,int index,int count) {
 int a=8+index*(w-16)/count,b=8+(index+1)*(w-16)/count;
 return (utility_native_rect){a+2,h-56,b-a-4,48};
}
static inline void un_fill(utility_native_rect r,uint32_t color) {
 portable_nova_native_fill(r.x,r.y,r.width,r.height,color);
}
/* Fonts are original native raster faces, never resized. Shrink by selecting a
 * smaller original face; ellipsis is presentation-only, never model mutation. */
static inline void un_text(utility_native_rect r,const char *text,bool large,uint32_t color) {
 static const unsigned faces[]={4,5,0,1,2};
 static const int heights[]={38,28,18,20,16};
 if(!text || r.width<8 || r.height<16)return;
 char bounded[96];size_t n=0;while(text[n]&&n<sizeof(bounded)-1){bounded[n]=text[n];n++;}bounded[n]=0;
 unsigned pick=4;
 for(unsigned i=large?0:2;i<5;i++)if(heights[i]<=r.height&&portable_nova_measure(faces[i],bounded)<=r.width-4){pick=i;break;}
 bool shortened=text[n]!=0 || portable_nova_measure(faces[pick],bounded)>r.width-4;
 if(shortened){
  if(n>sizeof(bounded)-4)n=sizeof(bounded)-4;
  for(;;){memcpy(bounded+n,"...",4);if(portable_nova_measure(faces[pick],bounded)<=r.width-4||!n)break;n--;}
 }
 int width=portable_nova_measure(faces[pick],bounded);
 portable_nova_native_text(faces[pick],r.x+(r.width-width)/2,r.y+(r.height-heights[pick])/2,r.width,bounded,color);
}
static inline void un_button(utility_native_rect r,const char *label,bool selected) {
 portable_nova_native_round(r.x,r.y,r.width,r.height,6,selected?NOVA_CYAN:NOVA_LINE);
 portable_nova_native_round(r.x+1,r.y+1,r.width-2,r.height-2,5,selected?NOVA_DIM:0);
 un_text((utility_native_rect){r.x+3,r.y+2,r.width-6,r.height-4},label,false,NOVA_TEXT);
}
static inline void un_header(const t5_app_api_v1 *api,const char *title) {
 /* Native primitives share an acquired frame; they never acquire one. */
 portable_nova_begin();
 un_fill((utility_native_rect){0,0,api->screen_width(),api->screen_height()},0);
 un_button(un_back(),"Back",false);
 un_text((utility_native_rect){64,4,api->screen_width()-72,32},title,false,NOVA_CYAN);
}
