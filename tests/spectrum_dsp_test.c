#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Apps/spectrum_dsp.h"

#define PI 3.14159265358979323846
static spectrum_dsp_state state, partial, saved;
static int16_t pcm[SPECTRUM_DSP_MAX_FFT], stream[8192 + 256];
static uint32_t seed = 0x706e61;
static unsigned analyzed;
static double worst_db_error, worst_dft_error;
static uint32_t random_u32(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static void bounds(void) {
    assert(state.used < state.config.fft_size);
    assert(state.peak_bin <= state.config.fft_size / 2u);
    assert(state.coherent_gain_q20 > 200000 && state.coherent_gain_q20 <= 1048576);
    for (unsigned i = 0; i < state.config.fft_size; ++i) {
        assert(llabs(state.real[i]) <= 16777216);
        assert(llabs(state.imag[i]) <= 16777216);
    }
    for (unsigned i = 0; i <= state.config.fft_size / 2u; ++i)
        assert(state.amplitude_q24[i] < 167772160u);
}
static void feed(void) {
    unsigned n = state.config.fft_size;
    uint32_t before = state.transforms;
    for (unsigned i = 0; i < n; i += 256) assert(spectrum_dsp_feed(&state, pcm + i, 256));
    assert(state.transforms == before + 1u && state.has_transform && state.used == 0);
    ++analyzed;
    bounds();
}
static void tone(double bin, double amplitude, double phase) {
    for (unsigned i = 0; i < state.config.fft_size; ++i)
        pcm[i] = (int16_t)lround(amplitude * sin(2 * PI * bin * i / state.config.fft_size + phase));
    feed();
}
static void check_db(unsigned bin, double amplitude, double tolerance) {
    double expected = 20 * log10(amplitude / 32768.0);
    double actual = spectrum_dsp_amplitude_db(state.amplitude_q24[bin], 0) / 100.0;
    double error = fabs(actual - expected);
    if (error > worst_db_error) worst_db_error = error;
    if (error > tolerance) fprintf(stderr, "dB mismatch N=%u window=%d bin=%u amplitude=%.0f expected=%.5f actual=%.5f tolerance=%.3f\n", state.config.fft_size, state.config.window, bin, amplitude, expected, actual, tolerance);
    assert(error <= tolerance);
}
static double reference_window(spectrum_dsp_window window, unsigned i, unsigned n) {
    double a = 2 * PI * i / (n - 1u);
    switch (window) {
    case SPECTRUM_DSP_RECT: return 1;
    case SPECTRUM_DSP_HANN: return .5 - .5 * cos(a);
    case SPECTRUM_DSP_HAMMING: return .54 - .46 * cos(a);
    case SPECTRUM_DSP_BLACKMAN: return .42 - .5 * cos(a) + .08 * cos(2*a);
    case SPECTRUM_DSP_FLAT_TOP: return .21557895 - .41663158 * cos(a) + .277263158 * cos(2*a) - .083578947 * cos(3*a) + .006947368 * cos(4*a);
    default: abort();
    }
}
static void reference_dft(void) {
    unsigned n = state.config.fft_size;
    double gain = 0;
    for (unsigned i = 0; i < n; ++i) gain += reference_window(state.config.window, i, n);
    for (unsigned k = 0; k <= n / 2; ++k) {
        double re = 0, im = 0;
        for (unsigned i = 0; i < n; ++i) {
            double sample = pcm[i] * reference_window(state.config.window, i, n);
            double angle = 2 * PI * k * i / n;
            re += sample * cos(angle); im -= sample * sin(angle);
        }
        double expected = hypot(re, im) * ((k == 0 || k == n / 2) ? 1 : 2) / (32768.0 * gain);
        double actual = state.amplitude_q24[k] / 16777216.0;
        double error = fabs(actual - expected);
        if (error > worst_dft_error) worst_dft_error = error;
        if (error >= 0.00006) fprintf(stderr, "DFT mismatch N=%u window=%d bin=%u expected=%.8f actual=%.8f\n", n, state.config.window, k, expected, actual);
        assert(error < 0.00006);
    }
}
static void math_helpers(void) {
    const uint64_t dividends[] = {0, 1, UINT32_MAX, (uint64_t)UINT32_MAX + 1u, (uint64_t)1 << 63, UINT64_MAX};
    const uint32_t divisors[] = {1, 2, 3, 255, 16000, 48000, 65536000, (uint32_t)1 << 31, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(dividends) / sizeof(dividends[0]); ++i)
        for (unsigned j = 0; j < sizeof(divisors) / sizeof(divisors[0]); ++j)
            assert(spectrum_dsp_div_u64_u32(dividends[i], divisors[j]) == dividends[i] / divisors[j]);
    assert(spectrum_dsp_div_u64_u32(123, 0) == UINT64_MAX);
    for (unsigned i = 0; i < 100000; ++i) {
        uint64_t value = ((uint64_t)random_u32() << 32) | random_u32();
        uint32_t denominator = random_u32() | 1u;
        assert(spectrum_dsp_div_u64_u32(value, denominator) == value / denominator);
    }
    for (unsigned phase = 0; phase < 65536; ++phase) {
        double actual = spectrum_dsp_sin(phase) / 1073741824.0;
        assert(fabs(actual - sin(2 * PI * phase / 65536.0)) < 0.00000031);
        assert(spectrum_dsp_sin(phase) == spectrum_dsp_sin(phase + 65536u));
    }
    const uint64_t squares[] = {0, 1, 2, UINT32_MAX, 65536ULL * 65536, UINT64_MAX};
    for (unsigned i = 0; i < sizeof(squares) / sizeof(squares[0]); ++i) {
        uint64_t r = spectrum_dsp_sqrt(squares[i]);
        assert(r*r <= squares[i]);
        if (r < UINT32_MAX) assert((r+1)*(r+1) > squares[i]);
    }
    assert(spectrum_dsp_log2_q16(0) == INT32_MIN);
    for (unsigned i = 0; i < 32; ++i) assert(spectrum_dsp_log2_q16(1u << i) == (int32_t)(i * 65536));
    for (unsigned i = 0; i < 10000; ++i) {
        uint32_t value = random_u32() | 1u;
        assert(fabs(spectrum_dsp_log2_q16(value) / 65536.0 - log2(value)) < .000016);
    }
    for (int exponent = -17 * 65536; exponent < 16 * 65536; exponent += 137) {
        double expected = exp2(exponent / 65536.0) * 65536;
        double result = spectrum_dsp_exp2_q16(exponent);
        assert(fabs(result - expected) <= .6 + expected * .00000002);
    }
    assert(spectrum_dsp_exp2_q16(INT32_MIN) == 0);
    assert(spectrum_dsp_exp2_q16(INT32_MAX) == UINT32_MAX);
    assert(spectrum_dsp_amplitude_db(0, 60) == -12000);
    assert(spectrum_dsp_amplitude_db(1u << 24, 0) == 0);
    assert(spectrum_dsp_amplitude_db(1u << 23, 0) == -602);
    assert(spectrum_dsp_amplitude_db(1u << 23, 12) == 598);
}
static void fft_matrix(void) {
    const unsigned rates[] = {16000, 48000};
    const double amplitudes[] = {32767, 16384, 4096};
    const int dc_values[] = {-32768, -16384, -1, 0, 1, 16384, 32767};
    for (unsigned rate = 0; rate < 2; ++rate) {
        for (unsigned size = 256; size <= 8192; size *= 2) {
            for (unsigned window = 0; window < SPECTRUM_DSP_WINDOW_COUNT; ++window) {
                spectrum_dsp_config config = spectrum_dsp_defaults();
                config.sample_rate = rates[rate]; config.high_hz = (uint16_t)(rates[rate] / 2); config.fft_size = (uint16_t)size;
                config.window = (spectrum_dsp_window)window; config.gain_db = 0;
                assert(spectrum_dsp_init(&state, &config));
                int64_t sum = 0;
                for (unsigned i = 0; i < size; ++i) {
                    int32_t w = spectrum_dsp_window_at(config.window, i, size);
                    assert(w == spectrum_dsp_window_at(config.window, size - 1u - i, size));
                    assert(fabs(w / 1048576.0 - reference_window(config.window, i, size)) < .00009);
                    sum += w;
                }
                assert(llabs(sum / (int64_t)size - state.coherent_gain_q20) <= 1);
                const unsigned bins[] = {5u, size / 8u, size / 4u, size / 2u - 5u};
                for (unsigned b = 0; b < 4; ++b) {
                    for (unsigned a = 0; a < 3; ++a) {
                        tone(bins[b], amplitudes[a], .127);
                        assert(state.peak_bin == bins[b]);
                        check_db(bins[b], amplitudes[a], .025);
                    }
                }
                for (unsigned d = 0; d < sizeof(dc_values)/sizeof(dc_values[0]); ++d) {
                    for (unsigned i = 0; i < size; ++i) pcm[i] = (int16_t)dc_values[d];
                    feed();
                    if (dc_values[d]) check_db(0, abs(dc_values[d]), dc_values[d] == 1 || dc_values[d] == -1 ? .35 : .025);
                    else for (unsigned i = 0; i <= size/2; ++i) assert(state.amplitude_q24[i] == 0);
                }
                for (unsigned i = 0; i < size; ++i) pcm[i] = i & 1u ? -16384 : 16384;
                feed(); check_db(size / 2u, 16384, .025);
                for (unsigned i = 0; i < size; ++i) pcm[i] = i & 1u ? INT16_MIN : INT16_MAX;
                feed(); check_db(size / 2u, 32767.5, .025);
                if (size == 256 && rate == 0) {
                    reference_dft();
                    tone(1, 20000, .2); reference_dft();
                    tone(127, 20000, .2); reference_dft();
                    tone(37.375, 24000, .3); reference_dft();
                    for (unsigned i = 0; i < size; ++i) pcm[i] = (int16_t)random_u32();
                    feed(); reference_dft();
                    memset(pcm, 0, sizeof(pcm)); pcm[0] = INT16_MIN;
                    feed(); reference_dft();
                }
                for (unsigned trial = 0; trial < 3; ++trial) {
                    for (unsigned i = 0; i < size; ++i) pcm[i] = (int16_t)random_u32();
                    feed();
                }
                if (size == 512 || size == 8192) {
                    const double quiet[] = {1024, 256, 64, 16, 4};
                    for (unsigned a = 0; a < 5; ++a) { tone(size / 8, quiet[a], 0); check_db(size / 8, quiet[a], quiet[a] >= 64 ? .04 : .65); }
                }
            }
        }
    }
}
static void axes_resampling_detection(void) {
    uint16_t levels[512]; int16_t db[512];
    for (unsigned rate = 16000; rate <= 48000; rate += 32000) {
        for (unsigned log = 0; log < 2; ++log) {
            spectrum_dsp_config config = spectrum_dsp_defaults();
            config.sample_rate = rate; config.high_hz = (uint16_t)(rate / 2u); config.low_hz = 0; config.log_frequency = !!log;
            config.fft_size = 8192; config.window = SPECTRUM_DSP_RECT; config.gain_db = 0;
            assert(spectrum_dsp_init(&state, &config));
            assert(spectrum_dsp_resample(&state, levels, db, 512));
            for (unsigned i = 0; i < 512; ++i) { assert(!levels[i]); assert(db[i] == -12000); }
            assert(spectrum_dsp_frequency_at(&config, 0, 512) == (log ? 10 : 0));
            assert(spectrum_dsp_frequency_at(&config, 511, 512) == rate / 2u);
            assert(spectrum_dsp_column_at(&config, 0, 512) == 0);
            assert(spectrum_dsp_column_at(&config, rate, 512) == 511);
            uint32_t previous = 0;
            for (unsigned x = 0; x < 512; ++x) {
                uint32_t f = spectrum_dsp_frequency_q16(&config, x, 512);
                double expected = log ? 10 * pow((rate / 2.0) / 10, x / 511.0) : (rate / 2.0) * x / 511;
                assert(f >= previous); previous = f;
                assert(fabs(f / 65536.0 - expected) < expected * .00003 + .0001);
                unsigned hz = spectrum_dsp_frequency_at(&config, x, 512);
                unsigned inverse = spectrum_dsp_column_at(&config, hz, 512);
                assert(abs((int)inverse - (int)x) <= (hz < 100 ? 4 : 1));
            }
            tone(768, 16384, 0);
            assert(spectrum_dsp_resample(&state, levels, db, 512));
            unsigned hz = 768 * rate / 8192;
            unsigned column = spectrum_dsp_column_at(&config, hz, 512), peak = 0;
            for (unsigned x = 0; x < 512; ++x) if (levels[x] > levels[peak]) peak = x;
            assert(abs((int)peak - (int)column) <= 1);
            assert(abs(db[peak] + 602) <= 1);
            uint16_t single_level; int16_t single_db;
            assert(spectrum_dsp_resample(&state, &single_level, &single_db, 1));
            assert(abs(single_db + 602) <= 1 && single_level == levels[peak]);
            assert(spectrum_dsp_detected(&state, hz));
            assert(!spectrum_dsp_detected(&state, hz + 1000));
            assert(!spectrum_dsp_detected(&state, rate / 2u + 1));
            assert(spectrum_dsp_db_at_hz(&state, rate / 2u + 1) == -12000);
            assert(abs(spectrum_dsp_db_at_hz(&state, hz) + 602) <= 1);
            assert(abs(spectrum_dsp_label_db(&state, hz + rate / config.fft_size) + 602) <= 1);
            for (int gain = -24; gain <= 60; gain += 3) {
                config.gain_db = (int8_t)gain;
                assert(spectrum_dsp_configure(&state, &config));
                assert(abs(spectrum_dsp_db_at_hz(&state, hz) - (-602 + gain * 100)) <= 1);
                assert(fabs(state.gain_q16 / 65536.0 - pow(10, gain / 20.0)) < pow(10, gain / 20.0) * .00003 + .00001);
                config.log_amplitude = false; assert(spectrum_dsp_configure(&state, &config));
                unsigned expected = (unsigned)fmin(32767, .5 * pow(10, gain / 20.0) * 32768);
                assert(abs((int)spectrum_dsp_level(&state, 1u << 23) - (int)expected) <= 2);
                config.log_amplitude = true;
            }
            config.gain_db = -24; config.threshold_db = -30; assert(spectrum_dsp_configure(&state, &config));
            assert(!spectrum_dsp_detected(&state, hz));
            config.threshold_db = -35; assert(spectrum_dsp_configure(&state, &config)); assert(spectrum_dsp_detected(&state, hz));
            config.low_hz = (uint16_t)(hz + 2000); config.high_hz = (uint16_t)(rate / 2u); assert(spectrum_dsp_configure(&state, &config));
            assert(spectrum_dsp_resample(&state, levels, db, 512));
            for (unsigned x = 0; x < 512; ++x) assert(levels[x] == 0);
        }
    }
    assert(!spectrum_dsp_resample(&state, levels, db, 0));
    assert(!spectrum_dsp_resample(&state, levels, db, 4097));
    assert(!spectrum_dsp_resample(&state, NULL, NULL, 512));
    assert(!spectrum_dsp_resample(NULL, levels, db, 512));
    assert(spectrum_dsp_resample(&state, levels, NULL, 512));
    assert(spectrum_dsp_resample(&state, NULL, db, 512));
}
static void invalid_and_partial(void) {
    spectrum_dsp_config config = spectrum_dsp_defaults(), invalid;
    assert(spectrum_dsp_init(&state, &config));
    for (unsigned i = 0; i < 8448; ++i) stream[i] = (int16_t)random_u32();
    partial = state;
    for (unsigned i = 0; i < 8448; i += 256) assert(spectrum_dsp_feed(&state, stream + i, 256));
    for (unsigned i = 0; i < 8448;) {
        unsigned n = 1 + random_u32() % 256;
        if (n > 8448 - i) n = 8448 - i;
        assert(spectrum_dsp_feed(&partial, stream + i, n)); i += n;
    }
    assert(memcmp(&state, &partial, sizeof(state)) == 0);
    /* Stop/discard can clear used without rewriting the derived phase state. */
    state.used = 0;
    assert(spectrum_dsp_init(&saved, &config));
    for (unsigned i = 0; i < config.fft_size; i += 256) {
        assert(spectrum_dsp_feed(&state, stream + i, 256));
        assert(spectrum_dsp_feed(&saved, stream + i, 256));
    }
    assert(memcmp(state.amplitude_q24, saved.amplitude_q24, sizeof(state.amplitude_q24)) == 0);
    state = partial;
    saved = state;
#define INVALID_CONFIG(field, value) do { invalid = config; invalid.field = (value); assert(!spectrum_dsp_configure(&state, &invalid)); assert(!spectrum_dsp_init(&state, &invalid)); assert(memcmp(&state, &saved, sizeof(state)) == 0); } while (0)
    INVALID_CONFIG(fft_size, 0); INVALID_CONFIG(fft_size, 128); INVALID_CONFIG(fft_size, 257); INVALID_CONFIG(fft_size, 16384);
    INVALID_CONFIG(sample_rate, 0); INVALID_CONFIG(sample_rate, 44100);
    INVALID_CONFIG(window, (spectrum_dsp_window)-1); INVALID_CONFIG(window, SPECTRUM_DSP_WINDOW_COUNT);
    INVALID_CONFIG(low_hz, 20000); INVALID_CONFIG(high_hz, 24001); INVALID_CONFIG(high_hz, 20);
    INVALID_CONFIG(gain_db, -25); INVALID_CONFIG(gain_db, 61); INVALID_CONFIG(threshold_db, -91); INVALID_CONFIG(threshold_db, -19);
#undef INVALID_CONFIG
    assert(!spectrum_dsp_configure(NULL, &config)); assert(!spectrum_dsp_configure(&state, NULL));
    assert(!spectrum_dsp_init(NULL, &config)); assert(!spectrum_dsp_init(&state, NULL));
    assert(!spectrum_dsp_feed(&state, stream, 257)); assert(!spectrum_dsp_feed(&state, NULL, 0)); assert(!spectrum_dsp_feed(NULL, stream, 1));
    assert(spectrum_dsp_feed(&state, stream, 0)); assert(memcmp(&state, &saved, sizeof(state)) == 0);
    state.used = config.fft_size; saved = state; assert(!spectrum_dsp_feed(&state, stream, 1)); assert(memcmp(&state, &saved, sizeof(state)) == 0);
    state = partial;
    config.gain_db = 18; config.log_frequency = false; assert(spectrum_dsp_configure(&state, &config));
    assert(state.used == partial.used && state.transforms == partial.transforms && state.has_transform);
    assert(memcmp(state.amplitude_q24, partial.amplitude_q24, sizeof(state.amplitude_q24)) == 0);
    config.fft_size = 256; assert(spectrum_dsp_configure(&state, &config)); assert(!state.used && !state.has_transform);
    for (unsigned i = 0; i < 4097; ++i) assert(state.amplitude_q24[i] == 0);
    assert(spectrum_dsp_feed(&state, stream, 256)); assert(state.has_transform);
    config.window = SPECTRUM_DSP_FLAT_TOP; assert(spectrum_dsp_configure(&state, &config)); assert(!state.has_transform);
    config.sample_rate = 16000; config.high_hz = 8000; assert(spectrum_dsp_configure(&state, &config)); assert(!state.has_transform);
    assert(spectrum_dsp_init(&state, &state.config)); assert(state.config.sample_rate == 16000 && state.config.window == SPECTRUM_DSP_FLAT_TOP);
    memset(&state, 0, sizeof(state)); assert(!spectrum_dsp_feed(&state, stream, 256));
    assert(spectrum_dsp_db_at_hz(NULL, 1000) == -12000); assert(!spectrum_dsp_detected(NULL, 1000));
}
int main(void) {
    math_helpers(); fft_matrix(); axes_resampling_detection(); invalid_and_partial();
    printf("NOVA DSP: %u frames; all 6 FFT sizes, 5 windows and 16/48 kHz; calibrated tones/DC/Nyquist, independent DFT, silence, gains/scales/ranges/detection and atomic invalid/partial reads passed. RAM %zu bytes; worst dB error %.4f dB, DFT error %.8f FS.\n", analyzed, sizeof(state), worst_db_error, worst_dft_error);
    return 0;
}
