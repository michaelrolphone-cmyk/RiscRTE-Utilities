#include "RiscResidentShellV1.h"
extern void spectrum_test_host(void);
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_HOST,0};
__attribute__((visibility("default"))) int app_main(void){spectrum_test_host();return 0;}
