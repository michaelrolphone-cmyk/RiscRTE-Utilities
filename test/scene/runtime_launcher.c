#include "RiscRuntimeV1.h"
#include "AlarmControlV1.h"
#include <assert.h>
extern int scene_test_launch(void);
extern void scene_test_headless(const alarm_control_api_v1 *);
extern void scene_test_after_gui(const alarm_control_api_v1 *);
__attribute__((visibility("default"))) void app_main(void){
    const risc_runtime_api_v1 *r=risc_runtime_get_api(1);assert(r);
    int phase=scene_test_launch();
    if(phase<0){
        risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
        assert(!r->acquire("ui.scene",1,0,&g));
        assert(r->acquire(ALARM_CONTROL_CAPABILITY,1,0,&g));scene_test_headless(g.api);
        assert(r->release(&g));return;
    }
    if(phase<2)assert(r->request_launch("alarms.elf"));
    else {
        risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
        assert(r->acquire(ALARM_CONTROL_CAPABILITY,1,0,&g));
        scene_test_after_gui(g.api);assert(r->release(&g));
    }
}
