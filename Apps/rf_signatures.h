#ifndef NOVA_RF_SIGNATURES_H
#define NOVA_RF_SIGNATURES_H
#include "rf_dsp.h"
/* RF canonical algorithm1: first256 coherent signed10-bit complex pairs,
 * symmetric Hann,128 linear two-bin FFT-shifted bands including DC and both
 * sidebands. Q30 mean-square digital magnitude relative to512; no dBm claim.
 * Receiver identity is independent from all display options. */
#define RF_SIGNATURE_FFT 256u
#define RF_SIGNATURE_BANDS 128u
#define RF_SIGNATURE_SLOTS 8u
#define RF_SIGNATURE_NAME 16u
#define RF_SIGNATURE_ROOM_FRAMES 64u
#define RF_SIGNATURE_MAX_FRAMES 1048575u
#define RF_SIGNATURE_POWER_MAX 2147483648u
#define RF_SIGNATURE_RECORD_SIZE 1124u
#define RF_SIGNATURE_FLOOR 16u
#define RF_SIGNATURE_NO_MATCH (-1)
enum {RF_SIGNATURE_EMPTY,RF_SIGNATURE_ROOM,RF_SIGNATURE_EVENT};
typedef struct {uint64_t sums[128];uint32_t frames;uint8_t kind;char name[17];rf_capture_identity identity;} rf_signature;
typedef struct {int32_t real[256],imag[256];uint32_t power[128],transforms,window_power_q16;rf_capture_identity identity;bool available;} rf_signature_analyzer;
typedef struct {uint32_t mean[128],frames;int candidate,selected;unsigned stable,misses,confidence,transient_frames;bool ambiguous;} rf_room_tracker;
_Static_assert(sizeof(rf_signature_analyzer)<2700u,"bounded RF signature analyzer");
_Static_assert(sizeof(rf_signature)<1120u,"bounded RF profile");
static inline uint64_t rf_signature_total(const uint32_t *v){uint64_t total=0;for(unsigned i=0;i<128;i++)total+=v[i];return total;}
static inline void rf_signature_init(rf_signature_analyzer *a){memset(a,0,sizeof(*a));uint64_t energy=0;for(unsigned k=0;k<256;k++){int32_t w=rf_dsp_window_at(RF_DSP_HANN,k,256);energy+=(uint64_t)((int64_t)w*w);}a->window_power_q16=(uint32_t)((energy+(1u<<23))>>24)/256u;}
static inline bool rf_signature_burst(rf_signature_analyzer *a,const rf_capture_identity *identity,const uint32_t *pairs,size_t count){
 if(!a||!pairs||!rf_identity_valid(identity)||count<256||count>8192||!a->window_power_q16)return false;
 for(unsigned k=0;k<256;k++){int32_t w=rf_dsp_window_at(RF_DSP_HANN,k,256);a->real[k]=(int32_t)((int64_t)rf_dsp_unpack(pairs[k],0)*w/64);a->imag[k]=(int32_t)((int64_t)rf_dsp_unpack(pairs[k],10)*w/64);}
 rf_dsp_fft(a->real,a->imag,256);memset(a->power,0,sizeof(a->power));
 for(unsigned k=0;k<256;k++){unsigned source=(k+128u)&255u;int64_t re=a->real[source],im=a->imag[source];uint64_t p=rf_dsp_div_u64_u32((uint64_t)(re*re)+(uint64_t)(im*im),a->window_power_q16);unsigned band=k/2u;p+=a->power[band];a->power[band]=(uint32_t)(p>RF_SIGNATURE_POWER_MAX?RF_SIGNATURE_POWER_MAX:p);}
 a->identity=*identity;a->available=true;++a->transforms;return true;
}
static inline uint32_t rf_signature_band_hz(const rf_capture_identity *i,unsigned band){if(!rf_identity_valid(i)||band>=128)return 0;return rf_identity_low_hz(i)+(uint32_t)rf_dsp_div_u64_u32((uint64_t)(4u*band+1u)*i->sample_rate_hz,512u);}
static inline bool rf_signature_name_valid(const char *name){if(!name||!name[0]||name[0]==' ')return false;unsigned n=0;for(;n<=RF_SIGNATURE_NAME&&name[n];n++)if((unsigned char)name[n]<32||(unsigned char)name[n]>126)return false;return n&&n<=RF_SIGNATURE_NAME&&name[n-1]!=' ';}
static inline bool rf_signature_valid(const rf_signature *s){
 if(!s||s->kind>RF_SIGNATURE_EVENT||!rf_identity_valid(&s->identity))return false;
 if(!s->kind){if(s->frames||s->name[0])return false;for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++)if(s->sums[i])return false;return true;}
 if(!s->frames||s->frames>RF_SIGNATURE_MAX_FRAMES||!rf_signature_name_valid(s->name))return false;
 for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++)if(s->sums[i]>(uint64_t)s->frames*RF_SIGNATURE_POWER_MAX)return false;
 return true;
}
static inline void rf_signature_mean(const rf_signature *s,uint32_t *out){for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++)out[i]=s->frames?(uint32_t)rf_dsp_div_u64_u32(s->sums[i]+s->frames/2,s->frames):0;}
/* Atomic validation before mutation; exact sums keep repeated averages unbiased. */
static inline bool rf_signature_add(rf_signature *s,const uint32_t *power){
 if(!s||!power||!s->kind||s->kind>RF_SIGNATURE_EVENT||!rf_identity_valid(&s->identity)||s->frames>=RF_SIGNATURE_MAX_FRAMES)return false;
 for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++)if(power[i]>RF_SIGNATURE_POWER_MAX||s->sums[i]>(uint64_t)s->frames*RF_SIGNATURE_POWER_MAX)return false;
 for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++)s->sums[i]+=power[i];
 ++s->frames;return true;
}
static inline bool rf_signature_combine(rf_signature *s,const rf_signature *add){
 if(!s||!add||s->kind!=add->kind||!rf_identity_equal(&s->identity,&add->identity)||!rf_signature_valid(s)||!rf_signature_valid(add)||add->frames>RF_SIGNATURE_MAX_FRAMES-s->frames)return false;
 for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++)s->sums[i]+=add->sums[i];
 s->frames+=add->frames;return true;
}
/* Room identity compares normalized spectral shape, not capture loudness.
 * Averaging a louder example of the same shape must not erase its identity.
 * The saved raw-power mean remains unchanged for background subtraction. */
static inline unsigned rf_signature_shape_score(const uint32_t *a,const uint32_t *b){
 uint64_t ta=rf_signature_total(a),tb=rf_signature_total(b);if(ta<RF_SIGNATURE_FLOOR||tb<RF_SIGNATURE_FLOOR)return 0;
 unsigned sa=0,sb=0;while((ta>>sa)>UINT32_MAX)++sa;while((tb>>sb)>UINT32_MAX)++sb;
 uint32_t da=(uint32_t)(ta>>sa),db=(uint32_t)(tb>>sb);unsigned distance=0;
 for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++){uint32_t na=(uint32_t)rf_dsp_div_u64_u32((uint64_t)a[i]*65536u,da),nb=(uint32_t)rf_dsp_div_u64_u32((uint64_t)b[i]*65536u,db);na>>=sa;nb>>=sb;distance+=na>nb?na-nb:nb-na;}
 unsigned penalty=(distance*1000u+65536u)/131072u;return penalty>=1000?0:1000-penalty;
}
/* Snapshot events retain their independent +/-6dB level gate. */
static inline unsigned rf_signature_score(const uint32_t *a,const uint32_t *b){
 uint64_t ta=rf_signature_total(a),tb=rf_signature_total(b);if(ta>tb*4||tb>ta*4)return 0;
 return rf_signature_shape_score(a,b);
}
static inline int rf_signature_best(const uint32_t *power,const rf_signature *profiles,const uint32_t means[RF_SIGNATURE_SLOTS][RF_SIGNATURE_BANDS],unsigned kind,unsigned *confidence,bool *ambiguous,const rf_capture_identity *identity){
 unsigned best=0,second=0;int slot=-1;
 for(unsigned i=0;i<RF_SIGNATURE_SLOTS;i++)if(rf_identity_equal(&profiles[i].identity,identity)&&profiles[i].kind==kind&&profiles[i].frames>=(kind==RF_SIGNATURE_ROOM?RF_SIGNATURE_ROOM_FRAMES:1u)){unsigned score=kind==RF_SIGNATURE_ROOM?rf_signature_shape_score(power,means[i]):rf_signature_score(power,means[i]);if(score>best){second=best;best=score;slot=(int)i;}else if(score>second)second=score;}
 *confidence=best/10;*ambiguous=best>=80*10&&best-second<80;
 return best>=80*10&&!*ambiguous?slot:-1;
}
static inline void rf_room_reset(rf_room_tracker *t){memset(t,0,sizeof(*t));t->candidate=t->selected=-1;}
static inline void rf_room_observe(rf_room_tracker *t,const uint32_t *power,const rf_signature *profiles,const uint32_t means[RF_SIGNATURE_SLOTS][RF_SIGNATURE_BANDS],const rf_capture_identity *identity){
 if(t->frames<UINT32_MAX)++t->frames;
 /* Reject brief event bursts before they can move a learned-room filter. */
 uint64_t total=rf_signature_total(power),prior=rf_signature_total(t->mean);
 bool louder=t->frames>8&&prior>=RF_SIGNATURE_FLOOR&&total>prior*4;
 if(louder){if(t->transient_frames<9)++t->transient_frames;}else t->transient_frames=0;
 bool transient=louder&&t->transient_frames<=8;
 /* Move at least one unit toward an observed change. Rounded integer EMA
  * otherwise leaves up to 16 units in every old band forever and never learns
  * a new quiet band, so accumulating room history destroys quiet matches. */
 if(!transient)for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++){
  if(t->frames==1)t->mean[i]=power[i];
  else if(power[i]<t->mean[i])t->mean[i]-=(t->mean[i]-power[i]+31u)/32u;
  else if(power[i]>t->mean[i])t->mean[i]+=(power[i]-t->mean[i]+31u)/32u;
 }
 bool ambiguous=false;unsigned confidence=0;int best=transient||total<RF_SIGNATURE_FLOOR?-1:rf_signature_best(t->mean,profiles,means,RF_SIGNATURE_ROOM,&confidence,&ambiguous,identity);
 t->confidence=confidence;t->ambiguous=ambiguous;
 if(best<0){t->candidate=-1;t->stable=0;if((t->misses<64?++t->misses:t->misses)>=64)t->selected=-1;return;}
 t->misses=0;if(best!=t->candidate){t->candidate=best;t->stable=1;}else if(t->stable<64)++t->stable;
 if(t->stable>=64)t->selected=best;
}
static inline uint32_t rf_signature_residual(uint32_t observed,uint32_t background){return observed>background?observed-background:0;}
/* Power-domain spectral subtraction at the canonical band resolution. A
 * residual/input power ratio preserves the fine-bin shape of the display FFT
 * without pretending a coarse tonal background is uniform broadband noise.
 * The gain is amplitude Q16 in [0,65536]; raw FFT bins are never changed. */
static inline void rf_signature_gains(const uint32_t *observed,const uint32_t *background,uint32_t gains[RF_SIGNATURE_BANDS]){
 if(!observed||!background||!gains)return;
 for(unsigned i=0;i<RF_SIGNATURE_BANDS;i++){
  if(!background[i]){gains[i]=65536;continue;}
  uint32_t residual=rf_signature_residual(observed[i],background[i]);
  gains[i]=residual?rf_dsp_sqrt(rf_dsp_div_u64_u32((uint64_t)residual<<32,observed[i])):0;
 }
}
static inline uint32_t rf_signature_filtered_amplitude(uint32_t raw,unsigned bin,unsigned fft_size,const uint32_t *gains){if(!gains||!rf_dsp_size_valid(fft_size)||bin>=fft_size)return raw;unsigned band=bin*128u/fft_size;if(band>=128)band=127;return (uint32_t)(((uint64_t)raw*gains[band]+32768u)>>16);}
#endif
