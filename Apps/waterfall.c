#include "T5AppApi.h"
#include "daily_draw.h"
#include "s3_iq_burst.h"
#include "waterfall_core.h"
#include <stddef.h>

static const t5_app_api_v1 *waterfall_api;
static waterfall_image waterfall_state;
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

static void waterfall_render(unsigned capture_status) {
    waterfall_api->clear();
    daily_draw_text(waterfall_api, 8, 4, "WATERFALL", 2);
    daily_draw_text(waterfall_api, 8, 20,
                    capture_status == S3_IQ_OK ? "BURST" : "NO RADIO", 1);
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
    unsigned status = S3_IQ_NEED_RADIO_BRINGUP;
    waterfall_render(status);
    for (;;) {
        t5_app_input_t input = {0};
        if (!waterfall_api->poll(&input, 30) || input.exit_requested ||
            (input.buttons & T5_APP_BUTTON_BACK)) break;
        if (input.tapped && input.touch_x >= 0 && input.touch_x < 80 &&
            input.touch_y >= 0 && input.touch_y < 30) break;
        uint32_t pairs[S3_IQ_PAIRS];
        unsigned next = s3_iq_take_burst(pairs, S3_IQ_PAIRS);
        if (next == S3_IQ_OK) {
            waterfall_ingest(pairs, S3_IQ_PAIRS);
            status = next;
            waterfall_render(status);
        }
    }
}
