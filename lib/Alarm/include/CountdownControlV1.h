#ifndef RISC_COUNTDOWN_CONTROL_V1_H
#define RISC_COUNTDOWN_CONTROL_V1_H
/* Semantic elapsed countdown operations. No graphics, RTC representation, device
 * identities, or presentation policy crosses this interface. An ordinary
 * provider adapts the deployed scheduler and clock policy to this contract. */
#include <stdint.h>
#define COUNTDOWN_CONTROL_CAPABILITY "countdown.control"
#define COUNTDOWN_CONTROL_API_V1 1u
#define COUNTDOWN_CONTROL_COMMAND_BYTES 96u
#define COUNTDOWN_CONTROL_PROTOCOL 1u
enum { COUNTDOWN_CONTROL_OK=0, COUNTDOWN_CONTROL_PENDING=1,
       COUNTDOWN_CONTROL_INVALID=-1, COUNTDOWN_CONTROL_STALE=-2,
       COUNTDOWN_CONTROL_STORAGE=-3, COUNTDOWN_CONTROL_CLOCK=-4,
       COUNTDOWN_CONTROL_AMBIGUOUS=-5, COUNTDOWN_CONTROL_BLOCKED=-6,
       COUNTDOWN_CONTROL_UNCERTAIN=-7, COUNTDOWN_CONTROL_RETAINED=-9,
       COUNTDOWN_CONTROL_EXPIRED=-10 };
enum { COUNTDOWN_CONTROL_ARM=1, COUNTDOWN_CONTROL_CANCEL=2 };
enum { COUNTDOWN_CONTROL_CONFIG_VALID=1u, COUNTDOWN_CONTROL_CLOCK_VALID=2u,
       COUNTDOWN_CONTROL_ENABLED=4u, COUNTDOWN_CONTROL_ALERT=8u,
       COUNTDOWN_CONTROL_HAS_SOUND=16u, COUNTDOWN_CONTROL_HAS_HAPTIC=32u,
       COUNTDOWN_CONTROL_SERVICE_READY=64u };
typedef struct {uint32_t kind,revision,deadline,generation;} countdown_control_occurrence_v1;
typedef struct {
    uint32_t api_version,struct_size,flags,revision,duration_seconds,remaining_seconds;
    uint32_t confirmed_revision,schedule_state;
    int32_t error;
    countdown_control_occurrence_v1 occurrence;
    char status[72];
} countdown_control_snapshot_v1;
/* Opaque but pointer-free and persistable. Save the exact command BEFORE
 * apply, retry that same command after an ambiguous write or controller reload.
 * Do not re-prepare it with a later 'now'. Service verifies clock-policy tag,
 * format, checksum and exact before/after records on every apply. */
typedef struct {uint32_t struct_size;uint8_t bytes[COUNTDOWN_CONTROL_COMMAND_BYTES];} countdown_control_command_v1;
typedef struct {
    uint32_t api_version,struct_size;
    void *context;
    /* Explicit reconciliation safe point. Not a background presentation tick. */
    int32_t (*step)(void *);
    /* Refresh config, clock and copied scheduler status. Ordinary errors still
     * return a complete explanatory snapshot; RETAINED leaves output alone. */
    int32_t (*read)(void *,countdown_control_snapshot_v1 *);
    /* expected is the last observed schedule revision; ARM value is elapsed seconds. */
    int32_t (*prepare)(void *,uint32_t operation,uint32_t value,uint32_t expected,
                       countdown_control_command_v1 *);
    int32_t (*apply)(void *,const countdown_control_command_v1 *);
    int32_t (*dismiss)(void *,const countdown_control_occurrence_v1 *);
} countdown_control_api_v1;
#endif
