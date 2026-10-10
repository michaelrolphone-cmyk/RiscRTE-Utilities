/* The actual RF retained path holds the invocation on its stack and yields;
 * it must never stop/reopen background capture or release borrowed storage. */
#define PORTABLE_CONTEXTS_CLIENT
#include "../../Apps/waterfall.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf retained_jump;
static unsigned stop_calls,yield_calls;
bool portable_contexts_stop(void){stop_calls++;return false;}
static void yield(uint32_t ms){assert(ms==50);yield_calls++;longjmp(retained_jump,1);}
int main(void){
 risc_runtime_api_v1 selected={.struct_size=sizeof(selected),.yield_ms=yield};runtime=&selected;
 rf_bulk=&rf_resident_bulk;event_initialized=true;event_files.retained=true;
 assert(temporal_retained());if(!setjmp(retained_jump))retain();
 assert(!stop_calls&&yield_calls==1);
 puts("Production RF retained storage path holds stack with no background service calls PASS");
}
