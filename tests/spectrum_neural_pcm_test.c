/* Actual FFT/background/segmentation and learned classifier. Synthetic PCM
 * only: separate level/ambient recordings; no physical microphone claim. */
#define main original_pcm_main
#include "spectrum_temporal_pcm_test.c"
#undef main
#include "../Apps/spectrum_neural.h"
static sn_trainer neural;
static void mixed_column(unsigned identity,unsigned amplitude,unsigned ambient,unsigned secondary){
 pcm_pipeline *p=&pipeline;int16_t pcm[256];uint64_t pair[128]={0};bool active=false;
 for(unsigned chunk=0;chunk<4;chunk++){
  for(unsigned i=0;i<256;i++,p->sample++){double t=(double)p->sample/16000;double v=amplitude*sin(2*PCM_PI*1000*t)+ambient*sin(2*PCM_PI*250*t);if(secondary){if(identity==0||identity==2)v+=secondary*sin(2*PCM_PI*1900*t);if(identity==1||identity==2)v+=secondary*sin(2*PCM_PI*2600*t);}assert(v>=-32768&&v<=32767);pcm[i]=(int16_t)lround(v);}
  unsigned before=p->fft.transforms;assert(spectrum_signature_feed(&p->fft,pcm,256));if(p->fft.transforms==before)continue;spectrum_background_observe(&p->background,p->fft.power);uint32_t salient[128];spectrum_background_salient(&p->background,salient);for(unsigned b=0;b<128;b++)pair[b]+=salient[b];active|=p->background.foreground&&spectrum_background_db(spectrum_signature_total(salient))>=-6000;
 }
 uint32_t power[128];for(unsigned b=0;b<128;b++)power[b]=(uint32_t)(pair[b]/2);st_frame f=st_frame_make(power,p->previous,active);memcpy(p->previous,power,sizeof(power));st_segment_observe(&p->segment,&f);p->columns++;
}
static st_example mixed_event(unsigned identity,unsigned recording){static unsigned env[]={100,70,48,31,21,11,5,1};unsigned gain=10000-recording*1000,ambient=recording==2?200:0;pcm_start(&pipeline,ambient);for(unsigned i=0;i<8;i++)mixed_column(identity,gain*env[i]/100,ambient,i==2?gain*env[i]/100:0);for(unsigned i=0;i<4;i++)mixed_column(identity,0,ambient,0);assert(pipeline.segment.ready&&st_example_valid(&pipeline.segment.event));st_example e=pipeline.segment.event;e.id=recording+1;return e;}
int main(void){
 memset(&library,0,sizeof(library));
 for(unsigned label_index=0;label_index<2;label_index++){
  st_label *label=&library.labels[label_index];label->present=true;label->next_id=6;
  snprintf(label->name,sizeof(label->name),"Resonance%u",label_index);
  for(unsigned i=0;i<3;i++)label->examples[i]=mixed_event(label_index,i);
  for(unsigned i=3;i<5;i++){label->examples[i]=mixed_event(2,i);label->examples[i].kind=ST_NEGATIVE;}
 }
 st_library unchanged=library;
 sn_reset(&neural,&library,255);unsigned ticks=0;
 while(neural.state>=SN_PREPARING&&neural.state<=SN_CHECKING){sn_tick(&neural,&library);assert(++ticks<3000);}
 assert(neural.has_active&&neural.state==SN_ACTIVE&&neural.calibrated==3&&!neural.regressions);
 assert(neural.checks==4&&neural.baseline_correct==2&&neural.candidate_correct==4);
 assert(neural.full_checks==10&&neural.full_candidate_correct==10&&neural.full_baseline_correct<10);
 assert(!memcmp(&library,&unchanged,sizeof(library)));
 for(unsigned recording=5;recording<=7;recording++)for(unsigned identity=0;identity<3;identity++){
  st_example query=mixed_event(identity,recording);st_match_begin(&matcher,&query);
  while(matcher.running)st_match_tick(&matcher,&library,64);
  assert(matcher.selected<0);bool resolved=sn_apply(&neural,&matcher);
  if(identity<2)assert(resolved&&matcher.selected==(int)identity);
  else assert(!resolved&&matcher.selected<0);
 }
 st_example octave=pcm_event(4000,12000,1,200,false);st_match_begin(&matcher,&octave);
 while(matcher.running)st_match_tick(&matcher,&library,64);
 assert(matcher.reason==ST_RESULT_UNKNOWN&&!sn_apply(&neural,&matcher));
 printf("Neural actual-PCM pipeline: held-out %u/%u versus temporal %u/%u, full collection %u/%u, nine fresh lower-level positive/unknown recordings and octave rejection PASS (synthetic PCM, host only)\n",neural.candidate_correct,neural.checks,neural.baseline_correct,neural.checks,neural.full_candidate_correct,neural.full_checks);
 return 0;
}
