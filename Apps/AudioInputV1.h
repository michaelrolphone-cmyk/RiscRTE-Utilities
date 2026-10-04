#pragma once
/* Consumer-only declaration of Watch include/twatch_caps.h audio.input@1.
 * This is the existing provider ABI, not a new microphone driver or export.
 * twatch_mic bounds reads to 256 mono s16 PCM frames / 40 ms at 8 or 16 kHz.
 * Always close an attempted open: a failed native open may retain cleanup.
 * A failed close must retain the invocation without any further provider I/O.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t api_version, struct_size;
    void *context;
    bool (*open)(void *context, uint32_t rate_hz);
    bool (*read)(void *context, int16_t *pcm, size_t frames, size_t *got);
    bool (*level)(void *context, uint16_t *rms_out);
    bool (*close)(void *context);
} twatch_audio_in_api_v1;
