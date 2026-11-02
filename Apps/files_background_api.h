#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "RiscRuntimeV1.h"
#include "ContextsServiceV1.h"
#include "RiscKeyValueV1.h"
#include "TelemetryBroadcastPolicyV1.h"
/* Header-only invocation-local implementation: include files_background.h in
 * exactly one root translation unit. alive must be pure. Callbacks are
 * synchronous; they may not
 * reenter the helper. No app frame/string pointers are retained. */
typedef struct {
 bool battery_valid,charging,local_time_valid;
 uint8_t battery_percent,local_hour;
} files_background_observation;
typedef struct {
 bool (*alive)(void*);void (*retain)(void*);void *context;
 /* Required when Contexts is present; called only at regular settled
  * progression. Supply checked battery and
  * LOCAL hour, preserving the user's timezone. UTC aging is separate. Clean absence
  * returns true with validity false; false aborts the invocation. */
 bool (*observe)(void*,files_background_observation*);
 /* Queued, copied root-owned UI/launch boundary. Message acceptance means the
  * root saved the draft and will consume dismissal. Launch acceptance means
  * the root validated and queued an allowed installed destination. */
 bool (*message)(void*,const char text[33]);
 bool (*launch)(void*,const char name[33]);
 /* Optional face policy: same native namespace/encoding; no device dispatch. */
 bool (*face_save)(const risc_key_value_v1*,unsigned);
 uint16_t action_mask;
 uint8_t alert_modes; /* confirmed alarm output modes; unsupported bytes stay saved */
} files_background_hooks;
typedef struct {
 bool settled,sharing,audio_allowed,radio_allowed,broadcast_allowed;
 /* Never automatically consume a workflow while a prior root modal/launch is
  * outstanding. Input-hold defer comes AFTER regular background progression. */
 bool workflow_ready;
} files_background_boundary;
typedef struct {
 bool contexts_available,broadcast_available,contexts_valid,broadcast_valid;
 bool retained,paused,capture_active,automatic_work,cleanup_pending;
 uint32_t pending_effects; /* bit (1 << RISC_SETTINGS_*), not permissions */
 contexts_status_v1 contexts;
 telemetry_broadcast_status_v1 broadcast;
} files_background_status;
/* Functions defined static in files_background.h:
 bool files_background_init(rt, const files_background_hooks*);
 bool files_background_step(const files_background_boundary*);
 bool files_background_capture(void);  // <=16ms, including PRESENTING
 bool files_background_pause(void);    // finish before storage/sharing/handoff
 bool files_background_before_storage(bool write); // read keeps loaded policy
 bool files_background_close(void);
 files_background_status files_background_status_copy(void); // no I/O
 bool files_background_capture_active(void);                  // no I/O
 bool files_background_effect_done(unsigned effect,unsigned outcome);
 // PENDING preserves the bit: root advances PROGRESS, never repeats APPLY.
 // effect_done clears only a copied terminal effect after root's settled
 // RISC_SETTINGS_APPLY/result-query sequence. No action/persistence replay.
 */
