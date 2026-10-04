#ifndef AUDIO_SPECTRUM_CORE_H
#define AUDIO_SPECTRUM_CORE_H
/* Fixed-size, integer-only DSP. No heap, floats, libm, ESP-DSP or runtime ABI.
 * Radix-2 FFT scales every stage by two (overall 1/N). A Hann window and
 * per-block DC removal precede the transform. Display intensity is logarithmic
 * relative PCM magnitude, not calibrated SPL or a measurement-grade dB scale.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define SPECTRUM_RATE 16000u
#define SPECTRUM_FRAMES 256u
#define SPECTRUM_BINS 112u
#define SPECTRUM_ROWS 64u
#define SPECTRUM_INTERVAL_MS 100u

typedef struct {
    int16_t pcm[SPECTRUM_FRAMES];
    int32_t real[SPECTRUM_FRAMES], imag[SPECTRUM_FRAMES];
    uint16_t magnitude[SPECTRUM_BINS];
    uint8_t history[SPECTRUM_ROWS][SPECTRUM_BINS];
    uint16_t used, next_row, rows, peak_bin;
    uint32_t transforms;
} spectrum_state;
_Static_assert(sizeof(spectrum_state) <= 11000, "Bounded spectrum RAM");

/* Q15 round(32767*sin(2*pi*k/256)), k=0..64. */
static const int16_t spectrum_sine[65] = {
    0,804,1608,2410,3212,4011,4808,5602,6393,7179,7962,8739,
    9512,10278,11039,11793,12539,13279,14010,14732,15446,16151,16846,17530,
    18204,18868,19519,20159,20787,21403,22005,22594,23170,23731,24279,24811,
    25329,25832,26319,26790,27245,27683,28105,28510,28898,29268,29621,29956,
    30273,30571,30852,31113,31356,31580,31785,31971,32137,32285,32412,32521,
    32609,32678,32728,32757,32767,
};
/* Q15 round(32767*(1-cos(2*pi*n/255))/2), n=0..255. */
static const int16_t spectrum_hann[256] = {
    0,5,20,45,80,124,179,243,317,401,495,598,
    711,833,965,1106,1257,1416,1585,1763,1949,2145,2349,2561,
    2782,3011,3249,3494,3747,4008,4276,4552,4834,5124,5421,5724,
    6034,6350,6672,7000,7334,7673,8018,8367,8722,9081,9444,9812,
    10184,10559,10938,11321,11706,12094,12485,12879,13274,13671,14070,14470,
    14872,15274,15677,16081,16484,16888,17291,17694,18096,18497,18897,19295,
    19691,20085,20477,20867,21254,21638,22019,22396,22770,23139,23505,23866,
    24223,24575,24922,25264,25601,25932,26257,26576,26889,27195,27495,27789,
    28075,28354,28626,28891,29148,29397,29638,29871,30096,30313,30521,30721,
    30912,31094,31267,31432,31587,31732,31869,31996,32114,32222,32320,32409,
    32488,32557,32617,32666,32706,32736,32756,32766,32766,32756,32736,32706,
    32666,32617,32557,32488,32409,32320,32222,32114,31996,31869,31732,31587,
    31432,31267,31094,30912,30721,30521,30313,30096,29871,29638,29397,29148,
    28891,28626,28354,28075,27789,27495,27195,26889,26576,26257,25932,25601,
    25264,24922,24575,24223,23866,23505,23139,22770,22396,22019,21638,21254,
    20867,20477,20085,19691,19295,18897,18497,18096,17694,17291,16888,16484,
    16081,15677,15274,14872,14470,14070,13671,13274,12879,12485,12094,11706,
    11321,10938,10559,10184,9812,9444,9081,8722,8367,8018,7673,7334,
    7000,6672,6350,6034,5724,5421,5124,4834,4552,4276,4008,3747,
    3494,3249,3011,2782,2561,2349,2145,1949,1763,1585,1416,1257,
    1106,965,833,711,598,495,401,317,243,179,124,80,
    45,20,5,0,
};

static inline int32_t spectrum_sin(unsigned phase) {
    phase &= 255u;
    unsigned quadrant = phase >> 6, index = phase & 63u;
    int32_t value = spectrum_sine[(quadrant & 1u) ? 64u-index : index];
    return quadrant >= 2u ? -value : value;
}
static inline uint32_t spectrum_sqrt(uint64_t value) {
    uint64_t bit = (uint64_t)1 << 62, root = 0;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= root + bit) { value -= root + bit; root = (root >> 1) + bit; }
        else root >>= 1;
        bit >>= 2;
    }
    return (uint32_t)root;
}
/* Zero remains zero. Each step doubles magnitude, with a floor that avoids
 * showing integer FFT roundoff as signal. Full-scale sine peaks near level 12. */
static inline uint8_t spectrum_intensity(uint16_t magnitude) {
    uint8_t level = 0;
    while (magnitude >= 4u && level < 15u) { ++level; magnitude >>= 1; }
    return level;
}
static inline void spectrum_analyze(spectrum_state *state) {
    int32_t sum = 0;
    for (unsigned i=0; i<SPECTRUM_FRAMES; ++i) sum += state->pcm[i];
    int32_t mean = sum / (int32_t)SPECTRUM_FRAMES;
    for (unsigned i=0; i<SPECTRUM_FRAMES; ++i) {
        unsigned reversed=0, value=i;
        for (unsigned b=0; b<8; ++b) { reversed=(reversed<<1)|(value&1u); value>>=1; }
        state->real[reversed]=(int32_t)(((int64_t)state->pcm[i]-mean)*spectrum_hann[i]/32768);
        state->imag[reversed]=0;
    }
    for (unsigned length=2; length<=SPECTRUM_FRAMES; length<<=1) {
        unsigned half=length/2;
        for (unsigned base=0; base<SPECTRUM_FRAMES; base+=length) {
            for (unsigned j=0; j<half; ++j) {
                unsigned a=base+j, b=a+half, phase=j*SPECTRUM_FRAMES/length;
                int32_t wr=spectrum_sin(phase+64), wi=-spectrum_sin(phase);
                int32_t tr=(int32_t)(((int64_t)wr*state->real[b]-(int64_t)wi*state->imag[b])/32768);
                int32_t ti=(int32_t)(((int64_t)wr*state->imag[b]+(int64_t)wi*state->real[b])/32768);
                int32_t ar=state->real[a], ai=state->imag[a];
                state->real[a]=(ar+tr)/2; state->imag[a]=(ai+ti)/2;
                state->real[b]=(ar-tr)/2; state->imag[b]=(ai-ti)/2;
            }
        }
    }
    memset(state->magnitude,0,sizeof(state->magnitude));
    uint32_t peak=0; state->peak_bin=0;
    for (unsigned k=1; k<=SPECTRUM_FRAMES/2; ++k) {
        int64_t re=state->real[k], im=state->imag[k];
        uint32_t magnitude=spectrum_sqrt((uint64_t)(re*re)+(uint64_t)(im*im));
        if (magnitude>65535u) magnitude=65535u;
        unsigned bin=(k-1)*SPECTRUM_BINS/(SPECTRUM_FRAMES/2);
        if (magnitude>state->magnitude[bin]) state->magnitude[bin]=(uint16_t)magnitude;
        if (magnitude>peak) { peak=magnitude; state->peak_bin=(uint16_t)k; }
    }
    ++state->transforms;
}
/* A successful short read is assembled across calls; a malformed read must
 * never reach this function. At most one transform is completed per call. */
static inline bool spectrum_feed(spectrum_state *state, const int16_t *pcm, size_t count) {
    if (!state || !pcm || count>SPECTRUM_FRAMES || state->used>=SPECTRUM_FRAMES) return false;
    while (count) {
        size_t n=SPECTRUM_FRAMES-state->used;
        if (n>count) n=count;
        memcpy(state->pcm+state->used,pcm,n*sizeof(*pcm));
        state->used=(uint16_t)(state->used+n); pcm+=n; count-=n;
        if (state->used==SPECTRUM_FRAMES) { spectrum_analyze(state); state->used=0; }
    }
    return true;
}
static inline void spectrum_record(spectrum_state *state) {
    for (unsigned i=0; i<SPECTRUM_BINS; ++i)
        state->history[state->next_row][i]=spectrum_intensity(state->magnitude[i]);
    state->next_row=(uint16_t)((state->next_row+1)%SPECTRUM_ROWS);
    if (state->rows<SPECTRUM_ROWS) ++state->rows;
}
/* Age zero is the newest row. White rows before enough samples exist. */
static inline const uint8_t *spectrum_history(const spectrum_state *state, unsigned age) {
    if (age>=state->rows || age>=SPECTRUM_ROWS) return NULL;
    return state->history[(state->next_row+SPECTRUM_ROWS-1-age)%SPECTRUM_ROWS];
}
#endif
