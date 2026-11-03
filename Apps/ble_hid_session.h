#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "RiscBluetoothHidV1.h"
#include "ble_hid_model.h"

/* Invocation-local controller. The owner supplies a native identity fence;
 * false is terminal and forbids every subsequent endpoint callback. */
typedef struct {
 const risc_bluetooth_hid_v1 *api;
 bool (*alive)(void *); void *owner;
 uint64_t token,last_poll,last_report;
 risc_bluetooth_hid_status_v1 status;
 hid_assignment held;
 bool terminal,closing,neutral,have_poll,holding,dirty,pair_accepted;
 int held_index;
} ble_hid_session;
static inline bool bhs_live(ble_hid_session *s){
 if(s->terminal)return false;
 if(!s->alive||!s->alive(s->owner)){s->terminal=true;return false;}
 return true;
}
static inline bool bhs_init(ble_hid_session *s,const risc_bluetooth_hid_v1 *api,
                           bool (*alive)(void *),void *owner){
 memset(s,0,sizeof(*s));s->api=api;s->alive=alive;s->owner=owner;s->held_index=-1;
 if(!bhs_live(s))return false;
 return api&&api->api_version==1&&api->struct_size>=sizeof(*api)&&api->open&&api->poll&&api->status&&api->confirm_pairing&&api->keyboard&&api->mouse&&api->release_all&&api->close&&api->forget_bond&&api->battery;
}
static inline bool bhs_release(ble_hid_session *s){
 if(!bhs_live(s))return false;
 s->holding=false;s->held_index=-1;s->neutral=false;
 if(!s->token||s->closing)return !s->closing;
 bool ok=s->api->release_all(s->api->context,s->token);
 if(!bhs_live(s))return false;
 if(!ok)s->closing=true;
 return ok;
}
static inline bool bhs_close(ble_hid_session *s){
 if(!bhs_live(s))return false;
 s->holding=false;s->held_index=-1;s->neutral=false;
 if(!s->token){s->closing=false;return true;}
 s->closing=true;
 bool ok=s->api->close(s->api->context,s->token);
 if(!bhs_live(s))return false;
 if(ok){s->token=0;s->closing=false;s->have_poll=false;memset(&s->status,0,sizeof(s->status));s->dirty=true;}
 return ok;
}
static inline bool bhs_open(ble_hid_session *s,const char *name,bool pairing,uint64_t now){
 if(!bhs_live(s)||s->token||s->closing)return false;
 uint64_t token=0;
 bool ok=s->api->open(s->api->context,name,pairing,&token);
 if(!bhs_live(s))return false;
 s->token=token;s->pair_accepted=false;s->neutral=false;s->last_poll=now;s->have_poll=true;s->dirty=true;
 if(!ok||!token){if(token)s->closing=true;return false;}
 return true;
}
static inline bool bhs_ready(const ble_hid_session *s,const hid_assignment *a){
 const uint32_t secure=RISC_HID_ENCRYPTED|RISC_HID_AUTHENTICATED;
 return s->token&&!s->closing&&s->status.state==RISC_HID_READY&&
 (s->status.flags&secure)==secure&&
 (s->status.flags&(a->kind==HID_ACTION_KEY?RISC_HID_KEYBOARD_READY:RISC_HID_MOUSE_READY));
}
static inline bool bhs_report(ble_hid_session *s,const hid_assignment *a){
 if(!bhs_live(s)||!bhs_ready(s,a))return false;
 bool ok;
 if(a->kind==HID_ACTION_KEY){uint8_t keys[6]={a->key,0,0,0,0,0};ok=s->api->keyboard(s->api->context,s->token,a->modifiers,keys);}
 else ok=s->api->mouse(s->api->context,s->token,a->buttons,a->dx,a->dy,a->wheel);
 if(!bhs_live(s))return false;
 if(!ok)s->closing=true;
 return ok;
}
static inline void bhs_neutral(ble_hid_session *s){if(!s->terminal&&!s->holding&&!s->closing)s->neutral=true;}
static inline bool bhs_press(ble_hid_session *s,unsigned index,const hid_assignment *a,uint64_t now){
 if(!bhs_live(s)||!s->neutral||s->holding||index>=4||!hid_assignment_valid(a)||!bhs_ready(s,a))return false;
 s->neutral=false;s->held=*a;s->held_index=(int)index;
 if(!bhs_report(s,a))return false;
 s->holding=true;s->last_report=now;return true;
}
static inline bool bhs_poll(ble_hid_session *s,uint64_t now){
 if(!bhs_live(s))return false;
 if(s->closing)return bhs_close(s);
 if(!s->token)return true;
 if(s->have_poll&&(now<s->last_poll||now-s->last_poll>100)){
  if(!bhs_release(s))return false;
 }
 s->last_poll=now;s->have_poll=true;
 bool ok=s->api->poll(s->api->context,s->token,8);
 if(!bhs_live(s))return false;
 if(!ok){s->closing=true;return false;}
 risc_bluetooth_hid_status_v1 next={.struct_size=sizeof(next)};
 ok=s->api->status(s->api->context,s->token,&next);
 if(!bhs_live(s))return false;
 if(!ok||next.struct_size!=sizeof(next)||next.state>RISC_HID_FAULT||(next.flags&~31u)||next.pairing_number>999999){s->closing=true;return false;}
 bool changed=next.state!=s->status.state||next.flags!=s->status.flags||next.pairing_number!=s->status.pairing_number||next.connection_generation!=s->status.connection_generation;
 if(changed){if(!bhs_release(s))return false;s->dirty=true;}
 if(next.state==RISC_HID_PAIR_CONFIRM&&(s->status.state!=RISC_HID_PAIR_CONFIRM||next.connection_generation!=s->status.connection_generation||next.pairing_number!=s->status.pairing_number))s->pair_accepted=false;
 if(s->status.state==RISC_HID_PAIR_CONFIRM&&next.state!=RISC_HID_PAIR_CONFIRM&&!s->pair_accepted){s->closing=true;return false;}
 s->status=next;
 if(next.state==RISC_HID_FAULT){s->closing=true;return false;}
 if(s->holding&&(s->held.kind==HID_ACTION_KEY||s->held.kind==HID_ACTION_MOUSE)&&now-s->last_report>=400){
  if(!bhs_report(s,&s->held))return false;
  s->last_report=now;
 }
 return true;
}
static inline bool bhs_confirm(ble_hid_session *s,uint32_t generation,uint32_t number,bool accept){
 if(!bhs_live(s)||s->closing||!s->token||s->status.state!=RISC_HID_PAIR_CONFIRM||s->status.connection_generation!=generation||s->status.pairing_number!=number)return false;
 bool ok=s->api->confirm_pairing(s->api->context,s->token,accept);
 if(!bhs_live(s))return false;
 s->neutral=false;if(!ok||!accept)s->closing=true;else s->pair_accepted=true;return ok;
}
static inline bool bhs_forget(ble_hid_session *s){
 if(!bhs_live(s)||s->token||s->closing)return false;
 bool ok=s->api->forget_bond(s->api->context);
 return bhs_live(s)&&ok;
}
static inline bool bhs_battery(ble_hid_session *s,uint8_t percent){
 if(!bhs_live(s)||!s->token||s->closing||(percent>100&&percent!=255))return false;
 bool ok=s->api->battery(s->api->context,s->token,percent);
 if(!bhs_live(s))return false;
 if(!ok)s->closing=true;
 return ok;
}
