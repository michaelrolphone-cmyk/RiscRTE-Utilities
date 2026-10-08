#ifndef NOVA_RF_DSP_H
#define NOVA_RF_DSP_H
#include "rf_identity.h"
/* Full complex, FFT-shifted RF transform. Signed10-bit I/Q, each component's
 * digital full scale512. Q24 magnitude512=0dBFS; complex corners may exceed0dB.
 * No physical power calibration, mean removal, one-sided doubling, allocation,
 * or concatenation of independent hardware bursts. */
typedef struct {
 rf_capture_identity identity;
 uint32_t low_hz,high_hz;
 uint16_t fft_size;
 rf_dsp_window window;
 bool log_frequency,log_amplitude;
 int8_t gain_db,threshold_db;
} rf_dsp_config;
typedef struct {
 int32_t real[RF_DSP_MAX_FFT],imag[RF_DSP_MAX_FFT];
 uint32_t amplitude_q24[RF_DSP_MAX_FFT];
 rf_dsp_config config;
 uint32_t transforms,gain_q16;
 int32_t coherent_gain_q20;
 uint16_t peak_bin,clipped_pairs;
 bool initialized,has_transform;
} rf_dsp_state;
_Static_assert(sizeof(rf_dsp_state)<99000u,"RF display FFT RAM bound");
static inline int16_t rf_dsp_unpack(uint32_t word,unsigned shift){unsigned x=(word>>shift)&1023u;return (int16_t)(x>=512u?(int)x-1024:(int)x);}
static inline bool rf_dsp_size_valid(unsigned n){return n>=256u&&n<=8192u&&!(n&(n-1u));}
static inline rf_dsp_config rf_dsp_defaults(void){rf_capture_identity i=rf_identity_default();return (rf_dsp_config){i,2400000000u,2480000000u,256u,RF_DSP_HANN,false,true,0,-60};}
static inline bool rf_dsp_config_valid(const rf_dsp_config *c){return c&&rf_identity_valid(&c->identity)&&rf_dsp_size_valid(c->fft_size)&&(unsigned)c->window<RF_DSP_WINDOW_COUNT&&c->low_hz>=rf_identity_low_hz(&c->identity)&&c->high_hz<=rf_identity_high_hz(&c->identity)&&c->low_hz<c->high_hz&&(!c->log_frequency||c->low_hz)&&c->gain_db>=-24&&c->gain_db<=60&&c->threshold_db>=-90&&c->threshold_db<=-20;}
static inline bool rf_dsp_configure(rf_dsp_state *s,const rf_dsp_config *c){
 if(!s||!rf_dsp_config_valid(c))return false;
 bool reset=!s->initialized||s->config.fft_size!=c->fft_size||s->config.window!=c->window||!rf_identity_equal(&s->config.identity,&c->identity);
 s->config=*c;s->gain_q16=rf_dsp_exp2_q16((int32_t)c->gain_db*10885294/1000);
 if(reset){int64_t sum=0;for(unsigned i=0;i<c->fft_size;i++)sum+=rf_dsp_window_at(c->window,i,c->fft_size);s->coherent_gain_q20=(int32_t)rf_dsp_div_u64_u32((uint64_t)sum+c->fft_size/2u,c->fft_size);s->has_transform=false;s->peak_bin=s->clipped_pairs=0;memset(s->amplitude_q24,0,sizeof(s->amplitude_q24));}
 s->initialized=true;return true;
}
static inline bool rf_dsp_init(rf_dsp_state *s,const rf_dsp_config *c){if(!s||!rf_dsp_config_valid(c))return false;rf_dsp_config saved=*c;memset(s,0,sizeof(*s));return rf_dsp_configure(s,&saved);}
/* Both quadratures participate in bit reversal; never zero the input Q. */
static inline void rf_dsp_fft(int32_t *re,int32_t *im,unsigned n){
 for(unsigned i=1,j=0;i<n;i++){unsigned bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){int32_t t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}
 for(unsigned length=2;length<=n;length<<=1){unsigned half=length/2u,step=65536u/length;for(unsigned base=0;base<n;base+=length)for(unsigned j=0;j<half;j++){unsigned a=base+j,b=a+half;int32_t wr=rf_dsp_sin(j*step+16384u),wi=-rf_dsp_sin(j*step);int32_t tr=(int32_t)(((int64_t)wr*re[b]-(int64_t)wi*im[b])/1073741824LL),ti=(int32_t)(((int64_t)wr*im[b]+(int64_t)wi*re[b])/1073741824LL);int32_t ar=re[a],ai=im[a];re[a]=(ar+tr)/2;im[a]=(ai+ti)/2;re[b]=(ar-tr)/2;im[b]=(ai-ti)/2;}}
}
static inline bool rf_dsp_burst(rf_dsp_state *s,const uint32_t *pairs,size_t count){
 if(!s||!pairs||!s->initialized||!rf_dsp_config_valid(&s->config)||count!=s->config.fft_size||s->coherent_gain_q20<=0)return false;
 unsigned n=s->config.fft_size;s->clipped_pairs=0;
 for(unsigned i=0;i<n;i++){int32_t a=rf_dsp_unpack(pairs[i],0),b=rf_dsp_unpack(pairs[i],10),w=rf_dsp_window_at(s->config.window,i,n);s->clipped_pairs+=(a==-512||a==511||b==-512||b==511);s->real[i]=(int32_t)((int64_t)a*w/64);s->imag[i]=(int32_t)((int64_t)b*w/64);}
 rf_dsp_fft(s->real,s->imag,n);uint32_t peak=0;s->peak_bin=0;
 for(unsigned k=0;k<n;k++){unsigned source=(k+n/2u)&(n-1u);int64_t re=s->real[source],im=s->imag[source];uint32_t magnitude=rf_dsp_sqrt((uint64_t)(re*re)+(uint64_t)(im*im));uint32_t amplitude=(uint32_t)rf_dsp_div_u64_u32((uint64_t)magnitude*2u*1048576u+(uint32_t)s->coherent_gain_q20/2u,(uint32_t)s->coherent_gain_q20);s->amplitude_q24[k]=amplitude;if(amplitude>peak){peak=amplitude;s->peak_bin=(uint16_t)k;}}
 ++s->transforms;s->has_transform=true;return true;
}
static inline uint32_t rf_dsp_bin_hz(const rf_dsp_config *c,unsigned bin){if(!c||bin>=c->fft_size)return 0;return rf_identity_low_hz(&c->identity)+(uint32_t)rf_dsp_div_u64_u32((uint64_t)bin*c->identity.sample_rate_hz+c->fft_size/2u,c->fft_size);}
/* Absolute-frequency logarithmic interpolation via geometric bisection keeps
 * sub-MHz precision near GHz without overflowing a Q16-Hz uint32. */
static inline uint32_t rf_dsp_frequency_at(const rf_dsp_config *c,unsigned x,unsigned width){
 if(!rf_dsp_config_valid(c))return 0;
 if(width<2||!x)return c->low_hz;
 if(x>=width-1u)return c->high_hz;
 if(!c->log_frequency)return c->low_hz+(uint32_t)rf_dsp_div_u64_u32((uint64_t)(c->high_hz-c->low_hz)*x,width-1u);
 uint32_t lo=c->low_hz,hi=c->high_hz,fraction=(uint32_t)rf_dsp_div_u64_u32((uint64_t)x<<30,width-1u);
 for(unsigned bit=0;bit<30&&hi-lo>1u;bit++){uint32_t mid=rf_dsp_sqrt((uint64_t)lo*hi);if(fraction&(1u<<(29u-bit)))lo=mid;else hi=mid;}
 return lo+(hi-lo)/2u;
}
static inline unsigned rf_dsp_column_at(const rf_dsp_config *c,uint32_t hz,unsigned width){if(!rf_dsp_config_valid(c)||width<2)return 0;if(hz<=c->low_hz)return 0;if(hz>=c->high_hz)return width-1u;unsigned lo=0,hi=width-1u;while(hi-lo>1u){unsigned mid=lo+(hi-lo)/2u;if(rf_dsp_frequency_at(c,mid,width)<hz)lo=mid;else hi=mid;}uint32_t a=rf_dsp_frequency_at(c,lo,width),b=rf_dsp_frequency_at(c,hi,width);return hz-a<=b-hz?lo:hi;}
static inline uint16_t rf_dsp_level(const rf_dsp_state *s,uint32_t amplitude){if(!s)return 0;if(s->config.log_amplitude){int32_t db=rf_dsp_amplitude_db(amplitude,s->config.gain_db);if(db<=RF_DSP_DISPLAY_FLOOR_DB)return 0;if(db>=0)return 32767;return (uint16_t)((db-RF_DSP_DISPLAY_FLOOR_DB)*32767/-RF_DSP_DISPLAY_FLOOR_DB);}uint64_t level=(uint64_t)amplitude*s->gain_q16/33554432u;return level>=32767?32767:(uint16_t)level;}
static inline uint32_t rf_dsp_amplitude_values_at(const rf_dsp_state *s,const uint32_t *v,uint32_t hz){if(!s||!v||!s->has_transform||hz<rf_identity_low_hz(&s->config.identity)||hz>rf_identity_high_hz(&s->config.identity))return 0;uint64_t bin=rf_dsp_div_u64_u32((uint64_t)(hz-rf_identity_low_hz(&s->config.identity))*s->config.fft_size*65536u,s->config.identity.sample_rate_hz);unsigned k=(unsigned)(bin>>16),f=(unsigned)(bin&65535u);if(k>=s->config.fft_size-1u)return v[s->config.fft_size-1u];return (uint32_t)(((uint64_t)v[k]*(65536u-f)+(uint64_t)v[k+1u]*f+32768u)>>16);}
static inline int16_t rf_dsp_db_at_hz(const rf_dsp_state *s,uint32_t hz){return rf_dsp_amplitude_db(rf_dsp_amplitude_values_at(s,s?s->amplitude_q24:NULL,hz),s?s->config.gain_db:0);}
/* Label tolerance is explicit absolute Hz or FFT bins. No percent-of-carrier
 * allowance:3% of2.4GHz would incorrectly match most of the receiver span. */
static inline int16_t rf_dsp_label_db(const rf_dsp_state *s,uint32_t hz,uint32_t tolerance_hz,unsigned tolerance_bins){
 if(!s||!s->has_transform||tolerance_bins>8192u)return RF_DSP_FLOOR_DB;
 uint64_t span=rf_dsp_div_u64_u32((uint64_t)tolerance_bins*s->config.identity.sample_rate_hz+s->config.fft_size-1u,s->config.fft_size);if(span<tolerance_hz)span=tolerance_hz;
 uint32_t peak=0;for(unsigned k=0;k<s->config.fft_size;k++){uint32_t f=rf_dsp_bin_hz(&s->config,k);uint64_t difference=f>hz?(uint64_t)f-hz:(uint64_t)hz-f;if(difference<=span&&s->amplitude_q24[k]>peak)peak=s->amplitude_q24[k];}return rf_dsp_amplitude_db(peak,s->config.gain_db);
}
static inline bool rf_dsp_resample_values(const rf_dsp_state *s,const uint32_t *v,uint16_t *levels,int16_t *db,unsigned width){
 if(!s||!v||!s->initialized||!rf_dsp_config_valid(&s->config)||!width||width>4096u||(!levels&&!db))return false;
 for(unsigned x=0;x<width;x++){uint32_t center=rf_dsp_frequency_at(&s->config,x,width),left=x?rf_dsp_frequency_at(&s->config,x-1u,width):center,right=x+1u<width?rf_dsp_frequency_at(&s->config,x+1u,width):center;uint32_t lo=width==1?s->config.low_hz:left+(center-left)/2u,hi=width==1?s->config.high_hz:center+(right-center)/2u;uint32_t amplitude=rf_dsp_amplitude_values_at(s,v,center);unsigned first=(unsigned)rf_dsp_div_u64_u32((uint64_t)(lo-rf_identity_low_hz(&s->config.identity))*s->config.fft_size+s->config.identity.sample_rate_hz-1u,s->config.identity.sample_rate_hz),last=(unsigned)rf_dsp_div_u64_u32((uint64_t)(hi-rf_identity_low_hz(&s->config.identity))*s->config.fft_size,s->config.identity.sample_rate_hz);if(last>=s->config.fft_size)last=s->config.fft_size-1u;if(s->has_transform)for(unsigned k=first;k<=last;k++)if(v[k]>amplitude)amplitude=v[k];if(levels)levels[x]=rf_dsp_level(s,amplitude);if(db)db[x]=rf_dsp_amplitude_db(amplitude,s->config.gain_db);}
 return true;
}
static inline bool rf_dsp_resample(const rf_dsp_state *s,uint16_t *levels,int16_t *db,unsigned width){return rf_dsp_resample_values(s,s?s->amplitude_q24:NULL,levels,db,width);}
#endif
