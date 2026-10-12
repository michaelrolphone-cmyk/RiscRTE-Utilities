#define ALARM_NATIVE_UTC
#define main original_points_main
#include "points_catalog_service_test.c"
#undef main
int main(void) {
    boot(true);idle();
    printf("Fresh boot phase=%d error=%d generation=%u replacements=%u\n",phase,error,points_occ.generation,replacements);
    boot(false);idle();
    printf("Second boot phase=%d error=%d generation=%u replacements=%u\n",phase,error,points_occ.generation,replacements);
    assert(provider->quiesce());for(unsigned i=0;i<2;i++)free(files[i].data);
}
