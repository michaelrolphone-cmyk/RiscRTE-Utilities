#include "RiscProviderV2.h"
#include <stddef.h>
extern const void *text_runtime_provider(const char *);
extern bool text_runtime_provider_event(const char *,unsigned);
static bool start(const risc_provider_dependency_v1*d,size_t n){(void)d;return !n&&text_runtime_provider_event(TEST_CAP,1);}
static void stop(void){(void)text_runtime_provider_event(TEST_CAP,2);}
static bool quiesce(void){return text_runtime_provider_event(TEST_CAP,3);}
static risc_driver_v2 driver={2,sizeof(driver),TEST_ID,TEST_CAP,1,NULL,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){if(abi!=2)return NULL;driver.capability=text_runtime_provider(TEST_CAP);return &driver;}
