#include "../Apps/utility_frame.h"
#include "WatchLayout.h"
#include <assert.h>
#include <stdio.h>
static int32_t w,h;
static int32_t width(void){return w;}
static int32_t height(void){return h;}
int main(void){
 const t5_app_api_v1 app={.screen_width=width,.screen_height=height};
 const int sizes[][2]={{240,240},{410,502},{320,480}};
 for(unsigned i=0;i<3;i++){
  w=sizes[i][0];h=sizes[i][1];assert(utility_color_dimensions(&app));
  watch_layout layout;assert(watch_layout_open(&layout,w,h,NULL));
  const watch_layout_rect button={16,180,64,48};watch_layout_rect physical;
  assert(watch_layout_native_rect(&layout,&button,&physical));
  for(int y=physical.y;y<physical.y+(int)physical.height;y++)for(int x=physical.x;x<physical.x+(int)physical.width;x++){
   t5_app_input_t in={.tapped=true,.touch_x=x,.touch_y=y};utility_color_input(&app,&in);
   assert(in.tapped&&watch_layout_contains(&button,in.touch_x,in.touch_y));
  }
  t5_app_input_t outside={.tapped=true,.touch_x=w,.touch_y=h};utility_color_input(&app,&outside);assert(!outside.tapped);
  assert(utility_color_y(&app,h-1)==(h-1)*240/w);
 }
 w=0;assert(!utility_color_dimensions(&app));puts("capability dimensions and rendered button/input inverse: 240x240, 410x502, 320x480 PASS");
}
