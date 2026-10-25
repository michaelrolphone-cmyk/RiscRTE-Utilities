#pragma once
#include <stdbool.h>
typedef struct { int x,y,width,height; } alarm_native_rect;
typedef struct {
 bool valid;
 alarm_native_rect back,title,value,caption,status,zone,date,edit,primary,cancel,retry;
 alarm_native_rect fields[3],minus,plus,ampm,done;
} alarm_native_layout;
static inline bool alarm_native_hit(alarm_native_rect r,int x,int y){return r.width>0&&r.height>0&&x>=r.x&&y>=r.y&&x-r.x<r.width&&y-r.y<r.height;}
/* All coordinates are native content-local pixels; System alone applies insets.
 * Zero rectangles are deliberately inactive on this page. */
static inline alarm_native_layout alarm_native_layout_make(int w,int h,unsigned kind,unsigned page){
 alarm_native_layout l={0};if(w<240||h<240||w>1024||h>1024||kind<1||kind>2||page>2)return l;
 l.valid=true;l.back=(alarm_native_rect){8,4,52,32};l.title=(alarm_native_rect){64,4,w-72,32};
 if(page==0){
  int extra=h-240,d=extra/3;if(d>20)d=20;
  l.value=(alarm_native_rect){8,38,w-16,36+extra-3*d};
  l.caption=(alarm_native_rect){8,74+extra-3*d,w-16,18};
  l.edit=(alarm_native_rect){8,94+extra-3*d,w-16,28+d};
  l.status=(alarm_native_rect){8,124+extra-2*d,w-16,18};
  l.zone=(alarm_native_rect){8,144+extra-2*d,w-16,18};
  l.date=(alarm_native_rect){8,164+extra-2*d,w-16,18};
  l.primary=(alarm_native_rect){8,h-56-2*d,(w-24)/2,28+d};
  l.cancel=(alarm_native_rect){16+(w-24)/2,h-56-2*d,w-24-(w-24)/2,28+d};
  l.retry=(alarm_native_rect){8,h-26-d,w-16,24+d};
 }else if(page==1){
  unsigned count=kind==1?2:3;int row=(h-100)/(int)count;
  for(unsigned i=0;i<count;i++)l.fields[i]=(alarm_native_rect){8,44+(int)i*row,w-16,row-6};
  l.done=(alarm_native_rect){8,h-44,w-16,36};
 }else{
  l.caption=(alarm_native_rect){8,42,w-16,24};l.value=(alarm_native_rect){8,68,w-16,h/2-72};
  l.minus=(alarm_native_rect){8,h/2,(w-24)/2,36};l.plus=(alarm_native_rect){16+(w-24)/2,h/2,w-24-(w-24)/2,36};
  l.ampm=(alarm_native_rect){8,h-80,w-16,32};l.done=(alarm_native_rect){8,h-44,w-16,36};
 }
 return l;
}
