#ifndef RF_BACKGROUND_H
#define RF_BACKGROUND_H
#include "rf_signatures.h"
/* Raw power background and foreground excess are separate. No display gain or
 * filtered output feeds this learner. A newly steady tone is eventually learned
 * after a bounded foreground hold; it cannot remain an event forever. */
#define RF_BG_WARMUP 64u
#define RF_BG_HOLD 1024u
#define RF_BG_FLOOR 1u

typedef struct {
 uint32_t slow[128],raw[128],excess[128];
 uint16_t age[128],frames;
 bool active[128],ready,foreground,freeze_upward;
} rf_background;
static inline void rf_background_reset(rf_background *b){memset(b,0,sizeof(*b));}
/* Room power is statistical evidence from complete frames, not a contiguous
 * event window. Keep its accumulated mean across a consumer scheduling gap;
 * raw/foreground output is invalid until the next complete frame. Adaptation
 * age and hysteresis count observed frames, never elapsed missing time. */
static inline void rf_background_interrupt(rf_background *b){
 memset(b->raw,0,sizeof(b->raw));memset(b->excess,0,sizeof(b->excess));
 b->foreground=b->freeze_upward=false;
}
static inline int16_t rf_background_db(uint64_t power){
 if(power>UINT32_MAX)power=UINT32_MAX;
 return rf_dsp_amplitude_db(rf_dsp_sqrt(power<<18),0);
}
static inline void rf_background_observe(rf_background *b,const uint32_t power[128]){
 b->foreground=false;
 if(b->frames<RF_BG_WARMUP){
  for(unsigned i=0;i<128;i++){b->slow[i]=(uint32_t)rf_dsp_div_u64_u32((uint64_t)b->slow[i]*b->frames+power[i],b->frames+1u);b->raw[i]=power[i];b->excess[i]=0;b->active[i]=false;}
  b->ready=++b->frames==RF_BG_WARMUP;return;
 }
 for(unsigned i=0;i<128;i++){
  uint32_t baseline=b->slow[i],floor=baseline>RF_BG_FLOOR?baseline:RF_BG_FLOOR;
  b->raw[i]=power[i];b->excess[i]=power[i]>baseline?power[i]-baseline:0;
  bool active=b->excess[i]>=RF_BG_FLOOR&&(uint64_t)power[i]>=(uint64_t)floor*(b->active[i]?2u:4u);
  b->active[i]=active;b->foreground|=active;
  if(active){if(b->age[i]<RF_BG_HOLD)++b->age[i];}else b->age[i]=0;
  /* Fast release, slow upward ambient adaptation. Ceil steps prevent an
   * integer EMA from retaining a nonzero floor forever at exact silence. */
  if(power[i]<baseline)b->slow[i]-=(baseline-power[i]+31u)/32u;
  else if(!active||(!b->freeze_upward&&b->age[i]>=RF_BG_HOLD)){unsigned divisor=512u;b->slow[i]+=(power[i]-baseline+divisor-1u)/divisor;}
 }
}
/* Label matching uses explicit Hz/bin tolerance around absolute RF carrier. */
static inline bool rf_background_label(const rf_background *b,const rf_capture_identity *identity,uint32_t hz,uint32_t tolerance_hz,unsigned tolerance_bins,unsigned fft_size,int floor_db,bool was_active,int16_t *excess_db,int16_t *snr_db){
 if(!b||!rf_identity_valid(identity)||!excess_db||!snr_db||!rf_dsp_size_valid(fft_size)||tolerance_bins>8192u)return false;
 uint64_t observed=0,background=0,excess=0;
 uint32_t tolerance=(uint32_t)rf_dsp_div_u64_u32((uint64_t)tolerance_bins*identity->sample_rate_hz+fft_size-1u,fft_size);if(tolerance<tolerance_hz)tolerance=tolerance_hz;
 /* Include intersecting canonical two-bin bands, never carrier-relative%. */
 uint32_t half=(identity->sample_rate_hz+255u)/256u;
 for(unsigned i=0;i<128;i++){uint32_t center=rf_signature_band_hz(identity,i);uint64_t difference=center>hz?(uint64_t)center-hz:(uint64_t)hz-center;if(difference<=(uint64_t)tolerance+half){observed+=b->raw[i];background+=b->slow[i];excess+=b->excess[i];}}
 *excess_db=rf_background_db(excess);int raw_db=rf_background_db(observed),base_db=rf_background_db(background>RF_BG_FLOOR?background:RF_BG_FLOOR);int difference=raw_db-base_db;*snr_db=(int16_t)(difference>0?difference:0);
 uint64_t reference=background>RF_BG_FLOOR?background:RF_BG_FLOOR;
 return b->ready&&hz>=rf_identity_low_hz(identity)&&hz<=rf_identity_high_hz(identity)&&excess>=RF_BG_FLOOR&&*excess_db>=floor_db*100&&observed>=reference*(was_active?2u:4u);
}
/* Event salience is excess in bands that independently passed their SNR
 * hysteresis. A tiny high-SNR bin cannot borrow energy from low-SNR ambient
 * fluctuations to pass the user's absolute excess floor. Raw/slow stay intact. */
static inline uint64_t rf_background_salient(const rf_background *b,uint32_t out[128]){
 uint64_t total=0;for(unsigned i=0;i<128;i++){uint32_t p=b->active[i]?b->excess[i]:0;if(out)out[i]=p;total+=p;}return total;
}
#endif
