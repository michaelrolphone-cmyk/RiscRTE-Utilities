/* Selected native RGB layout only. RTC/UTC, persistence and alarm ownership
 * remain in their existing controllers and the shared adapter. */
#include "PortableStopwatchNative.h"
#if !defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_NATIVE_CANVAS) || !defined(PORTABLE_APP_OWNS_TOUCH_CHROME) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE)
#error Native Stopwatch requires NOVA, native canvas, app-owned touch chrome and checked custody
#endif
typedef struct { int x,y,w,h; } stopwatch_native_rect;
enum { STOPWATCH_TOGGLE,STOPWATCH_RESET,STOPWATCH_RETRY,STOPWATCH_BACK };
typedef struct {
 int width,height;
 stopwatch_native_rect controls[4],title,value,fraction,state,status;
} stopwatch_native_layout;
static stopwatch_native_layout stopwatch_native_geometry;
static bool stopwatch_native_active,stopwatch_native_contact;
static int stopwatch_native_pressed;
static uint64_t stopwatch_native_input_generation;
static bool stopwatch_native_inside(stopwatch_native_rect r,int x,int y) {
 return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;
}
static bool stopwatch_native_open(int w,int h) {
 if(w<160||h<160||w>1024||h>1024)return false;
 stopwatch_native_layout l={.width=w,.height=h};
 int m=w<200?4:8,gap=w<200?3:6,header=h<200?26:40,button=h<200?32:48;
 l.controls[STOPWATCH_BACK]=(stopwatch_native_rect){m,m,header,header};
 l.title=(stopwatch_native_rect){m+header+gap,m,w-2*m-header-gap,header};
 int bottom=h-m-button;
 for(int i=0;i<3;i++){
  int left=m+(w-2*m+gap)*i/3,right=m+(w-2*m+gap)*(i+1)/3-gap;
  l.controls[i]=(stopwatch_native_rect){left,bottom,right-left,button};
 }
 int line=h<200?12:20,status_height=h<200?24:36;
 int top=m+header+gap,available=bottom-gap-top;
 int number=available-2*line-status_height;if(number>80)number=80;
 if(number<20)return false;
 int y=top+(available-number-2*line-status_height)/2;
 l.value=(stopwatch_native_rect){m,y,w-2*m,number};y+=number;
 l.fraction=(stopwatch_native_rect){m,y,w-2*m,line};y+=line;
 l.state=(stopwatch_native_rect){m,y,w-2*m,line};y+=line;
 l.status=(stopwatch_native_rect){m,y,w-2*m,status_height};
 stopwatch_native_geometry=l;return true;
}
static int stopwatch_native_target(int x,int y) {
 stopwatch_native_layout *l=&stopwatch_native_geometry;
 if(x<0||y<0||x>=l->width||y>=l->height)return -1;
 for(int i=0;i<4;i++)if(stopwatch_native_inside(l->controls[i],x,y))return i;
 return -1;
}
static void stopwatch_native_label(stopwatch_native_rect r,const char *s,uint32_t color,bool numeric) {
 unsigned faces[]={4,0,2,6};int heights[]={36,20,16,10};
 for(unsigned i=numeric?0:1;i<4;i++){
  unsigned face=faces[i];int width=portable_nova_measure(face,s);
  if(heights[i]<=r.h&&width<=r.w-4){
   portable_nova_native_text(face,r.x+(r.w-width)/2,r.y+(r.h-heights[i])/2,width,s,color);return;
  }
 }
}
static void stopwatch_native_status(stopwatch_native_rect r,const char *s) {
 /* Two bounded native text lines preserve the complete diagnostic even at
  * 160 pixels. No fixed viewport or scaled bitmap is used. */
 unsigned face=r.h>=32?2:6;int line=face==2?16:10;
 if(portable_nova_measure(face,s)<=r.w-4){stopwatch_native_label(r,s,NOVA_CAP,false);return;}
 char first[64],second[64];size_t n=strlen(s);if(n>=sizeof(first))n=sizeof(first)-1;
 memcpy(first,s,n);first[n]=0;
 while(n&&portable_nova_measure(face,first)>r.w-4)first[--n]=0;
 size_t split=n;while(split&&s[split]!=' ')--split;if(split)n=split;
 first[n]=0;while(s[n]==' ')++n;
 size_t rest=strlen(s+n);if(rest>=sizeof(second))rest=sizeof(second)-1;
 memcpy(second,s+n,rest);second[rest]=0;
 stopwatch_native_rect a=r,b=r;a.h=b.h=line;a.y=r.y+(r.h-2*line)/2;b.y=a.y+line;
 stopwatch_native_label(a,first,NOVA_CAP,false);stopwatch_native_label(b,second,NOVA_CAP,false);
}
static void stopwatch_native_button(stopwatch_native_rect r,const char *s,bool selected) {
 portable_nova_native_round(r.x,r.y,r.w,r.h,4,selected?NOVA_CYAN:NOVA_DIM);
 portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,3,0x031115u);
 stopwatch_native_label(r,s,selected?NOVA_WHITE:NOVA_TEXT,false);
}
static void stopwatch_native_mapping(void) {
 portable_stopwatch_native_layout(((uint64_t)(unsigned)stopwatch_native_geometry.width<<32)|(unsigned)stopwatch_native_geometry.height);
}
static void stopwatch_native_render(void) {
 stopwatch_native_layout *l=&stopwatch_native_geometry;char value[12],whole[9];
 sw_format(clock_state.elapsed_ms,value);const char *fraction=value;
 while(*fraction&&*fraction!='.')++fraction;
 size_t length=(size_t)(fraction-value);memcpy(whole,value,length);whole[length]=0;
 portable_nova_begin();portable_nova_native_fill(0,0,l->width,l->height,0x010708u);
 stopwatch_native_button(l->controls[STOPWATCH_BACK],"<",false);
 stopwatch_native_label(l->title,"STOPWATCH",NOVA_TEXT,false);
 stopwatch_native_label(l->value,whole,NOVA_CYAN,true);
 stopwatch_native_label(l->fraction,fraction,NOVA_TEXT,false);
 stopwatch_native_label(l->state,clock_state.running?"Running":loaded?"Paused":"Storage error",NOVA_CAP,false);
 if(status&&strcmp(status,"RUNNING")&&strcmp(status,"PAUSED"))stopwatch_native_status(l->status,status);
 stopwatch_native_button(l->controls[STOPWATCH_TOGGLE],pending_pause?"Save":clock_state.running?"Pause":"Start",clock_state.running);
 stopwatch_native_button(l->controls[STOPWATCH_RESET],reset_armed?"Confirm":"Reset",reset_armed);
 stopwatch_native_button(l->controls[STOPWATCH_RETRY],"Retry",false);
 portable_stopwatch_native_present(false);
}
static void stopwatch_native_input(t5_app_input_t *in) {
 uint64_t generation=0;bool ready=portable_stopwatch_native_ready(&generation,NULL);
 if(generation!=stopwatch_native_input_generation){stopwatch_native_contact=false;stopwatch_native_pressed=-1;stopwatch_native_input_generation=generation;}
 t5_app_contact_t sample={0};bool ok=app->touch_contact(&sample);
 if(!ready||!ok){stopwatch_native_contact=false;stopwatch_native_pressed=-1;in->tapped=false;return;}
 if(sample.down){int target=stopwatch_native_target(sample.x,sample.y);if(!stopwatch_native_contact){stopwatch_native_contact=true;stopwatch_native_pressed=target;}else if(target!=stopwatch_native_pressed)stopwatch_native_pressed=-1;}
 if(in->tapped){int target=stopwatch_native_target(in->touch_x,in->touch_y);if(!stopwatch_native_contact||target!=stopwatch_native_pressed)in->tapped=false;stopwatch_native_contact=false;stopwatch_native_pressed=-1;}
 else if(!sample.down){stopwatch_native_contact=false;stopwatch_native_pressed=-1;}
 if(in->tapped&&stopwatch_native_target(in->touch_x,in->touch_y)==STOPWATCH_BACK){portable_stopwatch_native_back();in->tapped=false;}
}
static bool stopwatch_native_select(void) {
 stopwatch_native_active=portable_stopwatch_native_available();stopwatch_native_contact=false;
 stopwatch_native_pressed=-1;stopwatch_native_input_generation=0;
 if(!stopwatch_native_active)return true;
 if(app->struct_size<offsetof(t5_app_api_v1,touch_contact)+sizeof(app->touch_contact)||!app->touch_contact||
    !stopwatch_native_open(app->screen_width(),app->screen_height()))return false;
 portable_stopwatch_native_begin();return true;
}
