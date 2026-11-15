/* Host witness only, exact production app. */
#include "../../Apps/stopwatch_scene.c"
#include <assert.h>
extern void scene_test_lifecycle(unsigned);
extern void scene_test_result(unsigned,bool);
static unsigned initialized;
__attribute__((visibility("default"))) int app_module_init(void){assert(initialized++==0);scene_test_lifecycle(1);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){assert(initialized==1);fprintf(stderr,"stopwatch witness ui=%p kv=%p utc=%p terminal=%d loaded=%d notice=%s session=%llu\n",(void*)ui,(void*)kv,(void*)utc,terminal,loaded,notice,(unsigned long long)session);scene_test_result(clock_state.elapsed_ms,clock_state.running);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){if(initialized)scene_test_lifecycle(3);}
