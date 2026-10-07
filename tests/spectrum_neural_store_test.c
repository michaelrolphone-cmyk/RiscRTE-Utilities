#define main neural_fixture_main
#include "spectrum_neural_test.c"
#undef main
#include "../Apps/spectrum_neural_store.h"
static uint8_t bytes[SN_RECORD_SIZE],valid[SN_RECORD_SIZE];
static sn_trainer before;
static void checksum(void){spectrum_signature_put32(bytes+SN_RECORD_SIZE-4,spectrum_signature_crc(bytes,SN_RECORD_SIZE-4));}
int main(void){
 setup();sn_reset(&trainer,&library,255);finish(&trainer);assert(trainer.has_active);
 uint32_t crc[2]={0x12345678u,0xabc01234u};library.generation[0]=12;library.generation[1]=24;
 assert(sn_record_encode(&trainer,&library,crc,bytes,sizeof(bytes)));memcpy(valid,bytes,sizeof(bytes));
 sn_reset(&repeat,&library,255);assert(sn_record_load(&repeat,&library,crc,bytes,sizeof(bytes)));
 assert(repeat.state==SN_ACTIVE&&repeat.has_active&&!memcmp(&repeat.active,&trainer.active,sizeof(sn_model))&&repeat.candidate_correct==4&&repeat.baseline_correct==2);
 before=repeat;
 for(unsigned i=0;i<SN_RECORD_SIZE;i++){bytes[i]^=1;assert(!sn_record_load(&repeat,&library,crc,bytes,sizeof(bytes)));assert(!memcmp(&repeat,&before,sizeof(repeat)));bytes[i]^=1;}
 assert(!sn_record_load(&repeat,&library,crc,bytes,sizeof(bytes)-1));
 crc[1]++;assert(!sn_record_load(&repeat,&library,crc,bytes,sizeof(bytes)));crc[1]--;
 library.generation[0]++;assert(!sn_record_load(&repeat,&library,crc,bytes,sizeof(bytes)));library.generation[0]--;
 repeat.held[0]=6;before=repeat;assert(!sn_record_load(&repeat,&library,crc,bytes,sizeof(bytes)));assert(!memcmp(&before,&repeat,sizeof(repeat)));repeat.held[0]=20;
 /* CRC-correct malformed values, schemas, dimensions and invented validation
  * claims fail closed, including NaN and infinity float encodings. */
 const unsigned offsets[]={4,5,6,7,8,9,17,36,40,44,48,52,56,60};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){memcpy(bytes,valid,sizeof(bytes));bytes[offsets[i]]=255;checksum();assert(!sn_record_valid(bytes,sizeof(bytes)));}
 const uint32_t hostile[]={0x7fc00000u,0x7f800000u,0xff800000u,0x42000000u};
 for(unsigned i=0;i<4;i++){memcpy(bytes,valid,sizeof(bytes));spectrum_signature_put32(bytes+64,hostile[i]);checksum();assert(!sn_record_valid(bytes,sizeof(bytes)));}
 memcpy(bytes,valid,sizeof(bytes));spectrum_signature_put32(bytes+64+(SN_FLOATS-2*SN_INPUTS)*4,0xbf800000u);checksum();assert(!sn_record_valid(bytes,sizeof(bytes)));
 memcpy(bytes,valid,sizeof(bytes));spectrum_signature_put32(bytes+64+(SN_FLOATS-SN_INPUTS)*4,0x42480000u);checksum();assert(!sn_record_valid(bytes,sizeof(bytes)));
 printf("Neural checkpoint: %u-byte canonical record, exact collection binding, atomic load, all-byte corruption and CRC-correct hostile data PASS\n",SN_RECORD_SIZE);
}
