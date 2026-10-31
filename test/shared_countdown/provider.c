/* Only physical APIs are doubled; Runtime, scene, app and scheduler are real. */
#include "RiscProviderV2.h"
#ifndef PROVIDER_INDEX
#error PROVIDER_INDEX required
#endif
extern const void *scene_test_hardware(unsigned index);
static bool start(const risc_provider_dependency_v1 *d,size_t n){(void)d;return n==0;}
static bool quiesce(void){return true;}
static void stop(void){}
static risc_driver_v2 d={2,sizeof(d),PROVIDER_ID,PROVIDER_CAP,PROVIDER_API,0,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){if(abi!=2)return 0;d.capability=scene_test_hardware(PROVIDER_INDEX);return &d;}
