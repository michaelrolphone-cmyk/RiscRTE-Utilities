#pragma once
#include <stdio.h>
#include "RiscSceneHoldV1.h"
#include "ble_buttons_session.h"
#include "ble_buttons_editor.h"
#include "AlarmServiceV1.h"
enum { BB_MAIN,BB_SETTINGS,BB_EDIT,BB_MODIFIERS,BB_FORGET,BB_PAIR,BB_ALERT };
enum { BB_BACK=1,BB_STOP,BB_RECONNECT,BB_SETTINGS_OPEN,BB_PAIR_START,
 BB_FORGET_OPEN,BB_FORGET_CONFIRM,BB_KIND,BB_PREVIOUS,BB_NEXT,BB_MODIFIERS_OPEN,
 BB_SAVE,BB_CANCEL,BB_PAIR_ACCEPT,BB_PAIR_REJECT,BB_ALARM_ACK,BB_EDIT_FIRST=32,
 BB_MODIFIER_FIRST=48,BB_HOLD_FIRST=64 };
typedef struct {
 unsigned page;bool hold_available,hid_available,storage_available,controls_available,alarm_available,key_available,background_available,idle_available;
 const char *notice;const alarm_status_v1 *alarm;
 ble_buttons_session *session;ble_buttons_editor *editor;
} ble_buttons_view;
static inline risc_scene_node_v1 *bbp_node(risc_components_document_v1 *d,unsigned kind,unsigned action,const char *label,const char *text){
 risc_scene_node_v1 *n=&d->nodes[d->node_count++];
 *n=(risc_scene_node_v1){.id=d->node_count,.route=1,.kind=kind,.action=action};
 snprintf(n->label,sizeof(n->label),"%.*s",(int)sizeof(n->label)-1,label);snprintf(n->text,sizeof(n->text),"%.*s",(int)sizeof(n->text)-1,text?text:"");return n;
}
static inline void bbp_assignment(const hid_assignment *a,char *text,size_t size){
 if(a->kind==HID_ACTION_KEY)snprintf(text,size,"%s / modifiers %02X",hid_keys[hid_key_index(a->key)].name,(unsigned)a->modifiers);
 else if(a->kind==HID_ACTION_MOUSE)snprintf(text,size,"Mouse %s",a->buttons==1?"Left":a->buttons==2?"Right":"Middle");
 else if(a->kind==HID_ACTION_WHEEL)snprintf(text,size,"Wheel %+d",(int)a->wheel);
 else snprintf(text,size,"Motion X %+d / Y %+d",(int)a->dx,(int)a->dy);
}
static inline const char *bbp_state(const ble_buttons_session *s){
 if(s->terminal)return "REMOTE STATE UNKNOWN - RESTART REQUIRED";
 if(s->closing)return "STOPPING - CLEANUP PENDING";
 if(!s->token)return "Stopped - choose Pair or Reconnect";
 switch(s->status.state){case RISC_HID_STARTING:return "Starting";case RISC_HID_ADVERTISING:return "Advertising";case RISC_HID_CONNECTED:return "Connected - securing";case RISC_HID_PAIR_CONFIRM:return "Compare pairing numbers";case RISC_HID_READY:return "Secure connection ready";case RISC_HID_FAULT:return "Bluetooth error";default:return "Starting";}
}
static inline void bbp_build(risc_components_document_v1 *d,const ble_buttons_view *v,uint64_t revision){
 *d=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*d),.revision=revision,.root=1,.route_count=1,.screen_key=v->page+1,.flags=RISC_COMPONENTS_ROOT_HEADER};
 static const char *titles[]={"BLE BUTTONS","BUTTON SETTINGS","EDIT BUTTON","KEY MODIFIERS","FORGET BOND","PAIRING","ALARM"};
 d->routes[0]=(risc_scene_route_v1){1,BB_BACK,""};snprintf(d->routes[0].title,sizeof(d->routes[0].title),"%s",titles[v->page]);
 char text[96],label[40];risc_scene_node_v1 *n;
 const ble_buttons_session *s=v->session;const ble_buttons_editor *e=v->editor;
 if(v->page==BB_ALERT){
  bbp_node(d,RISC_SCENE_TEXT_NODE,0,"ALARM",v->alarm?v->alarm->label:"Alarm");
  n=bbp_node(d,RISC_SCENE_ACTION,BB_ALARM_ACK,"DISMISS","");if(!v->alarm||v->alarm->state!=ALARM_STATE_ALERT)n->flags=RISC_SCENE_DISABLED;
 }else if(v->page==BB_MAIN){
  bbp_node(d,RISC_COMPONENT_HEADER_ACTION,s->token?BB_STOP:BB_RECONNECT,s->token?"STOP":"RECONNECT","");
  bbp_node(d,RISC_COMPONENT_HEADER_ACTION,BB_SETTINGS_OPEN,"SETTINGS","");
  for(unsigned i=0;i<4;i++){snprintf(label,sizeof(label),"BUTTON %u",i+1);bbp_assignment(&e->buttons[i],text,sizeof(text));
   n=bbp_node(d,v->hold_available?RISC_COMPONENT_HOLD_KEY:RISC_SCENE_TEXT_NODE,v->hold_available?BB_HOLD_FIRST+i:0,label,text);
   if(v->hold_available&&!bbs_ready(s,&e->buttons[i]))n->flags=RISC_SCENE_DISABLED;
  }
  bbp_node(d,RISC_SCENE_TEXT_NODE,0,"CONNECTION",v->hid_available?bbp_state(s):"BLUETOOTH HID UNAVAILABLE");
  if(!v->hold_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"INPUT","HELD INPUT UNAVAILABLE - no click substitute");
  if(!v->controls_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"CONTROLS","SHARED CONTROLS UNAVAILABLE");
  if(!v->alarm_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"ALARM SERVICE","UNAVAILABLE");
  if(!v->key_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"POWER KEY","UNAVAILABLE - use semantic Back");
  if(!v->background_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"BACKGROUND LEARNING","UNAVAILABLE");
  if(!v->idle_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"IDLE POLICY","SETTINGS OR HOST SUPPORT UNAVAILABLE");
 }else if(v->page==BB_SETTINGS){
  bbp_node(d,RISC_SCENE_ACTION,BB_PAIR_START,"PAIR NEW DEVICE","");bbp_node(d,RISC_SCENE_ACTION,BB_RECONNECT,"RECONNECT","");bbp_node(d,RISC_SCENE_ACTION,BB_FORGET_OPEN,"FORGET BOND","");
  for(unsigned i=0;i<4;i++){snprintf(label,sizeof(label),"EDIT BUTTON %u",i+1);bbp_assignment(&e->buttons[i],text,sizeof(text));bbp_node(d,RISC_SCENE_ACTION,BB_EDIT_FIRST+i,label,text);}
 }else if(v->page==BB_EDIT){
  static const char *kinds[]={"KEYBOARD","MOUSE BUTTON","WHEEL","MOTION"};
  bbp_node(d,RISC_SCENE_ACTION,BB_KIND,"KIND",kinds[e->draft.kind]);bbp_assignment(&e->draft,text,sizeof(text));bbp_node(d,RISC_SCENE_TEXT_NODE,0,"VALUE",text);
  bbp_node(d,RISC_SCENE_ACTION,BB_PREVIOUS,"PREVIOUS","");bbp_node(d,RISC_SCENE_ACTION,BB_NEXT,"NEXT","");
  if(e->draft.kind==HID_ACTION_KEY)bbp_node(d,RISC_SCENE_ACTION,BB_MODIFIERS_OPEN,"MODIFIERS","");
  n=bbp_node(d,RISC_SCENE_ACTION,BB_SAVE,e->save_failed?"RETRY SAVE":"SAVE","");if(!v->storage_available)n->flags=RISC_SCENE_DISABLED;
  bbp_node(d,RISC_SCENE_ACTION,BB_CANCEL,"CANCEL","");
 }else if(v->page==BB_MODIFIERS){
  static const char *mods[]={"Left Ctrl","Left Shift","Left Alt","Left GUI","Right Ctrl","Right Shift","Right Alt","Right GUI"};
  for(unsigned i=0;i<8;i++)bbp_node(d,RISC_SCENE_ACTION,BB_MODIFIER_FIRST+i,mods[i],(e->draft.modifiers&(1u<<i))?"ON":"OFF");
 }else if(v->page==BB_FORGET){
  bbp_node(d,RISC_SCENE_TEXT_NODE,0,"FORGET SAVED PEER?","Stop first. Pair again after forgetting.");bbp_node(d,RISC_SCENE_ACTION,BB_CANCEL,"CANCEL","");
  n=bbp_node(d,RISC_SCENE_ACTION,BB_FORGET_CONFIRM,"FORGET","");if(s->token||s->closing)n->flags=RISC_SCENE_DISABLED;
 }else if(v->page==BB_PAIR){
  snprintf(text,sizeof(text),"%06u",(unsigned)s->status.pairing_number);bbp_node(d,RISC_SCENE_TEXT_NODE,0,"COMPARE ON BOTH DEVICES",text);
  bbp_node(d,RISC_SCENE_ACTION,BB_PAIR_REJECT,"REJECT","");
  n=bbp_node(d,v->hold_available?RISC_COMPONENT_HOLD_KEY:RISC_SCENE_TEXT_NODE,v->hold_available?BB_PAIR_ACCEPT:0,"NUMBERS MATCH",v->hold_available?"Press and release to confirm":"Secure confirmation input unavailable");if(v->hold_available)n->step=RISC_HOLD_CONFIRM;
 }
 if(v->alarm&&(v->alarm->state==ALARM_STATE_CUE||v->alarm->state==ALARM_STATE_BLOCKED||v->alarm->state==ALARM_STATE_ALERT||v->alarm->state==ALARM_STATE_DISMISSING))for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].action==BB_RECONNECT||d->nodes[i].action==BB_PAIR_START)d->nodes[i].flags|=RISC_SCENE_DISABLED;
 if(e->unreadable)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"BUTTON STORAGE","Invalid or unreadable saved data; defaults shown");
 if(!v->storage_available)bbp_node(d,RISC_SCENE_TEXT_NODE,0,"STORAGE","SAVING UNAVAILABLE");
 if(v->notice&&v->notice[0])bbp_node(d,RISC_SCENE_TEXT_NODE,0,"NOTICE",v->notice);
}
