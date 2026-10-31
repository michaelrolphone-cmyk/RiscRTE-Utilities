#include "RiscRuntimeV1.h"
#include "CountdownControlV1.h"
#include <assert.h>
extern int scene_test_launch(void);

extern void scene_test_after_gui(const countdown_control_api_v1 *);
__attribute__((visibility("default"))) void app_main(void){
    const risc_runtime_api_v1 *r=risc_runtime_get_api(1);assert(r);
    int phase=scene_test_launch();
    if(phase<0){
        if(phase==-1)assert(r->request_launch("headless.elf"));
        return;
    }
    if(phase<2)assert(r->request_launch("countdown.elf"));
    else {
        risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
        assert(r->acquire(COUNTDOWN_CONTROL_CAPABILITY,1,0,&g));
        scene_test_after_gui(g.api);assert(r->release(&g));
    }
}
