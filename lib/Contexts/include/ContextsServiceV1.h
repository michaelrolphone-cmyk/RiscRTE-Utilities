#pragma once
/* Ordinary cooperative provider, never a Runtime policy or background task.
 * All output is copied. Input record bytes are consumed before return; no app
 * memory, storage API or callback is retained. Canonical records belong to
 * their source apps. This first revision imports signatures and preferences. */
#include <stdbool.h>
#include <stdint.h>
#define CONTEXTS_SERVICE_CAPABILITY "contexts.service"
#define CONTEXTS_AUDIO 1u
#define CONTEXTS_RADIO 2u
#define CONTEXTS_ALL 3u
#define CONTEXTS_NAME_SIZE 17u
#define CONTEXTS_SLOTS 8u
enum { CONTEXTS_OFF, CONTEXTS_LOADING, CONTEXTS_PAUSED, CONTEXTS_LIVE,
       CONTEXTS_UNAVAILABLE, CONTEXTS_RETAINED };
enum { CONTEXTS_MODEL_EMPTY, CONTEXTS_MODEL_REQUESTED, CONTEXTS_MODEL_LOADING,
       CONTEXTS_MODEL_READY, CONTEXTS_MODEL_FAILED };
enum { CONTEXTS_RECORD_PREFERENCES=1, CONTEXTS_RECORD_SIGNATURE=2,
       CONTEXTS_RECORD_TEMPORAL_BANK=3, CONTEXTS_RECORD_NEURAL=4 };
enum { CONTEXTS_EXPORT_OK=0, CONTEXTS_EXPORT_STORAGE=1,
       CONTEXTS_EXPORT_INVALID=2, CONTEXTS_EXPORT_UNSUPPORTED=3 };
enum { CONTEXTS_PRESET_NONE, CONTEXTS_PRESET_CLAIMED,
       CONTEXTS_PRESET_APPLIED, CONTEXTS_PRESET_PARTIAL };
typedef struct {
    uint32_t struct_size;
    bool enabled,awake,audio_allowed,radio_allowed;
    /* Desired source mask. A zero mask observes nothing. Callers must pause
     * before foreground capture/playback, alarm output, flash work or sleep.
     * radio_allowed additionally confirms current radio/BLE custody policy. */
    uint32_t sources;
} contexts_policy_v1;
typedef struct {
    uint32_t source,model_state,model_generation,model_error;
    uint32_t room_entry,preset_entry;
    uint32_t samples,age_ms;
    int32_t capture_error;
    int32_t room_slot,event_slot;
    uint32_t room_confidence,event_confidence;
    char room_name[CONTEXTS_NAME_SIZE],event_name[CONTEXTS_NAME_SIZE];
    bool current,room_valid,room_ambiguous,event_valid,event_ambiguous;
    bool signatures_ready,temporal_ready,neural_ready;
    /* Age UINT32_MAX means no observation in this awake capture session.
     * room_valid excludes ambiguous/held/verifying tracker states. */
} contexts_source_status_v1;
typedef struct {
    uint32_t struct_size,state,export_pending,export_active;
    bool cleanup_pending;
    contexts_source_status_v1 audio,radio;
    uint32_t preset_source,preset_generation,preset_result;
    int32_t preset_slot;
} contexts_status_v1;
typedef struct {
    uint32_t source,slot,kind;
    char name[CONTEXTS_NAME_SIZE];
} contexts_label_v1;
typedef struct {
    uint32_t api_version,struct_size;
    void *context;
    bool (*step)(void *,const contexts_policy_v1 *);
    bool (*pause)(void *);
    bool (*status)(void *,contexts_status_v1 *);
    /* Explicit retry only: failed sources never requeue themselves. Parent
     * launches each requested owner once through ordinary app handoff. */
    bool (*request_export)(void *,uint32_t sources);
    bool (*begin_export)(void *,uint32_t source);
    bool (*export_record)(void *,uint32_t source,uint32_t kind,uint32_t index,
                          const void *bytes,uint32_t size);
    bool (*finish_export)(void *,uint32_t source,uint32_t result);
    /* Returns 1 for a copied saved slot (including kind=0), 0 at end, -1
     * when this source is not ready. No observation is implied. */
    int32_t (*label)(void *,uint32_t source,uint32_t slot,contexts_label_v1 *);
    /* Consume before the first settings write. The same identity remains
     * consumed through pause, unknown observations and failed/partial writes.
     * Source-local room_entry advances on every distinct confirmed room,
     * including rooms without presets. preset_entry records its consumed edge.
     * A changed model or a different confirmed room permits another claim. */
    bool (*claim_preset)(void *,uint32_t source,uint32_t slot,const char *name,
                         uint32_t model_generation);
    bool (*preset_result)(void *,uint32_t source,uint32_t model_generation,
                          uint32_t result);
} contexts_service_v1;
