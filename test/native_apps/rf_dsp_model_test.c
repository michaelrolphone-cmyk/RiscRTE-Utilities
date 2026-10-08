#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/rf_dsp.h"
#include "../../Apps/rf_signatures.h"
static rf_dsp_state state;
static rf_signature_analyzer canonical;
static uint32_t pairs[8192];
static uint32_t pack(int i,int q){return ((uint32_t)i&1023u)|(((uint32_t)q&1023u)<<10);}
static void tone(unsigned n,int bin,double amplitude){for(unsigned j=0;j<n;j++){double phase=6.2831853071795864769*bin*j/n;pairs[j]=pack((int)lround(amplitude*cos(phase)),(int)lround(amplitude*sin(phase)));}}
static void complex_sidebands(void){
 rf_dsp_config c=rf_dsp_defaults();c.window=RF_DSP_RECT;assert(rf_dsp_init(&state,&c));
 assert(rf_dsp_unpack(pack(-512,511),0)==-512&&rf_dsp_unpack(pack(-512,511),10)==511);
 tone(256,17,256);assert(rf_dsp_burst(&state,pairs,256));assert(state.peak_bin==145);assert(abs(rf_dsp_amplitude_db(state.amplitude_q24[145],0)+602)<4);assert(state.amplitude_q24[111]<RF_DSP_FULL_SCALE/1000u);
 tone(256,-17,256);assert(rf_dsp_burst(&state,pairs,256));assert(state.peak_bin==111);assert(state.amplitude_q24[145]<RF_DSP_FULL_SCALE/1000u);
 for(unsigned i=0;i<256;i++)pairs[i]=pack(256,0);
 assert(rf_dsp_burst(&state,pairs,256));assert(state.peak_bin==128&&state.amplitude_q24[128]==RF_DSP_FULL_SCALE/2u);assert(rf_dsp_bin_hz(&c,128)==2440000000u);
 for(unsigned i=0;i<256;i++)pairs[i]=pack(-512,-512);
 assert(rf_dsp_burst(&state,pairs,256));assert(state.clipped_pairs==256);assert(abs(rf_dsp_amplitude_db(state.amplitude_q24[128],0)-301)<3);
 for(unsigned i=0;i<256;i++)pairs[i]=0;
 assert(rf_dsp_burst(&state,pairs,256));assert(rf_dsp_db_at_hz(&state,c.identity.lo_hz)==-12000);
 unsigned before=state.transforms;assert(!rf_dsp_burst(&state,pairs,128));assert(!rf_dsp_burst(&state,pairs,512));assert(state.transforms==before);
}
static void windows_and_sizes(void){
 rf_dsp_config c=rf_dsp_defaults();
 for(unsigned n=256;n<=8192;n*=2){c.fft_size=(uint16_t)n;for(unsigned w=0;w<RF_DSP_WINDOW_COUNT;w++){c.window=(rf_dsp_window)w;assert(rf_dsp_init(&state,&c));tone(n,-17,300);assert(rf_dsp_burst(&state,pairs,n));assert(state.peak_bin==n/2u-17u);int db=rf_dsp_amplitude_db(state.amplitude_q24[state.peak_bin],0);assert(abs(db+464)<4);assert(state.clipped_pairs==0);}}
 assert(rf_dsp_window_at(RF_DSP_FLAT_TOP,16,256)<0);
 c.fft_size=511;rf_dsp_config old=state.config;assert(!rf_dsp_configure(&state,&c));assert(!memcmp(&old,&state.config,sizeof(old)));
}
static void canonical_independent(void){
 rf_capture_identity identity=rf_identity_default();rf_signature_init(&canonical);tone(8192,256,320);assert(rf_signature_burst(&canonical,&identity,pairs,8192));uint32_t original[128];memcpy(original,canonical.power,sizeof(original));uint64_t total=rf_signature_total(original);assert(total>410000000u&&total<425000000u);assert(original[68]>original[60]*100u);
 rf_dsp_config c=rf_dsp_defaults();for(unsigned w=0;w<5;w++){c.window=(rf_dsp_window)w;c.gain_db=(int8_t)(w*15);c.fft_size=(uint16_t)(256u<<w);assert(rf_dsp_init(&state,&c));assert(rf_dsp_burst(&state,pairs,c.fft_size));assert(rf_signature_burst(&canonical,&identity,pairs,8192));assert(!memcmp(original,canonical.power,sizeof(original)));}
 /* DC is legitimate RF evidence, including LO leakage, and stays in signature. */
 for(unsigned i=0;i<256;i++)pairs[i]=pack(-512,-512);
 assert(rf_signature_burst(&canonical,&identity,pairs,256));total=rf_signature_total(canonical.power);assert(total>2100000000u&&total<2200000000ULL);assert(canonical.power[64]>1000000000u);
 uint32_t transforms=canonical.transforms;assert(!rf_signature_burst(&canonical,&identity,pairs,255));assert(canonical.transforms==transforms);
}
static void axes_and_display(void){
 rf_dsp_config c=rf_dsp_defaults();c.fft_size=8192;c.window=RF_DSP_RECT;assert(rf_dsp_init(&state,&c));tone(8192,31,256);assert(rf_dsp_burst(&state,pairs,8192));uint32_t peak_hz=rf_dsp_bin_hz(&c,state.peak_bin);assert(peak_hz>INT32_MAX);
 uint32_t raw=state.amplitude_q24[state.peak_bin];int16_t before=rf_dsp_db_at_hz(&state,peak_hz);c.gain_db=12;assert(rf_dsp_configure(&state,&c));assert(state.has_transform&&state.amplitude_q24[state.peak_bin]==raw);assert(rf_dsp_db_at_hz(&state,peak_hz)==before+1200);
 for(unsigned log=0;log<2;log++){c.log_frequency=!!log;assert(rf_dsp_configure(&state,&c));assert(rf_dsp_frequency_at(&c,0,320)==2400000000u);assert(rf_dsp_frequency_at(&c,319,320)==2480000000u);for(unsigned x=1;x<320;x++){uint32_t hz=rf_dsp_frequency_at(&c,x,320);assert(hz>rf_dsp_frequency_at(&c,x-1u,320));assert(rf_dsp_column_at(&c,hz,320)==x);}}
 c.log_frequency=true;uint32_t middle=rf_dsp_frequency_at(&c,1,3);double exact=sqrt((double)c.low_hz*c.high_hz);assert(fabs(middle-exact)<32.0);
 uint16_t levels[64];int16_t db[64];assert(rf_dsp_resample(&state,levels,db,64));int best=-12000;for(unsigned i=0;i<64;i++)if(db[i]>best)best=db[i];assert(best>500);
 assert(rf_dsp_label_db(&state,peak_hz,1000,1)>500);assert(rf_dsp_label_db(&state,peak_hz+10000000u,1000,1)<-6000);
 c.log_amplitude=false;assert(rf_dsp_configure(&state,&c));assert(rf_dsp_level(&state,raw)==32767);
 c.gain_db=0;assert(rf_dsp_configure(&state,&c));assert(abs((int)rf_dsp_level(&state,raw)-16384)<8);
 for(unsigned b=0;b<128;b++){uint32_t hz=rf_signature_band_hz(&c.identity,b);uint16_t coord=rf_hz_coord(&c.identity,hz);assert(coord);assert(llabs((long long)rf_coord_hz(&c.identity,coord)-hz)<2000);}
}
int main(void){complex_sidebands();windows_and_sizes();canonical_independent();axes_and_display();puts("RF complex IQ DSP, both sidebands/DC/clipping, all windows/sizes and canonical invariance passed");}
