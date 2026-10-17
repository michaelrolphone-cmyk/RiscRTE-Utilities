#ifndef BATTERY_NATIVE_LAYOUT_H
#define BATTERY_NATIVE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
/* Battery-local capability-pixel geometry. Other Utilities keep their profiles. */
typedef struct { int x,y,w,h; } battery_native_rect;
typedef struct {
 int width,height,margin;
 battery_native_rect back,title,content,footer[3];
} battery_native_layout;
static inline int battery_native_min(int a,int b){return a<b?a:b;}
static inline bool battery_native_inside(battery_native_rect r,int x,int y){
 return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&(int64_t)x<((int64_t)r.x+r.w)&&(int64_t)y<((int64_t)r.y+r.h);
}
static inline bool battery_native_layout_make(battery_native_layout *l,int w,int h,unsigned controls){
 if(!l||w<1||h<1||w>UINT16_MAX||h>UINT16_MAX||controls<1||controls>3)return false;
 *l=(battery_native_layout){.width=w,.height=h};
 int m=w>=240?8:w>=128?4:0;
 int top=h>=176?44:h/4,foot=h>=176?44:h/4;
 if(top<1)top=1;
 if(foot<1)foot=1;
 if(top>h)top=h;
 if(foot>h-top)foot=h-top;
 int back=battery_native_min(44,w/3);if(back<1)back=1;
 l->margin=m;l->back=(battery_native_rect){0,0,back,top};
 l->title=(battery_native_rect){back+m,0,w-back-m,top};
 int first=battery_native_min(top+m,h-foot),last=h-foot-m;
 l->content=(battery_native_rect){m,first,w-2*m,last>first?last-first:0};
 int available=w-2*m,gap=controls>1?m:0;
 int usable=available-gap*((int)controls-1);if(usable<0)usable=0;
 for(unsigned i=0;i<controls;i++){
  int x0=m+(int)i*usable/(int)controls+(int)i*gap;
  int x1=m+(int)(i+1)*usable/(int)controls+(int)i*gap;
  l->footer[i]=(battery_native_rect){x0,h-foot,x1-x0,foot};
 }
 return true;
}
#endif
