#ifndef SPECTRUM_SIGNATURE_STORE_H
#define SPECTRUM_SIGNATURE_STORE_H
#include "spectrum_signatures.h"
/* @2 one-key record, explicit little endian, algorithm identity and CRC32.
 * No record eviction, overwrite-on-read-error, or cross-key transaction. */
static inline uint32_t spectrum_signature_u32(const uint8_t *b){return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static inline void spectrum_signature_put32(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(n>>(i*8));}
static inline uint32_t spectrum_signature_crc(const uint8_t *b,size_t n){uint32_t crc=UINT32_MAX;for(size_t i=0;i<n;i++){crc^=b[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}return ~crc;}
static inline bool spectrum_signature_encode(const spectrum_signature *s,uint8_t *b){
 if(!b||!spectrum_signature_valid(s))return false;
 memset(b,0,SPECTRUM_SIGNATURE_RECORD_SIZE);memcpy(b,"SPSG",4);b[4]=1;b[5]=s->kind;b[6]=1;b[7]=128;spectrum_signature_put32(b+8,16000);b[12]=0;b[13]=2;b[14]=SPECTRUM_DSP_HANN;spectrum_signature_put32(b+16,s->frames);
 if(s->kind)memcpy(b+20,s->name,strlen(s->name));
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++){spectrum_signature_put32(b+60+i*8,(uint32_t)s->sums[i]);spectrum_signature_put32(b+64+i*8,(uint32_t)(s->sums[i]>>32));}
 spectrum_signature_put32(b+1084,spectrum_signature_crc(b,1084));return true;
}
static inline bool spectrum_signature_decode(spectrum_signature *out,const uint8_t *b,size_t size){
 if(!out||!b||size!=SPECTRUM_SIGNATURE_RECORD_SIZE||memcmp(b,"SPSG",4)||b[4]!=1||b[5]>2||b[6]!=1||b[7]!=128||spectrum_signature_u32(b+8)!=16000||b[12]||b[13]!=2||b[14]!=SPECTRUM_DSP_HANN||b[15]||spectrum_signature_u32(b+1084)!=spectrum_signature_crc(b,1084))return false;
 for(unsigned i=37;i<60;i++)if(b[i])return false;
 spectrum_signature s={0};s.kind=b[5];s.frames=spectrum_signature_u32(b+16);memcpy(s.name,b+20,17);
 unsigned end=0;while(end<17&&s.name[end])++end;if(end==17)return false;for(unsigned i=end;i<17;i++)if(s.name[i])return false;
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)s.sums[i]=spectrum_signature_u32(b+60+i*8)|((uint64_t)spectrum_signature_u32(b+64+i*8)<<32);
 if(!spectrum_signature_valid(&s))return false;
 *out=s;return true;
}
#endif
