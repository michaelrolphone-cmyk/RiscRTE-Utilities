/* Test-only copied observers, never application action injection. */
#include "../../Apps/ble_scanner_scene.c"
#include <assert.h>
extern void scene_test_lifecycle(unsigned);
extern void scene_test_register(unsigned(*)(unsigned));
static unsigned observe(unsigned key){
 if(key==0)return navigation.depth?navigation.focus[navigation.depth-1]:0;
 if(key==1)return scan.status.count;
 if(key==2)return view.detail;
 if(key==3)return scan.token!=0;
 if(key==4)return terminal;
 if(key==5)return text_token!=0;
 if(key==7)return alarm_ui;
 if(key==6){for(unsigned i=0;i<document.node_count;i++)if(strstr(document.nodes[i].text,"23.45"))return 1;return 0;}
 if(key>=100){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==key-100)return document.nodes[i].id;return 0;}
 return 0;
}
__attribute__((visibility("default"))) int app_module_init(void){scene_test_lifecycle(1);scene_test_register(observe);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){fprintf(stderr,"app fini terminal=%d token=%llu text=%llu scan=%llu framesrev=%u\n",terminal,(unsigned long long)ui_token,(unsigned long long)text_token,(unsigned long long)scan.token,revision);scene_test_register(NULL);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){scene_test_lifecycle(3);}
