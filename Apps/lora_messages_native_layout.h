#ifndef LORA_MESSAGES_NATIVE_LAYOUT_H
#define LORA_MESSAGES_NATIVE_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {int x,y,w,h;} lm_native_rect;
typedef struct {
 int width,height,line_height;unsigned history_rows,compose_rows,detail_rows;
 lm_native_rect back,title,radio,state,notice,home[4],body,footer[2],full_footer;
 lm_native_rect rf[3],history[16],picker[4],keys[35],entry,counter,detail_state,detail_info;
} lm_native_layout;
static inline bool lm_native_hit(lm_native_rect r,int x,int y){return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&(int64_t)x<r.x+(int64_t)r.w&&(int64_t)y<r.y+(int64_t)r.h;}
static inline bool lm_native_layout_make(lm_native_layout*l,int w,int h){
 if(!l||w<160||h<160||w>1024||h>1024)return false;
 *l=(lm_native_layout){.width=w,.height=h};
 int m=w<200?4:8,g=4,head=h>=360?48:h>=220?36:28;
 int foot=h>=360?48:h>=220?40:28,line=h>=220?20:14;
 int content=w-2*m,top=head+g,bottom=h-m-foot-g;
 l->line_height=line;
 l->back=(lm_native_rect){m,0,head,head};
 l->radio=(lm_native_rect){w-m-(w<200?42:60),0,w<200?42:60,head};
 l->title=(lm_native_rect){m+head+g,0,content-head-g,head};
 l->state=(lm_native_rect){m,top,content,line};
 l->notice=(lm_native_rect){m,top+line,content,line};
 int half=(content-g)/2;
 l->footer[0]=(lm_native_rect){m,h-m-foot,half,foot};
 l->footer[1]=(lm_native_rect){m+half+g,h-m-foot,content-half-g,foot};
 l->full_footer=(lm_native_rect){m,h-m-foot,content,foot};
 int home_top=top+2*line+g,home_h=(h-m-home_top-2*g)/3;
 l->home[0]=(lm_native_rect){m,home_top,content,home_h};
 l->home[1]=(lm_native_rect){m,home_top+home_h+g,content,home_h};
 l->home[2]=(lm_native_rect){m,home_top+2*(home_h+g),half,h-m-(home_top+2*(home_h+g))};
 l->home[3]=(lm_native_rect){m+half+g,l->home[2].y,content-half-g,l->home[2].h};
 l->body=(lm_native_rect){m,top,content,bottom-top-2*line};
 l->compose_rows=(unsigned)(l->body.h/line);if(!l->compose_rows)l->compose_rows=1;
 l->detail_state=(lm_native_rect){m,top+line,content,line};
 l->detail_info=(lm_native_rect){m,bottom-2*line,content,2*line};
 l->detail_rows=(unsigned)((bottom-top-4*line)/line);if(!l->detail_rows)l->detail_rows=1;
 for(int i=0;i<3;i++){int y=top+(bottom-top)*i/3,yy=top+(bottom-top)*(i+1)/3;l->rf[i]=(lm_native_rect){m,y,content,yy-y-g};}
 int rh=h>=220?44:30;l->history_rows=(unsigned)((bottom-top)/rh);if(!l->history_rows)l->history_rows=1;if(l->history_rows>16)l->history_rows=16;
 for(unsigned i=0;i<l->history_rows;i++)l->history[i]=(lm_native_rect){m,top+(int)i*rh,content,rh-g};
 int pick_bottom=h-m-line;
 for(int i=0;i<4;i++){int y=top+(pick_bottom-top)*i/4,yy=top+(pick_bottom-top)*(i+1)/4;l->picker[i]=(lm_native_rect){m,y,content,yy-y-2};}
 l->entry=(lm_native_rect){m,top,content,line};
 l->counter=(lm_native_rect){m,h-m-line,content,line};
 int key_top=top+line+g,key_bottom=h-m-line-g,actions=h>=220?30:22;
 for(int i=0;i<32;i++){int x=m+content*(i%8)/8,xx=m+content*(i%8+1)/8,y=key_top+(key_bottom-key_top-actions-g)*(i/8)/4,yy=key_top+(key_bottom-key_top-actions-g)*(i/8+1)/4;l->keys[i]=(lm_native_rect){x,y,xx-x-2,yy-y-2};}
 for(int i=0;i<3;i++){int x=m+content*i/3,xx=m+content*(i+1)/3;l->keys[32+i]=(lm_native_rect){x,key_bottom-actions,xx-x-2,actions};}
 return true;
}
#endif
