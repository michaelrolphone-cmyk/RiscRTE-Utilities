#ifndef WATERFALL_CORE_H
#define WATERFALL_CORE_H
/* One 256-pair burst becomes one scrolled waterfall row.
 * This is not a continuous 80 MSa/s display. The watch panel is updated
 * between bursts. */
#include <stddef.h>
#include <stdint.h>

#define WATERFALL_FFT 256
#define WATERFALL_BINS (WATERFALL_FFT / 2)
#define WATERFALL_WIDTH 240
#define WATERFALL_ROWS 200

/* Portable adapter fill_rounded_rect_tone colors, written into the RGB565 frame.
 * Index 0 is white (loud) through 3 black (quiet). */
static const uint16_t waterfall_rgb565[4] = {0xffffu, 0xbdf7u, 0x630cu, 0x0000u};

typedef struct {
    int16_t i;
    int16_t q;
} waterfall_iq;

typedef struct {
    int32_t i;
    int32_t q;
} waterfall_bin;

typedef struct {
    uint8_t tone[WATERFALL_ROWS][WATERFALL_WIDTH];
    uint32_t rows_pushed;
} waterfall_image;

static const int16_t waterfall_sin_q14[WATERFALL_FFT] = {
    0,402,804,1205,1606,2006,2404,2801,3196,3590,3981,4370,4756,5139,5520,5897,
    6270,6639,7005,7366,7723,8076,8423,8765,9102,9434,9760,10080,10394,10702,11003,11297,
    11585,11866,12140,12406,12665,12916,13160,13395,13623,13842,14053,14256,14449,14635,14811,14978,
    15137,15286,15426,15557,15679,15791,15893,15986,16069,16143,16207,16261,16305,16340,16364,16379,
    16384,16379,16364,16340,16305,16261,16207,16143,16069,15986,15893,15791,15679,15557,15426,15286,
    15137,14978,14811,14635,14449,14256,14053,13842,13623,13395,13160,12916,12665,12406,12140,11866,
    11585,11297,11003,10702,10394,10080,9760,9434,9102,8765,8423,8076,7723,7366,7005,6639,
    6270,5897,5520,5139,4756,4370,3981,3590,3196,2801,2404,2006,1606,1205,804,402,
    0,-402,-804,-1205,-1606,-2006,-2404,-2801,-3196,-3590,-3981,-4370,-4756,-5139,-5520,-5897,
    -6270,-6639,-7005,-7366,-7723,-8076,-8423,-8765,-9102,-9434,-9760,-10080,-10394,-10702,-11003,-11297,
    -11585,-11866,-12140,-12406,-12665,-12916,-13160,-13395,-13623,-13842,-14053,-14256,-14449,-14635,-14811,-14978,
    -15137,-15286,-15426,-15557,-15679,-15791,-15893,-15986,-16069,-16143,-16207,-16261,-16305,-16340,-16364,-16379,
    -16384,-16379,-16364,-16340,-16305,-16261,-16207,-16143,-16069,-15986,-15893,-15791,-15679,-15557,-15426,-15286,
    -15137,-14978,-14811,-14635,-14449,-14256,-14053,-13842,-13623,-13395,-13160,-12916,-12665,-12406,-12140,-11866,
    -11585,-11297,-11003,-10702,-10394,-10080,-9760,-9434,-9102,-8765,-8423,-8076,-7723,-7366,-7005,-6639,
    -6270,-5897,-5520,-5139,-4756,-4370,-3981,-3590,-3196,-2801,-2404,-2006,-1606,-1205,-804,-402,
};

static inline int16_t waterfall_sext10(uint32_t word, unsigned shift) {
    int32_t value = (int32_t)((word >> shift) & 1023u);
    if (value & 512) value -= 1024;
    return (int16_t)value;
}

/* Published eSpDR packing: I in bits 0-9, Q in bits 10-19, signed 10-bit. */
static inline waterfall_iq waterfall_unpack_pair(uint32_t word) {
    waterfall_iq sample;
    sample.i = waterfall_sext10(word, 0);
    sample.q = waterfall_sext10(word, 10);
    return sample;
}

static inline int32_t waterfall_abs32(int32_t value) {
    return value < 0 ? -value : value;
}

static inline int32_t waterfall_magnitude(int32_t i, int32_t q) {
    i = waterfall_abs32(i);
    q = waterfall_abs32(q);
    return i > q ? i + q / 2 : q + i / 2;
}

static void waterfall_fft(waterfall_bin *samples) {
    for (unsigned i = 1, j = 0; i < WATERFALL_FFT; ++i) {
        unsigned bit = WATERFALL_FFT >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            waterfall_bin swap = samples[i];
            samples[i] = samples[j];
            samples[j] = swap;
        }
    }
    for (unsigned length = 2; length <= WATERFALL_FFT; length <<= 1) {
        unsigned half = length >> 1;
        unsigned step = WATERFALL_FFT / length;
        for (unsigned start = 0; start < WATERFALL_FFT; start += length) {
            for (unsigned k = 0; k < half; ++k) {
                unsigned index = (k * step) & (WATERFALL_FFT - 1);
                int32_t wr = waterfall_sin_q14[(index + WATERFALL_FFT / 4) & (WATERFALL_FFT - 1)];
                int32_t wi = -((int32_t)waterfall_sin_q14[index]);
                waterfall_bin even = samples[start + k];
                waterfall_bin odd = samples[start + k + half];
                int32_t tr = (int32_t)(((int64_t)odd.i * wr - (int64_t)odd.q * wi) >> 14);
                int32_t tq = (int32_t)(((int64_t)odd.i * wi + (int64_t)odd.q * wr) >> 14);
                samples[start + k].i = even.i + tr;
                samples[start + k].q = even.q + tq;
                samples[start + k + half].i = even.i - tr;
                samples[start + k + half].q = even.q - tq;
            }
        }
    }
}

/* level 0 quiet .. 3 loud, then inverted into the adapter tone index. */
static uint8_t waterfall_tone(int32_t magnitude, int32_t peak) {
    if (peak < 1) peak = 1;
    if (magnitude < 0) magnitude = 0;
    if (magnitude > peak) magnitude = peak;
    unsigned level = (unsigned)((magnitude * 4) / peak);
    if (level > 3) level = 3;
    return (uint8_t)(3u - level);
}

static void waterfall_clear(waterfall_image *image) {
    for (unsigned row = 0; row < WATERFALL_ROWS; ++row)
        for (unsigned column = 0; column < WATERFALL_WIDTH; ++column)
            image->tone[row][column] = 3;
    image->rows_pushed = 0;
}

/* Positive-frequency bins 1..127 stretched across 240 columns. Newest row is 0. */
static void waterfall_push(waterfall_image *image, const waterfall_bin *bins) {
    int32_t column_mag[WATERFALL_WIDTH];
    int32_t peak = 1;
    for (unsigned x = 0; x < WATERFALL_WIDTH; ++x) {
        unsigned bin = 1u + (x * (WATERFALL_BINS - 2u)) / (WATERFALL_WIDTH - 1u);
        int32_t magnitude = waterfall_magnitude(bins[bin].i, bins[bin].q);
        column_mag[x] = magnitude;
        if (magnitude > peak) peak = magnitude;
    }
    for (unsigned row = WATERFALL_ROWS - 1; row > 0; --row)
        for (unsigned column = 0; column < WATERFALL_WIDTH; ++column)
            image->tone[row][column] = image->tone[row - 1][column];
    for (unsigned column = 0; column < WATERFALL_WIDTH; ++column)
        image->tone[0][column] = waterfall_tone(column_mag[column], peak);
    image->rows_pushed++;
}

static void waterfall_spectrum(const waterfall_iq *input, waterfall_image *image) {
    waterfall_bin bins[WATERFALL_FFT];
    for (unsigned n = 0; n < WATERFALL_FFT; ++n) {
        bins[n].i = input[n].i;
        bins[n].q = input[n].q;
    }
    waterfall_fft(bins);
    waterfall_push(image, bins);
}
#endif
