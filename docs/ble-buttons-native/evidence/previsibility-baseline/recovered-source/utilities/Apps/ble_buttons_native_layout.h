#ifndef BLE_BUTTONS_NATIVE_LAYOUT_H
#define BLE_BUTTONS_NATIVE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
/* App-local capability pixels. No logical viewport or completed-frame scaling. */
typedef struct { int x,y,w,h; } hid_native_rect;
typedef struct {
 int width,height,gap;
 hid_native_rect back,title,content,footer[2],status,buttons[4];
 hid_native_rect setting_pair[2],setting_forget,setting_buttons[4];
 hid_native_rect edit_kind,edit_prev,edit_value,edit_next,edit_modifiers,modifiers[8];
} hid_native_layout;
static inline int hid_native_min(int a,int b){return a<b?a:b;}
static inline bool hid_native_inside(hid_native_rect r,int x,int y){
 return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&(int64_t)x<(int64_t)r.x+r.w&&(int64_t)y<(int64_t)r.y+r.h;
}
static inline hid_native_rect hid_native_cell(hid_native_rect r,unsigned i,unsigned cols,unsigned rows,int gap){
 if(!cols||!rows||i>=cols*rows||r.w<=0||r.h<=0)return (hid_native_rect){0};
 int gx=hid_native_min(gap,r.w/(int)cols),gy=hid_native_min(gap,r.h/(int)rows);
 int w=r.w-gx*((int)cols-1),h=r.h-gy*((int)rows-1),col=(int)(i%cols),row=(int)(i/cols);
 int x0=col*w/(int)cols,y0=row*h/(int)rows;
 return (hid_native_rect){r.x+x0+col*gx,r.y+y0+row*gy,(col+1)*w/(int)cols-x0,(row+1)*h/(int)rows-y0};
}
static inline bool hid_native_layout_make(hid_native_layout*l,int w,int h){
 if(!l||w<1||h<1||w>UINT16_MAX||h>UINT16_MAX)return false;
 *l=(hid_native_layout){.width=w,.height=h};
 int m=w>=240?8:w>=64?4:0,gap=h>=176?8:h>=64?4:0;
 int header=h>=360?56:h>=176?44:h/5,foot=header;
 if(header<1)header=1;
 if(header>h)header=h;
 if(foot>h-header)foot=h-header;
 int back=hid_native_min(header,w/3);if(back<1)back=1;
 l->gap=gap;l->back=(hid_native_rect){m,0,back,header};
 l->title=(hid_native_rect){m+back+gap,0,w-2*m-back-gap,header};
 int first=hid_native_min(header+gap,h-foot),last=h-foot-gap;
 l->content=(hid_native_rect){m,first,w-2*m,last>first?last-first:0};
 hid_native_rect f={m,h-foot,w-2*m,foot};
 for(unsigned i=0;i<2;i++)l->footer[i]=hid_native_cell(f,i,2,1,gap);
 hid_native_rect c=l->content;
 int status=hid_native_min(c.h/3,h>=176?24:16);
 l->status=(hid_native_rect){c.x,c.y,c.w,status};
 int split=hid_native_min(status+gap/2,c.h);
 hid_native_rect b={c.x,c.y+split,c.w,c.h-split};
 for(unsigned i=0;i<4;i++)l->buttons[i]=hid_native_cell(b,i,2,2,gap);
 hid_native_rect row=hid_native_cell(c,0,1,3,gap);
 for(unsigned i=0;i<2;i++)l->setting_pair[i]=hid_native_cell(row,i,2,1,gap);
 l->setting_forget=hid_native_cell(c,1,1,3,gap);
 row=hid_native_cell(c,2,1,3,gap);
 for(unsigned i=0;i<4;i++)l->setting_buttons[i]=hid_native_cell(row,i,4,1,gap);
 l->edit_kind=hid_native_cell(c,0,1,3,gap);l->edit_modifiers=hid_native_cell(c,2,1,3,gap);
 row=hid_native_cell(c,1,1,3,gap);int arrow=hid_native_min(row.h,row.w/4);
 l->edit_prev=(hid_native_rect){row.x,row.y,arrow,row.h};
 l->edit_next=(hid_native_rect){row.x+row.w-arrow,row.y,arrow,row.h};
 l->edit_value=(hid_native_rect){row.x+arrow,row.y,row.w-2*arrow,row.h};
 hid_native_rect mods={c.x,c.y,c.w,h-c.y};
 for(unsigned i=0;i<8;i++)l->modifiers[i]=hid_native_cell(mods,i,2,4,gap);
 return true;
}
#endif
