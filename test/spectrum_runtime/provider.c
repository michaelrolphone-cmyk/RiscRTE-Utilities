#include "RiscProviderV2.h"
#include <stddef.h>
extern const void *spectrum_test_provider(const char *);
extern bool spectrum_test_provider_event(const char *,unsigned);
static bool start(const risc_provider_dependency_v1*d,size_t n){(void)d;return !n&&spectrum_test_provider_event(TEST_CAP,1);}
static void stop(void){(void)spectrum_test_provider_event(TEST_CAP,2);}
static bool quiesce(void){return spectrum_test_provider_event(TEST_CAP,3);}
static risc_driver_v2 driver={2,sizeof(driver),TEST_ID,TEST_CAP,TEST_API,NULL,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){if(abi!=2)return NULL;driver.capability=spectrum_test_provider(TEST_CAP);return &driver;}
