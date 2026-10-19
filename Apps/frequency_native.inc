/* Frequency-only native RGB presentation. Tone generation, audio ownership,
 * limits and parameter updates remain in frequency_generator.c. */
#include "PortableFrequencyNative.h"
#if !defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_NATIVE_CANVAS) || !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
#error Native Frequency requires NOVA, native canvas and app-owned touch chrome
#endif
typedef struct { int x,y,w,h; } frequency_native_rect;
enum { FREQUENCY_LESS,FREQUENCY_MORE,FREQUENCY_STEP_10,FREQUENCY_STEP_100,
       FREQUENCY_STEP_1000,FREQUENCY_LEVEL_LESS,FREQUENCY_TOGGLE,FREQUENCY_LEVEL_MORE,FREQUENCY_BACK };
typedef struct {
 int width,height;
 frequency_native_rect controls[9],title,value,level,specification,status;
} frequency_native_layout;
static frequency_native_layout frequency_native_geometry;
static bool frequency_native_active,frequency_native_contact;
static int frequency_native_pressed;
static uint64_t frequency_native_input_generation;
static bool frequency_native_inside(frequency_native_rect r,int x,int y) {
 return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;
}
static bool frequency_native_open(int w,int h) {
 if(w<160||h<160||w>4096||h>4096)return false;
 frequency_native_layout l={.width=w,.height=h};
 int margin=w<200?3:6,gap=w<200?2:4,header=h/7,value=h/5;
 if(header>52)header=52;
 if(value>82)value=82;
 int detail=h<200?12:16,status=h<200?14:20;
 int y=margin;
 l.controls[FREQUENCY_BACK]=(frequency_native_rect){margin,y,header,header};
 l.title=(frequency_native_rect){margin+header+gap,y,w-2*margin-header-gap,header};y+=header+gap;
 l.value=(frequency_native_rect){margin,y,(w-2*margin)*2/3,value};
 l.level=(frequency_native_rect){l.value.x+l.value.w,y,w-margin-l.value.x-l.value.w,value};y+=value;
 l.specification=(frequency_native_rect){margin,y,w-2*margin,detail};y+=detail;
 l.status=(frequency_native_rect){margin,y,w-2*margin,status};y+=status+gap;
 int available=h-margin-y;
 for(int row=0;row<3;row++) {
  int columns=row?3:2,first=row==0?0:row==1?2:5;
  int top=y+available*row/3,bottom=y+available*(row+1)/3;
  for(int col=0;col<columns;col++) {
   int left=margin+(w-2*margin+gap)*col/columns,right=margin+(w-2*margin+gap)*(col+1)/columns-gap;
   l.controls[first+col]=(frequency_native_rect){left,top,right-left,bottom-top-(row<2?gap:0)};
   if(right-left<24||l.controls[first+col].h<18)return false;
  }
 }
 frequency_native_geometry=l;return true;
}
static int frequency_native_target(int x,int y) {
 frequency_native_layout *l=&frequency_native_geometry;
 if(x<0||y<0||x>=l->width||y>=l->height)return -1;
 for(int i=0;i<9;i++)if(frequency_native_inside(l->controls[i],x,y))return i;
 return -1;
}
static void frequency_native_label(frequency_native_rect r,const char *s,uint32_t color) {
 /* The large face is numeric only; all units and captions use full text faces. */
 bool numeric=*s!=0;
 for(const char *p=s;*p;p++)if(*p<'0'||*p>'9')numeric=false;
 unsigned face=numeric&&r.h>=40?4:r.h>=24?0:r.h>=16?2:6;
 int line=face==4?36:face==0?20:face==2?16:10;
 if(portable_nova_measure(face,s)>r.w-4){face=6;line=10;}
 if(r.h<line||r.w<6)return;
 char fitted[64];size_t n=strlen(s);if(n>sizeof(fitted)-4)n=sizeof(fitted)-4;
 memcpy(fitted,s,n);fitted[n]=0;
 while(portable_nova_measure(face,fitted)>r.w-4&&n){--n;memcpy(fitted+n,"...",4);}
 int measured=portable_nova_measure(face,fitted);
 if(measured>r.w-4)return;
 portable_nova_native_text(face,r.x+(r.w-measured)/2,r.y+(r.h-line)/2,r.w,fitted,color);
}
static void frequency_native_button(frequency_native_rect r,const char *s,bool selected) {
 portable_nova_native_round(r.x,r.y,r.w,r.h,4,selected?NOVA_CYAN:NOVA_DIM);
 portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,3,0x031115u);
 frequency_native_label(r,s,selected?NOVA_WHITE:NOVA_TEXT);
}
static void frequency_native_mapping(void) {
 portable_frequency_native_layout(((uint64_t)(unsigned)frequency_native_geometry.width<<32)|(unsigned)frequency_native_geometry.height);
}
static void frequency_native_render(void) {
 frequency_native_layout *l=&frequency_native_geometry;char text[48];
 portable_nova_begin();portable_nova_native_fill(0,0,l->width,l->height,0x010708u);
 frequency_native_button(l->controls[FREQUENCY_BACK],"<",false);
 frequency_native_label(l->title,"FREQUENCY",NOVA_TEXT);
 snprintf(text,sizeof(text),"%u",frequency);
 frequency_native_rect number=l->value,unit=l->value;
 number.w=l->value.w-28;if(number.w>120)number.w=120;
 number.x+=(l->value.w-number.w-28)/2;unit.x=number.x+number.w;unit.w=28;
 frequency_native_label(number,text,NOVA_CYAN);frequency_native_label(unit,"Hz",NOVA_CYAN);
 snprintf(text,sizeof(text),"%u%%",volume);frequency_native_label(l->level,text,NOVA_CYAN);
 frequency_native_label(l->specification,"20-7000 Hz / 16 kHz PCM",NOVA_CAP);
 frequency_native_label(l->status,message?message:"",NOVA_TEXT);
 snprintf(text,sizeof(text),"- %u Hz",steps[step_index]);frequency_native_button(l->controls[FREQUENCY_LESS],text,false);
 snprintf(text,sizeof(text),"+ %u Hz",steps[step_index]);frequency_native_button(l->controls[FREQUENCY_MORE],text,false);
 for(unsigned i=0;i<3;i++){snprintf(text,sizeof(text),"%u Hz",steps[i]);frequency_native_button(l->controls[FREQUENCY_STEP_10+i],text,step_index==i);}
 frequency_native_button(l->controls[FREQUENCY_LEVEL_LESS],"Level -",false);
 frequency_native_button(l->controls[FREQUENCY_TOGGLE],playing?"Stop":"Start",playing);
 frequency_native_button(l->controls[FREQUENCY_LEVEL_MORE],"Level +",false);
 portable_frequency_native_present(false);
}
static void frequency_native_input(t5_app_input_t *in) {
 uint64_t generation=0;bool ready=portable_frequency_native_ready(&generation,NULL);
 if(generation!=frequency_native_input_generation){frequency_native_contact=false;frequency_native_pressed=-1;frequency_native_input_generation=generation;}
 t5_app_contact_t sample={0};bool ok=app->touch_contact(&sample);
 if(!ready||!ok){frequency_native_contact=false;frequency_native_pressed=-1;in->tapped=false;return;}
 if(sample.down){int target=frequency_native_target(sample.x,sample.y);if(!frequency_native_contact){frequency_native_contact=true;frequency_native_pressed=target;}else if(target!=frequency_native_pressed)frequency_native_pressed=-1;}
 if(in->tapped){int target=frequency_native_target(in->touch_x,in->touch_y);if(!frequency_native_contact||target!=frequency_native_pressed)in->tapped=false;frequency_native_contact=false;frequency_native_pressed=-1;}
 else if(!sample.down){frequency_native_contact=false;frequency_native_pressed=-1;}
 if(in->tapped&&frequency_native_target(in->touch_x,in->touch_y)==FREQUENCY_BACK){
  /* Re-enter the adapter's established orderly return path on the next poll. */
  portable_frequency_native_back();in->tapped=false;
 }
}
