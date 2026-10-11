#ifdef PORTABLE_RETURN_APP
#error Nested Nova app requires CALCULATOR_RETURN_APP, never generic eager return
#endif
/* 16 large arithmetic keys. Clear, sign and delete have their own roomy panel. */
#include "T5AppApi.h"
#include "utility_frame.h"
#include "PortableNovaUi.h"
#include "calculator_core.h"
static const t5_app_api_v1 *calculator_api;
static calculator calculator_state;
static bool calculator_menu;
static unsigned calculator_selected;
static const char calculator_keys[]={'7','8','9','/','4','5','6','*','1','2','3','-','0','.','=','+'};
static const char *const calculator_labels[]={"7","8","9","/","4","5","6","x","1","2","3","-","0",".","=","+"};
static const char *calculator_display(void) {
    return calculator_state.error==CALC_DIV_ZERO?"Divide by zero":calculator_state.error==CALC_OVERFLOW?"Overflow":calculator_state.text;
}
#ifdef PORTABLE_PAPER_UTILITIES
#include "calculator_paper.inc"
#endif
static void calculator_render(void) {
 if(!utility_frame_begin(calculator_api))return;
#ifdef PORTABLE_PAPER_UTILITIES
    if(utility_paper){calculator_paper_render();return;}
#endif
    portable_nova_begin();
    if(calculator_menu) {
        portable_nova_header("CALCULATOR");
        portable_nova_center(1,16,54,208,calculator_display(),NOVA_CYAN);
        portable_nova_button(20,78,200,44,"Clear (C)",calculator_selected==0);
        portable_nova_button(20,130,200,44,"Change sign (+/-)",calculator_selected==1);
        portable_nova_button(20,182,200,44,"Delete last digit",calculator_selected==2);
    } else {
        portable_nova_button(8,4,44,44,"<",false);
        portable_nova_button(188,4,44,44,"...",false);
        portable_nova_center(1,56,10,128,calculator_display(),NOVA_CYAN);
        if(calculator_state.error)portable_nova_center(3,56,29,128,"Digit or Clear to reset",NOVA_CAP);
        else if(calculator_state.pending){char text[]={calculator_state.pending,0};portable_nova_center(2,56,29,128,text,NOVA_CAP);}
        for(unsigned i=0;i<16;i++)portable_nova_button(16+(int)(i%4)*53,52+(int)(i/4)*46,49,44,calculator_labels[i],calculator_selected==i);
    }
    calculator_api->present(false);
}
static int calculator_hit(int x,int y) {
#ifdef PORTABLE_PAPER_UTILITIES
    if(utility_paper)return calculator_paper_hit(x,y);
#endif
    if(calculator_menu){for(unsigned i=0;i<3;i++)if(portable_nova_hit(x,y,20,78+(int)i*52,200,44))return (int)i;}
    else for(unsigned i=0;i<16;i++)if(portable_nova_hit(x,y,16+(int)(i%4)*53,52+(int)(i/4)*46,49,44))return (int)i;
    return -1;
}
void app_main(void) {
 utility_frame_reset();
    calculator_api=t5_app_get_api(1);
    if(!calculator_api || !calculator_api->poll || !calculator_api->present || !calculator_api->screen_width || !calculator_api->screen_height)return;
#ifdef PORTABLE_PAPER_UTILITIES
    up_open(calculator_api);
    if(!utility_paper && (calculator_api->screen_width()!=240 || calculator_api->screen_height()!=240))return;
#else
    if(calculator_api->screen_width()!=240 || calculator_api->screen_height()!=240)return;
#endif
    calc_reset(&calculator_state);calculator_menu=false;calculator_selected=0;calculator_render();
    for(;;) {
        t5_app_input_t input={0};if(!calculator_api->poll(&input,30))return;
#ifdef PORTABLE_PAPER_UTILITIES
        /* The shared adapter already queued global Home, including in Tools. */
        if(input.exit_requested&&!(input.buttons&T5_APP_BUTTON_BACK))return;
        if(utility_paper){up_input(&input);if(input.tapped && up_hit(input.touch_x,input.touch_y,32,688,calculator_menu?416:200,88))input.buttons|=T5_APP_BUTTON_BACK;}
#endif
        if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK)) {
            if(calculator_menu){calculator_menu=false;calculator_selected=0;calculator_render();continue;}
#ifdef CALCULATOR_RETURN_APP
            const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
            if(!rt || !rt->request_launch || !rt->request_launch(CALCULATOR_RETURN_APP)){calculator_render();continue;}
#endif
            return;
        }
        if(input.tapped && !calculator_menu && (
#ifdef PORTABLE_PAPER_UTILITIES
            utility_paper?up_hit(input.touch_x,input.touch_y,248,688,200,88):
#endif
            portable_nova_hit(input.touch_x,input.touch_y,188,4,44,44))) {
            calculator_menu=true;calculator_selected=0;calculator_render();continue;
        }
        unsigned count=calculator_menu?3:16;
        if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))calculator_selected=(calculator_selected+count-1)%count;
        else if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))calculator_selected=(calculator_selected+1)%count;
        int key=input.tapped?calculator_hit(input.touch_x,input.touch_y):(input.buttons&T5_APP_BUTTON_CONFIRM)?(int)calculator_selected:-1;
        if(key>=0) {
            calculator_selected=(unsigned)key;
            if(calculator_menu){static const char menu_keys[]={'C','S','B'};calc_key(&calculator_state,menu_keys[key]);calculator_menu=false;calculator_selected=0;}
            else calc_key(&calculator_state,calculator_keys[key]);
        }
        if(input.tapped || input.buttons || utility_frame_pending)calculator_render();
    }
}
