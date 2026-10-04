#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../Apps/spectrum_core.h"

static spectrum_state state, partial;
static int16_t pcm[256], stream[256*5];
static uint32_t random_state=0x8f0123;
static uint32_t random_u32(void) { random_state^=random_state<<13; random_state^=random_state>>17; random_state^=random_state<<5; return random_state; }
static void bounds(void) {
    assert(state.used<256 && state.rows<=64 && state.next_row<64 && state.peak_bin<=128);
    for (unsigned i=0;i<256;++i) { assert(abs(state.real[i])<65536); assert(abs(state.imag[i])<65536); }
    for (unsigned i=0;i<112;++i) assert(spectrum_intensity(state.magnitude[i])<=15);
}
static void zero(void) { for (unsigned i=0;i<112;++i) assert(state.magnitude[i]==0); assert(state.peak_bin==0); }
static void direct_reference(void) {
    double bins[112]={0}; int32_t sum=0;
    for (unsigned i=0;i<256;++i) sum+=pcm[i];
    int32_t mean=sum/256;
    for (unsigned k=1;k<=128;++k) {
        double re=0,im=0;
        for (unsigned i=0;i<256;++i) {
            double sample=(double)(pcm[i]-mean)*spectrum_hann[i]/32768;
            double angle=2*3.14159265358979323846*k*i/256;
            re+=sample*cos(angle)/256; im-=sample*sin(angle)/256;
        }
        double magnitude=hypot(re,im); unsigned bin=(k-1)*112/128;
        if (magnitude>bins[bin]) bins[bin]=magnitude;
    }
    for (unsigned i=0;i<112;++i) assert(fabs(bins[i]-state.magnitude[i])<16.0);
}
int main(void) {
    for (uint64_t n=0;n<100000;n+=7) { uint64_t r=spectrum_sqrt(n); assert(r*r<=n && (r+1)*(r+1)>n); }
    const uint64_t square_cases[]={0,1,65535ULL*65535,65536ULL*65536,UINT64_MAX};
    const uint32_t roots[]={0,1,65535,65536,UINT32_MAX};
    for (unsigned i=0;i<5;++i) assert(spectrum_sqrt(square_cases[i])==roots[i]);
    for (unsigned i=0;i<=65535;++i) { assert(spectrum_intensity((uint16_t)i)<=15); if(i)assert(spectrum_intensity((uint16_t)i)>=spectrum_intensity((uint16_t)(i-1))); }
    assert(spectrum_feed(&state,pcm,256));zero();bounds();
    const int dc[]={-32768,-30000,-1,0,1,30000,32767};
    for (unsigned d=0;d<7;++d) { for(unsigned i=0;i<256;++i)pcm[i]=(int16_t)dc[d]; assert(spectrum_feed(&state,pcm,256));zero();bounds(); }
    for (unsigned k=1;k<128;++k) {
        for (unsigned i=0;i<256;++i) pcm[i]=(int16_t)lround(30000*sin(2*3.14159265358979323846*k*i/256));
        assert(spectrum_feed(&state,pcm,256));assert(state.peak_bin==k);bounds();
        unsigned bin=(k-1)*112/128; assert(state.magnitude[bin]>7300 && state.magnitude[bin]<7600);
        if (k%17==0) direct_reference();
    }
    for (unsigned i=0;i<256;++i)pcm[i]=i%2?INT16_MAX:INT16_MIN;
    assert(spectrum_feed(&state,pcm,256));assert(state.peak_bin==128);bounds();direct_reference();
    for (unsigned trial=0;trial<2000;++trial) {
        for (unsigned i=0;i<256;++i)pcm[i]=(int16_t)random_u32();
        assert(spectrum_feed(&state,pcm,256));bounds();if(trial<16)direct_reference();
    }
    memset(&state,0,sizeof(state));memset(&partial,0,sizeof(partial));
    for (unsigned i=0;i<1280;++i)stream[i]=(int16_t)random_u32();
    for(unsigned i=0;i<5;++i)assert(spectrum_feed(&state,stream+i*256,256));
    for(unsigned i=0;i<1280;) { unsigned n=1+random_u32()%256;if(n>1280-i)n=1280-i;assert(spectrum_feed(&partial,stream+i,n));i+=n; }
    assert(memcmp(&state,&partial,sizeof(state))==0);
    partial=state;assert(!spectrum_feed(&state,pcm,257));assert(!spectrum_feed(NULL,pcm,1));assert(!spectrum_feed(&state,NULL,1));
    assert(memcmp(&state,&partial,sizeof(state))==0);assert(spectrum_feed(&state,pcm,0));assert(memcmp(&state,&partial,sizeof(state))==0);
    state.used=256;assert(!spectrum_feed(&state,pcm,1));state.used=0;
    memset(&state,0,sizeof(state));assert(spectrum_history(&state,0)==NULL);
    for(unsigned row=0;row<200;++row) {
        for(unsigned bin=0;bin<112;++bin)state.magnitude[bin]=(uint16_t)(1u<<(2+row%13));
        spectrum_record(&state);bounds();
        for(unsigned age=0;age<state.rows;++age) { const uint8_t*r=spectrum_history(&state,age);assert(r);for(unsigned bin=0;bin<112;++bin)assert(r[bin]==1+(row-age)%13); }
    }
    assert(state.rows==64 && spectrum_history(&state,64)==NULL && spectrum_history(&state,UINT32_MAX)==NULL);
    printf("Spectrum DSP: integer FFT/DFT, 127 tones, DC/zero/full-scale, 2000 random blocks, partial assembly and bounded ring passed (%zu bytes RAM)\n",sizeof(state));
    return 0;
}
