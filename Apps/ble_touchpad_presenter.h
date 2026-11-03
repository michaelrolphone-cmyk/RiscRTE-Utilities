#pragma once
#include <stdio.h>
#include "ble_touchpad_session.h"
#include "AlarmServiceV1.h"
enum { BB_MAIN,BB_SETTINGS,BB_FORGET,BB_PAIR,BB_ALERT };
enum { BB_BACK=1,BB_STOP,BB_RECONNECT,BB_SETTINGS_OPEN,BB_PAIR_START,BB_FORGET_OPEN,BB_FORGET_CONFIRM,BB_CANCEL,BB_PAIR_ACCEPT,BB_PAIR_REJECT,BB_ALARM_ACK,BB_POINTER };
typedef struct {
 unsigned page;bool hold_available,pointer_available,hid_available,controls_available,alarm_available,key_available,background_available,idle_available;
 const char *notice;const alarm_status_v1 *alarm;ble_hid_session *session;
} ble_touchpad_view;
static inline risc_scene_node_v1 *btp_node(risc_components_document_v1 *d,unsigned kind,unsigned action,const char *label,const char *text){
 risc_scene_node_v1*n=&d->nodes[d->node_count++];*n=(risc_scene_node_v1){.id=d->node_count,.route=1,.kind=kind,.action=action};
 snprintf(n->label,sizeof(n->label),"%.*s",(int)sizeof(n->label)-1,label);snprintf(n->text,sizeof(n->text),"%.*s",(int)sizeof(n->text)-1,text?text:"");return n;
}
static inline const char *btp_state(const ble_hid_session*s){
 if(s->terminal)return "REMOTE STATE UNKNOWN - RESTART REQUIRED";
 if(s->closing)return "STOPPING - CLEANUP PENDING";
 if(!s->token)return "Stopped - choose Pair or Reconnect";
 switch(s->status.state){case RISC_HID_STARTING:return "Starting";case RISC_HID_ADVERTISING:return "Advertising";case RISC_HID_CONNECTED:return "Connected - securing";case RISC_HID_PAIR_CONFIRM:return "Compare pairing numbers";case RISC_HID_READY:return "Secure connection ready";case RISC_HID_FAULT:return "Bluetooth error";default:return "Starting";}
}
static inline void btp_build(risc_components_document_v1*d,const ble_touchpad_view*v,uint32_t revision){
 *d=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*d),.revision=revision,.root=1,.route_count=1,.screen_key=v->page+1,.flags=RISC_COMPONENTS_ROOT_HEADER};
 static const char*titles[]={"BLE TOUCHPAD","TOUCHPAD SETTINGS","FORGET BOND","PAIRING","ALARM"};
 d->routes[0]=(risc_scene_route_v1){1,BB_BACK,""};snprintf(d->routes[0].title,sizeof(d->routes[0].title),"%s",titles[v->page]);
 risc_scene_node_v1*n;const ble_hid_session*s=v->session;char text[72];
 if(v->page==BB_MAIN){
  btp_node(d,RISC_COMPONENT_HEADER_ACTION,s->token?BB_STOP:BB_RECONNECT,s->token?"STOP":"RECONNECT","");
  btp_node(d,RISC_COMPONENT_HEADER_ACTION,BB_SETTINGS_OPEN,"SETTINGS","");
  n=btp_node(d,v->pointer_available?RISC_COMPONENT_POINTER:RISC_SCENE_TEXT_NODE,v->pointer_available?BB_POINTER:0,v->hid_available?btp_state(s):"BLUETOOTH HID UNAVAILABLE",!v->hid_available?"Bluetooth HID provider unavailable":!btp_ready(s)?"Use Reconnect or open Settings to pair":!btp_scroll(s)?"Tap: click. Tap-drag: hold. Right tap: two fingers. Scroll unavailable.":"Tap: click. Tap-drag: hold. Two-finger tap: right; move: scroll X/Y.");
  if(v->pointer_available&&!btp_ready(s))n->flags=RISC_SCENE_DISABLED;

  if(!v->pointer_available)btp_node(d,RISC_SCENE_TEXT_NODE,0,"INPUT","SHARED POINTER INPUT UNAVAILABLE");

 }else if(v->page==BB_SETTINGS){
  btp_node(d,RISC_SCENE_ACTION,BB_PAIR_START,"PAIR NEW DEVICE","");btp_node(d,RISC_SCENE_ACTION,BB_RECONNECT,"RECONNECT","");btp_node(d,RISC_SCENE_ACTION,BB_FORGET_OPEN,"FORGET BOND","");
  btp_node(d,RISC_SCENE_TEXT_NODE,0,"GESTURES","Tap then drag within 300 ms. Two-finger movement scrolls X/Y.");
 }else if(v->page==BB_FORGET){
  btp_node(d,RISC_SCENE_TEXT_NODE,0,"FORGET SAVED PEER?","Stop first. Pair again after forgetting.");btp_node(d,RISC_SCENE_ACTION,BB_CANCEL,"CANCEL","");
  n=btp_node(d,RISC_SCENE_ACTION,BB_FORGET_CONFIRM,"FORGET","");if(s->token||s->closing)n->flags=RISC_SCENE_DISABLED;
 }else if(v->page==BB_PAIR){
  snprintf(text,sizeof(text),"%06u",(unsigned)s->status.pairing_number);btp_node(d,RISC_SCENE_TEXT_NODE,0,"COMPARE ON BOTH DEVICES",text);
  btp_node(d,RISC_SCENE_ACTION,BB_PAIR_REJECT,"REJECT","");
  n=btp_node(d,v->hold_available?RISC_COMPONENT_HOLD_KEY:RISC_SCENE_TEXT_NODE,v->hold_available?BB_PAIR_ACCEPT:0,"NUMBERS MATCH",v->hold_available?"Press and release to confirm":"Secure confirmation input unavailable");if(v->hold_available)n->step=RISC_HOLD_CONFIRM;
 }else if(v->page==BB_ALERT){
  btp_node(d,RISC_SCENE_TEXT_NODE,0,"ALARM",v->alarm?v->alarm->label:"Alarm");n=btp_node(d,RISC_SCENE_ACTION,BB_ALARM_ACK,"DISMISS","");if(!v->alarm||v->alarm->state!=ALARM_STATE_ALERT)n->flags=RISC_SCENE_DISABLED;
 }
 if(v->page==BB_SETTINGS){
  btp_node(d,RISC_SCENE_TEXT_NODE,0,"CONNECTION",v->hid_available?btp_state(s):"BLUETOOTH HID UNAVAILABLE");
  if(v->hid_available&&!btp_scroll(s))btp_node(d,RISC_SCENE_TEXT_NODE,0,"SCROLL","TWO-AXIS SCROLL UNAVAILABLE - provider update needed");
  if(!v->controls_available)btp_node(d,RISC_SCENE_TEXT_NODE,0,"CONTROLS","SHARED CONTROLS UNAVAILABLE");
  if(!v->alarm_available)btp_node(d,RISC_SCENE_TEXT_NODE,0,"ALARM SERVICE","UNAVAILABLE");
  if(!v->key_available)btp_node(d,RISC_SCENE_TEXT_NODE,0,"POWER KEY","UNAVAILABLE - use semantic Back");
  if(!v->background_available)btp_node(d,RISC_SCENE_TEXT_NODE,0,"BACKGROUND LEARNING","UNAVAILABLE");
  if(!v->idle_available)btp_node(d,RISC_SCENE_TEXT_NODE,0,"IDLE POLICY","SETTINGS OR HOST SUPPORT UNAVAILABLE");
 }
 if(!v->hid_available)for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].action==BB_RECONNECT||d->nodes[i].action==BB_PAIR_START||d->nodes[i].action==BB_FORGET_OPEN||d->nodes[i].action==BB_FORGET_CONFIRM)d->nodes[i].flags|=RISC_SCENE_DISABLED;
 if(v->alarm&&(v->alarm->state==ALARM_STATE_CUE||v->alarm->state==ALARM_STATE_BLOCKED||v->alarm->state==ALARM_STATE_ALERT||v->alarm->state==ALARM_STATE_DISMISSING))for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].action==BB_RECONNECT||d->nodes[i].action==BB_PAIR_START)d->nodes[i].flags|=RISC_SCENE_DISABLED;
 if(v->notice&&v->notice[0]){snprintf(d->subtitle,sizeof(d->subtitle),"%.*s",(int)sizeof(d->subtitle)-1,v->notice);}
}
