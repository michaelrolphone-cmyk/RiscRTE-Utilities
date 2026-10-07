#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "RiscTouchV1.h"
#include "RiscKeyValueV1.h"
#define HID_BUTTONS_KEY "hid_buttons"
#define HID_RECORD_SIZE 40u
#define HID_TAP_MS 300u
#define HID_TAP_SLOP 8

enum { HID_ACTION_KEY, HID_ACTION_MOUSE, HID_ACTION_WHEEL, HID_ACTION_MOVE };
typedef struct { uint8_t kind, modifiers, key, buttons; int8_t dx,dy,wheel; uint8_t reserved; } hid_assignment;
typedef struct { uint8_t code; const char *name; } hid_key_option;
static const hid_key_option hid_keys[]={
 {4,"A"},{5,"B"},{6,"C"},{7,"D"},{8,"E"},{9,"F"},{10,"G"},{11,"H"},{12,"I"},{13,"J"},{14,"K"},{15,"L"},{16,"M"},{17,"N"},{18,"O"},{19,"P"},{20,"Q"},{21,"R"},{22,"S"},{23,"T"},{24,"U"},{25,"V"},{26,"W"},{27,"X"},{28,"Y"},{29,"Z"},
 {30,"1"},{31,"2"},{32,"3"},{33,"4"},{34,"5"},{35,"6"},{36,"7"},{37,"8"},{38,"9"},{39,"0"},
 {40,"Enter"},{41,"Escape"},{42,"Backspace"},{43,"Tab"},{44,"Space"},{45,"Minus"},{46,"Equals"},{47,"Left bracket"},{48,"Right bracket"},{49,"Backslash"},{51,"Semicolon"},{52,"Quote"},{53,"Grave"},{54,"Comma"},{55,"Period"},{56,"Slash"},{57,"Caps Lock"},
 {58,"F1"},{59,"F2"},{60,"F3"},{61,"F4"},{62,"F5"},{63,"F6"},{64,"F7"},{65,"F8"},{66,"F9"},{67,"F10"},{68,"F11"},{69,"F12"},
 {70,"Print Screen"},{71,"Scroll Lock"},{72,"Pause"},{73,"Insert"},{74,"Home"},{75,"Page Up"},{76,"Delete"},{77,"End"},{78,"Page Down"},{79,"Right"},{80,"Left"},{81,"Down"},{82,"Up"}
};
#define HID_KEY_COUNT (sizeof(hid_keys)/sizeof(hid_keys[0]))
static inline unsigned hid_key_index(uint8_t key){for(unsigned i=0;i<HID_KEY_COUNT;i++)if(hid_keys[i].code==key)return i;return 0;}
static inline bool hid_assignment_valid(const hid_assignment *a){
 if(a->reserved||a->kind>HID_ACTION_MOVE)return false;
 if(a->kind==HID_ACTION_KEY)return a->key==hid_keys[hid_key_index(a->key)].code&&!a->buttons&&!a->dx&&!a->dy&&!a->wheel;
 if(a->modifiers||a->key)return false;
 if(a->kind==HID_ACTION_MOUSE)return (a->buttons==1||a->buttons==2||a->buttons==4)&&!a->dx&&!a->dy&&!a->wheel;
 if(a->kind==HID_ACTION_WHEEL)return !a->buttons&&!a->dx&&!a->dy&&(a->wheel==1||a->wheel==-1||a->wheel==3||a->wheel==-3);
 return !a->buttons&&!a->wheel&&((!a->dx&&(a->dy==1||a->dy==-1||a->dy==5||a->dy==-5||a->dy==15||a->dy==-15))||(!a->dy&&(a->dx==1||a->dx==-1||a->dx==5||a->dx==-5||a->dx==15||a->dx==-15)));
}
static inline void hid_defaults(hid_assignment a[4]){
 a[0]=(hid_assignment){.kind=HID_ACTION_KEY,.key=75};a[1]=(hid_assignment){.kind=HID_ACTION_KEY,.key=78};
 a[2]=(hid_assignment){.kind=HID_ACTION_KEY,.key=6,.modifiers=1};a[3]=(hid_assignment){.kind=HID_ACTION_KEY,.key=25,.modifiers=1};
}
static inline uint32_t hid_checksum(const uint8_t *p,unsigned n){uint32_t v=2166136261u;for(unsigned i=0;i<n;i++)v=(v^p[i])*16777619u;return v;}
static inline bool hid_encode(const hid_assignment a[4],uint8_t b[HID_RECORD_SIZE]){
 memset(b,0,HID_RECORD_SIZE);b[0]='H';b[1]='B';b[2]=1;b[3]=4;
 for(unsigned i=0;i<4;i++){if(!hid_assignment_valid(a+i))return false;const hid_assignment *v=a+i;uint8_t *p=b+4+8*i;p[0]=v->kind;p[1]=v->modifiers;p[2]=v->key;p[3]=v->buttons;p[4]=(uint8_t)v->dx;p[5]=(uint8_t)v->dy;p[6]=(uint8_t)v->wheel;}
 uint32_t c=hid_checksum(b,36);for(unsigned i=0;i<4;i++)b[36+i]=(uint8_t)(c>>(8*i));return true;
}
static inline bool hid_decode(const uint8_t *b,uint32_t n,hid_assignment out[4]){
 if(n!=HID_RECORD_SIZE||b[0]!='H'||b[1]!='B'||b[2]!=1||b[3]!=4)return false;
 uint32_t c=0;for(unsigned i=0;i<4;i++)c|=(uint32_t)b[36+i]<<(8*i);if(c!=hid_checksum(b,36))return false;
 hid_assignment temp[4];for(unsigned i=0;i<4;i++){const uint8_t*p=b+4+8*i;temp[i]=(hid_assignment){p[0],p[1],p[2],p[3],(int8_t)p[4],(int8_t)p[5],(int8_t)p[6],p[7]};if(!hid_assignment_valid(temp+i))return false;}
 memcpy(out,temp,sizeof(temp));return true;
}
/* Missing is distinct from invalid/unreadable. Defaults are never implicitly saved. */
static inline int hid_load(const risc_key_value_v1 *kv,hid_assignment out[4]){
 if(!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get||!kv->put)return -1;
 uint8_t b[HID_RECORD_SIZE];uint32_t n=0;int rc=kv->get(kv->context,HID_BUTTONS_KEY,b,sizeof(b),&n);
 if(rc==RISC_KEY_VALUE_NOT_FOUND)return 0;
 return rc==RISC_KEY_VALUE_OK&&hid_decode(b,n,out)?1:-1;
}
static inline bool hid_save(const risc_key_value_v1*kv,const hid_assignment a[4]){
 uint8_t b[HID_RECORD_SIZE],check[HID_RECORD_SIZE];uint32_t n=0;
 if(!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get||!kv->put||!hid_encode(a,b))return false;
 /* Failed writes stay failures even if an old or ambiguously written copy matches. */
 if(kv->put(kv->context,HID_BUTTONS_KEY,b,sizeof(b))!=RISC_KEY_VALUE_OK)return false;
 return kv->get(kv->context,HID_BUTTONS_KEY,check,sizeof(check),&n)==RISC_KEY_VALUE_OK&&n==sizeof(check)&&!memcmp(b,check,sizeof(b));
}

typedef struct { bool down,valid,blocked,owned; uint8_t peak,id,seen; uint8_t ids[RISC_TOUCH_MAX_CONTACTS]; int sx[RISC_TOUCH_MAX_CONTACTS],sy[RISC_TOUCH_MAX_CONTACTS]; int x,y; uint32_t began; } hid_gesture;
typedef struct { int8_t dx,dy; uint8_t click; } hid_gesture_output;
static inline int hid_abs(int v){return v<0?-v:v;}
static inline bool hid_pad_point(int x,int y){return x>=8&&x<232&&y>=80&&y<190;}
static inline void hid_gesture_reset(hid_gesture *g,bool blocked){memset(g,0,sizeof(*g));g->blocked=blocked;}
/* One authoritative sample per complete controller report; two contacts never
 * inherit a preceding one-finger motion baseline. A gap requires neutral first. */
static inline hid_gesture_output hid_gesture_sample(hid_gesture*g,const risc_touch_snapshot_v1*s,uint32_t now){
 hid_gesture_output o={0};unsigned n=s->contact_count;
 if(n>RISC_TOUCH_MAX_CONTACTS){hid_gesture_reset(g,true);return o;}
 if(g->blocked){if(!n)g->blocked=false;return o;}
 if(!n){if(g->down&&g->valid&&(uint32_t)(now-g->began)<=HID_TAP_MS)o.click=g->peak==2?2:1;hid_gesture_reset(g,false);return o;}
 const risc_touch_contact_v1*c=s->contacts;
 if(!g->down){g->down=true;g->valid=true;g->began=now;g->peak=(uint8_t)n;g->id=c->id;g->x=c->x;g->y=c->y;g->owned=hid_pad_point(c->x,c->y);}
 for(unsigned i=0;i<n;i++){
  if(!hid_pad_point(c[i].x,c[i].y)){g->valid=false;g->owned=false;}
  unsigned j=0;while(j<g->seen&&g->ids[j]!=c[i].id)j++;
  if(j==g->seen){if(j==RISC_TOUCH_MAX_CONTACTS){g->valid=false;continue;}g->ids[j]=c[i].id;g->sx[j]=c[i].x;g->sy[j]=c[i].y;g->seen++;}
  if(hid_abs((int)c[i].x-g->sx[j])>HID_TAP_SLOP||hid_abs((int)c[i].y-g->sy[j])>HID_TAP_SLOP)g->valid=false;
 }
 if(g->seen>2||(g->seen>1&&g->peak==1&&n==1))g->valid=false;
 if(n>2)g->valid=false;
 if(n>g->peak)g->peak=(uint8_t)n;
 if(g->owned&&n==1&&g->peak==1&&g->id==c->id&&hid_pad_point(c->x,c->y)&&hid_pad_point(g->x,g->y)){
  int dx=(int)c->x-g->x,dy=(int)c->y-g->y;
  o.dx=(int8_t)(dx<-127?-127:dx>127?127:dx);o.dy=(int8_t)(dy<-127?-127:dy>127?127:dy);
 }
 g->x=c->x;g->y=c->y;g->id=c->id;return o;
}
