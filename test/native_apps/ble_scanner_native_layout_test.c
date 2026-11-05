#include "ble_scanner_native_layout.h"
#include <assert.h>
#include <stdio.h>
static void bounds(scan_native_rect r,int w,int h){assert(r.x>=0&&r.y>=0&&r.w>=0&&r.h>=0&&r.x+r.w<=w&&r.y+r.h<=h);}
int main(void){
 unsigned count=0;
 for(int w=64;w<=800;w+=7)for(int h=64;h<=800;h+=11){
  scan_native_layout l;assert(scan_native_layout_make(&l,w,h));
  bounds(l.back,w,h);bounds(l.title,w,h);bounds(l.status,w,h);bounds(l.list,w,h);bounds(l.details,w,h);
  assert(l.rows&&l.lines&&l.row_height>=2&&l.line_height>0);
  for(unsigned i=0;i<l.rows;i++){scan_native_rect r=scan_native_row(l,i);bounds(r,w,h);assert(scan_native_hit(r,r.x,r.y));assert(!scan_native_hit(r,r.x+r.w,r.y));assert(!scan_native_hit(r,r.x,r.y+r.h));assert(r.y+r.h<=l.list.y+l.list.h);}
  for(unsigned i=0;i<2;i++){scan_native_rect r=l.footer[i];bounds(r,w,h);assert(scan_native_hit(r,r.x+r.w/2,r.y+r.h/2));}
  assert(l.footer[0].x+l.footer[0].w<l.footer[1].x);
  assert(!scan_native_hit(l.footer[0],l.footer[0].x+l.footer[0].w,h-1));
  assert(!scan_native_hit(l.footer[1],l.footer[1].x-1,h-1));
  count++;
 }
 scan_native_layout l;assert(!scan_native_layout_make(NULL,240,240));assert(!scan_native_layout_make(&l,63,240));assert(!scan_native_layout_make(&l,240,65536));
 assert(scan_native_layout_make(&l,65535,65535));bounds(l.footer[1],65535,65535);
 printf("Scanner geometry: %u capability sizes and boundary hits passed\n",count);
}
