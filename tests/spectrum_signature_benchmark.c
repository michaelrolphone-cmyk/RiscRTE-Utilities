/* Deterministic operation budget; wall-clock output is host evidence only. */
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "../Apps/spectrum_signatures.h"
static spectrum_signature_analyzer a;
static spectrum_signature profiles[8];
static uint32_t means[8][128];
static spectrum_room_tracker tracker;
static int16_t pcm[256];
int main(void){
 spectrum_signature_init(&a);spectrum_room_reset(&tracker);
 for(unsigned i=0;i<256;i++)pcm[i]=(int16_t)(spectrum_dsp_sin(i*4096u)>>17);
 assert(spectrum_signature_feed(&a,pcm,256));assert(spectrum_signature_feed(&a,pcm,256));a.transforms=0;
 for(unsigned i=0;i<8;i++){profiles[i].kind=i<4?1:2;strcpy(profiles[i].name,"Fixture");profiles[i].frames=64;for(unsigned k=0;k<128;k++)profiles[i].sums[k]=(uint64_t)a.power[k]*(64+i);spectrum_signature_mean(&profiles[i],means[i]);}
 const unsigned transforms=2000;unsigned checksum=0;clock_t start=clock();
 for(unsigned i=0;i<transforms;i++){assert(spectrum_signature_feed(&a,pcm,256));assert(spectrum_signature_feed(&a,pcm,256));spectrum_room_observe(&tracker,a.power,profiles,means);unsigned confidence;bool ambiguous;int event=spectrum_signature_best(a.power,profiles,means,2,&confidence,&ambiguous);checksum+=confidence+tracker.confidence+(unsigned)(event+1)+(unsigned)ambiguous;}
 assert(a.transforms==transforms&&checksum>0);double ms=(double)(clock()-start)*1000/CLOCKS_PER_SEC;
 printf("Canonical signature: %u FFT512 frames + 8-profile room/event comparisons in %.3f ms, %.3f ms/frame (host only). Fixed 2304 butterflies/frame; 256 power bins; <=1024 profile-band comparisons; no heap. Analyzer %zu, profiles+means %zu, tracker %zu bytes; verification checksum %u.\n",transforms,ms,ms/transforms,sizeof(a),sizeof(profiles)+sizeof(means),sizeof(tracker),checksum);
}
