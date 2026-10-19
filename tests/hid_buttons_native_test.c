/* Real production controller with declared service doubles. Pixel proof lives
 * in the separate actual-adapter fixture. */
#define HID_TOUCHPAD 0
#define PORTABLE_BLE_BUTTONS_NATIVE_UI
#define PORTABLE_NATIVE_CANVAS
#define main old_hid_app_test_main
#include "hid_app_test.c"
#undef main
static int native_w,native_h;
static int32_t native_width(void){return native_w;}
static int32_t native_height(void){return native_h;}
static t5_app_api_v1 native_app;
static unsigned native_pixels,native_texts,native_icons;
void portable_ble_buttons_native_begin(void){}
void portable_ble_buttons_native_layout(uint64_t id){(void)id;}
void portable_ble_buttons_native_present(bool full){present(full);}
bool portable_ble_buttons_native_ready(uint64_t*g,uint64_t*q){*g=1;*q=0;return true;}
static risc_touch_event_v1 native_events[2];static unsigned native_event_count,native_event_at;
static int32_t native_next(void*c,uint64_t t,risc_touch_event_v1*e){(void)c;assert(t==33);if(native_event_at==native_event_count)return 0;*e=native_events[native_event_at++];drained=e->sequence;return 1;}
static risc_touch_api_v1 native_touch;

int portable_nova_measure(unsigned f,const char*s){(void)f;return (int)strlen(s)*6;}
void portable_nova_native_round(int x,int y,int w,int h,int r,uint32_t c){(void)r;(void)c;if(w<=0||h<=0)return;assert(x>=0&&y>=0&&x+w<=native_w&&y+h<=native_h);native_pixels++;}
void portable_nova_native_fill(int x,int y,int w,int h,uint32_t c){portable_nova_native_round(x,y,w,h,0,c);}
void portable_nova_native_text(unsigned f,int x,int y,int w,const char*s,uint32_t c){(void)f;(void)c;assert(x>=0&&y>=0&&x+w<=native_w&&y<native_h);text_capture(s);native_texts++;}
bool portable_nova_native_icon(int x,int y,int size,const char*n,uint32_t c){(void)c;assert(x>=0&&y>=0&&size>0&&x+size<=native_w&&y+size<=native_h&&!strcmp(n,"solid:f053"));native_icons++;return true;}
static void native_reset(int w,int h){reset();native_w=w;native_h=h;native_app=fake_app;native_app.screen_width=native_width;native_app.screen_height=native_height;app=&native_app;sample.width=(uint16_t)w;sample.height=(uint16_t)h;reset_button_queue();}
static void tap_rect(hid_native_rect r){assert(r.w&&r.h);tap(r.x+r.w/2,r.y+r.h/2);}
static void contact_rect(unsigned n,hid_native_rect r){
 unsigned before=sample.contact_count;uint16_t x=(uint16_t)(r.x+r.w/2),y=(uint16_t)(r.y+r.h/2);tick+=20;
 sample.contact_count=(uint8_t)n;sample.contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};sample.contacts[1]=(risc_touch_contact_v1){.id=2,.x=(uint16_t)(native_w/2),.y=(uint16_t)(native_h/2)};
 native_event_at=0;native_event_count=1;
 native_events[0]=(risc_touch_event_v1){.id=1,.x=x,.y=y,.timestamp_ms=tick,.sequence=++raw_seq,.kind=n?(before?RISC_TOUCH_EVENT_MOVE:RISC_TOUCH_EVENT_DOWN):RISC_TOUCH_EVENT_UP};
 if(n==2){native_events[1]=(risc_touch_event_v1){.id=2,.x=(uint16_t)(native_w/2),.y=(uint16_t)(native_h/2),.timestamp_ms=tick,.sequence=++raw_seq,.kind=RISC_TOUCH_EVENT_DOWN};native_event_count++;}
 native_touch=touch_api;native_touch.next=native_next;raw=&native_touch;pump();
}

static void bounds(hid_native_rect r,int w,int h){if(!(r.x>=0&&r.y>=0&&r.w>=0&&r.h>=0&&r.x+r.w<=w&&r.y+r.h<=h)){fprintf(stderr,"Bad rect %d,%d %dx%d on %dx%d\n",r.x,r.y,r.w,r.h,w,h);assert(0);}}
static void layout(void){
 hid_native_layout l;assert(!hid_native_layout_make(NULL,240,240));assert(!hid_native_layout_make(&l,0,240));assert(!hid_native_layout_make(&l,240,65536));
 for(int w=1;w<1024;w+=7)for(int h=1;h<1024;h+=11){
  assert(hid_native_layout_make(&l,w,h));bounds(l.back,w,h);bounds(l.title,w,h);bounds(l.content,w,h);bounds(l.status,w,h);
  for(unsigned i=0;i<2;i++){bounds(l.footer[i],w,h);bounds(l.setting_pair[i],w,h);}
  for(unsigned i=0;i<4;i++){bounds(l.buttons[i],w,h);bounds(l.setting_buttons[i],w,h);}
  for(unsigned i=0;i<8;i++)bounds(l.modifiers[i],w,h);
  bounds(l.setting_forget,w,h);bounds(l.edit_kind,w,h);bounds(l.edit_prev,w,h);bounds(l.edit_value,w,h);bounds(l.edit_next,w,h);bounds(l.edit_modifiers,w,h);
 }
}
static void controls(int w,int h){
 native_reset(w,h);hid_native_layout l=hid_native_geometry();draw();assert(native_texts&&native_icons&&native_pixels);
 for(unsigned i=0;i<4;i++){
  hid_native_rect r=l.buttons[i];assert(button_at(r.x,r.y)==(int)i);assert(button_at(r.x+r.w-1,r.y+r.h-1)==(int)i);
  assert(button_at(r.x+r.w,r.y)!= (int)i);assert(button_at(r.x,r.y+r.h)!=(int)i);
 }
 tap_rect(l.footer[1]);assert(view==HID_VIEW_SETTINGS);draw();
 for(unsigned i=0;i<4;i++){
  tap_rect(l.setting_buttons[i]);assert(view==HID_VIEW_EDIT&&selected==i);draw();hid_assignment original=assignments[i];
  tap_rect(l.edit_next);assert(draft.key!=original.key);tap_rect(l.edit_prev);assert(draft.key==original.key);
  tap_rect(l.edit_modifiers);assert(view==HID_VIEW_MODIFIERS);draft.modifiers=0;
  for(unsigned bit=0;bit<8;bit++){tap_rect(l.modifiers[bit]);assert(draft.modifiers==(uint8_t)((1u<<(bit+1))-1u));draw();}
  assert(back()&&view==HID_VIEW_EDIT);draft.modifiers=5;storage_bad=true;tap_rect(l.footer[1]);assert(view==HID_VIEW_EDIT&&save_failed&&assignments[i].modifiers==original.modifiers);draw();
  storage_bad=false;tap_rect(l.footer[1]);assert(view==HID_VIEW_MAIN&&!save_failed&&assignments[i].modifiers==5);tap_rect(l.footer[1]);
 }
 tap_rect(l.setting_forget);assert(view==HID_VIEW_FORGET);draw();tap_rect(l.footer[0]);assert(view==HID_VIEW_MAIN&&!forgets);
 tap_rect(l.footer[1]);tap_rect(l.setting_forget);tap_rect(l.footer[1]);assert(view==HID_VIEW_MAIN&&forgets==1);
 tap_rect(l.footer[1]);tap_rect(l.setting_buttons[0]);tap_rect(l.edit_kind);assert(draft.kind==HID_ACTION_MOUSE);draw();tap_rect(l.edit_next);tap_rect(l.edit_prev);tap_rect(l.footer[0]);assert(view==HID_VIEW_MAIN&&assignments[0].kind==HID_ACTION_KEY);
 start_session(false);step(0,0,0);for(unsigned i=0;i<4;i++){unsigned before=key_reports;contact_rect(1,l.buttons[i]);assert(held_button==(int)i&&key_reports==before+1);draw();contact_rect(0,l.buttons[i]);assert(held_button==-1&&key_reports==before+2&&!last_key&&!last_mod);}
 /* Multi-contact interruption never substitutes another button or emits repeats. */
 contact_rect(1,l.buttons[0]);assert(held_button==0);contact_rect(2,l.buttons[0]);assert(held_button==-1&&!last_key);step(0,0,0);
 assignments[0]=(hid_assignment){.kind=HID_ACTION_WHEEL,.wheel=-3};contact_rect(1,l.buttons[0]);assert(reports[report_count-1].wheel==-3);unsigned count=mouse_reports;for(unsigned i=0;i<30;i++)contact_rect(1,l.buttons[0]);assert(mouse_reports==count);contact_rect(0,l.buttons[0]);
 stop_session("next");start_session(true);fake_status.state=RISC_HID_PAIR_CONFIRM;fake_status.pairing_number=42;step(0,0,0);draw();assert(strstr(drawn,"000042"));tap_rect(l.footer[1]);assert(confirms==0);contact_rect(1,l.footer[1]);contact_rect(0,l.footer[1]);assert(confirms==1&&confirm_choice);
 stop_session("done");assert(!grants&&!raw_subs&&!token);printf("Native Buttons %dx%d: all pages, mappings/modifiers, save/retry/cancel, contacts and pairing PASS\n",w,h);
}
int main(void){layout();int sizes[][2]={{128,128},{160,128},{240,240},{410,502},{320,180},{502,410},{480,800}};for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++)controls(sizes[i][0],sizes[i][1]);puts("Native bounded geometry and controller coverage PASS");}
