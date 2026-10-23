/* Host-test-only lifecycle witness. Never linked into target application ELFs. */
#include <assert.h>
extern void scene_test_lifecycle(unsigned event);
static unsigned initialized;
__attribute__((visibility("default"))) int app_module_init(void){assert(initialized++==0);scene_test_lifecycle(1);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){assert(initialized==1);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){if(initialized)scene_test_lifecycle(3);}
