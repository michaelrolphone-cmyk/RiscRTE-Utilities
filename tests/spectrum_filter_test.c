#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../Apps/spectrum_signatures.h"
static spectrum_signature_analyzer a;
static spectrum_dsp_state d;
static uint32_t background[128],gains[128];
static void signal(unsigned n,unsigned amplitude,unsigned second){int16_t pcm[256];for(unsigned off=0;off<n;off+=256){for(unsigned i=0;i<256;i++)pcm[i]=(int16_t)lround(amplitude*sin(6.283185307179586*1000*(off+i)/16000)+second*sin(6.283185307179586*3500*(off+i)/16000));assert(spectrum_signature_feed(&a,pcm,256));assert(spectrum_dsp_feed(&d,pcm,256));}}
int main(void){spectrum_dsp_config c=spectrum_dsp_defaults();for(unsigned n=256;n<=8192;n*=2)for(unsigned w=0;w<5;w++){c.fft_size=n;c.window=(spectrum_dsp_window)w;spectrum_signature_init(&a);assert(spectrum_dsp_init(&d,&c));signal(n<512?512:n,10000,0);memcpy(background,a.power,sizeof(background));spectrum_signature_gains(a.power,background,gains);unsigned k=n/16;assert(spectrum_signature_filtered_amplitude(d.amplitude_q24[k],k,n,gains)==0);signal(n<512?512:n,20000,0);spectrum_signature_gains(a.power,background,gains);uint32_t kept=spectrum_signature_filtered_amplitude(d.amplitude_q24[k],k,n,gains);double ratio=(double)kept/d.amplitude_q24[k];assert(fabs(ratio-sqrt(.75))<.001);signal(n<512?512:n,10000,10000);spectrum_signature_gains(a.power,background,gains);unsigned eventbin=n*7/32;assert(spectrum_signature_filtered_amplitude(d.amplitude_q24[eventbin],eventbin,n,gains)>.999*d.amplitude_q24[eventbin]);}puts("Independent subtraction: exact learned-tone peak cancellation, predicted power residual and new-band event preservation across all6FFT/all5windows passed.");}
