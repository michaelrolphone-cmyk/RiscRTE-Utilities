#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "../Apps/spectrum_native_layout.h"

static void bounded(spectrum_native_rect r,int w,int h){
 assert(r.x>=0&&r.y>=0&&r.w>=0&&r.h>=0);
 assert((int64_t)r.x+r.w<=w&&(int64_t)r.y+r.h<=h);
 if(r.w&&r.h){
  assert(spectrum_native_hit(r,r.x,r.y));
  assert(spectrum_native_hit(r,r.x+r.w-1,r.y+r.h-1));
  assert(!spectrum_native_hit(r,r.x+r.w,r.y));
  assert(!spectrum_native_hit(r,r.x,r.y+r.h));
 }
}
static void check(int w,int h){
 spectrum_native_layout l;assert(spectrum_native_layout_make(&l,w,h));
 bounded(l.header,w,h);bounded(l.content,w,h);bounded(l.footer,w,h);bounded(l.plot,w,h);
 assert(l.header.y+l.header.h<=l.content.y);
 assert(l.content.y+l.content.h<=l.footer.y);
 for(unsigned i=0;i<4;i++){
  bounded(l.tabs[i],w,h);
  if(i)assert(l.tabs[i-1].x+l.tabs[i-1].w<=l.tabs[i].x);
 }
 for(unsigned i=0;i<2;i++)bounded(l.actions[i],w,h);
 const unsigned bins[]={2,166,228,8192,UINT_MAX};
 for(unsigned i=0;i<5;i++){
  assert(spectrum_native_sample(0,(unsigned)w,bins[i])==0);
  if(w>1)assert(spectrum_native_sample((unsigned)w-1,(unsigned)w,bins[i])==bins[i]-1);
  assert(spectrum_native_axis(0,bins[i],w)==0);
  assert(spectrum_native_axis(bins[i]-1,bins[i],w)==w-1);
 }
}
int main(void){
 spectrum_native_layout l;
 assert(!spectrum_native_layout_make(NULL,1,1));
 assert(!spectrum_native_layout_make(&l,0,1)&&!l.width);
 assert(!spectrum_native_layout_make(&l,1,-1)&&!l.height);
 const int sizes[][2]={{1,1},{80,64},{160,160},{240,240},{410,502},{502,410},{480,800},{800,480},{640,240},{240,640},{INT_MAX,INT_MAX},{INT_MAX,1}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)check(sizes[i][0],sizes[i][1]);
 uint32_t seed=0x42a87315u;
 for(unsigned i=0;i<100000;i++){
  seed=seed*1664525u+1013904223u;int w=(int)(seed%65535u)+1;
  seed=seed*1664525u+1013904223u;int h=(int)(seed%65535u)+1;check(w,h);
 }
 assert(!spectrum_native_hit((spectrum_native_rect){0,0,4,4},-1,0));
 assert(!spectrum_native_column((spectrum_native_rect){0,0,4,4},0,0,1).w);
 assert(!spectrum_native_column((spectrum_native_rect){0,0,4,4},4,4,1).w);
 puts("Spectrum native geometry: 12 profiles and 100000 deterministic dimensions; bounded half-open targets, nonoverlap and analysis-axis endpoints passed");
}
