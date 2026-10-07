#ifndef NOVA_RF_SIGNATURE_STORE_H
#define NOVA_RF_SIGNATURE_STORE_H
#include "rf_signatures.h"
/* Explicit little-endian RF identity, exact64-bit sums and CRC32. */
static inline bool rf_signature_encode(const rf_signature *s,uint8_t *b){
 if(!b||!rf_signature_valid(s))return false;
 memset(b,0,RF_SIGNATURE_RECORD_SIZE);memcpy(b,"RFSG",4);b[4]=1;b[5]=s->kind;b[6]=128;b[7]=1;(void)rf_identity_encode(&s->identity,b+8);rf_signature_put32(b+72,s->frames);if(s->kind)memcpy(b+76,s->name,strlen(s->name));
 for(unsigned i=0;i<128;i++){rf_signature_put32(b+96+i*8,(uint32_t)s->sums[i]);rf_signature_put32(b+100+i*8,(uint32_t)(s->sums[i]>>32));}rf_signature_put32(b+1120,rf_signature_crc(b,1120));return true;
}
static inline bool rf_signature_decode(rf_signature *out,const uint8_t *b,size_t size){
 if(!out||!b||size!=RF_SIGNATURE_RECORD_SIZE||memcmp(b,"RFSG",4)||b[4]!=1||b[5]>2||b[6]!=128||b[7]!=1||rf_signature_u32(b+1120)!=rf_signature_crc(b,1120))return false;
 for(unsigned i=93;i<96;i++)if(b[i])return false;
 rf_signature s={0};if(!rf_identity_decode(&s.identity,b+8,64))return false;s.kind=b[5];s.frames=rf_signature_u32(b+72);memcpy(s.name,b+76,17);unsigned end=0;while(end<17&&s.name[end])++end;if(end==17)return false;for(unsigned i=end;i<17;i++)if(s.name[i])return false;
 for(unsigned i=0;i<128;i++)s.sums[i]=rf_signature_u32(b+96+i*8)|((uint64_t)rf_signature_u32(b+100+i*8)<<32);
 if(!rf_signature_valid(&s))return false;
 *out=s;return true;
}
#endif
