/* Actual Nova production controller and adapter. Only hardware is modeled. */
#define grants fixture_grants
#define main capture_fixture_main
#define risc_runtime_get_api fixture_runtime_get_api
#include "calculator_native_peripherals.h"
#undef risc_runtime_get_api
#undef main
#undef grants
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version);
#include NOVA_APP_SOURCE
static void verify_native_labels(void) {
 unsigned count=calculator_menu?3u:16u;
 for(unsigned k=0;k<count+2;k++){
  calculator_native_rect r=k<count?(calculator_menu?calculator_native_geometry.menu[k]:calculator_native_geometry.keys[k]):k==count?calculator_native_geometry.back:calculator_native_geometry.more;
  if(calculator_menu&&k==count+1)continue;
  bool glyph=false;
  for(int y=r.y+1;y<r.y+r.h-1;y++)for(int x=r.x+4;x<r.x+r.w-4;x++)if(pixels[y*(native_width+4)+x]!=0x0082)glyph=true;
  if(!glyph)fprintf(stderr,"blank key=%u rect=%d,%d,%d,%d\n",k,r.x,r.y,r.w,r.h);
  assert(glyph&&"Every painted action must retain a visible label");
 }
}
static unsigned launches;
static bool refuse_first_launch;
static bool observed_launch(const char *name) {
    assert(!strcmp(name,"springboard.elf"));
    launches++;
    if(refuse_first_launch && launches==1)return false;
    return fake_launch(name);
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
    static risc_runtime_api_v1 observed;
    observed=runtime_api;observed.request_launch=observed_launch;
    return version==1?&observed:NULL;
}
static void touch(unsigned at,int x,int y) {
    assert(action_count<256);
    calculator_native_rect r={0};
    if(y==26)r=x<100?calculator_native_geometry.back:calculator_native_geometry.more;
    else if(x==120){int k=(y-100)/52;r=calculator_native_geometry.menu[k];}
    else {int col=(x-40+26)/53,row=(y-74+23)/46;r=calculator_native_geometry.keys[row*4+col];}
    x=r.x+r.w/2;y=r.y+r.h/2;
    actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;
}
int main(int argc,char **argv) {
    assert(argc==6);native_width=(unsigned)atoi(argv[3]);native_height=(unsigned)atoi(argv[4]);native_latency=(unsigned)atoi(argv[5]);assert(calculator_native_open(native_width,native_height));directory=argv[1];unsigned scenario=(unsigned)atoi(argv[2]);
    for(unsigned menu=0;menu<2;menu++){
      calculator_menu=menu!=0;
      for(unsigned k=0;k<(menu?3u:16u);k++){
        calculator_native_rect r=menu?calculator_native_geometry.menu[k]:calculator_native_geometry.keys[k];
        assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=(int)native_width&&r.y+r.h<=(int)native_height);
        assert(calculator_native_target(r.x,r.y)==(int)k);
        assert(calculator_native_target(r.x+r.w-1,r.y+r.h-1)==(int)k);
        assert(calculator_native_target(r.x+r.w,r.y)!=(int)k);
        assert(calculator_native_target(r.x,r.y+r.h)!=(int)k);
      }
    }
    calculator_menu=false;
    memset(pixels,0xa5,sizeof(pixels));stop_poll=400;
    if(scenario<7)touch(20,40,74); /* 7 */
    switch(scenario) {
    case 0: case 1:
        touch(40,199,212);touch(60,92,166);touch(80,145,212); /* +2= */
        if(scenario){touch(100,210,26);touch(120,120,152);} /* Sign */
        break;
    case 2: /* Nested Back must not queue an app exit. Root Back must. */
        touch(40,210,26);touch(60,30,26);touch(80,92,74);touch(100,30,26);
        break;
    case 3: /* Delete, Clear and continued input. */
        touch(40,92,74);touch(60,210,26);touch(80,120,204);
        touch(100,210,26);touch(120,120,100);touch(140,145,74);
        break;
    case 4: case 5: /* Divide-by-zero and a fresh digit after the error. */
        touch(40,199,74);touch(60,40,212);touch(80,145,212);
        if(scenario==5)touch(100,145,166);
        break;
    case 6: /* Rejected root return leaves controls usable; later retry succeeds. */
        refuse_first_launch=true;touch(40,30,26);touch(60,92,74);touch(80,30,26);
        break;
    case 7: /* A contact already held at entry must not become a tap. */
        touch(0,40,74);touch(20,92,74);break;
    case 8: { /* Moving across adjacent painted keys cancels, even near border. */
        calculator_native_rect a=calculator_native_geometry.keys[0],b=calculator_native_geometry.keys[1];
        actions[action_count].at=20;actions[action_count].x=a.x+a.w-1;actions[action_count++].y=a.y+a.h/2;
        actions[action_count].at=21;actions[action_count].x=b.x;actions[action_count++].y=b.y+b.h/2;
        touch(40,40,74);break;}
    case 9: /* Queue gap while a control is held; rearm only on fresh input. */
        gap_at=20;touch(20,40,74);touch(40,92,74);break;
    case 10:keyboard_mode=true;break;
    case 11:touch(20,40,74);break;
    default:assert(!"unknown scenario");
    }
    for(unsigned entry=0;entry<(scenario==11?2u:1u);entry++) {
    ticks=0;stop_poll=400;fake_touch_state=(risc_touch_snapshot_v1){0};fake_event_head=fake_event_count=0;
    assert(app_module_init()==0);app_main();
    assert(!calculator_menu);
    fprintf(stderr,"NATIVE state=%s frames=%u ticks=%u active=%u menu=%u\n",calculator_state.text,presents,ticks,calculator_native_active,calculator_menu);
    switch(scenario) {
    case 0:assert(!strcmp(calculator_state.text,"9"));break;
    case 1:assert(!strcmp(calculator_state.text,"-9"));break;
    case 2:assert(!strcmp(calculator_state.text,"78") && launches==1);break;
    case 3:assert(!strcmp(calculator_state.text,"9"));break;
    case 4:assert(calculator_state.error==CALC_DIV_ZERO);break;
    case 5:assert(!calculator_state.error && !strcmp(calculator_state.text,"3"));break;
    case 6:assert(!strcmp(calculator_state.text,"78") && launches==2);break;
    case 7:case 9:assert(!strcmp(calculator_state.text,"8"));break;
    case 8:case 11:assert(!strcmp(calculator_state.text,"7"));break;
    case 10:assert(!strcmp(calculator_state.text,"8"));break;
    }
    app_module_fini();assert(!fixture_grants && !frames && !subs && !native_pending);
    }
    printf("Calculator Nova scenario %u: real touches, navigation, recovery and cleanup passed\n",scenario);
    return 0;
}
