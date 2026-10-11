#define main canvas_entry_test_main
#include "app_test.c"
#undef main
static size_t allocated,peak,calls,fail_at;
static void *tracked_allocate(size_t n){
 if(++calls==fail_at)return NULL;
 size_t *p=malloc(n+sizeof(size_t));assert(p);*p=n;allocated+=n;if(allocated>peak)peak=allocated;return p+1;
}
static void tracked_release(void *v){size_t *p=(size_t*)v-1;assert(allocated>=*p);allocated-=*p;free(p);}
static void setup(ht_startup_memory *m){
 assert(ht_startup_memory_open(m,tracked_allocate,tracked_release,NULL));
 ht_bind(m->data[HT_MEM_SCENERY]);ht_native_a=m->data[HT_MEM_NATIVE_A];
 ht_native_b=ht_cached_near[2][0];ht_reader_bitmap=m->data[HT_MEM_READER];
 ht_service=NULL;ht_render_clock=NULL;ht_abort=false;
}
static void startup_cases(void){
 for(unsigned attempt=0;attempt<3;++attempt){
  ht_startup_memory m={0};calls=fail_at=0;setup(&m);
  assert(calls==4 && allocated==HT_MEMORY+HT_NATIVE_PIXELS+2*HT_PACKED_BYTES+60u);
  for(unsigned i=0;i<HT_MEM_BLOCKS;++i)assert(!((uintptr_t)m.data[i]&15u));
  assert(ht_native_b>=m.data[HT_MEM_SCENERY] && ht_native_b+HT_NATIVE_PIXELS<=m.data[HT_MEM_SCENERY]+HT_MEMORY);
  ht_startup_memory_close(&m,tracked_release);assert(!allocated);
  ht_startup_memory_close(&m,tracked_release);assert(!allocated);
 }
 for(unsigned failure=1;failure<=4;++failure){
  ht_startup_memory m={0};calls=0;fail_at=failure;
  bool ok=ht_startup_memory_open(&m,tracked_allocate,tracked_release,NULL);
  assert(ok==(failure==4));
  if(ok)assert(!m.data[HT_MEM_STAGING]);
  else assert(!allocated);
  ht_startup_memory_close(&m,tracked_release);assert(!allocated);
 }
 fail_at=0;printf("startup alignment, repeated launch, every allocation failure PASS; peak=%zu\n",peak);
}
static void parity_cases(void){
 ht_startup_memory m={0};setup(&m);
 uint8_t *alias=ht_native_b,*reference=malloc(HT_NATIVE_PIXELS),*expected=malloc(HT_NATIVE_PIXELS),*low=malloc(HT_PIXELS);assert(reference&&expected&&low);
 unsigned cases=0;
 for(unsigned level=0;level<HT_LEVELS;++level)for(unsigned position=0;position<3;++position){
  memset(&ht,0,sizeof(ht));ht.level=level;ht_spawn(true);
  ht.camera=(position==0?-60:position==1?950:2400)*256;ht.sway_phase=97;ht.rotation_phase=41;ht.ticks=731;
  ht_select_level(level);ht_camera_mode=HT_CAMERA_BASELINE;ht_forest_frame_valid=false;
  ht_render_scene();memcpy(low,ht_scene,HT_PIXELS);
  ht_camera_mode=HT_CAMERA_NATIVE;ht_native_b=reference;ht_render_scene();memcpy(expected,ht_scene,HT_NATIVE_PIXELS);
  ht_native_b=alias;ht_render_scene();assert(!memcmp(expected,ht_scene,HT_NATIVE_PIXELS));
  ht_camera_mode=HT_CAMERA_BASELINE;ht_render_scene();assert(!memcmp(low,ht_scene,HT_PIXELS));++cases;
 }
 for(unsigned id=1;id<=HT_CUTSCENE_WATCH;++id){
  memset(&ht,0,sizeof(ht));ht_spawn(true);ht_cutscene_begin(id);
  for(unsigned sample=0;sample<3;++sample){
   ht_cutscene.tick=(uint16_t)((ht_cutscene_tracks[id].duration-1)*sample/2);
   ht_camera_mode=HT_CAMERA_NATIVE;ht_native_b=reference;ht_cutscene_render(&ht_cutscene);memcpy(expected,ht_scene,HT_NATIVE_PIXELS);
   ht_native_b=alias;ht_cutscene_render(&ht_cutscene);assert(!memcmp(expected,ht_scene,HT_NATIVE_PIXELS));++cases;
  }
 }
 /* Both native cutscene dissolves overwrite B; cover their blending intervals. */
 const unsigned ids[]={HT_CUTSCENE_INTRO,HT_CUTSCENE_WEST,HT_CUTSCENE_WEST};
 const unsigned ticks[]={2110,48,592};
 for(unsigned i=0;i<3;++i){
  memset(&ht,0,sizeof(ht));ht_spawn(true);ht_cutscene_begin(ids[i]);ht_cutscene.tick=ticks[i];
  ht_camera_mode=HT_CAMERA_NATIVE;ht_native_b=reference;ht_cutscene_render(&ht_cutscene);memcpy(expected,ht_scene,HT_NATIVE_PIXELS);
  ht_native_b=alias;ht_cutscene_render(&ht_cutscene);assert(!memcmp(expected,ht_scene,HT_NATIVE_PIXELS));assert(!ht_forest_frame_valid);++cases;
 }
 ht_native_b=alias;free(reference);free(expected);free(low);ht_startup_memory_close(&m,tracked_release);assert(!allocated);
 printf("all chapters/camera round trips and cutscene scratch parity: %u cases PASS\n",cases);
}
int main(void){startup_cases();parity_cases();return 0;}
