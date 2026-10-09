#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
#include "PortableBackgroundServices.h"
#endif
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "daily_draw.h"
#include "radio_iq_v1.h"
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
static risc_runtime_capability_v1 waterfall_preferences;
static const risc_key_value_v1 *preferences;
static bool radio_acquired, preferences_acquired;
static bool waterfall_enabled, waterfall_uncertain, waterfall_dirty;
static unsigned waterfall_status;
static bool trace_capture_seen, trace_cleanup_seen;
static int trace_capture_result, trace_cleanup_result;
static unsigned waterfall_capture_pairs=RISC_RADIO_IQ_PAIRS;
static void rf_radio_suspended(void);
static bool rf_services_safe(void);
static void waterfall_log(const char *line) {
    if (waterfall_runtime && waterfall_runtime->diagnostic) waterfall_runtime->diagnostic(line);
}
static bool waterfall_trace_stage(void *context,const char *stage) {
    (void)context;
    /* Let the existing USB producer drain before each pre-operation marker.
     * This callback runs only before/after the DMA-owned interval. */
    waterfall_runtime->yield_ms(1);
    char line[96];snprintf(line,sizeof(line),"SDR stage=%s",stage);
    return !waterfall_runtime->diagnostic || waterfall_runtime->diagnostic(line);
}
static void waterfall_trace_capture(int result) {
    if (trace_capture_seen && trace_capture_result == result) return;
    trace_capture_seen = true; trace_capture_result = result;
    char line[96];
    snprintf(line,sizeof(line),"SDR capture rc=%d pairs=%u",result,waterfall_capture_pairs);
    waterfall_log(line);
    if (waterfall_radio->struct_size >= offsetof(risc_radio_iq_diagnostics_api_v1,capture_burst_traced)) {
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

bool portable_radio_services_safe(void) { return !waterfall_uncertain && rf_services_safe(); }
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
    rf_radio_suspended();
    return true;
}

/* Retry loops may hold uncertain RF or storage custody. Yielding here must
 * not acquire/pause unrelated background providers while cleanup is pending.
 * Safe foreground boundaries already stop those services explicitly. */
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


#include "rf_application.inc"
