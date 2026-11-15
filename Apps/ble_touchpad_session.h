#pragma once
#include "ble_hid_session.h"
#include "RiscScenePointerV1.h"
static inline bool btp_scroll(const ble_hid_session *s){
 return s->api&&s->api->struct_size>=sizeof(risc_bluetooth_hid_scroll_api_v1)&&((const risc_bluetooth_hid_scroll_api_v1*)s->api)->mouse_scroll;
}
static inline bool btp_ready(const ble_hid_session *s){
 const hid_assignment a={.kind=HID_ACTION_MOUSE};return bhs_ready(s,&a);
}
static inline bool btp_send(ble_hid_session *s,uint8_t buttons,int dx,int dy,int wx,int wy){
 if(!bhs_live(s)||!btp_ready(s))return false;
 bool ok;
 if(btp_scroll(s))ok=((const risc_bluetooth_hid_scroll_api_v1*)s->api)->mouse_scroll(s->api->context,s->token,buttons,dx,dy,wx,wy);
 else ok=s->api->mouse(s->api->context,s->token,buttons,(int8_t)dx,(int8_t)dy,0);
 if(!bhs_live(s))return false;
 if(!ok)s->closing=true;
 return ok;
}
static inline bool btp_report(ble_hid_session *s,const risc_scene_pointer_event_v1 *p,uint64_t now){
 if(p->buttons>1||p->click>2||(p->buttons&&p->click)||p->dx<-8192||p->dx>8192||p->dy<-8192||p->dy>8192||p->wheel_x<-8192||p->wheel_x>8192||p->wheel_y<-8192||p->wheel_y>8192)return false;
 if(!bhs_live(s)||!btp_ready(s))return false;
 if(p->dx||p->dy||p->buttons||s->holding||((p->wheel_x||p->wheel_y)&&btp_scroll(s))){
  int dx=p->dx,dy=p->dy,wx=btp_scroll(s)?p->wheel_x:0,wy=btp_scroll(s)?p->wheel_y:0;
  do {
   int x=dx<-127?-127:dx>127?127:dx,y=dy<-127?-127:dy>127?127:dy;
   int ax=wx<-127?-127:wx>127?127:wx,ay=wy<-127?-127:wy>127?127:wy;
   if(!btp_send(s,(uint8_t)p->buttons,x,y,ax,ay))return false;
   dx-=x;dy-=y;wx-=ax;wy-=ay;
  }while(dx||dy||wx||wy);
 }
 if(p->click){
  if(!btp_send(s,(uint8_t)p->click,0,0,0,0)||!btp_send(s,0,0,0,0,0))return false;
 }
 s->holding=p->buttons!=0;s->held=(hid_assignment){.kind=HID_ACTION_MOUSE,.buttons=(uint8_t)p->buttons};
 s->last_report=now;return true;
}
