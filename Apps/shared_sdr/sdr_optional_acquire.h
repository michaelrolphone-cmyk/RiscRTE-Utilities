#pragma once
/* The shared foreground has three explicit unavailable-service paths.
 * Manifest optionality alone does not establish cleanup or caller semantics.
 * Keep exact request identity central; never generalize a boolean refusal. */
#include "RiscRuntimeV1.h"
#include <string.h>
typedef struct { const char *name; uint32_t version; uint64_t instance; } sdr_optional_request;
static const sdr_optional_request sdr_optional_requests[]={
 {"display.output",1,0},
 {"ui.canvas-input",1,0},
 {"alarm.service.facade",2,0},
};
static bool sdr_optional_acquire_request(const char *name,uint32_t version,uint64_t instance) {
 if(!name)return false;
 for(unsigned i=0;i<sizeof(sdr_optional_requests)/sizeof(sdr_optional_requests[0]);i++) {
  const sdr_optional_request *r=&sdr_optional_requests[i];
  if(version==r->version&&instance==r->instance&&!strcmp(name,r->name))return true;
 }
 return false;
}
static bool sdr_optional_grant_empty(const risc_runtime_capability_v1 *grant) {
 return grant&&grant->struct_size==sizeof(*grant)&&!grant->api&&!grant->slot&&!grant->generation;
}
static bool sdr_optional_acquire_clean(const risc_runtime_api_v1 *runtime,
 const risc_runtime_capability_v1 *before,const risc_runtime_capability_v1 *grant) {
 if(!runtime||!sdr_optional_grant_empty(before)||!sdr_optional_grant_empty(grant)||
    risc_runtime_get_api(1)!=runtime||!runtime->health)return false;
 risc_runtime_health_v1 health={.struct_size=sizeof(health)};
 bool live=runtime->health(&health);
 return live&&risc_runtime_get_api(1)==runtime&&sdr_optional_grant_empty(grant);
}
