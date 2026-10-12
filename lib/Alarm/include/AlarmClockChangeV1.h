#ifndef UTILITIES_ALARM_CLOCK_CHANGE_V1_H
#define UTILITIES_ALARM_CLOCK_CHANGE_V1_H
#include "AlarmServiceV1.h"
/* Opt-in tagged suffix for the non-catalog Watch service. Keep the v1 base
 * and successful-Light-resume table byte-for-byte compatible. Other published
 * v1 suffixes are not this contract, even if their tables are large enough. */
#define ALARM_CLOCK_CHANGE_TAG UINT32_C(0x41434331)
#define ALARM_CLOCK_CHANGE_VERSION 1u
typedef struct {
    alarm_service_v1 base;
    int32_t (*resume_sleep)(void *, const alarm_sleep_v1 *);
} alarm_clock_change_prefix_v1;
typedef struct {
    alarm_clock_change_prefix_v1 sleep;
    uint32_t tag, version;
    /* Only the serialized time editor, after one explicit RTC write and
     * verified raw-calendar readback, may invoke this boundary. The service
     * independently reads RTC, accepts at most a +1-second tick, and reanchors
     * before full reconciliation. No schedule/ledger edits or acknowledgement.
     * Busy/output/error states cannot be cleared. Ordinary refresh still detects
     * unexplained clock discontinuities. */
    int32_t (*clock_changed)(void *, uint32_t verified_rtc_seconds);
} alarm_clock_change_v1;
static inline const alarm_clock_change_v1 *alarm_clock_change_control(const alarm_service_v1 *api) {
    if(!api || api->api_version!=ALARM_SERVICE_API_V1 || api->struct_size<sizeof(alarm_clock_change_v1))return NULL;
    const alarm_clock_change_v1 *control=(const alarm_clock_change_v1 *)api;
    if(control->tag!=ALARM_CLOCK_CHANGE_TAG || control->version!=ALARM_CLOCK_CHANGE_VERSION || !control->clock_changed)return NULL;
    return control;
}
#if defined(__cplusplus)
static_assert(offsetof(alarm_clock_change_v1,tag)==sizeof(alarm_clock_change_prefix_v1),"clock-change suffix offset");
#else
_Static_assert(offsetof(alarm_clock_change_v1,tag)==sizeof(alarm_clock_change_prefix_v1),"clock-change suffix offset");
#endif
#endif
