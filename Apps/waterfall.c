#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "daily_draw.h"
#include "radio_iq_v1.h"
#include "waterfall_core.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "PortableRadioPolicy.h"
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif

static const t5_app_api_v1 *waterfall_api;
static const risc_runtime_api_v1 *waterfall_runtime;
static risc_runtime_capability_v1 waterfall_grant;
static const risc_radio_iq_api_v1 *waterfall_radio;
static waterfall_image waterfall_state;
static risc_runtime_capability_v1 waterfall_preferences;
static const risc_key_value_v1 *preferences;
static bool radio_acquired, preferences_acquired;
static bool waterfall_enabled, waterfall_uncertain, waterfall_dirty;
static unsigned waterfall_status;
static bool trace_capture_seen, trace_cleanup_seen;
static int trace_capture_result, trace_cleanup_result;
static void waterfall_log(const char *line) {
    if (waterfall_runtime && waterfall_runtime->diagnostic) waterfall_runtime->diagnostic(line);
}
static void waterfall_trace_capture(int result) {
    if (trace_capture_seen && trace_capture_result == result) return;
    trace_capture_seen = true; trace_capture_result = result;
    char line[96];
    snprintf(line,sizeof(line),"SDR capture rc=%d pairs=%u",result,RISC_RADIO_IQ_PAIRS);
    waterfall_log(line);
    if (waterfall_radio->struct_size >= sizeof(risc_radio_iq_diagnostics_api_v1)) {
        const risc_radio_iq_diagnostics_api_v1 *extended=(const void*)waterfall_radio;
        risc_radio_iq_diagnostics_v1 detail={.struct_size=sizeof(detail)};
        if (extended->diagnostics && extended->diagnostics(waterfall_radio->context,&detail)) {
            snprintf(line,sizeof(line),"SDR detail stage=%lu rc=%ld ready=%lu cleanup=%lu",
                (unsigned long)detail.stage,(long)detail.result,(unsigned long)detail.dump_ready,(unsigned long)detail.cleanup_ok);
            waterfall_log(line);
            snprintf(line,sizeof(line),"SDR dump clk=%08lx start=%lu end=%lu cycles=%lu",
                (unsigned long)detail.clock_mask,(unsigned long)detail.dump_before,(unsigned long)detail.dump_after,(unsigned long)detail.elapsed_cycles);
            waterfall_log(line);
        }
    }
}
#define WATERFALL_STOPPED 8u
#define WATERFALL_AIRPLANE 9u
#define WATERFALL_NO_POLICY 10u
#define WATERFALL_NO_GRANT 11u

bool portable_radio_services_safe(void) { return !waterfall_uncertain; }
bool portable_radio_suspend(void) {
    waterfall_enabled = false;
    bool clean = !waterfall_radio || waterfall_radio->suspend(waterfall_radio->context);
    if (waterfall_radio && (!trace_cleanup_seen || trace_cleanup_result != (int)clean)) {
        waterfall_log(clean ? "SDR cleanup ok=1" : "SDR cleanup ok=0 retained=1");
        trace_cleanup_seen=true;trace_cleanup_result=(int)clean;
    }
    if (!clean) {
        waterfall_uncertain = true;
        return false;
    }
    waterfall_uncertain = false;
    waterfall_status = WATERFALL_STOPPED;
    waterfall_dirty = true;
    return true;
}

static void waterfall_yield(void) { waterfall_runtime->yield_ms(50); }
static void waterfall_drain(void) {
    while (!portable_radio_suspend()) waterfall_yield();
}
static void waterfall_release(risc_runtime_capability_v1 *grant, bool *acquired) {
    if (!*acquired) return;
    while (!waterfall_runtime->release(grant)) waterfall_yield();
    *acquired = false;
    *grant = (risc_runtime_capability_v1){0};
}
static bool waterfall_policy(void) {
    uint8_t flags = 0;
    if (!preferences || !portable_radio_load(preferences, &flags)) {
        waterfall_status = WATERFALL_NO_POLICY;
        return false;
    }
    if (flags & PORTABLE_RADIO_AIRPLANE) {
        waterfall_status = WATERFALL_AIRPLANE;
        return false;
    }
    return true;
}

static int waterfall_has_tone(void) {
    return waterfall_api->struct_size >=
               offsetof(t5_app_api_v1, fill_rounded_rect_tone) +
                   sizeof(waterfall_api->fill_rounded_rect_tone) &&
           waterfall_api->fill_rounded_rect_tone;
}

static void waterfall_paint_run(int x, int y, int width, uint8_t tone) {
    if (tone >= 3) return;
    if (waterfall_has_tone())
        waterfall_api->fill_rounded_rect_tone(x, y, width, 1, 0, tone);
    else
        waterfall_api->fill_rect(x, y, width, 1, false);
}

static const char *waterfall_status_text(unsigned status) {
    switch (status) {
    case RISC_RADIO_IQ_OK: return "BURST";
    case RISC_RADIO_IQ_NOT_RUNNING: return "NOT RUNNING";
    case RISC_RADIO_IQ_PLL_FAILED: return "PLL FAIL";
    case RISC_RADIO_IQ_PBUS_FAILED: return "PBUS FAIL";
    case RISC_RADIO_IQ_DUMP_TIMEOUT: return "DUMP FAIL";
    case RISC_RADIO_IQ_BAD_ARGUMENT: return "BAD ARG";
    case RISC_RADIO_IQ_BUSY: return "RADIO BUSY - RETRY";
    case RISC_RADIO_IQ_CLEANUP_RETAINED: return "CLEANUP RETAINED";
    case WATERFALL_STOPPED: return "PAUSED / 2440 MHZ";
    case WATERFALL_AIRPLANE: return "AIRPLANE MODE";
    case WATERFALL_NO_POLICY: return "RADIO POLICY UNAVAILABLE";
    default: return "NO GRANT";
    }
}

static void waterfall_render(unsigned capture_status) {
    waterfall_api->clear();
    daily_draw_text(waterfall_api, 8, 4, "BACK", 1);
    daily_draw_text(waterfall_api, 68, 4, "WATERFALL", 1);
    daily_draw_text(waterfall_api, 194, 4, waterfall_enabled ? "PAUSE" : "START", 1);
    daily_draw_text(waterfall_api, 8, 20, waterfall_status_text(capture_status), 1);
    waterfall_api->fill_rect(0, 32, WATERFALL_WIDTH, WATERFALL_ROWS, true);
    for (unsigned row = 0; row < WATERFALL_ROWS; ++row) {
        unsigned column = 0;
        while (column < WATERFALL_WIDTH) {
            uint8_t tone = waterfall_state.tone[row][column];
            unsigned end = column + 1;
            while (end < WATERFALL_WIDTH && waterfall_state.tone[row][end] == tone) ++end;
            waterfall_paint_run((int)column, 32 + (int)row, (int)(end - column), tone);
            column = end;
        }
    }
    waterfall_api->present(false);
    waterfall_dirty = false;
}

static void waterfall_ingest(const uint32_t *pairs, unsigned count) {
    waterfall_iq samples[WATERFALL_FFT];
    unsigned n = 0;
    for (; n < count && n < WATERFALL_FFT; ++n)
        samples[n] = waterfall_unpack_pair(pairs[n]);
    for (; n < WATERFALL_FFT; ++n) {
        samples[n].i = 0;
        samples[n].q = 0;
    }
    waterfall_spectrum(samples, &waterfall_state);
}

static int waterfall_leave(const t5_app_input_t *input) {
    if (input->exit_requested || (input->buttons & T5_APP_BUTTON_BACK)) return 1;
    return input->tapped && input->touch_x >= 0 && input->touch_x < 80 &&
           input->touch_y >= 0 && input->touch_y < 30;
}

static bool waterfall_open_radio(void) {
    if (waterfall_radio) return true;
    waterfall_grant = (risc_runtime_capability_v1){.struct_size = sizeof(waterfall_grant)};
    waterfall_log("SDR acquire begin");
    if (!waterfall_runtime->acquire("radio.iq", RISC_RADIO_IQ_API_V1, 0, &waterfall_grant)) {
        waterfall_log("SDR acquire ok=0");return false;
    }
    radio_acquired = true;
    const risc_radio_iq_api_v1 *candidate = waterfall_grant.api;
    if (!candidate || candidate->api_version != RISC_RADIO_IQ_API_V1 ||
        candidate->struct_size < sizeof(*candidate) || !candidate->capture_burst || !candidate->suspend) {
        waterfall_release(&waterfall_grant, &radio_acquired);
        return false;
    }
    waterfall_radio = candidate;
    waterfall_log("SDR acquire ok=1");
    trace_capture_seen=trace_cleanup_seen=false;
    return true;
}

void app_main(void) {
    waterfall_api = t5_app_get_api(T5_APP_ABI_VERSION);
    if (!waterfall_api || waterfall_api->abi_version != T5_APP_ABI_VERSION ||
        waterfall_api->struct_size < offsetof(t5_app_api_v1, poll) + sizeof(waterfall_api->poll) ||
        !waterfall_api->screen_width || !waterfall_api->screen_height || !waterfall_api->clear ||
        !waterfall_api->fill_rect || !waterfall_api->present || !waterfall_api->poll) return;
    if (waterfall_api->screen_width() < WATERFALL_WIDTH || waterfall_api->screen_height() < 32 + WATERFALL_ROWS) return;
    waterfall_runtime = risc_runtime_get_api(RISC_RUNTIME_API_V1);
    if (!waterfall_runtime || waterfall_runtime->api_version != RISC_RUNTIME_API_V1 ||
        waterfall_runtime->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
        !waterfall_runtime->acquire || !waterfall_runtime->release || !waterfall_runtime->yield_ms) return;
    waterfall_radio = NULL; preferences = NULL;
    trace_capture_seen=trace_cleanup_seen=false;
    waterfall_log("SDR app enter");
    radio_acquired = preferences_acquired = waterfall_uncertain = false;
    waterfall_grant = (risc_runtime_capability_v1){0};
    waterfall_preferences = (risc_runtime_capability_v1){.struct_size=sizeof(waterfall_preferences)};
    waterfall_clear(&waterfall_state);
    waterfall_status = WATERFALL_STOPPED;
    waterfall_dirty = true;
    waterfall_enabled = false;
    if (waterfall_runtime->acquire(RISC_KEY_VALUE_CAPABILITY, 1, 1, &waterfall_preferences)) {
        preferences_acquired = true;
        preferences = waterfall_preferences.api;
    }
    if (waterfall_policy()) {
        waterfall_enabled = waterfall_open_radio();
        if (!waterfall_enabled) waterfall_status = WATERFALL_NO_GRANT;
    }
    for (;;) {
        if (waterfall_uncertain) {
            /* Keep the invocation and grants pinned. No adapter, display,
             * storage or alarm I/O until cleanup has positively succeeded. */
            waterfall_drain();
        }
        if (waterfall_dirty) waterfall_render(waterfall_status);
        t5_app_input_t input = {0};
        if (!waterfall_api->poll(&input, 30)) {
#ifdef PORTABLE_ALARM_CLIENT
            if (portable_app_sleep_retained()) return;
#endif
            break;
        }
        if (waterfall_leave(&input)) {
            waterfall_drain();
#ifdef PORTABLE_RETURN_APP
            if (!input.exit_requested && waterfall_runtime->request_launch &&
                !waterfall_runtime->request_launch(PORTABLE_RETURN_APP)) {
                waterfall_dirty = true;
                continue;
            }
#endif
            break;
        }
        if (input.tapped && input.touch_x >= 170 && input.touch_x < 240 && input.touch_y >= 0 && input.touch_y < 30) {
            if (waterfall_enabled) waterfall_drain();
            else if (waterfall_policy()) {
                waterfall_enabled = waterfall_open_radio();
                if (!waterfall_enabled) waterfall_status = WATERFALL_NO_GRANT;
            }
            waterfall_dirty = true;
        }
        if (!waterfall_enabled) continue;
        if (!waterfall_policy()) { waterfall_enabled = false; waterfall_dirty = true; continue; }
        uint32_t pairs[RISC_RADIO_IQ_PAIRS];
        int next = waterfall_radio->capture_burst(waterfall_radio->context, pairs, RISC_RADIO_IQ_PAIRS);
        waterfall_trace_capture(next);
        waterfall_status = (unsigned)next;
        waterfall_dirty = true;
        if (next == RISC_RADIO_IQ_OK) waterfall_ingest(pairs, RISC_RADIO_IQ_PAIRS);
        else {
            waterfall_enabled = false; /* Explicit START retries an ordinary failure. */
            if (next == RISC_RADIO_IQ_CLEANUP_RETAINED) waterfall_uncertain = true;
        }
    }
    waterfall_drain();
    waterfall_release(&waterfall_grant, &radio_acquired);
    waterfall_radio = NULL;
    waterfall_release(&waterfall_preferences, &preferences_acquired);
    preferences = NULL;
    waterfall_log("SDR app exit");
}
