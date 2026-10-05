#ifndef UTILITIES_TONE_CORE_H
#define UTILITIES_TONE_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TONE_RATE 16000u
#define TONE_MIN_HZ 20u
#define TONE_MAX_HZ 7000u
#define TONE_MAX_PERCENT 100u
#define TONE_FRAMES 256u
/* Source-owned bounded DDS. The table is round(32767*sin(2*pi*n/256)).
 * Linear interpolation and a short gain ramp preserve phase across edits.
 * No libm imports, allocation, task, audio device or platform policy. */
static const int16_t tone_sine[256]={
    0,804,1608,2410,3212,4011,4808,5602,6393,7179,7962,8739,9512,10278,11039,11793,
    12539,13279,14010,14732,15446,16151,16846,17530,18204,18868,19519,20159,20787,21403,22005,22594,
    23170,23731,24279,24811,25329,25832,26319,26790,27245,27683,28105,28510,28898,29268,29621,29956,
    30273,30571,30852,31113,31356,31580,31785,31971,32137,32285,32412,32521,32609,32678,32728,32757,
    32767,32757,32728,32678,32609,32521,32412,32285,32137,31971,31785,31580,31356,31113,30852,30571,
    30273,29956,29621,29268,28898,28510,28105,27683,27245,26790,26319,25832,25329,24811,24279,23731,
    23170,22594,22005,21403,20787,20159,19519,18868,18204,17530,16846,16151,15446,14732,14010,13279,
    12539,11793,11039,10278,9512,8739,7962,7179,6393,5602,4808,4011,3212,2410,1608,804,
    0,-804,-1608,-2410,-3212,-4011,-4808,-5602,-6393,-7179,-7962,-8739,-9512,-10278,-11039,-11793,
    -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
    -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
    -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
    -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,-32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
    -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
    -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
    -12539,-11793,-11039,-10278,-9512,-8739,-7962,-7179,-6393,-5602,-4808,-4011,-3212,-2410,-1608,-804,
};
typedef struct { uint32_t phase,increment; uint16_t gain,target; } tone_state;
static inline bool tone_configure(tone_state *s,unsigned hz,unsigned percent) {
    if(!s||hz<TONE_MIN_HZ||hz>TONE_MAX_HZ||percent>TONE_MAX_PERCENT)return false;
    /* For the fixed16000Hz rate: 2^32 / 16000 = 268435 + 456/1000.
     * Exact nearest-integer DDS increment without 64-bit division helpers.
     * All terms fit uint32 at the checked7000Hz limit. */
    s->increment=hz*268435u+(hz*456u+500u)/1000u;
    s->target=(uint16_t)(percent*32767u/100u);return true;
}
static inline bool tone_generate(tone_state *s,int16_t *pcm,size_t frames) {
    if(!s||!pcm||!frames||frames>TONE_FRAMES||!s->increment||s->gain>32767||s->target>32767)return false;
    for(size_t i=0;i<frames;i++) {
        /* Keep the original ~4 ms gain ramp at the expanded 0-100% range. */
        if(s->gain<s->target)s->gain=(uint16_t)(s->target-s->gain>512?s->gain+512:s->target);
        else if(s->gain>s->target)s->gain=(uint16_t)(s->gain-s->target>512?s->gain-512:s->target);
        unsigned index=s->phase>>24;int32_t first=tone_sine[index];
        int32_t delta=(int32_t)tone_sine[(index+1)&255u]-first;
        int32_t wave=first+delta*(int32_t)((s->phase>>8)&65535u)/65536;
        pcm[i]=(int16_t)(wave*s->gain/32767);s->phase+=s->increment;
    }
    return true;
}
static inline unsigned tone_adjust(unsigned value,int delta,unsigned minimum,unsigned maximum) {
    int64_t next=(int64_t)value+delta;
    return next<(int64_t)minimum?minimum:next>(int64_t)maximum?maximum:(unsigned)next;
}
#endif
