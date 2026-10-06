#ifndef SPECTRUM_NEURAL_FIXTURE_H
#define SPECTRUM_NEURAL_FIXTURE_H
#include <assert.h>
#include "../Apps/spectrum_neural.h"
/* Independently generated recordings share a main decay and differ in a brief
 * secondary resonance. Their temporal similarity is high enough to be genuinely
 * ambiguous. Held-out recordings vary level and decay, rather than self-match. */
static st_example sn_fixture_example(unsigned identity,unsigned recording,unsigned kind){
 st_segmenter s={0};uint32_t power[128]={0},previous[128]={0};
 st_frame f=st_frame_make(power,previous,false);for(unsigned i=0;i<4;i++)st_segment_observe(&s,&f);
 const unsigned envelope[]={100,70,48,31,21,11,5,1};
 for(unsigned i=0;i<8;i++){
  unsigned amplitude=(envelope[i]+(recording==2&&i>0?1u:0u))*(10+recording);
  memset(power,0,sizeof(power));power[15]=amplitude*10000;power[18]=amplitude*8000;
  if(i==2&&identity<2)power[identity?40:30]=amplitude*8000;
  if(i==2&&identity==2)power[30]=power[40]=amplitude*4000;
  if(identity==3){memset(power,0,sizeof(power));power[64]=amplitude*10000;}
  f=st_frame_make(power,previous,true);st_segment_observe(&s,&f);memcpy(previous,power,sizeof(power));
 }
 for(unsigned i=0;i<4;i++){memset(power,0,sizeof(power));f=st_frame_make(power,previous,false);st_segment_observe(&s,&f);memcpy(previous,power,sizeof(power));}
 s.event.id=recording+1;s.event.kind=(uint8_t)kind;assert(st_example_valid(&s.event));return s.event;
}
#endif
