#include "ble_hid_model.h"
#include <assert.h>
#include <stdio.h>
static uint8_t stored[HID_RECORD_SIZE];static uint32_t size;static bool fail_put,fail_get,corrupt_read;static unsigned writes;
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(!strcmp(k,HID_BUTTONS_KEY));*n=size;if(fail_get)return RISC_KEY_VALUE_IO;if(!size)return RISC_KEY_VALUE_NOT_FOUND;if(cap<size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,stored,size);if(corrupt_read)((uint8_t*)b)[0]^=1;return RISC_KEY_VALUE_OK;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!strcmp(k,HID_BUTTONS_KEY)&&n==sizeof(stored));writes++;if(fail_put)return RISC_KEY_VALUE_IO;memcpy(stored,b,n);size=n;return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static risc_touch_snapshot_v1 sample(unsigned n,int x,int y){risc_touch_snapshot_v1 s={.width=240,.height=240,.contact_count=(uint8_t)n};s.contacts[0]=(risc_touch_contact_v1){.id=2,.x=(uint16_t)x,.y=(uint16_t)y};s.contacts[1]=(risc_touch_contact_v1){.id=5,.x=180,.y=130};return s;}
int main(void){
 hid_assignment a[4],b[4];hid_defaults(a);assert(hid_load(&kv,b)==0&&!writes);assert(hid_save(&kv,a)&&hid_load(&kv,b)==1&&!memcmp(a,b,sizeof(a)));
 uint8_t encoded[40];assert(hid_encode(a,encoded));for(unsigned i=0;i<40;i++){uint8_t old=encoded[i];encoded[i]^=1;assert(!hid_decode(encoded,40,b));encoded[i]=old;}
 assert(!hid_decode(encoded,39,b));fail_put=true;a[0].key=40;assert(!hid_save(&kv,a)&&hid_load(&kv,b)==1&&b[0].key==75);fail_put=false;corrupt_read=true;assert(!hid_save(&kv,a));corrupt_read=false;fail_get=true;assert(hid_load(&kv,b)==-1);fail_get=false;
 for(unsigned i=0;i<256;i++){a[0]=(hid_assignment){.kind=HID_ACTION_KEY,.key=(uint8_t)i,.modifiers=255};bool found=false;for(unsigned j=0;j<HID_KEY_COUNT;j++)if(hid_keys[j].code==i)found=true;assert(hid_assignment_valid(a)==found);}
 a[0]=(hid_assignment){.kind=HID_ACTION_MOUSE,.buttons=31};assert(!hid_assignment_valid(a));a[0]=(hid_assignment){.kind=HID_ACTION_MOVE,.dx=-128};assert(!hid_assignment_valid(a));
 hid_gesture g;hid_gesture_reset(&g,false);risc_touch_snapshot_v1 s=sample(1,100,120);hid_gesture_output o=hid_gesture_sample(&g,&s,0);assert(!o.click&&!o.dx);s=sample(0,0,0);o=hid_gesture_sample(&g,&s,100);assert(o.click==1);
 s=sample(1,100,120);hid_gesture_sample(&g,&s,200);s=sample(2,100,120);hid_gesture_sample(&g,&s,220);s.contact_count=1;s.contacts[0]=s.contacts[1];o=hid_gesture_sample(&g,&s,240);assert(!o.dx&&!o.dy);s.contact_count=0;o=hid_gesture_sample(&g,&s,260);assert(o.click==2);
 s=sample(2,100,120);hid_gesture_sample(&g,&s,300);s.contacts[1].x+=20;hid_gesture_sample(&g,&s,320);s.contact_count=0;assert(!hid_gesture_sample(&g,&s,340).click);
 s=sample(1,100,120);hid_gesture_sample(&g,&s,400);s.contacts[0].x+=30;o=hid_gesture_sample(&g,&s,420);assert(o.dx==30&&!o.click);s.contact_count=0;assert(!hid_gesture_sample(&g,&s,440).click);
 s=sample(1,10,30);hid_gesture_sample(&g,&s,500);s.contacts[0].y=120;assert(!hid_gesture_sample(&g,&s,520).dy);s.contacts[0].x+=10;assert(!hid_gesture_sample(&g,&s,530).dx);s.contact_count=0;assert(!hid_gesture_sample(&g,&s,540).click);
 s=sample(1,100,120);hid_gesture_sample(&g,&s,600);s.contact_count=0;assert(!hid_gesture_sample(&g,&s,1000).click);
 hid_gesture_reset(&g,true);s=sample(1,100,120);hid_gesture_sample(&g,&s,1100);s.contact_count=0;assert(!hid_gesture_sample(&g,&s,1120).click);s.contact_count=1;hid_gesture_sample(&g,&s,1140);s.contact_count=0;assert(hid_gesture_sample(&g,&s,1160).click==1);
 hid_gesture_reset(&g,false);s=sample(1,100,120);hid_gesture_sample(&g,&s,UINT32_MAX-100);s.contact_count=0;assert(hid_gesture_sample(&g,&s,40).click==1);
 puts("HID model: left/right tap, second-contact motion, bounds, gaps, wrap and atomic persistence passed");
}
