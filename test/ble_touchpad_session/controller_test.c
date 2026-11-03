#define main buttons_regression_main
#include "../ble_buttons_session/controller_test.c"
#undef main
#include "ble_touchpad_presenter.h"
static int sum_x,sum_y,sum_wx,sum_wy;static unsigned last_buttons;
static bool scroll(void*c,uint64_t t,uint8_t b,int16_t x,int16_t y,int16_t wx,int16_t wy){
 (void)c;assert(t==7);assert(x>=-127&&x<=127&&y>=-127&&y<=127&&wx>=-127&&wx<=127&&wy>=-127&&wy<=127);
 calls++;reports++;last_buttons=b;sum_x+=x;sum_y+=y;sum_wx+=wx;sum_wy+=wy;return true;
}
static risc_scene_pointer_event_v1 packet(int x,int y,int wx,int wy,unsigned b,unsigned click){return (risc_scene_pointer_event_v1){.struct_size=sizeof(risc_scene_pointer_event_v1),.kind=RISC_POINTER_REPORT,.dx=x,.dy=y,.wheel_x=wx,.wheel_y=wy,.buttons=b,.click=click};}
int main(void){
 assert(!buttons_regression_main());ble_hid_session s;reset(&s);
 risc_bluetooth_hid_scroll_api_v1 ext={.base={.base=api},.mouse_scroll=scroll};ext.base.base.struct_size=sizeof(ext);s.api=&ext.base.base;
 risc_scene_pointer_event_v1 p=packet(254,-255,12,-9,0,0);assert(btp_report(&s,&p,0));assert(sum_x==254&&sum_y==-255&&sum_wx==12&&sum_wy==-9&&reports==3&&!s.holding);
 p=packet(20,0,0,0,1,0);assert(btp_report(&s,&p,10));assert(s.holding&&last_buttons==1);
 unsigned n=reports;for(unsigned t=20;t<=400;t+=20)assert(bhs_poll(&s,t));assert(reports==n);assert(bhs_poll(&s,420)&&reports==n+1);
 p=packet(0,0,0,0,0,0);assert(btp_report(&s,&p,421)&&!s.holding&&last_buttons==0);
 p=packet(0,0,0,0,0,2);n=reports;assert(btp_report(&s,&p,440)&&reports==n+2&&last_buttons==0);
 reset(&s);assert(!btp_scroll(&s));p=packet(0,0,20,30,0,0);n=reports;assert(btp_report(&s,&p,0)&&reports==n);
 p=packet(0,0,0,0,0,1);assert(btp_report(&s,&p,1)&&reports==n+2);
 p=packet(1,0,0,0,0,0);current.flags=RISC_HID_MOUSE_READY;assert(bhs_poll(&s,20));n=calls;assert(!btp_report(&s,&p,20)&&calls==n);
 reset(&s);live=false;n=calls;assert(!btp_report(&s,&p,0)&&s.terminal&&calls==n);
 reset(&s);p.dx=8193;n=calls;assert(!btp_report(&s,&p,0)&&calls==n);
 risc_components_document_v1 d;ble_touchpad_view view={.session=&s,.pointer_available=true,.hold_available=true,.hid_available=true};
 btp_build(&d,&view,1);assert(d.node_count&&d.nodes[2].kind==RISC_COMPONENT_POINTER);
 view.pointer_available=false;btp_build(&d,&view,2);for(unsigned i=0;i<d.node_count;i++)assert(d.nodes[i].kind!=RISC_COMPONENT_POINTER);
 view.page=BB_PAIR;view.hold_available=false;btp_build(&d,&view,3);for(unsigned i=0;i<d.node_count;i++)assert(d.nodes[i].kind!=RISC_COMPONENT_HOLD_KEY);
 puts("Touchpad shared controller and legacy Buttons regression PASS");return 0;
}
