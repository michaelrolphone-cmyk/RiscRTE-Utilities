/* Optional CPU-only host benchmark. This is not Watch hardware timing.
 * Build: cc -std=c11 -Os -Wall -Wextra -Werror tests/spectrum_dsp_benchmark.c
 *        -o /tmp/spectrum_dsp_benchmark && /tmp/spectrum_dsp_benchmark
 */
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "../Apps/spectrum_dsp.h"
static spectrum_dsp_state state;
static int16_t pcm[SPECTRUM_DSP_MAX_FFT];
static uint16_t spectrum[196], waterfall[166];
static int16_t spectrum_db[196], waterfall_db[166];
static volatile int32_t observed;
int main(void) {
    static const char *names[] = {"RECT", "HANN", "HAMMING", "BLACKMAN", "FLAT TOP"};
    const unsigned repetitions = 200;
    uint32_t random = 0x78654;
    for (unsigned i = 0; i < SPECTRUM_DSP_MAX_FFT; ++i) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        pcm[i] = (int16_t)random;
    }
    puts("Host CPU benchmark only; not Watch real-time qualification. 8192 samples / 16000 Hz = 512 ms capture interval.");
    for (unsigned window = 0; window < SPECTRUM_DSP_WINDOW_COUNT; ++window) {
        spectrum_dsp_config config = spectrum_dsp_defaults();
        config.fft_size = 8192; config.window = (spectrum_dsp_window)window;
        assert(spectrum_dsp_init(&state, &config));
        clock_t start = clock();
        for (unsigned frame = 0; frame < repetitions; ++frame) {
            for (unsigned i = 0; i < config.fft_size; i += SPECTRUM_DSP_CHUNK)
                assert(spectrum_dsp_feed(&state, pcm + i, SPECTRUM_DSP_CHUNK));
        }
        clock_t analyzed = clock();
        for (unsigned frame = 0; frame < repetitions; ++frame) {
            assert(spectrum_dsp_resample(&state, spectrum, spectrum_db, 196));
            assert(spectrum_dsp_resample(&state, waterfall, waterfall_db, 166));
            for (unsigned label = 0; label < 8; ++label) observed += spectrum_dsp_label_db(&state, label * 900u + 100u);
        }
        clock_t finish = clock();
        double transform_ms = (double)(analyzed - start) * 1000 / CLOCKS_PER_SEC / repetitions;
        double display_ms = (double)(finish - analyzed) * 1000 / CLOCKS_PER_SEC / repetitions;
        printf("%-8s FFT/window %.3f ms; 196+166 resample + 8 labels %.3f ms; combined %.3f ms; state %zu bytes\n", names[window], transform_ms, display_ms, transform_ms + display_ms, sizeof(state));
    }
    return 0;
}
