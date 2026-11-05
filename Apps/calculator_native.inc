/* Calculator-only native color presentation. Arithmetic and menu state remain
 * owned by calculator_nova.inc; the shared legacy utility helpers are unchanged. */
#include "PortableCalculatorNative.h"
#include <stddef.h>
typedef struct { int x,y,w,h; } calculator_native_rect;
typedef struct {
 int width,height; calculator_native_rect back,more,value,status,keys[16],menu[3];
} calculator_native_layout;
static calculator_native_layout calculator_native_geometry;
static bool calculator_native_active,calculator_native_contact;
static int calculator_native_pressed;
static uint64_t calculator_native_input_generation;
static bool calculator_native_inside(calculator_native_rect r,int x,int y) {
 return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;
}
static bool calculator_native_open(int w,int h) {
 if(w<96||h<112||w>65535||h>65535)return false;
 calculator_native_layout l={.width=w,.height=h};
 int margin=w<200?3:8,gap=w<200?2:4,header=h/5;
 if(header<30)header=30;
 if(header>64)header=64;
 int chrome=header-2*margin;if(chrome<20)chrome=20;
 l.back=(calculator_native_rect){margin,margin,chrome,chrome};
 l.more=(calculator_native_rect){w-margin-chrome,margin,chrome,chrome};
 l.value=(calculator_native_rect){margin+chrome+gap,margin,w-2*(margin+chrome+gap),header-2*margin};
 /* Narrow displays get a full-width numeric line below the chrome. */
 if(l.value.w<80){l.value=(calculator_native_rect){margin,header,w-2*margin,20};header+=22;}
 l.status=(calculator_native_rect){margin,header,w-2*margin,16};header+=18;
 int usable=h-header-margin;
 for(int i=0;i<16;i++){
  int x=margin+(w-2*margin)*(i%4)/4,xx=margin+(w-2*margin)*(i%4+1)/4;
  int y=header+usable*(i/4)/4,yy=header+usable*(i/4+1)/4;
  l.keys[i]=(calculator_native_rect){x,y,xx-x-gap,yy-y-gap};
  if(l.keys[i].w<12||l.keys[i].h<10)return false;
 }
 for(int i=0;i<3;i++){int y=header+usable*i/3,yy=header+usable*(i+1)/3;l.menu[i]=(calculator_native_rect){margin,y,w-2*margin,yy-y-gap};}
 calculator_native_geometry=l;return true;
}
static int calculator_native_target(int x,int y) {
 calculator_native_layout *l=&calculator_native_geometry;
 if(x<0||y<0||x>=l->width||y>=l->height)return -1;
 if(calculator_native_inside(l->back,x,y))return 16;
 if(!calculator_menu&&calculator_native_inside(l->more,x,y))return 17;
 for(int i=0;i<(calculator_menu?3:16);i++)if(calculator_native_inside(calculator_menu?l->menu[i]:l->keys[i],x,y))return i;
 return -1;
}
static void calculator_native_label(calculator_native_rect r,const char *s,uint32_t color) {
 bool numeric=true;
 for(const char *c=s;*c;c++)if((*c<'0'||*c>'9')&&*c!='.'&&*c!='-')numeric=false;
 /* The large retained face contains numeric glyphs only. */
 unsigned face=r.h>=40&&r.w>=50&&numeric?4:r.h>=24?0:r.h>=16?2:6;
 int line=face==4?36:face==0?20:face==2?16:10;
 if(r.h<line||r.w<4)return;
 if(portable_nova_measure(face,s)>r.w-4){face=6;line=10;}
 char fitted[96];unsigned n=0;while(s[n]&&n<sizeof(fitted)-4){fitted[n]=s[n];n++;}fitted[n]=0;
 while(portable_nova_measure(face,fitted)>r.w-4&&n){--n;fitted[n]='.';fitted[n+1]='.';fitted[n+2]='.';fitted[n+3]=0;}
 int measure=portable_nova_measure(face,fitted);if(measure>r.w-4)return;
 portable_nova_native_text(face,r.x+(r.w-measure)/2,r.y+(r.h-line)/2,r.w,fitted,color);
}
static void calculator_native_button(calculator_native_rect r,const char *s,bool selected) {
 portable_nova_native_round(r.x,r.y,r.w,r.h,4,selected?NOVA_CYAN:NOVA_DIM);
 if(r.w>4&&r.h>4)portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,3,0x031115u);
 calculator_native_label(r,s,selected?NOVA_WHITE:NOVA_TEXT);
}
static void calculator_native_mapping(void) {
 uint64_t id=((uint64_t)(unsigned)calculator_native_geometry.width<<32)|((uint64_t)(unsigned)calculator_native_geometry.height<<8)|(calculator_menu?2u:1u);
 portable_calculator_native_layout(id);
}
static void calculator_native_render(void) {
 portable_nova_begin();
 calculator_native_layout *l=&calculator_native_geometry;
 portable_nova_native_fill(0,0,l->width,l->height,0x010708u);
 calculator_native_button(l->back,"<",false);
 if(!calculator_menu)calculator_native_button(l->more,"...",false);
 calculator_native_label(l->value,calculator_display(),NOVA_CYAN);
 char pending[]={calculator_state.pending,0};
 calculator_native_label(l->status,calculator_state.error?"Digit/C resets":calculator_state.pending?pending:calculator_menu?"TOOLS":"",NOVA_CAP);
 static const char *const tools[]={"Clear (C)","Change sign (+/-)","Delete last digit"};
 for(unsigned i=0;i<(calculator_menu?3u:16u);i++)calculator_native_button(calculator_menu?l->menu[i]:l->keys[i],calculator_menu?tools[i]:calculator_labels[i],calculator_selected==i);
 portable_calculator_native_present(false);
}
static void calculator_native_input(t5_app_input_t *in) {
 uint64_t generation=0,sequence=0;bool ready=portable_calculator_native_ready(&generation,&sequence);(void)sequence;
 if(generation!=calculator_native_input_generation){calculator_native_contact=false;calculator_native_pressed=-1;calculator_native_input_generation=generation;}
 t5_app_contact_t sample={0};bool ok=calculator_api->touch_contact(&sample);
 if(!ready||!ok){calculator_native_contact=false;calculator_native_pressed=-1;in->tapped=false;return;}
 if(sample.down){int target=calculator_native_target(sample.x,sample.y);if(!calculator_native_contact){calculator_native_contact=true;calculator_native_pressed=target;}else if(target!=calculator_native_pressed)calculator_native_pressed=-1;}
 if(in->tapped){int target=calculator_native_target(in->touch_x,in->touch_y);if(!calculator_native_contact||target!=calculator_native_pressed)in->tapped=false;calculator_native_contact=false;calculator_native_pressed=-1;}
 else if(!sample.down){calculator_native_contact=false;calculator_native_pressed=-1;}
 if(in->tapped&&calculator_native_target(in->touch_x,in->touch_y)==16){in->tapped=false;in->buttons|=T5_APP_BUTTON_BACK;}
}
