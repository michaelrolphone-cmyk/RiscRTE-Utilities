#include "RiscProviderV2.h"
extern const void*canvas_runtime_display(void);
extern bool canvas_runtime_event(unsigned);
static bool start(const risc_provider_dependency_v1*d,size_t n){(void)d;return !n&&canvas_runtime_event(1);}
static void stop(void){(void)canvas_runtime_event(2);}
static bool quiesce(void){return canvas_runtime_event(3);}
static risc_driver_v2 driver={2,sizeof(driver),"display","display.output",1,0,start,stop,quiesce};
__attribute__((visibility("default")))const risc_driver_v2*t5_driver_get(uint32_t abi){if(abi!=2)return 0;driver.capability=canvas_runtime_display();return &driver;}
