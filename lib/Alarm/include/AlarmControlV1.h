#ifndef RISC_ALARM_CONTROL_V1_H
#define RISC_ALARM_CONTROL_V1_H
/* Semantic one-shot alarm operations. No graphics, RTC representation, device
 * identities, or presentation policy crosses this interface. An ordinary
 * provider adapts the deployed scheduler and clock policy to this contract. */
#include <stdint.h>
#define ALARM_CONTROL_CAPABILITY "alarm.control"
#define ALARM_CONTROL_API_V1 1u
#define ALARM_CONTROL_COMMAND_BYTES 96u
#define ALARM_CONTROL_PROTOCOL 1u
enum { ALARM_CONTROL_OK=0, ALARM_CONTROL_PENDING=1,
       ALARM_CONTROL_INVALID=-1, ALARM_CONTROL_STALE=-2,
       ALARM_CONTROL_STORAGE=-3, ALARM_CONTROL_CLOCK=-4,
       ALARM_CONTROL_AMBIGUOUS=-5, ALARM_CONTROL_BLOCKED=-6,
       ALARM_CONTROL_UNCERTAIN=-7, ALARM_CONTROL_RETAINED=-9,
       ALARM_CONTROL_EXPIRED=-10 };
enum { ALARM_CONTROL_ARM=1, ALARM_CONTROL_CANCEL=2, ALARM_CONTROL_VOLUME=3 };
enum { ALARM_CONTROL_CONFIG_VALID=1u, ALARM_CONTROL_CLOCK_VALID=2u,
       ALARM_CONTROL_ENABLED=4u, ALARM_CONTROL_ALERT=8u,
       ALARM_CONTROL_HAS_VOLUME=16u, ALARM_CONTROL_VOLUME_VALID=32u,
       ALARM_CONTROL_SERVICE_READY=64u };
typedef struct {uint32_t kind,revision,deadline,generation;} alarm_control_occurrence_v1;
typedef struct {
    uint32_t api_version,struct_size,flags,revision,minutes,volume;
    uint32_t confirmed_revision,schedule_state;
    int32_t error;
    alarm_control_occurrence_v1 occurrence;
    char zone[40],status[72];
} alarm_control_snapshot_v1;
/* Opaque but pointer-free and persistable. Save the exact command BEFORE
 * apply, retry that same command after an ambiguous write or controller reload.
 * Do not re-prepare it with a later 'now'. Service verifies clock-policy tag,
 * format, checksum and exact before/after records on every apply. */
typedef struct {uint32_t struct_size;uint8_t bytes[ALARM_CONTROL_COMMAND_BYTES];} alarm_control_command_v1;
typedef struct {
    uint32_t api_version,struct_size;
    void *context;
    /* Explicit reconciliation safe point. Not a background presentation tick. */
    int32_t (*step)(void *);
    /* Refresh config, clock and copied scheduler status. Ordinary errors still
     * return a complete explanatory snapshot; RETAINED leaves output alone. */
    int32_t (*read)(void *,alarm_control_snapshot_v1 *);
    /* expected is the last observed schedule revision, or volume for VOLUME. */
    int32_t (*prepare)(void *,uint32_t operation,uint32_t value,uint32_t expected,
                       alarm_control_command_v1 *);
    int32_t (*apply)(void *,const alarm_control_command_v1 *);
    int32_t (*dismiss)(void *,const alarm_control_occurrence_v1 *);
} alarm_control_api_v1;
#endif
