#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../Apps/spectrum_signature_store.h"
static spectrum_signature profiles[8],copy;
static uint32_t means[8][128];
static spectrum_signature_analyzer analyzer;
static spectrum_room_tracker tracker;
static spectrum_dsp_state display;
static uint8_t bytes[SPECTRUM_SIGNATURE_RECORD_SIZE];
static void tone(unsigned hz,unsigned amp){int16_t pcm[256];for(unsigned half=0;half<2;half++){for(unsigned i=0;i<256;i++)pcm[i]=(int16_t)lround(amp*sin(6.283185307179586*hz*(i+half*256)/16000));assert(spectrum_signature_feed(&analyzer,pcm,256));}}
int main(void){
 spectrum_signature_init(&analyzer);int16_t dc[256];for(unsigned i=0;i<256;i++)dc[i]=10000;assert(spectrum_signature_feed(&analyzer,dc,256)&&spectrum_signature_feed(&analyzer,dc,256));assert(spectrum_signature_total(analyzer.power)==0);
 spectrum_signature_init(&analyzer);tone(1000,10000);assert(analyzer.available&&analyzer.transforms==1);uint64_t total=spectrum_signature_total(analyzer.power);double expected=10000.0*10000/2;assert(fabs((double)total-expected)/expected<.01);
 profiles[0].kind=SPECTRUM_SIGNATURE_ROOM;strcpy(profiles[0].name,"Office");for(unsigned i=0;i<64;i++)assert(spectrum_signature_add(&profiles[0],analyzer.power));spectrum_signature_mean(&profiles[0],means[0]);assert(spectrum_signature_encode(&profiles[0],bytes));assert(spectrum_signature_decode(&copy,bytes,sizeof(bytes)));assert(!memcmp(&copy,&profiles[0],sizeof(copy)));
 spectrum_signature unchanged=copy;for(unsigned i=0;i<sizeof(bytes);i++){bytes[i]^=1;assert(!spectrum_signature_decode(&copy,bytes,sizeof(bytes)));assert(!memcmp(&copy,&unchanged,sizeof(copy)));bytes[i]^=1;}assert(!spectrum_signature_decode(&copy,bytes,sizeof(bytes)-1));
 spectrum_room_reset(&tracker);for(unsigned i=0;i<63;i++)spectrum_room_observe(&tracker,analyzer.power,profiles,means);assert(tracker.selected==-1);spectrum_room_observe(&tracker,analyzer.power,profiles,means);assert(tracker.selected==0&&tracker.confidence>=99);
 profiles[1]=profiles[0];memcpy(means[1],means[0],sizeof(means[1]));unsigned confidence;bool ambiguous;assert(spectrum_signature_best(analyzer.power,profiles,means,1,&confidence,&ambiguous)==-1&&ambiguous);memset(&profiles[1],0,sizeof(profiles[1]));
 tone(3500,30000);for(unsigned i=0;i<3;i++)spectrum_room_observe(&tracker,analyzer.power,profiles,means);assert(tracker.selected==0);profiles[2].kind=SPECTRUM_SIGNATURE_EVENT;strcpy(profiles[2].name,"Door");assert(spectrum_signature_add(&profiles[2],analyzer.power));spectrum_signature_mean(&profiles[2],means[2]);assert(spectrum_signature_best(analyzer.power,profiles,means,2,&confidence,&ambiguous)==2);
 memset(analyzer.power,0,sizeof(analyzer.power));assert(spectrum_signature_best(analyzer.power,profiles,means,2,&confidence,&ambiguous)==-1);for(unsigned i=0;i<512;i++)spectrum_room_observe(&tracker,analyzer.power,profiles,means);assert(tracker.selected==-1);
 assert(spectrum_signature_filtered_amplitude(999,1,0,means[0])==999);
 assert(spectrum_signature_residual(20,50)==0&&spectrum_signature_residual(50,20)==30);uint32_t gains[128];spectrum_signature_gains(means[0],means[0],gains);assert(spectrum_signature_filtered_amplitude(1000000,1,8192,gains)<=1000000);
 copy=profiles[0];copy.frames=SPECTRUM_SIGNATURE_MAX_FRAMES;assert(!spectrum_signature_add(&copy,analyzer.power));assert(copy.frames==SPECTRUM_SIGNATURE_MAX_FRAMES);
 spectrum_signature empty={0};assert(spectrum_signature_encode(&empty,bytes));assert(spectrum_signature_decode(&copy,bytes,sizeof(bytes))&&!copy.kind);

 /* Partial reads cross frame boundaries without carrying a prior DC sum. */
 spectrum_signature_analyzer aligned,partial;spectrum_signature_init(&aligned);spectrum_signature_init(&partial);int16_t stream[512*5];for(unsigned i=0;i<512*5;i++)stream[i]=(int16_t)(10000+lround(5000*sin(6.283185307179586*1000*i/16000)));
 for(unsigned i=0;i<512*5;i+=256)assert(spectrum_signature_feed(&aligned,stream+i,256));
 for(unsigned i=0;i<512*5;){unsigned n=17;if(n>512*5-i)n=512*5-i;assert(spectrum_signature_feed(&partial,stream+i,n));i+=n;}
 assert(!memcmp(&aligned,&partial,sizeof(aligned))&&aligned.transforms==5&&aligned.pcm_sum==0);
 /* Display choices cannot change the canonical captured identity. */
 spectrum_signature_analyzer reference; spectrum_signature_init(&reference);tone(1000,10000);reference=analyzer;
 for(unsigned fft=256;fft<=8192;fft*=2)for(unsigned window=0;window<5;window++){
  spectrum_dsp_config c=spectrum_dsp_defaults();c.fft_size=fft;c.window=(spectrum_dsp_window)window;assert(spectrum_dsp_init(&display,&c));
  spectrum_signature_init(&analyzer);tone(1000,10000);assert(!memcmp(reference.power,analyzer.power,sizeof(reference.power)));
  uint32_t observed[128],noise[128];for(unsigned i=0;i<128;i++)observed[i]=noise[i]=10000;
  spectrum_signature_gains(observed,noise,gains);
  for(unsigned bin=1;bin<=fft/2;bin++){assert(spectrum_signature_filtered_amplitude(1000000,bin,fft,gains)==0);assert(spectrum_signature_filtered_amplitude(1000000,0,fft,gains)==1000000);}
  observed[20]=50000;noise[30]=0;spectrum_signature_gains(observed,noise,gains);assert(gains[20]>58600&&gains[20]<58700&&gains[30]==65536);for(unsigned i=0;i<128;i++)assert(gains[i]<=65536);

 }
 /* A sustained louder different room eventually replaces the old room, while
  * the short burst above did not. Initial transient rejection is bounded. */
 profiles[1].kind=1;strcpy(profiles[1].name,"Workshop");tone(3500,30000);for(unsigned i=0;i<64;i++)assert(spectrum_signature_add(&profiles[1],analyzer.power));spectrum_signature_mean(&profiles[1],means[1]);
 spectrum_room_reset(&tracker);for(unsigned i=0;i<64;i++)spectrum_room_observe(&tracker,means[0],profiles,means);assert(tracker.selected==0);
 for(unsigned i=0;i<240;i++)spectrum_room_observe(&tracker,means[1],profiles,means);
 assert(tracker.selected==1);
 /* A quiet saved profile must release even when rounded EMA bands stop
  * decaying. Actual raw silence/level gates take precedence over stale means. */
 memset(profiles,0,sizeof(profiles));memset(means,0,sizeof(means));profiles[0].kind=1;profiles[0].frames=64;strcpy(profiles[0].name,"Quiet");for(unsigned i=0;i<128;i++){means[0][i]=16;profiles[0].sums[i]=1024;}
 spectrum_room_reset(&tracker);for(unsigned i=0;i<64;i++)spectrum_room_observe(&tracker,means[0],profiles,means);assert(tracker.selected==0);uint32_t quiet[128]={0};for(unsigned i=0;i<64;i++)spectrum_room_observe(&tracker,quiet,profiles,means);assert(tracker.selected==-1);
 /* Well-checksummed unknown schemas/algorithms and hostile counts fail closed. */
 assert(spectrum_signature_encode(&profiles[0],bytes));bytes[6]=2;spectrum_signature_put32(bytes+1084,spectrum_signature_crc(bytes,1084));assert(!spectrum_signature_decode(&copy,bytes,sizeof(bytes)));
 assert(spectrum_signature_encode(&profiles[0],bytes));spectrum_signature_put32(bytes+16,UINT32_MAX);spectrum_signature_put32(bytes+1084,spectrum_signature_crc(bytes,1084));assert(!spectrum_signature_decode(&copy,bytes,sizeof(bytes)));
 puts("Canonical power FFT, exact averaging, codec corruption/atomicity, ambiguity, hysteresis, transient events and subtraction passed");
}
