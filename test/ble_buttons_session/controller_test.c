#include <assert.h>
#include <stdio.h>
#include "ble_buttons_session.h"
#include "ble_buttons_editor.h"
static bool live=true,close_ok=true,open_ok=true;static unsigned calls,reports,releases,closes,gets,put_count;static int put_rc,get_rc;static unsigned stored_size=40;static uint8_t stored[40];
static risc_bluetooth_hid_status_v1 current;
static bool alive(void *p){(void)p;return live;}
static bool op(void *p,const char*n,bool pair,uint64_t*t){(void)p;(void)n;(void)pair;calls++;*t=7;return open_ok;}
static bool poller(void*p,uint64_t t,uint32_t n){(void)p;(void)t;(void)n;calls++;return true;}
static bool status(void*p,uint64_t t,risc_bluetooth_hid_status_v1*out){(void)p;(void)t;calls++;*out=current;return true;}
static bool confirm(void*p,uint64_t t,bool b){(void)p;(void)t;(void)b;calls++;return true;}
static bool key(void*p,uint64_t t,uint8_t m,const uint8_t k[6]){(void)p;(void)t;(void)m;(void)k;calls++;reports++;return true;}
static bool mouse(void*p,uint64_t t,uint8_t b,int8_t x,int8_t y,int8_t w){(void)p;(void)t;(void)b;(void)x;(void)y;(void)w;calls++;reports++;return true;}
static bool release(void*p,uint64_t t){(void)p;(void)t;calls++;releases++;return true;}
static bool close_(void*p,uint64_t t){(void)p;(void)t;calls++;closes++;return close_ok;}
static bool forget(void*p){(void)p;calls++;return true;}
static bool battery(void*p,uint64_t t,uint8_t b){(void)p;(void)t;(void)b;calls++;return true;}
static int32_t get(void*p,const char*k,void*b,uint32_t cap,uint32_t*n){(void)p;assert(!strcmp(k,"hid_buttons"));gets++;*n=0;if(get_rc)return get_rc;assert(cap>=stored_size);memcpy(b,stored,stored_size);*n=stored_size;return 0;}
static int32_t put(void*p,const char*k,const void*b,uint32_t n){(void)p;assert(!strcmp(k,"hid_buttons"));put_count++;if(put_rc)return put_rc;memcpy(stored,b,n);stored_size=n;return 0;}
static risc_bluetooth_hid_v1 api={1,sizeof(api),0,op,poller,status,confirm,key,mouse,release,close_,forget,battery};
static risc_key_value_v1 kv={1,sizeof(kv),0,get,put};
static void reset(ble_buttons_session*s){live=close_ok=open_ok=true;calls=reports=releases=closes=0;current=(risc_bluetooth_hid_status_v1){sizeof(current),RISC_HID_READY,15,0,0,1};assert(bbs_init(s,&api,alive,0));assert(bbs_open(s,false,0));assert(bbs_poll(s,0));bbs_neutral(s);}
int main(void){
 ble_buttons_session s;hid_assignment a[4];hid_defaults(a);reset(&s);
 assert(bbs_press(&s,0,a,0));assert(reports==1);for(unsigned t=20;t<=380;t+=20)assert(bbs_poll(&s,t));assert(reports==1);assert(bbs_poll(&s,400));assert(reports==2);assert(bbs_release(&s));assert(!bbs_press(&s,0,a,401));bbs_neutral(&s);assert(bbs_press(&s,0,a,401));assert(bbs_poll(&s,502));assert(!s.holding&&!s.neutral);
 reset(&s);a[0]=(hid_assignment){.kind=HID_ACTION_WHEEL,.wheel=1};assert(bbs_press(&s,0,a,0));for(unsigned t=20;t<=1000;t+=20)assert(bbs_poll(&s,t));assert(reports==1);
 reset(&s);hid_defaults(a);current.flags=RISC_HID_KEYBOARD_READY;assert(bbs_poll(&s,20));bbs_neutral(&s);assert(!bbs_press(&s,0,a,20));
 reset(&s);assert(bbs_press(&s,0,a,0));current.connection_generation++;assert(bbs_poll(&s,20));assert(!s.holding&&!s.neutral);
 reset(&s);close_ok=false;assert(!bbs_close(&s));unsigned n=calls;assert(!bbs_press(&s,0,a,0));assert(!bbs_battery(&s,50));assert(calls==n);assert(!bbs_poll(&s,20));assert(closes==2);close_ok=true;assert(bbs_poll(&s,40));assert(!s.token);
 reset(&s);live=false;n=calls;assert(!bbs_poll(&s,20));assert(!bbs_close(&s));assert(calls==n&&s.terminal);
 live=true;assert(bbs_init(&s,&api,alive,0));open_ok=false;assert(!bbs_open(&s,false,0));assert(s.token&&s.closing);assert(bbs_close(&s));
 ble_buttons_editor e;bbe_init(&e,alive,0);get_rc=RISC_KEY_VALUE_NOT_FOUND;gets=put_count=0;assert(bbe_load(&e,&kv));assert(put_count==0&&e.buttons[0].key==75);bbe_begin(&e,0);bbe_value(&e,1);assert(e.draft.key==76);put_rc=RISC_KEY_VALUE_IO;assert(!bbe_save(&e,&kv));assert(gets==1&&e.save_failed&&e.editing&&e.buttons[0].key==75);put_rc=0;get_rc=0;assert(bbe_save(&e,&kv));assert(!e.editing&&e.buttons[0].key==76&&stored_size==40);assert(hid_decode(stored,40,a));
 bbe_begin(&e,1);for(unsigned i=0;i<8;i++)bbe_modifier(&e,i);assert(e.draft.modifiers==255);bbe_cancel(&e);assert(e.buttons[1].modifiers==0);bbe_begin(&e,1);bbe_kind(&e);assert(e.draft.kind==HID_ACTION_MOUSE&&e.draft.buttons==1);bbe_value(&e,-1);assert(e.draft.buttons==4);bbe_kind(&e);assert(e.draft.wheel==1);bbe_value(&e,-1);assert(e.draft.wheel==-3);bbe_kind(&e);assert(e.draft.dx==5);bbe_value(&e,-1);assert(e.draft.dy==-1);
 put_rc=RISC_KEY_VALUE_CONTEXT;n=gets;assert(!bbe_save(&e,&kv));assert(e.terminal&&gets==n);put_rc=0;assert(!bbe_save(&e,&kv));assert(gets==n);
 put_count=0;bbe_init(&e,alive,0);get_rc=0;stored[0]=0;assert(!bbe_load(&e,&kv)&&e.unreadable&&put_count==0);
 put_count=0;bbe_init(&e,alive,0);get_rc=-99;assert(!bbe_load(&e,&kv)&&e.terminal);
 put_count=0;put_count=put_count;printf("controller regression cases passed\n");return 0;
}
