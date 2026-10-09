#pragma once
#include "RiscRuntimeV1.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
/* Frozen Runtime prefix plus its existing optional terminal fence. No provider
 * access, release, diagnostic or background retry follows a retained backend. */
static inline bool (*app_retained_fence_method(const risc_runtime_api_v1 *runtime))(void){
 bool (*method)(void)=NULL;const size_t offset=RISC_RUNTIME_CAPABILITIES_V1_SIZE+sizeof(bool (*)(void));
 if(runtime&&runtime->struct_size>=offset+sizeof(method))memcpy(&method,(const uint8_t*)runtime+offset,sizeof(method));
 return method;
}
static inline bool app_retained_fence(const risc_runtime_api_v1 *runtime){
 bool (*method)(void)=app_retained_fence_method(runtime);return method&&method();
}
