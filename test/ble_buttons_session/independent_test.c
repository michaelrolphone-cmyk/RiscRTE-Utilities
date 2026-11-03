#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_buttons_session.h"
#include "ble_buttons_editor.h"

enum endpoint { OPEN, POLL, STATUS, CONFIRM, KEY, MOUSE, RELEASE, CLOSE,
                FORGET, BATTERY, GET, PUT, ENDPOINTS };
typedef struct {
 bool live, ok[ENDPOINTS], copy_failed_put;
 int kill, get_rc, put_rc;
 unsigned calls[ENDPOINTS], total;
 uint32_t bytes;
 uint8_t stored[HID_RECORD_SIZE];
 risc_bluetooth_hid_status_v1 status;
} fixture;
static fixture f;
static unsigned cases;
static void entered(enum endpoint e) {
 ++f.calls[e]; ++f.total;
 if(f.kill==(int)e) f.live=false;
}
static bool alive(void *p) {(void)p; return f.live;}
static bool open_cb(void*p,const char*n,bool pair,uint64_t*t) {
 (void)p;(void)n;(void)pair;entered(OPEN);*t=19;return f.ok[OPEN];
}
static bool poll_cb(void*p,uint64_t t,uint32_t n) {
 (void)p;assert(t==19);assert(n==8);entered(POLL);return f.ok[POLL];
}
static bool status_cb(void*p,uint64_t t,risc_bluetooth_hid_status_v1*out) {
 (void)p;assert(t==19);entered(STATUS);*out=f.status;return f.ok[STATUS];
}
static bool confirm_cb(void*p,uint64_t t,bool yes) {
 (void)p;(void)yes;assert(t==19);entered(CONFIRM);return f.ok[CONFIRM];
}
static bool key_cb(void*p,uint64_t t,uint8_t m,const uint8_t keys[6]) {
 (void)p;(void)m;assert(t==19);assert(keys[0]);entered(KEY);return f.ok[KEY];
}
static bool mouse_cb(void*p,uint64_t t,uint8_t b,int8_t x,int8_t y,int8_t w) {
 (void)p;(void)b;(void)x;(void)y;(void)w;assert(t==19);entered(MOUSE);return f.ok[MOUSE];
}
static bool release_cb(void*p,uint64_t t) {
 (void)p;assert(t==19);entered(RELEASE);return f.ok[RELEASE];
}
static bool close_cb(void*p,uint64_t t) {
 (void)p;assert(t==19);entered(CLOSE);return f.ok[CLOSE];
}
static bool forget_cb(void*p) {(void)p;entered(FORGET);return f.ok[FORGET];}
static bool battery_cb(void*p,uint64_t t,uint8_t v) {
 (void)p;(void)v;assert(t==19);entered(BATTERY);return f.ok[BATTERY];
}
static int32_t get_cb(void*p,const char*k,void*b,uint32_t cap,uint32_t*n) {
 (void)p;assert(!strcmp(k,HID_BUTTONS_KEY));entered(GET);*n=0;
 if(f.get_rc) return f.get_rc;
 assert(cap>=f.bytes);memcpy(b,f.stored,f.bytes);*n=f.bytes;return 0;
}
static int32_t put_cb(void*p,const char*k,const void*b,uint32_t n) {
 (void)p;assert(!strcmp(k,HID_BUTTONS_KEY));assert(n==HID_RECORD_SIZE);entered(PUT);
 if(!f.put_rc||f.copy_failed_put){memcpy(f.stored,b,n);f.bytes=n;}
 return f.put_rc;
}
static risc_bluetooth_hid_v1 api={1,sizeof(api),0,open_cb,poll_cb,status_cb,
 confirm_cb,key_cb,mouse_cb,release_cb,close_cb,forget_cb,battery_cb};
static risc_key_value_v1 kv={1,sizeof(kv),0,get_cb,put_cb};
static void fresh(void) {
 memset(&f,0,sizeof(f));f.live=true;f.kill=-1;
 for(unsigned i=0;i<ENDPOINTS;i++)f.ok[i]=true;
 f.status=(risc_bluetooth_hid_status_v1){sizeof(f.status),RISC_HID_READY,15,0,0,3};
 hid_assignment a[4];hid_defaults(a);assert(hid_encode(a,f.stored));f.bytes=HID_RECORD_SIZE;
}
static void ready(ble_buttons_session*s) {
 fresh();assert(bbs_init(s,&api,alive,0));assert(bbs_open(s,false,0));
 assert(bbs_poll(s,0));bbs_neutral(s);
}
static void pairing(ble_buttons_session*s) {
 ready(s);f.status.state=RISC_HID_PAIR_CONFIRM;f.status.flags=0;f.status.pairing_number=123456;
 assert(bbs_poll(s,20));
}
static void no_more_session_io(ble_buttons_session*s) {
 unsigned n=f.total;hid_assignment a[4];hid_defaults(a);
 assert(!bbs_poll(s,40));assert(!bbs_close(s));assert(!bbs_release(s));
 assert(!bbs_open(s,false,40));assert(!bbs_press(s,0,a,40));
 assert(!bbs_battery(s,42));assert(!bbs_forget(s));
 assert(!bbs_confirm(s,3,123456,true));assert(f.total==n&&s->terminal);
}
static void callback_identity_loss(void) {
 for(int endpoint=OPEN;endpoint<=BATTERY;endpoint++) {
  ble_buttons_session s;hid_assignment a[4];hid_defaults(a);
  if(endpoint==OPEN||endpoint==FORGET){fresh();assert(bbs_init(&s,&api,alive,0));}
  else if(endpoint==CONFIRM)pairing(&s);else ready(&s);
  f.kill=endpoint;unsigned before=f.total;bool result=true;
  switch(endpoint) {
   case OPEN:result=bbs_open(&s,false,0);break;
   case POLL:case STATUS:result=bbs_poll(&s,20);break;
   case CONFIRM:result=bbs_confirm(&s,3,123456,true);break;
   case KEY:result=bbs_press(&s,0,a,0);break;
   case MOUSE:a[0]=(hid_assignment){.kind=HID_ACTION_MOUSE,.buttons=1};result=bbs_press(&s,0,a,0);break;
   case RELEASE:result=bbs_release(&s);break;
   case CLOSE:result=bbs_close(&s);break;
   case FORGET:result=bbs_forget(&s);break;
   case BATTERY:result=bbs_battery(&s,50);break;
  }
  assert(!result&&!f.live&&f.total>before);no_more_session_io(&s);++cases;
 }
}
static void cleanup_only(void) {
 ble_buttons_session s;hid_assignment a[4];hid_defaults(a);ready(&s);
 f.ok[CLOSE]=false;assert(!bbs_close(&s));unsigned n=f.total;
 assert(!bbs_press(&s,0,a,0));assert(!bbs_battery(&s,50));assert(!bbs_release(&s));
 assert(!bbs_open(&s,false,20));assert(!bbs_forget(&s));assert(f.total==n);
 assert(!bbs_poll(&s,20)&&f.total==n+1);f.ok[CLOSE]=true;
 assert(bbs_poll(&s,40)&&!s.token&&!s.closing);++cases;
 fresh();assert(bbs_init(&s,&api,alive,0));f.ok[OPEN]=false;
 assert(!bbs_open(&s,false,0)&&s.token==19&&s.closing);
 n=f.total;assert(!bbs_press(&s,0,a,0)&&f.total==n);assert(bbs_close(&s));++cases;
 for(int endpoint=POLL;endpoint<=STATUS;endpoint++) {
  ready(&s);f.ok[endpoint]=false;assert(!bbs_poll(&s,20)&&s.closing);
  n=f.total;assert(!bbs_press(&s,0,a,20)&&f.total==n);
  assert(bbs_poll(&s,40)&&!s.token);++cases;
 }
 ready(&s);assert(bbs_press(&s,0,a,0));f.ok[RELEASE]=false;
 unsigned polls=f.calls[POLL];assert(!bbs_poll(&s,101)&&s.closing);
 assert(f.calls[POLL]==polls);assert(bbs_poll(&s,102)&&!s.token);++cases;
}
static void timing_and_neutral(void) {
 for(unsigned kind=0;kind<4;kind++) {
  ble_buttons_session s;ready(&s);hid_assignment a={.kind=(uint8_t)kind};
  if(kind==HID_ACTION_KEY)a.key=4;else if(kind==HID_ACTION_MOUSE)a.buttons=1;
  else if(kind==HID_ACTION_WHEEL)a.wheel=1;else a.dx=5;
  assert(bbs_press(&s,0,&a,0));for(uint64_t t=20;t<=1000;t+=20)assert(bbs_poll(&s,t));
  assert(f.calls[KEY]+f.calls[MOUSE]==(kind<=HID_ACTION_MOUSE?3u:1u));
  assert(bbs_release(&s));unsigned n=f.total;assert(!bbs_press(&s,0,&a,1000)&&f.total==n);
  bbs_neutral(&s);assert(bbs_press(&s,0,&a,1000));++cases;
 }
 ble_buttons_session s;hid_assignment a[4];hid_defaults(a);ready(&s);assert(bbs_press(&s,0,a,0));
 assert(bbs_poll(&s,100)&&s.holding);assert(bbs_poll(&s,201)&&!s.holding&&!s.neutral);++cases;
 ready(&s);assert(bbs_press(&s,0,a,10));assert(bbs_poll(&s,20));
 assert(bbs_poll(&s,19)&&!s.holding&&!s.neutral);++cases;
}
static void pairing_scope(void) {
 ble_buttons_session s;pairing(&s);unsigned n=f.total;
 assert(!bbs_confirm(&s,4,123456,true));assert(!bbs_confirm(&s,3,123457,true));
 assert(f.total==n);assert(bbs_confirm(&s,3,123456,true));
 f.status.state=RISC_HID_READY;f.status.flags=15;f.status.pairing_number=0;
 assert(bbs_poll(&s,40)&&!s.closing);++cases;
 for(unsigned variant=0;variant<3;variant++) {
  pairing(&s);assert(bbs_confirm(&s,3,123456,true));assert(s.pair_accepted);
  if(variant!=0)++f.status.connection_generation;
  if(variant!=1)++f.status.pairing_number;
  assert(bbs_poll(&s,40));
  assert(!s.pair_accepted); /* Acknowledging challenge A must not approve B. */
  f.status.state=RISC_HID_READY;f.status.flags=15;f.status.pairing_number=0;
  assert(!bbs_poll(&s,60)&&s.closing);++cases;
 }
 pairing(&s);assert(bbs_confirm(&s,3,123456,false)&&s.closing);++cases;
}
static void storage_fences(void) {
 for(int endpoint=GET;endpoint<=PUT;endpoint++) {
  fresh();ble_buttons_editor e;bbe_init(&e,alive,0);assert(bbe_load(&e,&kv));
  bbe_begin(&e,0);bbe_value(&e,1);f.kill=endpoint;assert(!bbe_save(&e,&kv));
  assert(e.terminal&&e.buttons[0].key==75);unsigned n=f.total;
  assert(!bbe_save(&e,&kv));assert(!bbe_load(&e,&kv));assert(f.total==n);++cases;
 }
 fresh();ble_buttons_editor e;bbe_init(&e,alive,0);assert(bbe_load(&e,&kv));
 bbe_begin(&e,0);bbe_value(&e,1);f.put_rc=RISC_KEY_VALUE_IO;f.copy_failed_put=true;
 unsigned gets=f.calls[GET];assert(!bbe_save(&e,&kv));assert(f.calls[GET]==gets);
 assert(e.save_failed&&e.editing&&e.buttons[0].key==75);hid_assignment actual[4];
 assert(hid_decode(f.stored,f.bytes,actual)&&actual[0].key==76);++cases;
 f.put_rc=0;assert(bbe_save(&e,&kv)&&!e.editing&&e.buttons[0].key==76);++cases;
 fresh();bbe_init(&e,alive,0);assert(bbe_load(&e,&kv));bbe_begin(&e,0);bbe_value(&e,1);
 f.put_rc=RISC_KEY_VALUE_CONTEXT;gets=f.calls[GET];assert(!bbe_save(&e,&kv));
 assert(e.terminal&&f.calls[GET]==gets);++cases;
 fresh();bbe_init(&e,alive,0);f.stored[0]=0;assert(!bbe_load(&e,&kv));
 assert(e.unreadable&&f.calls[PUT]==0);++cases;
}
int main(void) {
 callback_identity_loss();cleanup_only();timing_and_neutral();pairing_scope();storage_fences();
 printf("independent Buttons session/editor: %u cases passed\n",cases);return 0;
}
