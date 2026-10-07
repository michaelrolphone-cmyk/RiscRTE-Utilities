#ifndef RF_NEURAL_STORE_H
#define RF_NEURAL_STORE_H
#include "rf_neural.h"
#include "rf_temporal_store.h"
#define RN_FLOATS (RN_HIDDEN*RN_INPUTS+RN_HIDDEN+RT_LABELS*RN_HIDDEN+RT_LABELS+2u*RN_INPUTS)
#define RN_RECORD_HEADER 128u
#define RN_RECORD_SIZE (RN_RECORD_HEADER+RN_FLOATS*4u+4u)
_Static_assert(2u*RT_BANK_MAX+RN_RECORD_SIZE<=131072u,"two RF banks and neural checkpoint fit namespace");
_Static_assert(sizeof(float)==4&&sizeof(rn_model)==RN_FLOATS*4u,"binary32 model layout");
/* Derived, disposable model checkpoint. The two canonical example-bank CRCs
 * and generations bind it to the complete collection, including labels not
 * eligible for neural training. A missing/stale/bad checkpoint never changes
 * the authoritative sample files. Validated models are saved once on promotion,
 * never per training update. Integers and binary32 bits are little endian. */
static inline bool rn_record_valid(const uint8_t *bytes,size_t size){
 if(!bytes||size!=RN_RECORD_SIZE||memcmp(bytes,"RFN1",4)||bytes[4]!=1||bytes[5]!=RN_INPUTS||bytes[6]!=RN_HIDDEN||bytes[7]!=RT_LABELS||!bytes[8])return false;
 unsigned labels=0;
 for(unsigned i=0;i<RT_LABELS;i++){
  unsigned held=bytes[9+i],count=0;for(unsigned b=0;b<RT_EXAMPLES;b++)count+=!!(held&(1u<<b));
  if(held&~63u||((bytes[8]&(1u<<i))?count!=2:held!=0))return false;
  labels+=!!(bytes[8]&(1u<<i));
 }
 if(labels<2||bytes[17]!=bytes[8]||bytes[18]||bytes[19])return false;
 uint32_t checks=rf_signature_u32(bytes+36),baseline=rf_signature_u32(bytes+40),candidate=rf_signature_u32(bytes+44);
 uint32_t full=rf_signature_u32(bytes+56);unsigned full_checks=full>>16,full_baseline=(full>>8)&255u,full_candidate=full&255u;
 if(checks!=labels*2u||baseline>=candidate||candidate>checks||full_checks<labels*5u||full_checks>labels*RT_EXAMPLES||full_baseline>=full_candidate||full_candidate>full_checks||rf_signature_u32(bytes+48)!=sizeof(rn_model)||rf_signature_u32(bytes+52)!=RN_EPOCHS||rf_signature_u32(bytes+60))return false;
 rf_capture_identity identity;if(!rf_identity_decode(&identity,bytes+64,RF_IDENTITY_SIZE))return false;
 if(rf_signature_u32(bytes+size-4)!=rf_signature_crc(bytes,size-4))return false;
 for(unsigned i=0;i<RN_FLOATS;i++){
  uint32_t bits=rf_signature_u32(bytes+RN_RECORD_HEADER+i*4u);float value;memcpy(&value,&bits,4);
  float lo=i<RN_FLOATS-2u*RN_INPUTS?-8.0f:0.0f,hi=i<RN_FLOATS-2u*RN_INPUTS?8.0f:i<RN_FLOATS-RN_INPUTS?1.0f:32.0f;
  if(!(value>=lo&&value<=hi))return false;
 }
 return true;
}
static inline bool rn_record_encode(const rn_trainer *t,const rt_library *l,const uint32_t crc[2],uint8_t *out,size_t size){
 if(!t||!l||!crc||!out||size<RN_RECORD_SIZE||!t->has_active||t->state!=RN_ACTIVE||t->regressions||!rn_source_current(t,l))return false;
 memset(out,0,RN_RECORD_SIZE);memcpy(out,"RFN1",4);out[4]=1;out[5]=RN_INPUTS;out[6]=RN_HIDDEN;out[7]=RT_LABELS;out[8]=t->eligible;memcpy(out+9,t->held,RT_LABELS);out[17]=t->calibrated;rf_identity_encode(&l->identity,out+64);
 for(unsigned i=0;i<2;i++){rf_signature_put32(out+20+i*4,crc[i]);rf_signature_put32(out+28+i*4,l->generation[i]);}
 rf_signature_put32(out+36,t->checks);rf_signature_put32(out+40,t->baseline_correct);rf_signature_put32(out+44,t->candidate_correct);rf_signature_put32(out+48,sizeof(rn_model));rf_signature_put32(out+52,RN_EPOCHS);
 rf_signature_put32(out+56,(t->full_checks<<16)|(t->full_baseline_correct<<8)|t->full_candidate_correct);
 for(unsigned i=0;i<RN_FLOATS;i++){uint32_t bits;memcpy(&bits,(const uint8_t*)&t->active+i*4u,4);rf_signature_put32(out+RN_RECORD_HEADER+i*4u,bits);}
 rf_signature_put32(out+RN_RECORD_SIZE-4,rf_signature_crc(out,RN_RECORD_SIZE-4));return rn_record_valid(out,RN_RECORD_SIZE);
}
static inline bool rn_record_load(rn_trainer *t,const rt_library *l,const uint32_t crc[2],const uint8_t *bytes,size_t size){
 if(!t||!l||!crc||!rn_source_current(t,l)||!rn_record_valid(bytes,size)||bytes[8]!=t->eligible||memcmp(bytes+9,t->held,RT_LABELS))return false;
 rf_capture_identity identity;if(!rf_identity_decode(&identity,bytes+64,RF_IDENTITY_SIZE)||!rf_identity_equal(&identity,&l->identity))return false;
 for(unsigned i=0;i<2;i++)if(rf_signature_u32(bytes+20+i*4)!=crc[i]||rf_signature_u32(bytes+28+i*4)!=l->generation[i])return false;
 /* Complete validation precedes the first write to the active model. */
 for(unsigned i=0;i<RN_FLOATS;i++){uint32_t bits=rf_signature_u32(bytes+RN_RECORD_HEADER+i*4u);memcpy((uint8_t*)&t->active+i*4u,&bits,4);}
 t->checks=rf_signature_u32(bytes+36);t->baseline_correct=rf_signature_u32(bytes+40);t->candidate_correct=rf_signature_u32(bytes+44);uint32_t full=rf_signature_u32(bytes+56);t->full_checks=full>>16;t->full_baseline_correct=(full>>8)&255u;t->full_candidate_correct=full&255u;t->calibrated=bytes[17];t->epoch=RN_EPOCHS;t->has_active=true;t->state=RN_ACTIVE;return true;
}
#endif
