#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_provider.h"
#include "PortableHomeCanvas.h"
static unsigned width,height,format,stride;
static unsigned char *allocation,*pixels;
static uint64_t sequence;
static unsigned polls,physical_state;
static bool brightness(void*c,uint16_t v,uint16_t m){(void)c;return m&&v<=m;}
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=width,.height=height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(format)};return true;}
static bool acquire(void*c,uint32_t encoding,risc_display_surface_v1*out){(void)c;assert(encoding==format);*out=(risc_display_surface_v1){1,pixels,width,height,stride,(size_t)stride*height,format};return true;}
static void release(void*c,uint64_t id){(void)c;assert(id==1);}
static bool submit(void*c,uint64_t id,const risc_display_rect_v1*r,size_t count,const risc_display_present_options_v1*o,uint64_t*out){(void)c;(void)r;(void)count;(void)o;assert(id==1);*out=++sequence;return true;}
static bool status(void*c,uint64_t id,risc_display_present_status_v1*out){(void)c;assert(id==sequence);out->state=physical_state?physical_state:(polls++?RISC_DISPLAY_PRESENT_COMPLETE:RISC_DISPLAY_PRESENT_QUEUED);return true;}
static risc_touch_snapshot_v1 raw;
static uint64_t subscribe(void*c){(void)c;return 1;}
static bool unsubscribe(void*c,uint64_t id){(void)c;return id==1;}
static bool touch_poll(void*c,size_t n){(void)c;(void)n;return true;}
static int32_t touch_next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)id;(void)out;return 0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=raw;return true;}
static uint16_t color(unsigned x,unsigned y){return (uint16_t)(x+256u*y);}
static void run(unsigned w,unsigned h,unsigned output,unsigned rotation){
 width=w;height=h;format=output;stride=(format==RISC_DISPLAY_FORMAT_RGB565?width*2:(width+7)/8)+3;
 size_t bytes=(size_t)stride*height;allocation=malloc(bytes+32);assert(allocation);memset(allocation,0xa5,bytes+32);pixels=allocation+16;
 risc_display_output_api_v1 display={.api_version=1,.struct_size=sizeof(display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&display,NULL,rotation);
 home_canvas_surface bridge;assert(home_canvas_open(&bridge,&p.api,brightness,NULL));
 const risc_display_output_api_v1*a=&bridge.display;risc_display_info_v1 logical;
 assert(a->get_info(a->context,&logical));
 assert(logical.width==(rotation%2?height:width)&&logical.height==(rotation%2?width:height));
 risc_display_surface_v1 frame={0};
 assert(!a->acquire(a->context,RISC_DISPLAY_FORMAT_MONO1,&frame));
 assert(a->acquire(a->context,RISC_DISPLAY_FORMAT_RGB565,&frame));
 assert(!home_canvas_close(&bridge)&&!home_canvas_quiesce(&bridge));
 risc_display_present_options_v1 unsupported={RISC_DISPLAY_PRESENT_CLEAN,0,0};uint64_t receipt=0;
 assert(!a->submit(a->context,frame.frame,NULL,0,&unsupported,&receipt)&&bridge.lease==frame.frame);
 a->release(a->context,frame.frame);assert(!bridge.lease);
 assert(a->acquire(a->context,RISC_DISPLAY_FORMAT_RGB565,&frame));
 for(unsigned y=0;y<frame.height;y++)for(unsigned x=0;x<frame.width;x++){uint16_t v=color(x,y);unsigned char*b=(unsigned char*)frame.pixels+(size_t)y*frame.stride_bytes+x*2;b[0]=(unsigned char)v;b[1]=(unsigned char)(v>>8);}
 assert(a->submit(a->context,frame.frame,NULL,0,NULL,&receipt));polls=0;
 assert(!bridge.lease&&bridge.receipt==receipt&&!home_canvas_close(&bridge));
 assert(!a->acquire(a->context,RISC_DISPLAY_FORMAT_RGB565,&frame));
 risc_display_present_status_v1 pending={0};assert(a->present_status(a->context,receipt,&pending)&&pending.state==RISC_DISPLAY_PRESENT_QUEUED);

 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
  unsigned lx=x,ly=y;switch(rotation){case 1:lx=y;ly=width-1-x;break;case 2:lx=width-1-x;ly=height-1-y;break;case 3:lx=height-1-y;ly=x;break;default:break;}
  uint16_t expected=color(lx,ly);
  if(output==RISC_DISPLAY_FORMAT_RGB565){unsigned char*b=pixels+(size_t)y*stride+x*2;assert((uint16_t)(b[0]|(uint16_t)b[1]<<8)==expected);}
  else {bool black=(pixels[(size_t)y*stride+x/8]&(0x80u>>(x&7)))!=0;assert(black!=canvas_rgb565_white(expected,x,y));}
 }
 for(unsigned n=0;n<16;n++)assert(allocation[n]==0xa5&&allocation[16+bytes+n]==0xa5);
 risc_display_present_status_v1 settled={0};assert(a->present_status(a->context,receipt,&settled)&&settled.state==RISC_DISPLAY_PRESENT_COMPLETE);
 assert(!bridge.receipt&&!p.frames.receipts);
 assert(home_canvas_quiesce(&bridge)&&home_canvas_resume(&bridge));
 assert(a->set_brightness(a->context,30,100)&&!a->set_brightness(a->context,101,100));
 assert(home_canvas_close(&bridge));free(allocation);
}
static int existing_surface_checks(void){
 unsigned previous=0;
 for(unsigned gray=0;gray<256;gray++){
  uint16_t p=(uint16_t)((gray*31/255)<<11|(gray*63/255)<<5|(gray*31/255));unsigned white=0;
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)white+=canvas_rgb565_white(p,x,y);
  assert(white>=previous);previous=white;if(!gray)assert(!white);if(gray==255)assert(white==16);
 }
 unsigned red=0,green=0,blue=0;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){red+=canvas_rgb565_white(0xf800,x,y);green+=canvas_rgb565_white(0x07e0,x,y);blue+=canvas_rgb565_white(0x001f,x,y);}
 assert(green>red&&red>blue);
 const unsigned sizes[][2]={{7,11},{240,240},{410,502},{480,800},{256,256}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)for(unsigned rotation=0;rotation<4;rotation++){
  run(sizes[i][0],sizes[i][1],RISC_DISPLAY_FORMAT_RGB565,rotation);
  run(sizes[i][0],sizes[i][1],RISC_DISPLAY_FORMAT_MONO1,rotation);
 }


 width=17;height=23;format=RISC_DISPLAY_FORMAT_RGB565;stride=34;pixels=calloc(height,stride);
 risc_display_output_api_v1 input_display={.api_version=1,.struct_size=sizeof(input_display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
 raw=(risc_touch_snapshot_v1){.sequence=1,.width=17,.height=23};
 canvas_provider input_provider;canvas_provider_init(&input_provider,&input_display,&touch,1);
 home_canvas_surface input_bridge;assert(home_canvas_open(&input_bridge,&input_provider.api,brightness,NULL));
 home_canvas_observation observation=home_canvas_capture(&input_bridge);assert(!observation.valid);
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&!observation.count);
 raw.sequence++;raw.contact_count=1;raw.contacts[0]=(risc_touch_contact_v1){.id=19,.x=3,.y=7};
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.count==1&&observation.x==4&&observation.y==5);
 uint8_t identity=observation.id;
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.id==identity);
 raw.sequence++;raw.contacts[0].id=20;
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.id!=identity);
 raw.sequence++;raw.contact_count=5;
 for(unsigned i=0;i<5;i++)raw.contacts[i]=(risc_touch_contact_v1){.id=(uint8_t)(30+i),.x=(uint16_t)i,.y=2};
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.count==2);
 raw.sequence++;raw.contact_count=0;observation=home_canvas_capture(&input_bridge);assert(observation.valid&&!observation.count);
 assert(home_canvas_close(&input_bridge));free(pixels);
 /* A lower provider's failed transfer is uncertain custody, never success. */
 width=height=4;format=RISC_DISPLAY_FORMAT_RGB565;stride=8;pixels=calloc(1,32);
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&d,NULL,0);home_canvas_surface bridge;
 assert(home_canvas_open(&bridge,&p.api,brightness,NULL));risc_display_surface_v1 f;
 assert(bridge.display.acquire(&bridge,RISC_DISPLAY_FORMAT_RGB565,&f));
 uint64_t receipt;assert(bridge.display.submit(&bridge,f.frame,NULL,0,NULL,&receipt));
 physical_state=RISC_DISPLAY_PRESENT_FAILED;risc_display_present_status_v1 r;
 assert(!bridge.display.present_status(&bridge,receipt,&r)&&bridge.retained);
 assert(!home_canvas_close(&bridge)&&!home_canvas_resume(&bridge)&&!home_canvas_quiesce(&bridge));
 assert(!bridge.display.acquire(&bridge,RISC_DISPLAY_FORMAT_RGB565,&f));
 free(pixels);
 puts("Home Canvas adapter:40 real-provider raster geometry/rotation/format cases, receipt lifecycle, cancellation, quiesce/resume and custody fence PASS");
 return 0;
}

#include "rf_capability_canvas.h"
static const risc_canvas_api_v1 *owned_canvas_api;
static bool with_input,with_controls,deny_release,change_owner,control_failure;
static unsigned control_calls,control_raster_calls;
static unsigned release_calls,retain_calls,activate_calls,deactivate_calls;
static canvas_provider *input_provider_owned;
static unsigned nav_phase;
static bool nav_poll(void *ctx,risc_input_navigation_frame_v1 *out) {
 (void)ctx;*out=(risc_input_navigation_frame_v1){0};
 if(nav_phase==1){out->buttons=out->pressed=RISC_NAV_BACK;nav_phase=2;}
 else if(nav_phase==2){out->released=RISC_NAV_BACK;nav_phase=0;}
 return true;
}
static bool nav_reset(void *ctx){(void)ctx;nav_phase=0;return true;}
static bool nav_foreground(void *ctx,const risc_input_foreground_v1 *claims,size_t count) {
 (void)ctx;(void)claims;assert(count==0);return true;
}
static const risc_input_navigation_api_v1 buttons_only={.api_version=1,.struct_size=sizeof(buttons_only),
 .poll=nav_poll,.reset=nav_reset,.foreground=nav_foreground};
static int32_t input_activate(void *c,uint64_t session){(void)c;assert(session);activate_calls++;return canvas_provider_bind_inputs(input_provider_owned,session,NULL,&buttons_only);}
static int32_t input_deactivate(void *c,uint64_t session){(void)c;assert(session);deactivate_calls++;return canvas_provider_unbind_inputs(input_provider_owned,session);}
static const risc_canvas_input_v1 input_api={.api_version=1,.struct_size=sizeof(input_api),
 .tag=RISC_CANVAS_INPUT_TAG,.activate=input_activate,.deactivate=input_deactivate};
static bool control_info(void *ctx,risc_display_info_v1 *out) {
 (void)ctx;*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),
 .width=777,.height=888,.flags=RISC_DISPLAY_INFO_BRIGHTNESS};return true;
}
static bool control_set(void *ctx,uint16_t value,uint16_t maximum) {
 (void)ctx;assert(value==30&&maximum==100);control_calls++;return !control_failure;
}
static bool control_acquire(void *ctx,uint32_t format,risc_display_surface_v1 *out) {
 (void)ctx;(void)format;(void)out;control_raster_calls++;assert(!"control grant must not supply raster");return false;
}
static const risc_display_output_api_v1 controls_api={.api_version=1,.struct_size=sizeof(controls_api),
 .get_info=control_info,.set_brightness=control_set,.acquire=control_acquire};
static bool runtime_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 assert(version==1&&instance==0);
 if(!strcmp(name,RISC_CANVAS_RASTER_CAPABILITY))grant->api=owned_canvas_api;
 else if(!strcmp(name,RISC_CANVAS_INPUT_CAPABILITY)){if(!with_input)return false;grant->api=&input_api;}
 else if(!strcmp(name,"display.output")){if(!with_controls)return false;grant->api=&controls_api;}
 else {assert(!"unexpected physical capability request");return false;}
 grant->slot=1;grant->generation=1;return true;
}
static bool runtime_release(risc_runtime_capability_v1 *grant){assert(grant->api);release_calls++;return !deny_release;}
static const risc_runtime_api_v1 owner={.api_version=1,.struct_size=sizeof(owner),.acquire=runtime_acquire,.release=runtime_release};
static const risc_runtime_api_v1 other={.api_version=1,.struct_size=sizeof(other)};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?(change_owner?&other:&owner):NULL;}
static void retained(void *c){(void)c;retain_calls++;}
static void owned_case(bool input,bool release_failure,bool replace_owner) {
 width=17;height=23;format=RISC_DISPLAY_FORMAT_RGB565;stride=34;physical_state=0;polls=0;
 pixels=calloc(height,stride);assert(pixels);
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,
 .acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&d,NULL,0);
 input_provider_owned=&p;owned_canvas_api=&p.api;with_input=input;deny_release=false;change_owner=false;
 release_calls=retain_calls=activate_calls=deactivate_calls=0;
 rf_capability_canvas c={0};
 assert(rf_canvas_open(&c,&owner,NULL,retained,NULL)==RF_CANVAS_READY);
 assert(c.surface.encoding==RISC_CANVAS_RGB565_ADAPTIVE&&c.surface.geometry.width==17);
 assert(rf_canvas_input_open(&c)==(input?RF_CANVAS_READY:RF_CANVAS_UNAVAILABLE));
 assert(c.input_active==input);
 if(input) {
  const risc_touch_api_v1 *logical=&c.touch.api;
  uint64_t sub=logical->subscribe(logical->context);assert(sub);
  assert(logical->poll(logical->context,1)); /* initial geometry gap */
  nav_phase=1;assert(logical->poll(logical->context,1));
  bool controls=false;
  risc_input_navigation_frame_v1 down=home_canvas_touch_navigation(&c.touch,&controls);
  risc_input_navigation_frame_v1 up=home_canvas_touch_navigation(&c.touch,&controls);
  assert(down.pressed==RISC_NAV_BACK&&up.released==RISC_NAV_BACK);
  assert(!c.touch.snapshot.contact_count&&!p.touch);
  assert(rf_canvas_close(&c)==RF_CANVAS_PENDING&&!release_calls);
  assert(logical->unsubscribe(logical->context,sub));
 }
 assert(rf_canvas_pause(&c)==RF_CANVAS_READY&&c.surface.quiesced&&!c.input_active);
 assert(rf_canvas_resume(&c)==RF_CANVAS_READY&&!c.surface.quiesced&&c.input_active==input);
 if(input)assert(activate_calls==2&&deactivate_calls==1);
 activate_calls=deactivate_calls=0;
 assert(!c.surface.brightness); /* Physical output grant was never requested. */
 risc_display_surface_v1 f={0};
 assert(c.surface.display.acquire(&c.surface,RISC_DISPLAY_FORMAT_RGB565,&f));
 assert(rf_canvas_close(&c)==RF_CANVAS_PENDING&&!release_calls);
 c.surface.display.release(&c.surface,f.frame);
 assert(!c.surface.lease);
 if(input)assert(rf_canvas_prepare_sleep(&c)==RF_CANVAS_UNAVAILABLE);
 else {assert(rf_canvas_prepare_sleep(&c)==RF_CANVAS_READY);assert(rf_canvas_resume_sleep(&c)==RF_CANVAS_READY);}
 deny_release=release_failure;change_owner=replace_owner;
 int result=rf_canvas_close(&c);
 if(release_failure||replace_owner) {
  assert(result==RF_CANVAS_RETAINED&&retain_calls==1);
  unsigned prior=release_calls;
  assert(rf_canvas_close(&c)==RF_CANVAS_RETAINED);
  assert(rf_canvas_input_open(&c)==RF_CANVAS_RETAINED&&release_calls==prior&&retain_calls==1);
 }else {assert(result==RF_CANVAS_READY);assert(release_calls==(input?2u:1u));assert(deactivate_calls==(input?1u:0u));}
 change_owner=false;deny_release=false;free(pixels);
}
static void control_case(bool available,bool fail) {
 width=17;height=23;format=RISC_DISPLAY_FORMAT_RGB565;stride=34;physical_state=0;
 pixels=calloc(height,stride);assert(pixels);
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,
 .acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&d,NULL,0);owned_canvas_api=&p.api;
 with_input=false;with_controls=available;deny_release=change_owner=false;
 control_failure=fail;control_calls=control_raster_calls=release_calls=retain_calls=0;
 rf_capability_canvas c={0};
 assert(rf_canvas_open(&c,&owner,NULL,retained,NULL)==RF_CANVAS_READY);
 assert(rf_canvas_controls_open(&c)==(available?RF_CANVAS_READY:RF_CANVAS_UNAVAILABLE));
 risc_display_info_v1 logical;assert(c.surface.display.get_info(&c.surface,&logical));
 assert(logical.width==17&&logical.height==23);
 assert(!!(logical.flags&RISC_DISPLAY_INFO_BRIGHTNESS)==available);
 if(available) {
  assert(c.surface.display.set_brightness(&c.surface,30,100)==!fail);
  assert(control_calls==1);
 }
 assert(!control_raster_calls);
 if(fail&&available)assert(c.retained&&retain_calls==1&&rf_canvas_close(&c)==RF_CANVAS_RETAINED);
 else {assert(rf_canvas_close(&c)==RF_CANVAS_READY);assert(release_calls==(available?2u:1u));}
 with_controls=false;control_failure=false;free(pixels);
}
int main(void) {
 assert(existing_surface_checks()==0);
 owned_case(false,false,false);owned_case(true,false,false);
 owned_case(false,true,false);owned_case(true,true,false);
 owned_case(false,false,true);owned_case(true,false,true);
 control_case(false,false);control_case(true,false);control_case(true,true);
 puts("RF capability canvas lifecycle and optional controls cases PASS");return 0;
}
