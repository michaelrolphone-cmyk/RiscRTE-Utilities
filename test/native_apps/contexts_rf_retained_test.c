/* Exercise the production editor's terminal retained branch without loading
 * the display/capture fixture. Link GC retains the actual helper and state. */
#define PORTABLE_CONTEXTS_CLIENT
#include "../../Apps/waterfall.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf retained_jump;
static unsigned stop_calls,fence_calls,yield_calls;
bool portable_contexts_stop(void){stop_calls++;return false;}
static bool fence(void){fence_calls++;return true;}
static void yield(uint32_t ms){assert(ms==50);yield_calls++;longjmp(retained_jump,1);}
int main(void){
 struct {risc_runtime_api_v1 base;bool (*confirm)(void);bool (*fence)(void);} extended={.base={.yield_ms=yield},.fence=fence};
 extended.base.struct_size=sizeof(extended);runtime=&extended.base;
 rf_bulk=&rf_resident_bulk;event_initialized=true;event_files.retained=true;
 assert(temporal_retained());retain_app_data();
 assert(fence_calls==1&&!stop_calls&&!yield_calls);
 extended.base.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
 if(!setjmp(retained_jump))retain_app_data();
 assert(fence_calls==1&&!stop_calls&&yield_calls==1);
 puts("Production RF editor: retained AppData fences/direct return; old Runtime stays on stack with no background service calls PASS");
}
