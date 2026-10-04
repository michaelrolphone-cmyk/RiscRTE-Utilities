#include <assert.h>
#include <stdio.h>
#include "../Apps/waterfall_core.h"

static int16_t qsin(unsigned n) { return waterfall_sin_q14[n & (WATERFALL_FFT - 1)]; }

static void tone(waterfall_iq *out, unsigned bin, int amplitude) {
    for (unsigned n = 0; n < WATERFALL_FFT; ++n) {
        unsigned angle = (bin * n) & (WATERFALL_FFT - 1);
        out[n].i = (int16_t)(((int32_t)amplitude * qsin(angle + WATERFALL_FFT / 4)) >> 14);
        out[n].q = (int16_t)(((int32_t)amplitude * qsin(angle)) >> 14);
    }
}

int main(void) {
    assert(waterfall_unpack_pair(0x200u).i == -512);
    assert(waterfall_unpack_pair(0x1ffu).i == 511);
    assert(waterfall_unpack_pair(0x1ffu << 10).q == 511);
    assert(waterfall_unpack_pair(0x200u << 10).q == -512);

    waterfall_iq samples[WATERFALL_FFT];
    waterfall_image image;
    waterfall_clear(&image);
    tone(samples, 10, 400);
    waterfall_spectrum(samples, &image);
    assert(image.rows_pushed == 1);

    unsigned best = 0;
    uint8_t best_tone = 3;
    for (unsigned x = 0; x < WATERFALL_WIDTH; ++x) {
        if (image.tone[0][x] < best_tone) {
            best_tone = image.tone[0][x];
            best = x;
        }
    }
    unsigned best_bin = 1u + (best * (WATERFALL_BINS - 2u)) / (WATERFALL_WIDTH - 1u);
    if (best_bin < 8 || best_bin > 12) {
        fprintf(stderr, "peak column %u bin %u tone %u\n", best, best_bin, best_tone);
        assert(0);
    }
    assert(best_tone == 0);
    assert(waterfall_rgb565[image.tone[0][best]] == 0xffffu);

    tone(samples, 40, 400);
    waterfall_spectrum(samples, &image);
    assert(image.rows_pushed == 2);
    assert(image.tone[1][best] == 0);

    printf("waterfall core ok\n");
    return 0;
}
