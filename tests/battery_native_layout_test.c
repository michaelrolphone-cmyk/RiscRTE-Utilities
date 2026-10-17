#include "../Apps/battery_native_layout.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static void rect(battery_native_rect r,int w,int h){assert(r.x>=0&&r.y>=0&&r.w>=0&&r.h>=0&&r.x+r.w<=w&&r.y+r.h<=h);}
static void check(int w,int h,unsigned controls){
 battery_native_layout l;assert(battery_native_layout_make(&l,w,h,controls));
 rect(l.back,w,h);rect(l.title,w,h);rect(l.content,w,h);
 assert(!battery_native_inside(l.back,-1,0));assert(!battery_native_inside(l.back,INT_MAX,INT_MAX));
 for(unsigned i=0;i<controls;i++){
  battery_native_rect r=l.footer[i];rect(r,w,h);
  assert(!battery_native_inside(r,r.x-1,r.y));assert(!battery_native_inside(r,r.x,r.y-1));
  assert(!battery_native_inside(r,r.x+r.w,r.y));assert(!battery_native_inside(r,r.x,r.y+r.h));
  if(r.w&&r.h){assert(battery_native_inside(r,r.x,r.y));assert(battery_native_inside(r,r.x+r.w-1,r.y+r.h-1));}
  if(i)assert(l.footer[i-1].x+l.footer[i-1].w<=r.x);
  assert(l.content.y+l.content.h<=r.y);
 }
}
int main(void){
 battery_native_layout l;
 assert(!battery_native_layout_make(NULL,240,240,2));
 assert(!battery_native_layout_make(&l,0,240,2));assert(!battery_native_layout_make(&l,240,-1,2));
 assert(!battery_native_layout_make(&l,65536,240,2));assert(!battery_native_layout_make(&l,240,65536,2));
 assert(!battery_native_layout_make(&l,240,240,0));assert(!battery_native_layout_make(&l,240,240,4));
 for(int w=1;w<=256;w++)for(int h=1;h<=256;h++)for(unsigned c=1;c<=3;c++)check(w,h,c);
 int sizes[][2]={{410,502},{502,410},{320,480},{480,320},{800,480},{65535,65535},{1,65535},{65535,1}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)for(unsigned c=1;c<=3;c++)check(sizes[i][0],sizes[i][1],c);
 puts("Battery native layout: 196632 dimension/control cases, edge hit and bounded rectangle checks PASS");
}
