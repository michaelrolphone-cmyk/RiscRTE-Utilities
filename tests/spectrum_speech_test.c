#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../Apps/spectrum_speech.h"
static spectrum_speech voice;
static spectrum_signature_analyzer fft;
static spectrum_background background;
static uint32_t excess[128];
static unsigned random_state;
static double noise(void){random_state=random_state*1664525u+1013904223u;return (double)(int32_t)random_state/2147483648.;}
static unsigned scene(unsigned kind,int floor_db){
 spectrum_speech_reset(&voice);spectrum_signature_init(&fft);spectrum_background_reset(&background);random_state=1;unsigned active=0;double low=0;
 for(unsigned chunk=0;chunk<600;chunk++){
  int16_t pcm[256];
  for(unsigned i=0;i<256;i++){
   double t=(chunk*256u+i)/16000.;double value=300*sin(6.28318530718*80*t);
   if(kind==0)value=0;
   if(kind==1)value=10000;
   if(kind==2)value+=5000*sin(6.28318530718*440*t)+2200*sin(6.28318530718*1000*t);
   if(chunk>=160){
    double envelope=kind==3?1:0.6+0.4*sin(6.28318530718*4*t);
    double voiced=envelope*(3000*sin(6.28318530718*180*t)+2500*sin(6.28318530718*540*t)+2000*sin(6.28318530718*1080*t)+1800*sin(6.28318530718*2160*t));
    if(kind==3||kind==4)value+=voiced;
    if(kind==5)value+=8000*envelope*sin(6.28318530718*1000*t);
    if(kind==6)value+=6000*noise();
    if(kind==7&&chunk==160)value+=16000*noise();
    if(kind==8)value+=voiced/128.;
    if(kind==9){low+=0.25*(noise()-low);value+=12000*low;}
   }
   pcm[i]=(int16_t)value;
  }
  assert(spectrum_speech_feed(&voice,pcm,256));assert(spectrum_signature_feed(&fft,pcm,256));
  if(chunk%2){spectrum_background_observe(&background,fft.power);spectrum_background_salient(&background,excess);spectrum_speech_observe(&voice,fft.power,excess,background.ready,floor_db);if(chunk>=160&&voice.active)++active;assert(voice.confidence<=100);}
 }
 printf("voice scene%u floor%d active%u\n",kind,floor_db,active);return active;
}
int main(void){
 for(unsigned kind=0;kind<4;kind++)assert(scene(kind,-45)==0);
 assert(scene(4,-45)>80);
 assert(scene(5,-45)==0);assert(scene(6,-45)==0);assert(scene(7,-45)==0);assert(scene(8,-45)==0);assert(scene(4,0)==0);
 assert(scene(6,-60)==0);assert(scene(9,-60)==0);
 spectrum_speech a,b;spectrum_speech_reset(&a);spectrum_speech_reset(&b);int16_t pcm[256];for(unsigned j=0;j<100;j++){for(unsigned i=0;i<256;i++)pcm[i]=(int16_t)(4000*sin(6.28318530718*190*(j*256+i)/16000.));assert(spectrum_speech_feed(&a,pcm,256));for(unsigned i=0;i<256;){unsigned n=256-i>17?17:256-i;assert(spectrum_speech_feed(&b,pcm+i,n));i+=n;}}
 assert(!memcmp(&a,&b,sizeof(a)));assert(!spectrum_speech_feed(&a,pcm,257));spectrum_speech_reset(&a);assert(!a.active&&!a.used&&!a.confidence);
 puts("Speech activity: real fixed-point GMM plus background/floor/bandwidth/modulation; synthetic voiced syllables, stationary tones/motor/noise/impact/DC negatives, chunk invariance and reset PASS");
}
