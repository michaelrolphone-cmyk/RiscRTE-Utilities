#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRealtimeV1.h"
#include "PortableNativeCustody.h"
#include "PortableStageLog.h"
#include "PortablePerformance.h"
#include "PortableContextsClient.h"
static const risc_runtime_api_v1 *rt,*native_custody_runtime;
static bool native_custody_retained,failed,native_sleep_retained;
static void *gauge;
static risc_display_surface_v1 surface,native_custody_surface;
static bool portable_paper_frame_drain(void){return true;}
static bool broadcast_before_storage(void){return true;}
static bool broadcast_before_storage_read(void){return true;}
#include "contexts_adapter.inc"
#include "native_custody_adapter.inc"
void fixture_adapter_init(const risc_runtime_api_v1 *raw){
 native_custody_runtime=raw;settings_runtime=*raw;
 settings_runtime.acquire=custody_acquire;settings_runtime.release=custody_release;
 settings_runtime.health=custody_health;settings_runtime.yield_ms=custody_yield;
 settings_runtime.diagnostic=custody_diagnostic;settings_runtime.request_launch=custody_launch;
 rt=&settings_runtime;
}
bool portable_broadcast_stop(void){return true;}
