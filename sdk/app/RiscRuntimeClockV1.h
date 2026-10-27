#pragma once
#include "RiscRuntimeV1.h"
/* Read the active invocation's wrapping, boot-local millisecond clock.
 * This helper adds no authority: callers must enforce their own custody fences
 * before calling it, including when they supply a copied Runtime table.
 * A present clock callback is authoritative; failure never retries through
 * health. Older SDK/table prefixes and null suffixes retain the health fallback.
 * Zero is valid. Output is committed only on success. No native ABI changes. */
static inline bool risc_runtime_monotonic_ms_v1(const risc_runtime_api_v1 *api,
                                               uint32_t *out) {
  if(!api || !out || api->api_version!=RISC_RUNTIME_API_V1 ||
     api->struct_size<offsetof(risc_runtime_api_v1,health)+sizeof(api->health))return false;
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
  if(api->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE && api->monotonic_ms) {
    uint32_t value=0;
    if(!api->monotonic_ms(&value))return false;
    *out=value;return true;
  }
#endif
  if(!api->health)return false;
  risc_runtime_health_v1 health={0};health.struct_size=sizeof(health);
  if(!api->health(&health))return false;
  *out=health.uptime_ms;return true;
}
