#ifndef SPECTRUM_SPEECH_H
#define SPECTRUM_SPEECH_H
#include "spectrum_background.h"
#include "spectrum_vad_impl.inc"
/* WebRTC's fixed-point GMM supplies the voice decision. Additional ambient,
 * bandwidth and within-activity modulation checks suppress stationary tones.
 * Confidence is recent positive-frame support, not speaker identity/probability. */
typedef struct {
 VadInstT core;
 int16_t pcm[320];
 int16_t period_pcm[160];
 int32_t period_sum;
 uint64_t energy[8];
 uint32_t previous[128];
 uint16_t used,votes,period_votes;
 uint8_t period_next,period_used,decimation;
 uint8_t vote_count,history,next,changes,hold,confidence;
 int16_t amplitude_db;
 bool available,active;
} spectrum_speech;
_Static_assert(sizeof(spectrum_speech)<4096,"bounded voice detector state");
static inline void spectrum_speech_reset(spectrum_speech*s){
 memset(s,0,sizeof(*s));s->available=WebRtcVad_InitCore(&s->core)==0&&WebRtcVad_set_mode_core(&s->core,2)==0;s->amplitude_db=-12000;
}
static inline bool spectrum_speech_periodic(const spectrum_speech*s){
 if(s->period_used<160)return false;
 int32_t mean=0,peak=0;int16_t samples[160];unsigned correlation[68]={0};
 for(unsigned i=0;i<160;i++){mean+=s->period_pcm[i];}mean/=160;
 for(unsigned i=0;i<160;i++){int32_t v=s->period_pcm[i]-mean;if(v<0)v=-v;if(v>peak)peak=v;}
 if(peak<4)return false;
 unsigned shift=0;while((peak>>shift)>255)++shift;
 for(unsigned i=0;i<160;i++)samples[i]=(int16_t)((s->period_pcm[(s->period_next+i)%160]-mean)/(1<<shift));
 for(unsigned lag=8;lag<=67;lag++){
  int32_t dot=0;uint32_t a=0,b=0;
  for(unsigned i=0;i<160-lag;i++){int32_t x=samples[i],y=samples[i+lag];dot+=x*y;a+=(uint32_t)(x*x);b+=(uint32_t)(y*y);}
  uint32_t denominator=spectrum_dsp_sqrt((uint64_t)a*b);if(dot>0&&denominator)correlation[lag]=(unsigned)spectrum_dsp_div_u64_u32((uint64_t)(uint32_t)dot*1000u,denominator);
 }
 for(unsigned lag=9;lag<67;lag++)if(correlation[lag]>=720&&correlation[lag]>correlation[lag-1]&&correlation[lag]>=correlation[lag+1])return true;
 return false;
}
static inline bool spectrum_speech_feed(spectrum_speech*s,const int16_t*pcm,size_t n){
 if(!s||!pcm||n>256||s->used>=320||!s->available)return false;
 while(n--){int16_t sample=*pcm++;s->pcm[s->used++]=sample;s->period_sum+=sample;if(++s->decimation==4){s->period_pcm[s->period_next]=(int16_t)(s->period_sum/4);s->period_next=(uint8_t)((s->period_next+1)%160);if(s->period_used<160)++s->period_used;s->period_sum=0;s->decimation=0;}if(s->used==320){int v=WebRtcVad_CalcVad16khz(&s->core,s->pcm,320);if(v<0){s->available=false;s->active=false;return false;}s->used=0;s->votes=(uint16_t)(((s->votes<<1)|(v>0))&1023u);s->period_votes=(uint16_t)(((s->period_votes<<1)|spectrum_speech_periodic(s))&1023u);if(s->vote_count<10)++s->vote_count;}}
 return true;
}
static inline void spectrum_speech_observe(spectrum_speech*s,const uint32_t*raw,const uint32_t*excess,bool background_ready,int floor_db){
 uint64_t total=spectrum_signature_total(excess),voice=0,raw_voice=0,peak=0;
 for(unsigned i=2;i<64;i++){voice+=excess[i];raw_voice+=raw[i];uint64_t local=raw[i];if(i>2)local+=raw[i-1];if(i<63)local+=raw[i+1];if(local>peak)peak=local;}
 s->amplitude_db=spectrum_background_db(total);
 bool shaped=s->available&&background_ready&&raw_voice&&peak*100<raw_voice*75&&spectrum_background_db(raw_voice)>=floor_db*100;
 bool eligible=shaped&&s->amplitude_db>=floor_db*100&&total&&voice*100>=total*55;
 if(shaped){
  bool changed=s->history&&spectrum_signature_shape_score(raw,s->previous)<940;
  s->changes=(uint8_t)((s->changes<<1)|changed);memcpy(s->previous,raw,sizeof(s->previous));
  s->energy[s->next]=raw_voice;s->next=(uint8_t)((s->next+1)%8);if(s->history<8)++s->history;
 }else{s->history=s->next=s->changes=0;memset(s->previous,0,sizeof(s->previous));}
 uint64_t low=UINT64_MAX,high=0;for(unsigned i=0;i<s->history;i++){if(s->energy[i]<low)low=s->energy[i];if(s->energy[i]>high)high=s->energy[i];}
 unsigned yes=0,voiced=0;for(unsigned bits=s->votes;bits;bits>>=1)yes+=bits&1u;for(unsigned bits=s->period_votes;bits;bits>>=1)voiced+=bits&1u;
 bool modulation=s->history==8&&(high*2>low*3||s->changes);
 bool detected=eligible&&s->vote_count==10&&yes>=6&&voiced>=3&&modulation;
 if(detected){s->hold=8;s->confidence=(uint8_t)(yes*10);}else if(s->hold)--s->hold;
 s->active=detected||s->hold;if(!s->active)s->confidence=0;
}
#endif
