#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "RiscDisplayOutputV1.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "../image_observer.h"
static uint64_t milliseconds,present_at;
static unsigned width=240,height=240,format=5,frames,starts,stops;
static bool held,pending,complete,failed,cancel_requested;
static unsigned scenario,io_calls;
static void io(){assert(!failed&&"physical callback after failed custody");++io_calls;}
static unsigned char pixels[466*466*2];
static bool info(void*,risc_display_info_v1*out){io();*out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=width;out->height=height;out->supported_formats=RISC_DISPLAY_FORMAT_BIT(format);out->preferred_format=format;return true;}
static bool acquire(void*,uint32_t f,risc_display_surface_v1*out){io();assert(!held&&!pending&&f==format);held=true;unsigned stride=format==5?width*2:(width+7)/8;*out={frames+1,pixels,width,height,stride,stride*height,format};return true;}
static void release(void*,uint64_t f){io();assert(held&&f);held=false;}
static bool submit(void*,uint64_t f,const risc_display_rect_v1*,size_t count,const risc_display_present_options_v1*,uint64_t*out){io();assert(held&&!pending&&f&&!count);held=false;pending=true;present_at=milliseconds;*out=++frames;if(scenario==2)cancel_requested=true;return true;}
static bool status(void*,uint64_t token,risc_display_present_status_v1*out){io();assert(token==frames&&!held);if(scenario==4){failed=true;out->state=RISC_DISPLAY_PRESENT_FAILED;return true;}if(milliseconds-present_at>=5)pending=false;out->state=pending?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),nullptr,info,acquire,release,submit,status,nullptr,nullptr};
extern "C" const void*canvas_runtime_display(){return &display;}
extern "C" bool canvas_runtime_event(unsigned event){if(event==1)starts++;else if(event==2)stops++;else if(event==3)return !held&&!pending;return true;}
extern "C" void canvas_runtime_complete(){complete=true;}
extern "C" bool canvas_runtime_done(){return complete;}
extern "C" unsigned canvas_runtime_case(){return scenario;}
int main(int argc,char**argv){assert(argc==3);scenario=static_cast<unsigned>(strtoul(argv[2],nullptr,10));assert(scenario<=4);
 ImageObserver::set_root(argv[1]);for(const char* name:{"canvas.elf","profile.elf","display.elf"})ImageObserver::declare_image(name,"t5_driver_get");ImageObserver::declare_image("default.elf","app_main");
 RiscBoot::Port port{[](){return true;},[](risc_runtime_health_v1*h){h->uptime_ms=(uint32_t)milliseconds;return true;},[](uint32_t n){milliseconds+=n;},[](const char*s){fprintf(stderr,"diagnostic: %s\n",s);return true;}};
 port.controlExitRequested=[](){return cancel_requested;};port.controlEnd=[](){bool was=cancel_requested;cancel_requested=false;return was;};
 auto* sources=new RiscTest::FileSources({argv[1]});auto* runtime=new RiscBoot::Runtime(port);assert(sources->bind(*runtime));bool prepared=runtime->prepare(argv[1]);if(!prepared)fprintf(stderr,"prepare: %s\n",runtime->error());assert(prepared);
 bool ok=runtime->run();
 if(scenario>=3){
  assert(!ok&&runtime->retained()&&!risc_runtime_get_api(1)&&!complete&&frames==1&&pending&&!stops);
  for(const char* name:{"default.elf","canvas.elf","display.elf","profile.elf"})assert(ImageObserver::retained_image(name));
  const unsigned calls=io_calls;assert(!runtime->run()&&io_calls==calls&&runtime->retained());
  printf("Actual canvas case %u PASS: app/provider/dependencies retained, no successful teardown or post-fence physical calls\n",scenario);return 0;
 }
 assert(ok&&complete&&frames==(scenario==1?0u:scenario==2?1u:3u)&&!held&&!pending);
 delete runtime;delete sources;assert(starts==stops);ImageObserver::all_images_closed();
 printf("Actual canvas case %u PASS: cancellation/settlement, quiesce/resume, exact image teardown\n",scenario);
}
