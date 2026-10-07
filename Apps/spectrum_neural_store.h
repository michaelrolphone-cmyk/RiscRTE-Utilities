#ifndef SPECTRUM_NEURAL_STORE_H
#define SPECTRUM_NEURAL_STORE_H
#include "spectrum_neural.h"
#include "spectrum_signature_store.h"
#define SN_FLOATS (SN_HIDDEN*SN_INPUTS+SN_HIDDEN+ST_LABELS*SN_HIDDEN+ST_LABELS+2u*SN_INPUTS)
#define SN_RECORD_SIZE (64u+SN_FLOATS*4u+4u)
_Static_assert(sizeof(float)==4&&sizeof(sn_model)==SN_FLOATS*4u,"binary32 model layout");
/* Derived, disposable model checkpoint. The two canonical example-bank CRCs
 * and generations bind it to the complete collection, including labels not
 * eligible for neural training. A missing/stale/bad checkpoint never changes
 * the authoritative sample files. Validated models are saved once on promotion,
 * never per training update. Integers and binary32 bits are little endian. */
static inline bool sn_record_valid(const uint8_t *bytes,size_t size){
 if(!bytes||size!=SN_RECORD_SIZE||memcmp(bytes,"SNN1",4)||bytes[4]!=1||bytes[5]!=SN_INPUTS||bytes[6]!=SN_HIDDEN||bytes[7]!=ST_LABELS||!bytes[8])return false;
 unsigned labels=0;
 for(unsigned i=0;i<ST_LABELS;i++){
  unsigned held=bytes[9+i],count=0;for(unsigned b=0;b<ST_EXAMPLES;b++)count+=!!(held&(1u<<b));
  if(held&~63u||((bytes[8]&(1u<<i))?count!=2:held!=0))return false;
  labels+=!!(bytes[8]&(1u<<i));
 }
 if(labels<2||bytes[17]!=bytes[8]||bytes[18]||bytes[19])return false;
 uint32_t checks=spectrum_signature_u32(bytes+36),baseline=spectrum_signature_u32(bytes+40),candidate=spectrum_signature_u32(bytes+44);
 uint32_t full=spectrum_signature_u32(bytes+56);unsigned full_checks=full>>16,full_baseline=(full>>8)&255u,full_candidate=full&255u;
 if(checks!=labels*2u||baseline>=candidate||candidate>checks||full_checks<labels*5u||full_checks>labels*ST_EXAMPLES||full_baseline>=full_candidate||full_candidate>full_checks||spectrum_signature_u32(bytes+48)!=sizeof(sn_model)||spectrum_signature_u32(bytes+52)!=SN_EPOCHS||spectrum_signature_u32(bytes+60))return false;
 if(spectrum_signature_u32(bytes+size-4)!=spectrum_signature_crc(bytes,size-4))return false;
 for(unsigned i=0;i<SN_FLOATS;i++){
  uint32_t bits=spectrum_signature_u32(bytes+64+i*4u);float value;memcpy(&value,&bits,4);
  float lo=i<SN_FLOATS-2u*SN_INPUTS?-8.0f:0.0f,hi=i<SN_FLOATS-2u*SN_INPUTS?8.0f:i<SN_FLOATS-SN_INPUTS?1.0f:32.0f;
  if(!(value>=lo&&value<=hi))return false;
 }
 return true;
}
static inline bool sn_record_encode(const sn_trainer *t,const st_library *l,const uint32_t crc[2],uint8_t *out,size_t size){
 if(!t||!l||!crc||!out||size<SN_RECORD_SIZE||!t->has_active||t->state!=SN_ACTIVE)return false;
 memset(out,0,SN_RECORD_SIZE);memcpy(out,"SNN1",4);out[4]=1;out[5]=SN_INPUTS;out[6]=SN_HIDDEN;out[7]=ST_LABELS;out[8]=t->eligible;memcpy(out+9,t->held,ST_LABELS);out[17]=t->calibrated;
 for(unsigned i=0;i<2;i++){spectrum_signature_put32(out+20+i*4,crc[i]);spectrum_signature_put32(out+28+i*4,l->generation[i]);}
 spectrum_signature_put32(out+36,t->checks);spectrum_signature_put32(out+40,t->baseline_correct);spectrum_signature_put32(out+44,t->candidate_correct);spectrum_signature_put32(out+48,sizeof(sn_model));spectrum_signature_put32(out+52,SN_EPOCHS);
 spectrum_signature_put32(out+56,(t->full_checks<<16)|(t->full_baseline_correct<<8)|t->full_candidate_correct);
 for(unsigned i=0;i<SN_FLOATS;i++){uint32_t bits;memcpy(&bits,(const uint8_t*)&t->active+i*4u,4);spectrum_signature_put32(out+64+i*4u,bits);}
 spectrum_signature_put32(out+SN_RECORD_SIZE-4,spectrum_signature_crc(out,SN_RECORD_SIZE-4));return sn_record_valid(out,SN_RECORD_SIZE);
}
static inline bool sn_record_load(sn_trainer *t,const st_library *l,const uint32_t crc[2],const uint8_t *bytes,size_t size){
 if(!t||!l||!crc||!sn_record_valid(bytes,size)||bytes[8]!=t->eligible||memcmp(bytes+9,t->held,ST_LABELS))return false;
 for(unsigned i=0;i<2;i++)if(spectrum_signature_u32(bytes+20+i*4)!=crc[i]||spectrum_signature_u32(bytes+28+i*4)!=l->generation[i])return false;
 /* Complete validation precedes the first write to the active model. */
 for(unsigned i=0;i<SN_FLOATS;i++){uint32_t bits=spectrum_signature_u32(bytes+64+i*4u);memcpy((uint8_t*)&t->active+i*4u,&bits,4);}
 t->checks=spectrum_signature_u32(bytes+36);t->baseline_correct=spectrum_signature_u32(bytes+40);t->candidate_correct=spectrum_signature_u32(bytes+44);uint32_t full=spectrum_signature_u32(bytes+56);t->full_checks=full>>16;t->full_baseline_correct=(full>>8)&255u;t->full_candidate_correct=full&255u;t->calibrated=bytes[17];t->epoch=SN_EPOCHS;t->has_active=true;t->state=SN_ACTIVE;return true;
}
#endif
