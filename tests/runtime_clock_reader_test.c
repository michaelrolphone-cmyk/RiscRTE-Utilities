#include "RiscRuntimeClockV1.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned cheap_calls,health_calls;
static uint32_t value;
static bool ok=true;
static bool clock_read(uint32_t*out){cheap_calls++;*out=value;return ok;}
static bool health_read(risc_runtime_health_v1*out){health_calls++;assert(out->struct_size==sizeof(*out));out->uptime_ms=value;return ok;}
static void check(const risc_runtime_api_v1*api,bool success,unsigned cheap,unsigned health){
 uint32_t out=12345;unsigned c=cheap_calls,h=health_calls;
 assert(risc_runtime_monotonic_ms_v1(api,&out)==success);
 assert(out==(success?value:12345));assert(cheap_calls==c+cheap&&health_calls==h+health);
}
int main(void){
 risc_runtime_api_v1 a;memset(&a,0,sizeof(a));a.api_version=1;a.struct_size=sizeof(a);a.health=health_read;
 check(NULL,false,0,0);assert(!risc_runtime_monotonic_ms_v1(&a,NULL));
 a.api_version=2;check(&a,false,0,0);a.api_version=1;
 a.struct_size=offsetof(risc_runtime_api_v1,health);check(&a,false,0,0);
 size_t prefix=offsetof(risc_runtime_api_v1,health)+sizeof(a.health);a.struct_size=(uint32_t)prefix;
 void *old=malloc(prefix);assert(old);memcpy(old,&a,prefix);value=0;check((const risc_runtime_api_v1*)old,true,0,1);free(old);
 a.struct_size=sizeof(a);value=UINT32_MAX;check(&a,true,0,1);value=0;check(&a,true,0,1);
 ok=false;check(&a,false,0,1);ok=true;a.health=NULL;check(&a,false,0,0);a.health=health_read;
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
 a.monotonic_ms=clock_read;value=0;check(&a,true,1,0);value=UINT32_MAX;check(&a,true,1,0);value=0;check(&a,true,1,0);
 ok=false;check(&a,false,1,0);ok=true;a.health=NULL;check(&a,true,1,0);a.health=health_read;
 a.struct_size=RISC_RUNTIME_MONOTONIC_V1_SIZE-1;check(&a,true,0,1);
#else
 (void)clock_read;
#endif
 puts("Canonical helper: null/version, exact prefix allocation, zero/wrap, authoritative failure, fallback/output PASS");
}
