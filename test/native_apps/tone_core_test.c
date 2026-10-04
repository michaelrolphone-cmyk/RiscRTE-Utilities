#include <assert.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/tone_core.h"
int main(void) {
    tone_state s={0};int16_t pcm[256];
    assert(!tone_configure(NULL,440,5)&&!tone_configure(&s,19,5)&&!tone_configure(&s,7001,5)&&!tone_configure(&s,440,26));
    assert(!tone_generate(&s,pcm,1));assert(tone_configure(&s,1000,25));
    for(unsigned hz=20;hz<=7000;hz++) {
        assert(tone_configure(&s,hz,25));
        assert(s.increment==(uint32_t)((((uint64_t)hz<<32)+8000u)/16000u));
    }
    assert(tone_configure(&s,1000,25));
    assert(!tone_generate(&s,pcm,0)&&!tone_generate(&s,pcm,257));
    assert(tone_generate(&s,pcm,256));
    for(unsigned i=64;i<256;i++){int expected=(int)lrint(sin(2*3.14159265358979323846*i/16)*8191);assert(abs((int)pcm[i]-expected)<=2);}
    uint32_t random=1;
    for(unsigned run=0;run<20000;run++) {
        random=random*1664525u+1013904223u;unsigned hz=20+random%6981;
        unsigned pct=random%26;uint32_t before=s.phase;
        assert(tone_configure(&s,hz,pct)&&s.phase==before);
        assert(tone_generate(&s,pcm,256));
        for(unsigned i=0;i<256;i++)assert(pcm[i]>=-8191&&pcm[i]<=8191);
    }
    assert(tone_configure(&s,440,0)&&tone_generate(&s,pcm,256));
    for(unsigned i=64;i<256;i++)assert(!pcm[i]);
    assert(tone_adjust(440,INT_MAX,20,7000)==7000&&tone_adjust(440,INT_MIN,20,7000)==20);
    puts("tone DDS frequency/phase/level/bounds randomized properties passed");
}
