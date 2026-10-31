/* Test-only observer; canonical target builds compile the application directly. */
#include "../../Apps/battery_scene.c"
#include <assert.h>
extern void scene_test_lifecycle(unsigned);
extern void scene_test_register(unsigned(*)(unsigned));
static unsigned observe(unsigned key){
 if(key==0)return navigation.depth?navigation.routes[navigation.depth-1]:0;
 if(key==1)return broadcast_status.enabled;
 if(key==2)return detail_page;
 if(key==3)return broadcast_result;
 if(key==4)return navigation.depth?navigation.focus[navigation.depth-1]:0;
 return document.revision;
}
__attribute__((visibility("default"))) int app_module_init(void){scene_test_lifecycle(1);scene_test_register(observe);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){scene_test_register(NULL);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){scene_test_lifecycle(3);}
