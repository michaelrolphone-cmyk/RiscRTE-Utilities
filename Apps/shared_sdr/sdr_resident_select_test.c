#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
static bool native_custody_retained,sdr_resident_active,resident_ready;
static risc_resident_client_v1 resident;
static risc_runtime_api_v1 runtime;
static const risc_runtime_api_v1 *rt=&runtime,*native_custody_runtime=&runtime;
static bool identity_lost;
static unsigned calls,retains,mode;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!identity_lost?&runtime:NULL;}
static void portable_adapter_retain_silent(void){native_custody_retained=true;retains++;}
static int32_t checkpoint(uint64_t i,const risc_resident_request_v1 *r,risc_resident_reply_v1 *p){
 (void)i;(void)r;(void)p;assert(!"selection must not checkpoint");return -1;
}
static bool getter(risc_resident_client_v1 *out){
 calls++;
 if(mode==0)return false;
 if(mode==1){out->role=99;return false;}
 if(mode==2){identity_lost=true;return false;}
 *out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=7,
 .role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint};
 if(mode==3)out->role=RISC_RESIDENT_ROLE_HOST;
 if(mode==4)out->checkpoint=NULL;
 if(mode==5)out->struct_size--;
 if(mode==6)out->reserved=1;
 if(mode==7)identity_lost=true;
 if(mode==8)native_custody_retained=true;
 return true;
}
#include "sdr_resident_select.inc"
static void reset(void){
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.resident_shell=getter};
 native_custody_retained=sdr_resident_active=resident_ready=identity_lost=false;
 resident=(risc_resident_client_v1){0};calls=retains=0;
}
int main(void){
 reset();runtime.struct_size=RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE-1;
 assert(sdr_resident_select()&&!sdr_resident_active&&!calls);
 reset();runtime.resident_shell=NULL;assert(sdr_resident_select()&&!sdr_resident_active&&!calls);
 reset();mode=0;assert(sdr_resident_select()&&!sdr_resident_active&&calls==1&&!retains);
 for(mode=1;mode<=8;mode++){
  reset();assert(!sdr_resident_select()&&!sdr_resident_active);
  assert(native_custody_retained&&retains==1);
  assert(!sdr_resident_select()&&calls==1&&retains==1);
 }
 reset();mode=9;assert(sdr_resident_select()&&sdr_resident_active&&resident_ready);
 assert(resident.invocation==7&&resident.checkpoint==checkpoint&&!retains);
 reset();native_custody_retained=true;assert(!sdr_resident_select()&&!calls);
 reset();identity_lost=true;assert(!sdr_resident_select()&&!calls);
 puts("SDR resident selection: legacy, admission, malformed and retained cases PASS");
 return 0;
}
