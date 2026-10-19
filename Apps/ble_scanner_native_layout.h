#ifndef BLE_SCANNER_NATIVE_LAYOUT_H
#define BLE_SCANNER_NATIVE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {int x,y,w,h;} scan_native_rect;
typedef struct {
 int width,height,row_height,line_height;unsigned rows,lines;
 scan_native_rect back,title,status,list,details,footer[2];
} scan_native_layout;
static inline int scan_native_min(int a,int b){return a<b?a:b;}
static inline bool scan_native_hit(scan_native_rect r,int x,int y){return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&(int64_t)x<r.x+(int64_t)r.w&&(int64_t)y<r.y+(int64_t)r.h;}
static inline scan_native_rect scan_native_row(scan_native_layout l,unsigned row){return (scan_native_rect){l.list.x,l.list.y+(int)row*l.row_height,l.list.w,l.row_height-2};}
static inline bool scan_native_layout_make(scan_native_layout*l,int w,int h){
 if(!l||w<64||h<64||w>UINT16_MAX||h>UINT16_MAX)return false;
 *l=(scan_native_layout){.width=w,.height=h};
 int margin=w>=240?8:4,gap=4,header=h>=360?52:h>=176?40:24;
 int footer=h>=176?40:h>=96?24:16,status=h>=176?20:h>=96?16:8;
 header=scan_native_min(header,(w-2*margin-gap)/2);
 l->back=(scan_native_rect){margin,0,header,header};
 l->title=(scan_native_rect){margin+header+gap,0,w-2*margin-header-gap,header};
 l->status=(scan_native_rect){margin,header,w-2*margin,status};
 int bottom=h-footer-gap,top=header+status+gap;
 l->list=(scan_native_rect){margin,top,w-2*margin,bottom>top?bottom-top:0};
 l->row_height=h>=176?44:32;
 l->rows=(unsigned)(l->list.h/l->row_height);if(!l->rows)l->rows=1;
 if(l->list.h<l->row_height)l->row_height=l->list.h;
 l->details=(scan_native_rect){margin,header+gap,w-2*margin,bottom-header-gap};
 l->line_height=h>=176?21:16;l->lines=(unsigned)(l->details.h/l->line_height);if(!l->lines)l->lines=1;
 int fw=(w-2*margin-gap)/2;
 l->footer[0]=(scan_native_rect){margin,h-footer,fw,footer};
 l->footer[1]=(scan_native_rect){margin+fw+gap,h-footer,w-margin-(margin+fw+gap),footer};
 return true;
}
#endif
