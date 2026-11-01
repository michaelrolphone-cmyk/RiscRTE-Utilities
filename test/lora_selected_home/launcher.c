#include "RiscRuntimeV1.h"
#include <assert.h>
__attribute__((visibility("default"))) void app_main(void){const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r&&r->request_launch("default.elf"));}
