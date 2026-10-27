#include "native_geometry_raster.h"
#include <stdio.h>
#if APP_KIND==1
#include "../../Apps/calculator.c"
#elif APP_KIND==2
#include "../../Apps/battery.c"
#elif APP_KIND==3
#define PORTABLE_STOPWATCH_NATIVE_UTC
#define PORTABLE_STOPWATCH_NATIVE_STANDALONE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_APP_LAUNCH_GUARD
#include "../../Apps/stopwatch.c"
#else
#error app kind
#endif
static unsigned presents,step,shape_frame;
static void snapshot(const char *state){
 const char*directory=getenv("NATIVE_GEOMETRY_RASTERS");if(!directory)return;
 char path[1024];snprintf(path,sizeof(path),"%s/app-%d-%ux%u-%s.ppm",directory,APP_KIND,surface.width,surface.height,state);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",surface.width,surface.height);
 for(unsigned y=0;y<surface.height;y++)for(unsigned x=0;x<surface.width;x++){
  uint16_t p=((uint16_t*)surface.pixels)[y*(surface.width+3)+x];
  unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
static t5_app_api_v1 api;
static int32_t swidth(void){return ng_width();}
static int32_t sheight(void){return ng_height();}
static void clear(void){assert(0);}
static void fill(int32_t x,int32_t y,int32_t w,int32_t h,bool b){(void)x;(void)y;(void)w;(void)h;(void)b;assert(0);}
static void present(bool full){assert(!full&&surface_acquired);surface_acquired=false;ng_check();presents++;if(!shape_frame++)snapshot("initial");}
static void legacy_text(int32_t x,int32_t y,const char*s){(void)x;(void)y;(void)s;assert(0);}
static void legacy_label(int32_t x,int32_t y,int32_t w,const char*s){(void)x;(void)y;(void)w;(void)s;assert(0);}
static uint32_t ticks(void){return 1000;}
static bool poll_input(t5_app_input_t*in,uint32_t ms){(void)ms;*in=(t5_app_input_t){0};
#if APP_KIND==1
 const char*sequence="2+3=";
 if(step<4){const char*k=memchr(calculator_keys,sequence[step],CALCULATOR_KEYS);assert(k);int index=(int)(k-calculator_keys);
  /* Independent cell centers from native third-height/five-row contract. */
  int col=index%4,row=index/4,top=ng_height()/3;
  in->tapped=true;in->touch_x=(int16_t)(4+(2*col+1)*(ng_width()-8)/8);
  in->touch_y=(int16_t)(top+(2*row+1)*(ng_height()-top-4)/10);
 }else{assert(!strcmp(calculator_state.text,"5"));in->buttons=T5_APP_BUTTON_BACK;}
#elif APP_KIND==2
 if(step<row_count+3){in->tapped=true;in->touch_x=(int16_t)(8+5*(ng_width()-16)/6);in->touch_y=(int16_t)(ng_height()-32);}
 else in->buttons=T5_APP_BUTTON_BACK;
#else
 in->buttons=T5_APP_BUTTON_BACK;
#endif
 step++;return true;}
const t5_app_api_v1*t5_app_get_api(uint32_t v){return v==1?&api:NULL;}
#if APP_KIND==2
static bool read_battery(t5_battery_state_t*s){memset(s,0,sizeof(*s));s->available=1;s->soc_percent=UINT16_MAX;strcpy(s->board_name,"Native geometry test");return true;}
static const t5_battery_api_v1 battery_api={.api_version=T5_BATTERY_API_VERSION,.struct_size=sizeof(battery_api),.read=read_battery};
const t5_battery_api_v1*t5_battery_get_api(uint32_t v){return v==T5_BATTERY_API_VERSION?&battery_api:NULL;}
#elif APP_KIND==3
bool portable_adapter_retained(void){return failed;}
void portable_adapter_retain(void){failed=true;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
#endif
static void check_rect(utility_native_rect r){assert(r.x>=0&&r.y>=0&&r.width>0&&r.height>0&&r.x+r.width<=ng_width()&&r.y+r.height<=ng_height());
 assert(un_hit(r,r.x,r.y)&&un_hit(r,r.x+r.width-1,r.y+r.height-1));assert(!un_hit(r,r.x-1,r.y)&&!un_hit(r,r.x+r.width,r.y));}
int main(void){
 /* Samples validate the range-based contract, not a product whitelist. */
 const unsigned shapes[][6]={{240,240,0,0,0,0},{466,466,69,69,69,69},{410,502,17,29,23,11},{640,320,21,7,33,13},{503,601,31,47,18,20},{1024,1024,0,0,0,0}};
 for(unsigned k=0;k<sizeof(shapes)/sizeof(shapes[0]);k++){
  const unsigned*g=shapes[k];ng_open(g[0],g[1],g[2],g[3],g[4],g[5]);
  api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(api),.screen_width=swidth,.screen_height=sheight,.clear=clear,.fill_rect=fill,.present=present,.poll=poll_input,.millis=ticks,.draw_text=legacy_text,.draw_label=legacy_label};
  assert(un_dimensions(&api));assert(!surface_acquired);portable_nova_native_fill(0,0,16,16,NOVA_CYAN);assert(((uint16_t*)surface.pixels)[content_view.y*(surface.width+3)+content_view.x]==0xa55a);step=0;shape_frame=0;check_rect(un_back());for(int i=0;i<3;i++)check_rect(un_footer(ng_width(),ng_height(),i,3));
#if APP_KIND==1
  app_main();assert(step==5);for(int i=0;i<CALCULATOR_KEYS;i++){calculator_rect r=calculator_key_rect(i);check_rect((utility_native_rect){r.x,r.y,r.width,r.height});assert(calculator_hit(r.x,r.y)==i);assert(calculator_hit(r.x+r.width-1,r.y+r.height-1)==i);}
  calc_key(&calculator_state,'C');for(const char*s="1/0=";*s;s++)calc_key(&calculator_state,*s);
  rendered_count=0;calculator_render();assert(saw_text("C OR DIGIT TO RESET")&&saw_text("DIV BY ZERO"));
  unsigned error_ink=0;for(int y=38;y<CALCULATOR_GRID_TOP-24;y++)for(int x=8;x<ng_width()-8;x++)if(((uint16_t*)surface.pixels)[(content_view.y+y)*(surface.width+3)+content_view.x+x])error_ink++;
  assert(error_ink>20);snapshot("divide-zero");
  for(const char*s="C999999999*2=";*s;s++)calc_key(&calculator_state,*s);
  calculator_render();
  for(const char*s="C2+";*s;s++)calc_key(&calculator_state,*s);
  rendered_count=0;calculator_render();assert(saw_text("6 DP")&&saw_text("+"));snapshot("pending");
#elif APP_KIND==2
  app_main();assert(step>row_count);for(int i=0;i<battery_visible();i++)check_rect(battery_row_rect(i));
  t5_battery_state_t detailed={0};detailed.available=detailed.detailed_telemetry=1;detailed.soc_percent=UINT16_MAX;selected_index=0;render_state(&detailed);assert(!strcmp(values[1],"Unknown"));
  for(unsigned i=0;i<row_count;i++){selected_index=(int)i;render_state(&detailed);}
#else
  app=&api;loaded=true;reset_armed=pending_pause=false;clock_state=(sw_clock){0};status="UTC OR STORAGE UNAVAILABLE";utility_frame_reset();draw();
  status="RUNNING";assert(!strcmp(stopwatch_native_status("RUNNING"),""));
  status="PAUSED";assert(!strcmp(stopwatch_native_status("PAUSED"),""));
  const char*important[]={"RUNNING - APPROXIMATE","SAVE UNCONFIRMED - RETRY","RTC UNAVAILABLE - RETRY"};
  for(unsigned i=0;i<3;i++){status=important[i];assert(stopwatch_native_status("RUNNING")==status);rendered_count=0;draw();}
  clock_state.elapsed_ms=SW_MAX_MS;clock_state.running=true;draw();reset_armed=true;pending_pause=true;status="SAVE UNCONFIRMED - RETRY";draw();
#endif
  assert(surface_acquisitions==shape_frame);ng_check();snapshot("last");
  /* Same original Back glyph/primitive footprint at every native size. */
  static uint16_t back_pixels[52*32];
  for(unsigned by=0;by<32;by++)for(unsigned bx=0;bx<52;bx++){
   uint16_t pixel=((uint16_t*)surface.pixels)[(content_view.y+4+by)*(surface.width+3)+content_view.x+8+bx];
   if(!k)back_pixels[by*52+bx]=pixel;else assert(pixel==back_pixels[by*52+bx]);
  }
  /* Real native renderer must not write anything after retained custody. */
  size_t bytes=(size_t)surface.stride_bytes*surface.height;void*copy=malloc(bytes);assert(copy);memcpy(copy,surface.pixels,bytes);failed=true;
  un_header(&api,"Retained");assert(!memcmp(copy,surface.pixels,bytes));failed=false;free(copy);
  /* No second transform: adapter's exact inverse maps physical origin once. */
  uint16_t x=(uint16_t)(content_view.x+8),y=(uint16_t)(content_view.y+4);assert(portable_safe_viewport_touch(&content_view,&x,&y)&&x==8&&y==4);
  if(k==1){/* Every content corner is inside this circular provider aperture. */
   for(int a=0;a<2;a++)for(int b=0;b<2;b++){int px=(int)content_view.x+(a?(int)content_view.width-1:0),py=(int)content_view.y+(b?(int)content_view.height-1:0);int dx=2*px+1-466,dy=2*py+1-466;assert(dx*dx+dy*dy<=466*466);}
  }
  free(surface.pixels);
 }
 /* Generalized layout validation over hundreds of unlisted dimensions. */
 for(unsigned n=0;n<257;n++){
  content_view=(portable_safe_viewport){0,0,240+(n*97)%785,240+(n*193)%785};
  for(int i=0;i<3;i++)check_rect(un_footer(ng_width(),ng_height(),i,3));
#if APP_KIND==1
  calculator_width=ng_width();calculator_height=ng_height();
  for(int i=0;i<CALCULATOR_KEYS;i++){
   calculator_rect r=calculator_key_rect(i);check_rect((utility_native_rect){r.x,r.y,r.width,r.height});
   assert(calculator_hit(r.x+r.width/2,r.y+r.height/2)==i);
  }
#elif APP_KIND==2
  for(int i=0;i<battery_visible();i++)check_rect(battery_row_rect(i));
#endif
 }
 ng_open(260,260,11,11,11,11);assert(!un_dimensions(&api));free(surface.pixels);
 printf("PASS native Utilities app %d: capability geometry, production glyphs, stride/custody, controller rendering and inverse input (%u frames)\n",APP_KIND,presents);
}
