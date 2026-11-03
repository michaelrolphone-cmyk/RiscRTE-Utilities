/* Copied observer only. Native navigation is the sole UI action source. */
#include "../../Apps/ble_touchpad_scene.c"
extern void scene_test_lifecycle(unsigned);
extern void scene_test_register(unsigned(*)(unsigned));
static unsigned observe(unsigned key){
 if(key==0)return navigation.depth?navigation.focus[navigation.depth-1]:0;
 if(key==1)return view.page;
 if(key==2)return hid.token!=0;
 if(key==3)return hid.status.state;
 if(key==4)return owner.terminal;
 if(key==5)return reset_view;
 if(key==6)return dirty;
 if(key==7)return view.page==BB_ALERT;
 if(key>=100){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==key-100)return document.nodes[i].id;}
 return 0;
}
__attribute__((visibility("default"))) int app_module_init(void){scene_test_lifecycle(1);scene_test_register(observe);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){scene_test_register(NULL);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){scene_test_lifecycle(3);}
