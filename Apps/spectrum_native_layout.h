#ifndef SPECTRUM_NATIVE_LAYOUT_H
#define SPECTRUM_NATIVE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

/* Audio Spectrum presentation geometry only. Analysis bins, history capacity,
 * storage encodings and provider ownership do not depend on display size. */
typedef struct { int x,y,w,h; } spectrum_native_rect;
typedef struct {
 int width,height,margin,gap,header_height,footer_height;
 spectrum_native_rect header,content,footer,plot;
 spectrum_native_rect tabs[4],actions[2];
} spectrum_native_layout;

static inline int spectrum_native_min(int a,int b){return a<b?a:b;}
static inline bool spectrum_native_hit(spectrum_native_rect r,int x,int y){
 return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&
        (int64_t)x<(int64_t)r.x+r.w&&(int64_t)y<(int64_t)r.y+r.h;
}
static inline spectrum_native_rect spectrum_native_column(spectrum_native_rect r,unsigned index,unsigned count,int gap){
 if(!count||index>=count||r.w<=0||r.h<=0)return (spectrum_native_rect){0};
 if(gap<0)gap=0;
 if((uint64_t)gap*(count-1)>=(uint64_t)r.w)gap=0;
 int usable=r.w-(int)((uint64_t)gap*(count-1));
 int left=(int)((uint64_t)(unsigned)usable*index/count);
 int right=(int)((uint64_t)(unsigned)usable*(index+1)/count);
 return (spectrum_native_rect){r.x+left+(int)((uint64_t)gap*index),r.y,right-left,r.h};
}
static inline bool spectrum_native_layout_make(spectrum_native_layout *l,int width,int height){
 if(!l)return false;
 *l=(spectrum_native_layout){0};
 if(width<=0||height<=0)return false;
 l->width=width;l->height=height;
 int smaller=spectrum_native_min(width,height);
 l->margin=spectrum_native_min(12,smaller/40);
 l->gap=spectrum_native_min(6,smaller/80);
 l->header_height=spectrum_native_min(44,height/4);
 l->footer_height=spectrum_native_min(44,height/4);
 l->header=(spectrum_native_rect){0,0,width,l->header_height};
 l->footer=(spectrum_native_rect){0,height-l->footer_height,width,l->footer_height};
 l->content=(spectrum_native_rect){l->margin,l->header_height+l->gap,
  width-2*l->margin,height-l->header_height-l->footer_height-2*l->gap};
 l->plot=l->content;
 for(unsigned i=0;i<4;i++)l->tabs[i]=spectrum_native_column(l->header,i,4,l->gap);
 for(unsigned i=0;i<2;i++)l->actions[i]=spectrum_native_column(l->footer,i,2,l->gap);
 return true;
}
/* Map an existing finite analysis axis into a native pixel interval, including
 * both endpoints. No framebuffer or raster rescaling is involved. */
static inline unsigned spectrum_native_sample(unsigned pixel,unsigned pixels,unsigned samples){
 if(samples<2||pixels<2)return 0;
 if(pixel>=pixels)pixel=pixels-1;
 return (unsigned)((uint64_t)pixel*(samples-1)/(pixels-1));
}
static inline int spectrum_native_axis(unsigned sample,unsigned samples,int pixels){
 if(pixels<2||samples<2)return 0;
 if(sample>=samples)sample=samples-1;
 return (int)((uint64_t)sample*(unsigned)(pixels-1)/(samples-1));
}
#endif
