#include <assert.h>
#include <stdio.h>
#include "../../Apps/rf_store.h"
#include "../../Apps/rf_signature_store.h"
#include "../../Apps/rf_background.h"
static rf_signature profiles[8];static uint32_t means[8][128];
static void codec_tests(void){
 uint8_t b[RF_SIGNATURE_RECORD_SIZE];rf_preferences p=rf_preferences_default(),decoded={0};assert(rf_preferences_encode(&p,b));assert(rf_preferences_decode(&decoded,b,RF_PREFERENCES_SIZE));assert(decoded.identity.lo_hz==2440000000u&&decoded.high_hz==2480000000u);for(unsigned k=0;k<RF_PREFERENCES_SIZE;k++){b[k]^=1;assert(!rf_preferences_decode(&decoded,b,RF_PREFERENCES_SIZE));b[k]^=1;}b[100]=1;rf_signature_put32(b+124,rf_signature_crc(b,124));assert(!rf_preferences_decode(&decoded,b,RF_PREFERENCES_SIZE));
 rf_label l={true,2440000000u,125000u,2,3,"RF carrier"},copy={0};assert(rf_label_encode(&l,b));assert(rf_label_decode(&copy,b,RF_LABEL_RECORD_SIZE));assert(copy.frequency_hz==2440000000u&&copy.tolerance_hz==125000u&&copy.tolerance_bins==2);b[40]=1;rf_signature_put32(b+44,rf_signature_crc(b,44));assert(!rf_label_decode(&copy,b,RF_LABEL_RECORD_SIZE));l=(rf_label){0};assert(rf_label_encode(&l,b)&&rf_label_decode(&copy,b,RF_LABEL_RECORD_SIZE)&&!copy.present);
 rf_signature s={0},loaded={0};s.identity=rf_identity_default();s.kind=RF_SIGNATURE_ROOM;memcpy(s.name,"Room RF",8);uint32_t power[128]={0};power[22]=RF_SIGNATURE_POWER_MAX;for(unsigned i=0;i<1000;i++)assert(rf_signature_add(&s,power));assert(s.sums[22]==(uint64_t)RF_SIGNATURE_POWER_MAX*1000);assert(rf_signature_encode(&s,b));assert(rf_signature_decode(&loaded,b,sizeof(b)));assert(loaded.sums[22]==s.sums[22]&&rf_identity_equal(&loaded.identity,&s.identity));for(unsigned k=0;k<sizeof(b);k++){b[k]^=1;assert(!rf_signature_decode(&loaded,b,sizeof(b)));b[k]^=1;}b[94]=1;rf_signature_put32(b+1120,rf_signature_crc(b,1120));assert(!rf_signature_decode(&loaded,b,sizeof(b)));
 rf_capture_identity different=s.identity;different.dc[0]=20;assert(!rf_identity_equal(&s.identity,&different));loaded=s;loaded.identity=different;assert(!rf_signature_combine(&s,&loaded));different=s.identity;different.sample_rate_hz=16000000;assert(!rf_identity_equal(&s.identity,&different));different=s.identity;different.raw_gain++;assert(!rf_identity_equal(&s.identity,&different));
 rf_capture_identity id=rf_identity_default(),out=id;uint8_t bytes[64];assert(rf_identity_encode(&id,bytes)&&rf_identity_decode(&out,bytes,sizeof(bytes)));bytes[62]=99;assert(!rf_identity_decode(&out,bytes,sizeof(bytes)));assert(rf_identity_equal(&out,&id));
}
static void rooms_and_floor(void){
 rf_capture_identity id=rf_identity_default();for(unsigned i=0;i<8;i++)profiles[i].identity=id;
 profiles[0].kind=profiles[1].kind=RF_SIGNATURE_ROOM;memcpy(profiles[0].name,"North",6);memcpy(profiles[1].name,"South",6);uint32_t a[128]={0},b[128]={0},quiet[128]={0};a[20]=2048;b[100]=32;for(unsigned k=0;k<64;k++){assert(rf_signature_add(&profiles[0],a));assert(rf_signature_add(&profiles[1],b));}rf_signature_mean(&profiles[0],means[0]);rf_signature_mean(&profiles[1],means[1]);rf_room_tracker t;rf_room_reset(&t);for(unsigned k=0;k<64;k++)rf_room_observe(&t,a,profiles,means,&id);assert(t.selected==0);b[100]=8;for(unsigned k=0;k<600;k++)rf_room_observe(&t,b,profiles,means,&id);/* Below absolute floor stays unknown, never a phantom quiet match. */assert(t.selected==-1);b[100]=32;for(unsigned k=0;k<200;k++)rf_room_observe(&t,b,profiles,means,&id);assert(t.selected==1);assert(t.mean[20]==0);for(unsigned k=0;k<64;k++)rf_room_observe(&t,quiet,profiles,means,&id);assert(t.selected==-1);for(unsigned k=0;k<200;k++)rf_room_observe(&t,quiet,profiles,means,&id);assert(rf_signature_total(t.mean)==0);
 rf_room_reset(&t);for(unsigned k=0;k<64;k++)rf_room_observe(&t,a,profiles,means,&id);uint32_t transient[128]={0};transient[60]=100000;for(unsigned k=0;k<8;k++)rf_room_observe(&t,transient,profiles,means,&id);assert(t.selected==0&&t.mean[60]==0);for(unsigned k=0;k<96;k++)rf_room_observe(&t,transient,profiles,means,&id);assert(t.selected==-1&&t.mean[60]>0);
 id.raw_gain++;unsigned confidence=0;bool ambiguous=false;assert(rf_signature_best(a,profiles,means,RF_SIGNATURE_ROOM,&confidence,&ambiguous,&id)==-1);
 id=rf_identity_default();profiles[2]=profiles[0];memcpy(means[2],means[0],sizeof(means[0]));assert(rf_signature_best(a,profiles,means,RF_SIGNATURE_ROOM,&confidence,&ambiguous,&id)==-1&&ambiguous);profiles[2].kind=0;
 rf_background bg;rf_background_reset(&bg);for(unsigned k=0;k<64;k++)rf_background_observe(&bg,a);assert(bg.ready);rf_background_observe(&bg,transient);assert(bg.foreground&&bg.raw[60]==100000&&bg.slow[60]==0);assert(rf_background_salient(&bg,NULL)==100000);uint32_t gains[128];rf_signature_gains(bg.raw,bg.slow,gains);assert(gains[60]==65536);assert(rf_signature_filtered_amplitude(400000,120,256,gains)==400000);for(unsigned k=0;k<1400;k++)rf_background_observe(&bg,transient);assert(bg.slow[60]>0);for(unsigned k=0;k<500;k++)rf_background_observe(&bg,quiet);assert(rf_signature_total(bg.slow)==0);rf_background_interrupt(&bg);assert(!bg.foreground&&rf_signature_total(bg.raw)==0);
}
static void canonical_label_tolerance(void){
 rf_capture_identity id=rf_identity_default();rf_background b={0};uint32_t quiet[128]={0},tone[128]={0};
 for(unsigned n=0;n<RF_BG_WARMUP;n++)rf_background_observe(&b,quiet);
 tone[64]=500000u;rf_background_observe(&b,tone);int16_t level=0,snr=0;
 uint32_t adjacent=rf_signature_band_hz(&id,63),far=rf_signature_band_hz(&id,60);
 /* Two canonical256 FFT bins reach the adjacent two-bin band. A display8192
  * tolerance would shrink to1/32 and incorrectly lose this saved label. */
 assert(rf_background_label(&b,&id,adjacent,0,2,-60,false,&level,&snr));
 assert(!rf_background_label(&b,&id,far,0,2,-60,false,&level,&snr));
 assert(rf_background_label(&b,&id,far,3000000u,0,-60,false,&level,&snr));
}
int main(void){codec_tests();rooms_and_floor();canonical_label_tolerance();puts("RF strict codecs, identity isolation, exact weighted sums, room switching and quiet floor passed");}
