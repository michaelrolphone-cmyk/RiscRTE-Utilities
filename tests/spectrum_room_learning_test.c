#include <assert.h>
#include <stdio.h>
#include "../Apps/spectrum_signature_store.h"
static spectrum_signature profiles[8],saved;
static uint32_t means[8][128],room[128],louder[128],different[128];
static uint8_t record[SPECTRUM_SIGNATURE_RECORD_SIZE];
static void add(unsigned slot,const uint32_t*power){for(unsigned i=0;i<64;i++)assert(spectrum_signature_add(&profiles[slot],power));spectrum_signature_mean(&profiles[slot],means[slot]);}
static int match(const uint32_t*p,bool *ambiguous){unsigned confidence;int found=spectrum_signature_best(p,profiles,means,SPECTRUM_SIGNATURE_ROOM,&confidence,ambiguous);if(found==0)assert(confidence>=99);return found;}
int main(void){
 room[4]=1000;room[12]=300;room[30]=100;for(unsigned i=0;i<128;i++)louder[i]=room[i]*16u;
 profiles[0]=(spectrum_signature){.kind=SPECTRUM_SIGNATURE_ROOM,.name="Office"};add(0,room);bool ambiguous=false;assert(match(room,&ambiguous)==0&&!ambiguous);
 add(0,louder);assert(profiles[0].frames==128&&means[0][4]==8500);assert(spectrum_signature_score(room,means[0])==0);assert(match(room,&ambiguous)==0&&!ambiguous);assert(match(louder,&ambiguous)==0);
 for(unsigned sample=0;sample<12;sample++){add(0,sample&1?room:louder);assert(match(room,&ambiguous)==0);assert(match(louder,&ambiguous)==0);}
 /* More distinct rooms must not weaken an existing unique match. */
 for(unsigned slot=1;slot<8;slot++){for(unsigned i=0;i<128;i++)different[i]=0;different[40+slot*3]=10000;profiles[slot]=(spectrum_signature){.kind=SPECTRUM_SIGNATURE_ROOM,.name="Other"};add(slot,different);assert(match(room,&ambiguous)==0&&!ambiguous);}
 /* An acoustically indistinguishable room stays ambiguous, not arbitrarily
  * assigned from different recording volume. This is useful uncertainty. */
 profiles[1]=(spectrum_signature){.kind=SPECTRUM_SIGNATURE_ROOM,.name="Similar"};add(1,louder);assert(match(room,&ambiguous)==-1&&ambiguous);
 uint32_t silence[128]={0};assert(match(silence,&ambiguous)==-1&&!ambiguous);
 /* The persisted raw-power filter profile retains its exact sums and count. */
 assert(spectrum_signature_encode(&profiles[0],record));assert(spectrum_signature_decode(&saved,record,sizeof(record)));assert(!memcmp(&saved,&profiles[0],sizeof(saved)));
 uint32_t gains[128];spectrum_signature_gains(louder,means[0],gains);assert(gains[4]>0&&gains[4]<65536);
 uint32_t huge[128],tiny[128];for(unsigned i=0;i<128;i++){huge[i]=SPECTRUM_SIGNATURE_POWER_MAX;tiny[i]=i<127?1:0;}assert(spectrum_signature_shape_score(huge,tiny)>=990);
 for(unsigned i=0;i<128;i++){tiny[i]=i<16?1:0;}assert(spectrum_signature_shape_score(huge,tiny)>=120&&spectrum_signature_shape_score(huge,tiny)<=130);
 puts("Room learning: same shape across additional louder captures retains identity; distinct-room additions, genuine ambiguity/silence and unchanged raw filter persistence PASS");
}
