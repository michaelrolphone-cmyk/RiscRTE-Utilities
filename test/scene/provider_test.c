#define main existing_scene_main
#ifndef STOPWATCH_SCENE_HOST_FIXTURE
#error Pass the exact selected System scene host fixture path
#endif
#include STOPWATCH_SCENE_HOST_FIXTURE
#undef main
#include "RiscSceneLifecycleV1.h"
extern void stopwatch_document(risc_scene_document_v1 *);
static void render(const char *out){if(!out)return;FILE*f=fopen(out,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",w,h);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned char rgb[3];if(format==5){unsigned v=((unsigned short*)(pixels+16))[y*w+x];rgb[0]=(v>>11)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;}else{unsigned v=(pixels[16+y*((w+7)/8)+x/8]>>(7-x%8))&1;rgb[0]=rgb[1]=rgb[2]=v?0:255;}fwrite(rgb,1,3,f);}fclose(f);}
int main(int argc,char**argv){
 assert(argc==2);unsigned shape=(unsigned)atoi(argv[1]);
 if(shape==0){w=h=240;format=5;}else if(shape==1){w=h=466;format=5;}else {w=480;h=800;format=1;}
 prof.preferred_format=format;prof.font_scale=shape==0?1:2;prof.row_height=shape==0?36:64;
 if(shape==2){prof.foreground_rgb565=0;prof.background_rgb565=65535;prof.accent_rgb565=0;}
 driver=t5_driver_get(2);api=driver->capability;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));stopwatch_document(&doc);assert(api->open(NULL,&doc,NULL,&session)==0);
 const risc_scene_lifecycle_api_v1*l=risc_scene_lifecycle_get_v1(api);assert(l&&l->configure(NULL,session,0)==0);settle();render(getenv("STOPWATCH_PPM"));
 /* All actions remain accessible through provider-owned native navigation,
  * regardless of class pagination. No coordinates exist in the app. */
 for(unsigned action=1;action<=4;action++){
  nav_pressed=RISC_NAV_DOWN;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);settle();nav_pressed=RISC_NAV_CONFIRM;
  assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_ACTION_EVENT&&e.action==action);
  doc.revision++;assert(api->update(NULL,session,&doc)==0);settle();
 }
 /* A held real touch is reported busy, even with no pending raster. */
 event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms,1,0,30,100};risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
 risc_scene_navigation_v1 state={.struct_size=sizeof(state)};uint32_t flags=0;assert(api->snapshot(NULL,session,&state,&flags)==0);assert(flags&RISC_SCENE_INPUT_BUSY);
 event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms+300,3,0,30,100};(void)tick(&e);
 /* Generate an actual HOME handoff, observe its fence, reject it using the
  * supported boundary, and prove actions are accepted again. */
 assert(l->configure(NULL,session,0)==0);settle();
 nav_pressed=RISC_NAV_HOME;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
 assert(api->snapshot(NULL,session,&state,&flags)==0&&(flags&RISC_SCENE_INPUT_BUSY));
 assert(l->configure(NULL,session,0)==0);settle();
 nav_pressed=RISC_NAV_DOWN;assert(tick(&e)==RISC_SCENE_IDLE);settle();nav_pressed=RISC_NAV_CONFIRM;
 assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_ACTION_EVENT&&e.action==1);
 finish();printf("Stopwatch real scene %ux%u: native raster, all4actions, held-input safety passed\n",w,h);return 0;
}
