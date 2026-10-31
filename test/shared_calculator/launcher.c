#include "RiscRuntimeV1.h"
#include <assert.h>
extern int scene_test_launch(void);
__attribute__((visibility("default"))) void app_main(void){const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);if(scene_test_launch()<2)assert(r->request_launch("calculator.elf"));}
