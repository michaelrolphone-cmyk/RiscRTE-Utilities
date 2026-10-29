/* Optional GUI-free owner of alarm reconciliation. Product policy selects this
 * entry explicitly; it never replaces a foreground/default application itself.
 * Commands and persisted schedules remain the alarm.control provider's concern. */
#include "RiscRuntimeV1.h"
#include "AlarmControlV1.h"
#include <stddef.h>

#ifdef ALARM_HEADLESS_TEST
extern void scene_test_headless_tick(const alarm_control_api_v1 *);
#endif

static void retain(const risc_runtime_api_v1 *runtime) {
    if (risc_runtime_get_api(1) == runtime)
        (void)runtime->retain_invocation();
}

__attribute__((visibility("default"))) void app_main(void) {
    const risc_runtime_api_v1 *runtime = risc_runtime_get_api(1);
    if (!runtime || runtime->api_version != 1 ||
        runtime->struct_size < RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE ||
        !runtime->acquire || !runtime->release || !runtime->yield_ms ||
        !runtime->exit_requested || !runtime->retain_invocation) return;
    risc_runtime_capability_v1 grant = {.struct_size = sizeof(grant)};
    if (!runtime->acquire(ALARM_CONTROL_CAPABILITY, 1, 0, &grant)) return;
    if (risc_runtime_get_api(1) != runtime) return;
    const alarm_control_api_v1 *control = grant.api;
    if (!control || control->api_version != 1 ||
        control->struct_size < sizeof(*control) || !control->step) {
        if (!runtime->release(&grant)) retain(runtime);
        return;
    }
    for (;;) {
        if (runtime->exit_requested()) break;
        if (risc_runtime_get_api(1) != runtime) return;
        int32_t result = control->step(control->context);
        if (risc_runtime_get_api(1) != runtime) return;
        if (result == ALARM_CONTROL_RETAINED) { retain(runtime); return; }
        /* Ordinary clock/storage errors are recoverable. Keep the owner alive
         * so the provider can reconcile after clock/storage recovery. */
        #ifdef ALARM_HEADLESS_TEST
        scene_test_headless_tick(control);
#endif
        runtime->yield_ms(20);
        if (risc_runtime_get_api(1) != runtime) return;
    }
    if (risc_runtime_get_api(1) == runtime && !runtime->release(&grant))
        retain(runtime);
}
