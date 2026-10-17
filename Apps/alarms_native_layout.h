#ifndef ALARMS_NATIVE_LAYOUT_H
#define ALARMS_NATIVE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int x,y,w,h; } alarms_native_rect;
typedef struct {
 int width,height;
 alarms_native_rect back,title,retry,content,primary,secondary,status;
 alarms_native_rect fields[3],label,minus,value,plus,message,footer[2],done;
} alarms_native_layout;
static inline bool alarms_native_inside(alarms_native_rect r,int x,int y){
 return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&(int64_t)x<(int64_t)r.x+r.w&&(int64_t)y<(int64_t)r.y+r.h;
}
static inline bool alarms_native_layout_make(alarms_native_layout *l,int w,int h,unsigned count){
 if(!l||w<120||h<128||w>UINT16_MAX||h>UINT16_MAX||count<2||count>3)return false;
 *l=(alarms_native_layout){.width=w,.height=h};
 int m=w>=240?8:4,g=m,top=h>=176?44:28,foot=h>=176?44:28;
 int chrome=w>=200?44:28,body=h-top-foot-2*m;
 l->back=(alarms_native_rect){m,0,chrome,top};
 l->retry=(alarms_native_rect){w-m-chrome,0,chrome,top};
 l->title=(alarms_native_rect){m+chrome,0,w-2*m-2*chrome,top};
 l->content=(alarms_native_rect){m,top+m,w-2*m,body};
 int fw=(w-2*m-g)/2;
 l->footer[0]=(alarms_native_rect){m,h-foot,fw,foot};
 l->footer[1]=(alarms_native_rect){m+fw+g,h-foot,w-m-(m+fw+g),foot};
 l->done=(alarms_native_rect){m,h-foot,w-2*m,foot};
 int state=body>=92?28:12,rows=body-state-2*g;
 if(rows<2)rows=2;
 int first=rows/2;
 l->primary=(alarms_native_rect){m,top+m,w-2*m,first};
 l->secondary=(alarms_native_rect){m,top+m+first+g,w-2*m,rows-first};
 l->status=(alarms_native_rect){m,top+m+rows+2*g,w-2*m,state};
 int fieldspace=body-g*((int)count-1);
 for(unsigned i=0;i<count;i++){
  int y0=(int)i*fieldspace/(int)count,y1=(int)(i+1)*fieldspace/(int)count;
  l->fields[i]=(alarms_native_rect){m,top+m+y0+(int)i*g,w-2*m,y1-y0};
 }
 int label=body>=100?20:12,message=body>=80?28:12;
 int middle=body-label-message-2*g;if(middle<12)middle=12;
 int btn=(w-2*m)/4;if(btn>56)btn=56;
 int cy=top+m+label+g;
 l->label=(alarms_native_rect){m,top+m,w-2*m,label};
 l->minus=(alarms_native_rect){m,cy,btn,middle};
 l->plus=(alarms_native_rect){w-m-btn,cy,btn,middle};
 l->value=(alarms_native_rect){m+btn+g,cy,w-2*m-2*btn-2*g,middle};
 l->message=(alarms_native_rect){m,cy+middle+g,w-2*m,message};
 return true;
}
#endif
