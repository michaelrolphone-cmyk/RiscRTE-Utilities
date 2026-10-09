#include "ble_hid_model.h"
#include <assert.h>
#include <stdio.h>
static hid_gesture g;
static risc_touch_snapshot_v1 s;
static uint32_t now;
static hid_gesture_output at(unsigned n,int x,int y,int x2,int y2){
 s.contact_count=(uint8_t)n;s.contacts[0]=(risc_touch_contact_v1){.id=0,.x=(uint16_t)x,.y=(uint16_t)y};
 s.contacts[1]=(risc_touch_contact_v1){.id=7,.x=(uint16_t)x2,.y=(uint16_t)y2};now+=20;
 return hid_gesture_sample_bounds(&g,&s,now,0,0,s.width,s.height);
}
static void reset(unsigned width){hid_gesture_reset(&g,false);s=(risc_touch_snapshot_v1){.width=(uint16_t)width,.height=(uint16_t)width};now=0;}
static void tap(void){assert(!at(1,100,100,0,0).click);assert(at(0,0,0,0,0).click==1);}
int main(void){
 reset(240);tap();for(unsigned i=0;i<5;i++)at(0,0,0,0,0);
 at(1,101,101,0,0);hid_gesture_output o=at(1,106,101,0,0);assert(!o.dx&&!o.buttons);
 o=at(1,121,103,0,0);assert(o.button_changed&&o.buttons==1&&o.dx==20&&o.dy==2&&!o.click);
 o=at(1,126,111,0,0);assert(o.buttons==1&&o.dx==5&&o.dy==8&&!o.button_changed);
 o=at(1,126,111,0,0);assert(o.buttons==1&&!o.dx&&!o.dy);
 o=at(0,0,0,0,0);assert(o.button_changed&&!o.buttons&&!o.click&&!g.armed);
 /* Two quick stationary taps remain two clicks; late/distant presses move. */
 reset(240);tap();at(1,100,100,0,0);assert(at(0,0,0,0,0).click==1);
 now+=301;at(1,100,100,0,0);o=at(1,110,100,0,0);assert(o.dx==10&&!o.buttons);at(0,0,0,0,0);
 tap();at(1,170,100,0,0);o=at(1,180,100,0,0);assert(o.dx==10&&!o.buttons);
 /* Bounds, ID substitution, third finger, second finger during drag and GAP cancel. */
 for(unsigned fault=0;fault<5;fault++){
  reset(240);tap();at(1,100,100,0,0);assert(at(1,120,100,0,0).buttons==1);
  if(fault==0)o=at(1,240,100,0,0);
  else if(fault==1){s.contacts[0].id=9;o=hid_gesture_sample_bounds(&g,&s,now+20,0,0,240,240);}
  else if(fault==2)o=at(3,120,100,170,100);
  else if(fault==3)o=at(2,120,100,170,100);
  else o=hid_gesture_cancel(&g);
  assert(o.button_changed&&!o.buttons&&g.blocked);o=at(1,130,100,0,0);assert(!o.buttons&&!o.dx);at(0,0,0,0,0);assert(!g.blocked);
 }
 /* Match IDs through reorder; centroid gives equal signed XY regardless order. */
 reset(240);at(2,80,80,140,100);o=at(2,92,92,152,112);assert(o.wheel_x==3&&o.wheel_y==-3&&g.axis==HID_SCROLL_FREE);
 risc_touch_contact_v1 temp=s.contacts[0];s.contacts[0]=s.contacts[1];s.contacts[1]=temp;
 s.contacts[0].x+=4;s.contacts[1].x+=4;s.contacts[0].y+=4;s.contacts[1].y+=4;
 o=hid_gesture_sample_bounds(&g,&s,now+20,0,0,240,240);assert(o.wheel_x==1&&o.wheel_y==-1);
 /* Diagonal start remains fluid even after long nearly-axial movement. */
 for(unsigned i=0;i<10;i++){s.contacts[0].x++;s.contacts[1].x++;o=hid_gesture_sample_bounds(&g,&s,now+40+i*20,0,0,240,240);assert(g.axis==HID_SCROLL_FREE);}
 /* Both directions and axes lock; tiny transverse jitter cannot leak. */
 for(unsigned axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2){
  reset(240);at(2,90,90,150,110);
  o=at(2,90+(axis?1:12*sign),90+(axis?12*sign:1),150+(axis?1:12*sign),110+(axis?12*sign:1));
  assert(g.axis==(axis?HID_SCROLL_Y:HID_SCROLL_X));assert(axis?!o.wheel_x:!o.wheel_y);
  for(unsigned i=0;i<8;i++){s.contacts[0].x+=(axis?(i%2?-1:1):sign);s.contacts[1].x+=(axis?(i%2?-1:1):sign);s.contacts[0].y+=(axis?sign:(i%2?-1:1));s.contacts[1].y+=(axis?sign:(i%2?-1:1));o=hid_gesture_sample_bounds(&g,&s,now+40+i*20,0,0,240,240);assert(axis?!o.wheel_x:!o.wheel_y);}
 }
 /* A deliberate perpendicular turn crosses the wider threshold; no old suppressed delta bursts. */
 reset(240);at(2,50,50,100,70);at(2,62,51,112,71);assert(g.axis==HID_SCROLL_X);
 for(unsigned i=0;i<10;i++){s.contacts[0].y+=5;s.contacts[1].y+=5;o=hid_gesture_sample_bounds(&g,&s,now+40+i*20,0,0,240,240);assert(o.wheel_y>=-2);}
 assert(g.axis==HID_SCROLL_FREE&&o.wheel_y<0);
 /* Subpixel EMA state must preserve slow deliberate turns in either axis. */
 for(unsigned scale=1;scale<=2;scale++)for(unsigned axis=0;axis<2;axis++){
  reset(240*scale);at(2,50*scale,50*scale,100*scale,70*scale);
  at(2,(50+(axis?0:12))*scale,(50+(axis?12:0))*scale,(100+(axis?0:12))*scale,(70+(axis?12:0))*scale);
  int moved=0;
  for(unsigned i=0;i<60;i++){
   if(axis){s.contacts[0].x+=scale;s.contacts[1].x+=scale;}
   else{s.contacts[0].y+=scale;s.contacts[1].y+=scale;}
   o=hid_gesture_sample_bounds(&g,&s,now+40+i*20,0,0,s.width,s.height);moved+=axis?o.wheel_x:-o.wheel_y;
  }
  assert(g.axis==HID_SCROLL_FREE&&moved>0);
 }
 /* Lifting one scroll finger never resumes pointer; returning it cancels until neutral. */
 o=at(1,62,120,0,0);assert(!o.dx&&!o.dy&&!o.click);o=at(2,62,120,112,140);assert(g.blocked&&!o.wheel_y);assert(!at(0,0,0,0,0).click);
 reset(240);at(2,80,80,140,100);at(1,80,80,0,0);assert(at(0,0,0,0,0).click==2);
 /* Pinch with stable centroid emits no scroll and is not a click. */
 reset(240);at(2,80,80,140,100);o=at(2,60,80,160,100);assert(!o.wheel_x&&!o.wheel_y);assert(!at(0,0,0,0,0).click);
 reset(240);at(2,80,80,140,100);s.contacts[1].id=0;o=hid_gesture_sample_bounds(&g,&s,now+20,0,0,240,240);assert(g.blocked&&!o.click);
 /* Same physical threshold at the paper surface's doubled density. */
 reset(480);at(2,100,100,200,140);o=at(2,124,124,224,164);assert(o.wheel_x==3&&o.wheel_y==-3);
 reset(240);now=UINT32_MAX-90;tap();at(1,100,100,0,0);o=at(1,120,100,0,0);assert(o.buttons==1);assert(at(0,0,0,0,0).button_changed);
 puts("Touchpad gestures: drag/double-tap, neutral/cancel, identity, XY rails, hysteresis, diagonal/pinch, density and wrap PASS");
}
