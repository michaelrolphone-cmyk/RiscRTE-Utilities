#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "daily_draw.h"
#include "radio_iq_v1.h"
#include "waterfall_core.h"
#include <stddef.h>

static const t5_app_api_v1 *waterfall_api;
static const risc_runtime_api_v1 *waterfall_runtime;
static risc_runtime_capability_v1 waterfall_grant;
static const risc_radio_iq_api_v1 *waterfall_radio;
static waterfall_image waterfall_state;
static unsigned waterfall_shown = 0xffffffffu;

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
    default: return "NO GRANT";
    }
}

static void waterfall_render(unsigned capture_status) {
    waterfall_api->clear();
    daily_draw_text(waterfall_api, 8, 4, "WATERFALL", 2);
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
    waterfall_shown = capture_status;
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

void app_main(void) {
    waterfall_api = t5_app_get_api(T5_APP_ABI_VERSION);
    if (!waterfall_api || waterfall_api->abi_version != T5_APP_ABI_VERSION ||
        waterfall_api->struct_size < offsetof(t5_app_api_v1, poll) + sizeof(waterfall_api->poll) ||
        !waterfall_api->screen_width || !waterfall_api->screen_height || !waterfall_api->clear ||
        !waterfall_api->fill_rect || !waterfall_api->present || !waterfall_api->poll) return;
    int width = waterfall_api->screen_width();
    int height = waterfall_api->screen_height();
    if (width < WATERFALL_WIDTH || height < 32 + WATERFALL_ROWS) return;
    waterfall_clear(&waterfall_state);
    waterfall_runtime = risc_runtime_get_api(RISC_RUNTIME_API_V1);
    unsigned status = 0xffffffffu;
    if (!waterfall_runtime || waterfall_runtime->api_version != RISC_RUNTIME_API_V1 ||
        waterfall_runtime->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
        !waterfall_runtime->acquire || !waterfall_runtime->release) {
        waterfall_render(status);
    } else {
        waterfall_grant.struct_size = sizeof(waterfall_grant);
        waterfall_grant.slot = 0;
        waterfall_grant.generation = 0;
        waterfall_grant.api = 0;
        if (!waterfall_runtime->acquire("radio.iq", RISC_RADIO_IQ_API_V1, 0, &waterfall_grant) ||
            !waterfall_grant.api) {
            waterfall_grant.api = 0;
            waterfall_render(status);
        } else {
            waterfall_radio = waterfall_grant.api;
            if (waterfall_radio->api_version != RISC_RADIO_IQ_API_V1 ||
                waterfall_radio->struct_size <
                    offsetof(risc_radio_iq_api_v1, capture_burst) + sizeof(waterfall_radio->capture_burst) ||
                !waterfall_radio->capture_burst) {
                waterfall_radio = 0;
                waterfall_render(status);
            }
        }
    }
    for (;;) {
        t5_app_input_t input = {0};
        if (!waterfall_api->poll(&input, 30) || waterfall_leave(&input)) break;
        if (!waterfall_radio) continue;
        uint32_t pairs[RISC_RADIO_IQ_PAIRS];
        int next = waterfall_radio->capture_burst(waterfall_radio->context, pairs, RISC_RADIO_IQ_PAIRS);
        if (next == RISC_RADIO_IQ_OK) waterfall_ingest(pairs, RISC_RADIO_IQ_PAIRS);
        if ((unsigned)next != waterfall_shown) waterfall_render((unsigned)next);
    }
    if (waterfall_grant.api && waterfall_runtime && waterfall_runtime->release)
        (void)waterfall_runtime->release(&waterfall_grant);
}
