#include "utility_paper.h"
static void calculator_paper_render(void) {
 up_begin("CALCULATOR");
 up_text(32,120,416,calculator_display(),true,true);
 if(calculator_state.error)up_wrap(158,"Digit or Clear to reset");
 else if(calculator_state.pending){char p[]={calculator_state.pending,0};up_text(32,166,416,p,false,true);}
 if(calculator_menu) {
  up_button(32,232,416,88,"Clear (C)","solid:f2ed",calculator_selected==0);
  up_button(32,336,416,88,"Change sign (+/-)",NULL,calculator_selected==1);
  up_button(32,440,416,88,"Delete last digit","solid:f053",calculator_selected==2);
  up_button(32,688,416,88,"Back","solid:f053",false);
 } else {
  for(unsigned i=0;i<16;i++)up_button(32+(int)(i%4)*106,232+(int)(i/4)*106,98,98,calculator_labels[i],NULL,calculator_selected==i);
  up_button(32,688,200,88,"Back","solid:f053",false);
  up_button(248,688,200,88,"Tools","solid:f141",false);
 }
 calculator_api->present(false);
}
static int calculator_paper_hit(int x,int y) {
 if(calculator_menu){for(unsigned i=0;i<3;i++)if(up_hit(x,y,32,232+(int)i*104,416,88))return (int)i;}
 else for(unsigned i=0;i<16;i++)if(up_hit(x,y,32+(int)(i%4)*106,232+(int)(i/4)*106,98,98))return (int)i;
 return -1;
}
