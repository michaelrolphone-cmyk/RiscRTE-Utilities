/* Pure physical geometry checks supplement, never replace, the actual-app UI
 * fixture. Rectangles are half-open and stay within the capability dimensions. */
#include "lora_messages_native_layout.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static bool overlaps(lm_native_rect a,lm_native_rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
static void rect(lm_native_layout*l,lm_native_rect r){
 assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=l->width&&r.y+r.h<=l->height);
 assert(lm_native_hit(r,r.x,r.y)&&lm_native_hit(r,r.x+r.w-1,r.y+r.h-1));
 assert(!lm_native_hit(r,r.x-1,r.y)&&!lm_native_hit(r,r.x+r.w,r.y)&&!lm_native_hit(r,r.x,r.y-1)&&!lm_native_hit(r,r.x,r.y+r.h));
 assert(!lm_native_hit(r,INT_MIN,INT_MIN)&&!lm_native_hit(r,INT_MAX,INT_MAX));
}
static void group(lm_native_layout*l,lm_native_rect*r,unsigned count){for(unsigned i=0;i<count;i++){rect(l,r[i]);for(unsigned j=0;j<i;j++)assert(!overlaps(r[i],r[j]));}}
static void size(unsigned w,unsigned h){
 lm_native_layout l;assert(lm_native_layout_make(&l,(int)w,(int)h));
 lm_native_rect fixed[]={l.back,l.title,l.radio,l.state,l.notice,l.body,l.full_footer,l.entry,l.counter,l.detail_state,l.detail_info};
 for(unsigned i=0;i<sizeof(fixed)/sizeof(*fixed);i++)rect(&l,fixed[i]);
 group(&l,l.home,4);group(&l,l.rf,3);group(&l,l.picker,4);group(&l,l.keys,35);group(&l,l.footer,2);group(&l,l.history,l.history_rows);
 assert(l.history_rows&&l.history_rows<=16&&l.compose_rows&&l.detail_rows&&l.line_height>0);
 assert(!overlaps(l.back,l.radio));
 for(unsigned i=0;i<35;i++){assert(!overlaps(l.keys[i],l.entry));assert(!overlaps(l.keys[i],l.counter));}
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<2;j++)assert(!overlaps(l.rf[i],l.footer[j]));
 for(unsigned i=0;i<l.history_rows;i++)for(unsigned j=0;j<2;j++)assert(!overlaps(l.history[i],l.footer[j]));
}
int main(void){
 const unsigned edges[]={160,180,240,410,502,1024};unsigned count=0;
 for(unsigned d=160;d<=1024;d++)for(unsigned i=0;i<sizeof(edges)/sizeof(*edges);i++){size(d,edges[i]);size(edges[i],d);count+=2;}
 lm_native_layout l;assert(!lm_native_layout_make(NULL,160,160));assert(!lm_native_layout_make(&l,159,160));assert(!lm_native_layout_make(&l,160,159));assert(!lm_native_layout_make(&l,1025,160));assert(!lm_native_layout_make(&l,160,1025));assert(!lm_native_layout_make(&l,INT_MIN,INT_MAX));
 printf("LoRa physical layout: %u dimension pairs, half-open edges, visible cells and non-overlapping action groups PASS\n",count);return 0;
}
