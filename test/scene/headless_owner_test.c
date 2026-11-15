#include "RiscRuntimeV1.h"
#include "AlarmControlV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void app_main(void);
static risc_runtime_api_v1 runtime;
static alarm_control_api_v1 control;
static unsigned scenario,steps,yields,releases,retains,acquires;
static bool alive=true;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return alive?&runtime:NULL;}
static bool acquire(const char* cap,uint32_t api,uint64_t id,risc_runtime_capability_v1* out){
 assert(alive&&!retains);assert(!strcmp(cap,"alarm.control")&&api==1&&!id);++acquires;
 if(scenario==1)return false;
 out->api=&control;if(scenario==2)alive=false;return true;
}
static bool release(risc_runtime_capability_v1* grant){assert(alive&&!retains&&grant->api==&control);++releases;return scenario!=7;}
static bool retain(void){assert(alive&&!retains);++retains;return true;}
static int32_t step(void* c){(void)c;assert(alive&&!retains);++steps;
 if(scenario==3)return ALARM_CONTROL_RETAINED;
 if(scenario==4)alive=false;
 return scenario==8&&steps<3?ALARM_CONTROL_CLOCK:ALARM_CONTROL_OK;
}
static void yield(uint32_t ms){assert(alive&&!retains&&ms==20);++yields;if(scenario==5)alive=false;}
static bool exit_requested(void){assert(alive&&!retains);if(scenario==6)alive=false;return steps==3;}
int main(void){
 for(scenario=0;scenario<11;scenario++){
  steps=yields=releases=retains=acquires=0;alive=true;
  control=(alarm_control_api_v1){.api_version=1,.struct_size=sizeof(control),.step=step};
  runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.yield_ms=yield,.acquire=acquire,.release=release,.retain_invocation=retain,.exit_requested=exit_requested};
  if(scenario==9)control.struct_size=8;
  if(scenario==10)runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
  app_main();
  if(scenario==0||scenario==8)assert(steps==3&&yields==3&&releases==1&&!retains);
  if(scenario==1)assert(acquires==1&&!steps&&!releases&&!retains);
  if(scenario==2)assert(acquires==1&&!steps&&!releases&&!retains);
  if(scenario==3)assert(steps==1&&!yields&&!releases&&retains==1);
  if(scenario==4)assert(steps==1&&!yields&&!releases&&!retains);
  if(scenario==5)assert(steps==1&&yields==1&&!releases&&!retains);
  if(scenario==6)assert(!steps&&!releases&&!retains);
  if(scenario==7)assert(releases==1&&retains==1);
  if(scenario==9)assert(!steps&&releases==1&&!retains);
  if(scenario==10)assert(!acquires&&!steps&&!releases&&!retains);
 }
 puts("11 production headless owner lifecycle/fault cases passed");
}
