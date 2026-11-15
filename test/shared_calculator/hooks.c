/* Host witness only; not linked into target ELF. Exact production app included. */
#include "../../Apps/calculator_scene.c"
#include <assert.h>
extern void scene_test_lifecycle(unsigned);
extern void scene_test_result(const char*);
static unsigned initialized;
__attribute__((visibility("default"))) int app_module_init(void){assert(initialized++==0);scene_test_lifecycle(1);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){assert(initialized==1);fprintf(stderr,"Calculator witness acquired=%d terminal=%d session=%llu scene=%p components=%p lifecycle=%p text=%s\n",acquired,terminal,(unsigned long long)session,(void*)scene,(void*)components,(void*)lifecycle,state.text);scene_test_result(state.text);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){if(initialized)scene_test_lifecycle(3);}
