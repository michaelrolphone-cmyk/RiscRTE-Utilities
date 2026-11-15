#define t5_driver_get scene_actual_driver_get
#include SCENE_SOURCE
#undef t5_driver_get
extern void scene_test_pointer_register(int(*)(int*,int*));
static int pad_position(int*x,int*y){
 const hit*h=pointer_hit();if(!h||!pointer_visible())return 0;*x=h->x+h->w/2;*y=h->y+h->h/2;return 1;
}
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){scene_test_pointer_register(pad_position);return scene_actual_driver_get(abi);}
__attribute__((destructor)) static void drop_observer(void){scene_test_pointer_register(NULL);}
