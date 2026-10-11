#pragma once
/* Nova7 paper presenter. Uses the pinned shared typography and Font Awesome
 * renderer; application models and the Watch presentation remain shared. */
#include "PaperPresentation.h"
#include "RiscRuntimeV1.h"
#include <stdio.h>
static const paper_presentation *utility_paper;
static const t5_app_api_v1 *utility_api;
static uint32_t utility_previous_buttons;
static bool utility_buttons_armed,utility_contact_down;
static inline int up_x(int x){return x*utility_api->screen_width()/480;}
static inline int up_y(int y){return y*utility_api->screen_height()/800;}
static inline void up_open(const t5_app_api_v1 *api) {
 utility_api=api;utility_paper=paper_presentation_get();
 utility_previous_buttons=0;utility_buttons_armed=utility_contact_down=false;
}
static inline void up_rect(int x,int y,int w,int h,bool black){utility_api->fill_rect(up_x(x),up_y(y),up_x(w),up_y(h),black);}
static inline void up_text(int x,int y,int w,const char *s,bool heading,bool black) {
 utility_paper->text(up_x(x),up_y(y),up_x(w),s,1,heading,black);
}
static inline void up_center(int y,const char *s,bool large) {
 int w=utility_paper->measure(s,!large),zoom=large&&w*3<=up_x(416)?3:1;
 utility_paper->text((utility_api->screen_width()-w*zoom)/2,up_y(y),up_x(416),s,1u|(zoom==3?PAPER_TEXT_CLOCK:0),!large,true);
}
static inline void up_wrap(int y,const char *s) {
 if(!s)return;
 for(unsigned row=0;row<3&&*s;row++) {
  char line[97]={0};unsigned n=0;
  while(n<96&&s[n]){line[n]=s[n];line[n+1]=0;if(utility_paper->measure(line,false)>up_x(416)){line[n]=0;break;}n++;}
  if(!n)break;
  up_text(32,y+(int)row*30,416,line,false,true);s+=n;
 }
}
static inline void up_button(int x,int y,int w,int h,const char *label,const char *icon,bool selected) {
 up_rect(x,y,w,h,true);if(!selected)up_rect(x+3,y+3,w-6,h-6,false);
 int text=utility_paper->measure(label,false),available=up_x(w-12);
 int left=up_x(x)+(up_x(w)-(text<available?text:available))/2;
 if(icon&&utility_api->draw_icon)utility_api->draw_icon(up_x(x)+(up_x(w)-24)/2,up_y(y+12),icon,24,!selected);
 utility_paper->text(left,up_y(y+(icon?48:(h-26)/2)),available,label,1|PAPER_TEXT_LITERAL,false,!selected);
}
static inline bool up_hit(int x,int y,int left,int top,int w,int h) {
 return x>=up_x(left)&&x<up_x(left+w)&&y>=up_y(top)&&y<up_y(top+h);
}
static inline void up_begin(const char *title) {
 utility_paper->begin();up_text(32,30,416,title,true,true);
 up_rect(32,88,416,3,true);
}
/* Consume only complete, eligible releases. Never synthesize actions from a
 * held contact, interrupted gesture or the stale contact behind a Home event. */
static inline void up_input(t5_app_input_t *in) {
 if(in->exit_requested)return;
 uint32_t buttons=in->buttons;
 if(!buttons)utility_buttons_armed=true;
 in->buttons=utility_buttons_armed?(buttons&~utility_previous_buttons):0;
 utility_previous_buttons=buttons;
 springboard_contact c={0};utility_paper->contact(&c);
 if(!c.valid||c.cancelled||!c.tap_eligible)utility_contact_down=false;
 if(c.valid&&c.down&&c.began&&c.tap_eligible&&!c.cancelled)utility_contact_down=true;
 in->tapped=utility_contact_down&&c.valid&&c.released&&c.tap_eligible&&!c.cancelled;
 if(c.released)utility_contact_down=false; /* Modal return may expose the last release again. */
 if(in->tapped){in->touch_x=c.x;in->touch_y=c.y;in->buttons&=~T5_APP_BUTTON_CONFIRM;}
}
static inline bool up_return(void) {
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
 return rt&&rt->request_launch&&rt->request_launch("springboard.elf");
}
