#pragma once
#include "ble_hid_model.h"
/* Exact HBv1 codec remains public and unchanged. This controller fences raw
 * storage status before allowing any second callback (including readback). */
typedef struct {
 hid_assignment buttons[4],draft;
 unsigned selected;
 bool editing,save_failed,unreadable,terminal;
 bool (*alive)(void *);void *owner;
} ble_buttons_editor;
static inline bool bbe_live(ble_buttons_editor *e){
 if(e->terminal)return false;
 if(!e->alive||!e->alive(e->owner)){e->terminal=true;return false;}
 return true;
}
static inline bool bbe_status(ble_buttons_editor *e,int rc){
 if(!bbe_live(e))return false;
 if(rc==RISC_KEY_VALUE_CONTEXT||rc>0||rc<RISC_KEY_VALUE_BUSY){e->terminal=true;return false;}
 return true;
}
static inline void bbe_init(ble_buttons_editor *e,bool (*alive)(void *),void *owner){
 memset(e,0,sizeof(*e));e->alive=alive;e->owner=owner;hid_defaults(e->buttons);
}
static inline bool bbe_api(const risc_key_value_v1 *kv){return kv&&kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get&&kv->put;}
static inline bool bbe_load(ble_buttons_editor *e,const risc_key_value_v1 *kv){
 if(!bbe_live(e))return false;
 if(!bbe_api(kv)){e->unreadable=true;return false;}
 uint8_t bytes[HID_RECORD_SIZE];uint32_t n=0;
 int rc=kv->get(kv->context,HID_BUTTONS_KEY,bytes,sizeof(bytes),&n);
 if(!bbe_status(e,rc))return false;
 if(rc==RISC_KEY_VALUE_NOT_FOUND&&n==0){e->unreadable=false;return true;}
 if(rc!=RISC_KEY_VALUE_OK||!hid_decode(bytes,n,e->buttons)){e->unreadable=true;return false;}
 e->unreadable=false;return true;
}
static inline void bbe_begin(ble_buttons_editor *e,unsigned index){
 if(e->terminal||index>=4)return;
 e->selected=index;e->draft=e->buttons[index];e->editing=true;e->save_failed=false;
}
static inline void bbe_cancel(ble_buttons_editor *e){e->editing=false;e->save_failed=false;}
static inline void bbe_kind(ble_buttons_editor *e){
 if(!e->editing||e->terminal)return;
 unsigned k=(e->draft.kind+1)%4;e->draft=(hid_assignment){.kind=(uint8_t)k};
 if(k==0)e->draft.key=4;else if(k==1)e->draft.buttons=1;else if(k==2)e->draft.wheel=1;else e->draft.dx=5;
}
static inline void bbe_value(ble_buttons_editor *e,int delta){
 if(!e->editing||e->terminal||(delta!=-1&&delta!=1))return;
 hid_assignment *d=&e->draft;
 if(d->kind==HID_ACTION_KEY){int n=(int)hid_key_index(d->key)+delta;if(n<0)n=(int)HID_KEY_COUNT-1;if(n>=(int)HID_KEY_COUNT)n=0;d->key=hid_keys[n].code;}
 else if(d->kind==HID_ACTION_MOUSE){unsigned n=d->buttons==1?0:d->buttons==2?1:2;n=(unsigned)((int)n+delta+3)%3;d->buttons=(uint8_t)(1u<<n);}
 else if(d->kind==HID_ACTION_WHEEL){static const int8_t v[]={1,-1,3,-3};unsigned n=0;while(n<3&&v[n]!=d->wheel)n++;d->wheel=v[((int)n+delta+4)%4];}
 else {static const int8_t v[][2]={{5,0},{-5,0},{0,5},{0,-5},{15,0},{-15,0},{0,15},{0,-15},{1,0},{-1,0},{0,1},{0,-1}};unsigned n=0;while(n<11&&(v[n][0]!=d->dx||v[n][1]!=d->dy))n++;n=(unsigned)((int)n+delta+12)%12;d->dx=v[n][0];d->dy=v[n][1];}
}
static inline void bbe_modifier(ble_buttons_editor *e,unsigned bit){
 if(e->editing&&!e->terminal&&e->draft.kind==HID_ACTION_KEY&&bit<8)e->draft.modifiers^=(uint8_t)(1u<<bit);
}
static inline bool bbe_save(ble_buttons_editor *e,const risc_key_value_v1 *kv){
 if(!bbe_live(e)||!e->editing)return false;
 e->save_failed=true;
 if(!bbe_api(kv))return false;
 hid_assignment next[4];memcpy(next,e->buttons,sizeof(next));next[e->selected]=e->draft;
 uint8_t bytes[HID_RECORD_SIZE],check[HID_RECORD_SIZE];uint32_t n=0;
 if(!hid_encode(next,bytes))return false;
 int rc=kv->put(kv->context,HID_BUTTONS_KEY,bytes,sizeof(bytes));
 if(!bbe_status(e,rc)||rc!=RISC_KEY_VALUE_OK)return false;
 rc=kv->get(kv->context,HID_BUTTONS_KEY,check,sizeof(check),&n);
 if(!bbe_status(e,rc)||rc!=RISC_KEY_VALUE_OK||n!=sizeof(check)||memcmp(bytes,check,sizeof(bytes)))return false;
 memcpy(e->buttons,next,sizeof(next));e->editing=false;e->save_failed=false;e->unreadable=false;return true;
}
