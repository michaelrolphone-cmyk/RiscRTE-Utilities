#ifndef SPECTRUM_SIGNATURES_H
#define SPECTRUM_SIGNATURES_H
/* Canonical live-PCM signature v1: 16 kHz, 512-point symmetric Hann, 128
 * non-DC power bands (two FFT bins each, 62.5 Hz). Display FFT/window/gain
 * cannot change this identity. Mean-square power is Q30 relative to PCM FS;
 * this is not SPL, an impulse response, or proof of physical room identity. */
#include "spectrum_dsp.h"
#define SPECTRUM_SIGNATURE_FFT 512u
#define SPECTRUM_SIGNATURE_BANDS 128u
#define SPECTRUM_SIGNATURE_SLOTS 8u
#define SPECTRUM_SIGNATURE_NAME 16u
#define SPECTRUM_SIGNATURE_ROOM_FRAMES 64u
#define SPECTRUM_SIGNATURE_MAX_FRAMES 1048575u
#define SPECTRUM_SIGNATURE_POWER_MAX 1073741824u
#define SPECTRUM_SIGNATURE_RECORD_SIZE 1088u
#define SPECTRUM_SIGNATURE_FLOOR 16u
#define SPECTRUM_SIGNATURE_NO_MATCH (-1)
enum {SPECTRUM_SIGNATURE_EMPTY, SPECTRUM_SIGNATURE_ROOM, SPECTRUM_SIGNATURE_EVENT};
typedef struct {
 uint64_t sums[SPECTRUM_SIGNATURE_BANDS];
 uint32_t frames;
 uint8_t kind;
 char name[17];
} spectrum_signature;
typedef struct {
 int32_t real[SPECTRUM_SIGNATURE_FFT],imag[SPECTRUM_SIGNATURE_FFT];
 uint32_t power[SPECTRUM_SIGNATURE_BANDS];
 uint32_t transforms,window_power_q16,window_phase,window_error,phase_step,phase_remainder;
 int32_t pcm_sum;
 uint16_t used;
 bool available;
} spectrum_signature_analyzer;
typedef struct {
 uint32_t mean[SPECTRUM_SIGNATURE_BANDS];
 uint32_t frames;
 int candidate,selected;
 unsigned stable,misses,confidence,transient_frames;
 bool ambiguous;
} spectrum_room_tracker;
_Static_assert(sizeof(spectrum_signature_analyzer)<=4660,"bounded signature FFT RAM");
_Static_assert(sizeof(spectrum_signature)<=1056,"bounded profile RAM");
static inline uint64_t spectrum_signature_total(const uint32_t *v){uint64_t total=0;for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)total+=v[i];return total;}
static inline void spectrum_signature_init(spectrum_signature_analyzer *a){
 memset(a,0,sizeof(*a));uint64_t energy=0;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_FFT;i++){int32_t w=spectrum_dsp_window_at(SPECTRUM_DSP_HANN,i,SPECTRUM_SIGNATURE_FFT);energy+=(uint64_t)((int64_t)w*w);}
 a->phase_step=(uint32_t)spectrum_dsp_div_u64_u32((uint64_t)1<<32,511u);a->phase_remainder=(uint32_t)(((uint64_t)1<<32)-(uint64_t)a->phase_step*511u);a->window_error=255u;
 a->window_power_q16=(uint32_t)((energy+(1u<<23))>>24)/SPECTRUM_SIGNATURE_FFT;
}
static inline bool spectrum_signature_feed(spectrum_signature_analyzer *a,const int16_t *pcm,size_t n){
 if(!a||!pcm||n>SPECTRUM_DSP_CHUNK||a->used>=SPECTRUM_SIGNATURE_FFT||!a->window_power_q16)return false;
 if(n&&!a->used){a->window_phase=0;a->window_error=255u;a->pcm_sum=0;}
 while(n--){int32_t w=spectrum_dsp_window_phase(SPECTRUM_DSP_HANN,a->window_phase);a->pcm_sum+=*pcm;a->real[a->used++]=(int32_t)((int64_t)*pcm++*w/4096);
  a->window_phase+=a->phase_step;a->window_error+=a->phase_remainder;if(a->window_error>=511u){++a->window_phase;a->window_error-=511u;}
  if(a->used==SPECTRUM_SIGNATURE_FFT){
   /* Remove the unwindowed frame mean before the Hann transform. Merely
    * omitting bin0 would still learn DC leakage in the adjacent Hann bins. */
   int32_t mean_q8=a->pcm_sum/2;uint32_t phase=0,error=255u;
   for(unsigned i=0;i<SPECTRUM_SIGNATURE_FFT;i++){int32_t window=spectrum_dsp_window_phase(SPECTRUM_DSP_HANN,phase);a->real[i]-=(int32_t)((int64_t)mean_q8*window/1048576);phase+=a->phase_step;error+=a->phase_remainder;if(error>=511u){++phase;error-=511u;}}
   spectrum_dsp_fft(a->real,a->imag,SPECTRUM_SIGNATURE_FFT);memset(a->power,0,sizeof(a->power));
   for(unsigned k=1;k<=SPECTRUM_SIGNATURE_FFT/2;k++){int64_t re=a->real[k],im=a->imag[k];uint64_t p=spectrum_dsp_div_u64_u32((uint64_t)(re*re+im*im)*(k==SPECTRUM_SIGNATURE_FFT/2?1u:2u),a->window_power_q16);unsigned band=(k-1u)/2u;p+=a->power[band];a->power[band]=(uint32_t)(p>SPECTRUM_SIGNATURE_POWER_MAX?SPECTRUM_SIGNATURE_POWER_MAX:p);}
   a->used=0;a->pcm_sum=0;a->window_phase=0;a->window_error=255u;a->available=true;++a->transforms;
  }
 }return true;
}
static inline bool spectrum_signature_name_valid(const char *name){if(!name||!name[0]||name[0]==' ')return false;unsigned n=0;for(;n<=SPECTRUM_SIGNATURE_NAME&&name[n];n++)if((unsigned char)name[n]<32||(unsigned char)name[n]>126)return false;return n&&n<=SPECTRUM_SIGNATURE_NAME&&name[n-1]!=' ';}
static inline bool spectrum_signature_valid(const spectrum_signature *s){
 if(!s||s->kind>SPECTRUM_SIGNATURE_EVENT)return false;
 if(!s->kind){if(s->frames||s->name[0])return false;for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)if(s->sums[i])return false;return true;}
 if(!s->frames||s->frames>SPECTRUM_SIGNATURE_MAX_FRAMES||!spectrum_signature_name_valid(s->name))return false;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)if(s->sums[i]>(uint64_t)s->frames*SPECTRUM_SIGNATURE_POWER_MAX)return false;
 return true;
}
static inline void spectrum_signature_mean(const spectrum_signature *s,uint32_t *out){for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)out[i]=s->frames?(uint32_t)spectrum_dsp_div_u64_u32(s->sums[i]+s->frames/2,s->frames):0;}
/* Atomic validation before mutation; exact sums keep repeated averages unbiased. */
static inline bool spectrum_signature_add(spectrum_signature *s,const uint32_t *power){
 if(!s||!power||!s->kind||s->kind>SPECTRUM_SIGNATURE_EVENT||s->frames>=SPECTRUM_SIGNATURE_MAX_FRAMES)return false;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)if(power[i]>SPECTRUM_SIGNATURE_POWER_MAX)return false;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)s->sums[i]+=power[i];
 ++s->frames;return true;
}
static inline bool spectrum_signature_combine(spectrum_signature *s,const spectrum_signature *add){
 if(!s||!add||s->kind!=add->kind||!spectrum_signature_valid(s)||!spectrum_signature_valid(add)||add->frames>SPECTRUM_SIGNATURE_MAX_FRAMES-s->frames)return false;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)s->sums[i]+=add->sums[i];
 s->frames+=add->frames;return true;
}
/* Similarity is normalized L1 power overlap, 0..1000, with an independent
 * +/-6 dB total-level gate. Silence never identifies a room or event. */
static inline unsigned spectrum_signature_score(const uint32_t *a,const uint32_t *b){
 uint64_t ta=spectrum_signature_total(a),tb=spectrum_signature_total(b);if(ta<SPECTRUM_SIGNATURE_FLOOR||tb<SPECTRUM_SIGNATURE_FLOOR||ta>tb*4||tb>ta*4)return 0;
 unsigned shift=0;uint64_t max=ta>tb?ta:tb;while(max>UINT32_MAX){max>>=1;++shift;}
 uint32_t da=(uint32_t)(ta>>shift),db=(uint32_t)(tb>>shift);unsigned distance=0;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++){uint32_t na=(uint32_t)spectrum_dsp_div_u64_u32((uint64_t)a[i]*1000u,da),nb=(uint32_t)spectrum_dsp_div_u64_u32((uint64_t)b[i]*1000u,db);na>>=shift;nb>>=shift;distance+=na>nb?na-nb:nb-na;}
 return distance>=2000?0:1000-distance/2;
}
static inline int spectrum_signature_best(const uint32_t *power,const spectrum_signature *profiles,const uint32_t means[SPECTRUM_SIGNATURE_SLOTS][SPECTRUM_SIGNATURE_BANDS],unsigned kind,unsigned *confidence,bool *ambiguous){
 unsigned best=0,second=0;int slot=-1;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_SLOTS;i++)if(profiles[i].kind==kind&&profiles[i].frames>=(kind==SPECTRUM_SIGNATURE_ROOM?SPECTRUM_SIGNATURE_ROOM_FRAMES:1u)){unsigned score=spectrum_signature_score(power,means[i]);if(score>best){second=best;best=score;slot=(int)i;}else if(score>second)second=score;}
 *confidence=best/10;*ambiguous=best>=80*10&&best-second<80;
 return best>=80*10&&!*ambiguous?slot:-1;
}
static inline void spectrum_room_reset(spectrum_room_tracker *t){memset(t,0,sizeof(*t));t->candidate=t->selected=-1;}
static inline void spectrum_room_observe(spectrum_room_tracker *t,const uint32_t *power,const spectrum_signature *profiles,const uint32_t means[SPECTRUM_SIGNATURE_SLOTS][SPECTRUM_SIGNATURE_BANDS]){
 ++t->frames;
 /* Reject brief event bursts before they can move a learned-room filter. */
 uint64_t total=spectrum_signature_total(power),prior=spectrum_signature_total(t->mean);
 bool louder=t->frames>8&&prior>=SPECTRUM_SIGNATURE_FLOOR&&total>prior*4;
 if(louder){if(t->transient_frames<9)++t->transient_frames;}else t->transient_frames=0;
 bool transient=louder&&t->transient_frames<=8;
 if(!transient)for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)t->mean[i]=t->frames==1?power[i]:(uint32_t)(((uint64_t)t->mean[i]*31+power[i]+16)/32);
 bool ambiguous=false;unsigned confidence=0;int best=transient||total<SPECTRUM_SIGNATURE_FLOOR?-1:spectrum_signature_best(t->mean,profiles,means,SPECTRUM_SIGNATURE_ROOM,&confidence,&ambiguous);
 if(best>=0){uint64_t reference=spectrum_signature_total(means[best]);if(total>reference*4||reference>total*4)best=-1;}
 t->confidence=confidence;t->ambiguous=ambiguous;
 if(best<0){t->candidate=-1;t->stable=0;if(++t->misses>=64)t->selected=-1;return;}
 t->misses=0;if(best!=t->candidate){t->candidate=best;t->stable=1;}else if(t->stable<64)++t->stable;
 if(t->stable>=64)t->selected=best;
}
static inline uint32_t spectrum_signature_residual(uint32_t observed,uint32_t background){return observed>background?observed-background:0;}
/* Power-domain spectral subtraction at the canonical band resolution. A
 * residual/input power ratio preserves the fine-bin shape of the display FFT
 * without pretending a coarse tonal background is uniform broadband noise.
 * The gain is amplitude Q16 in [0,65536]; raw FFT bins are never changed. */
static inline void spectrum_signature_gains(const uint32_t *observed,const uint32_t *background,uint32_t gains[SPECTRUM_SIGNATURE_BANDS]){
 if(!observed||!background||!gains)return;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++){
  if(!background[i]){gains[i]=65536;continue;}
  uint32_t residual=spectrum_signature_residual(observed[i],background[i]);
  gains[i]=residual?spectrum_dsp_sqrt(spectrum_dsp_div_u64_u32((uint64_t)residual<<32,observed[i])):0;
 }
}
static inline uint32_t spectrum_signature_filtered_amplitude(uint32_t raw,unsigned bin,unsigned fft_size,const uint32_t *gains){
 if(!gains||!bin||fft_size<256||fft_size>SPECTRUM_DSP_MAX_FFT||(fft_size&(fft_size-1u))||bin>fft_size/2u)return raw;
 unsigned canonical=(bin*SPECTRUM_SIGNATURE_FFT+fft_size/2u)/fft_size;unsigned band=canonical?(canonical-1u)/2u:0;if(band>=SPECTRUM_SIGNATURE_BANDS)band=SPECTRUM_SIGNATURE_BANDS-1;
 return (uint32_t)(((uint64_t)raw*gains[band]+32768u)>>16);
}
#endif
