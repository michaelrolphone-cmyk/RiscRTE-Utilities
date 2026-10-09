#pragma once
#include "spectrum_temporal_store.h"
#include "spectrum_neural_store.h"
#include "rf_temporal_store.h"
#include "rf_neural_store.h"
/* Inference templates only: no editor snapshots, recording state, candidate
 * weights, trainer, heap, storage authority or automatic learning. */
typedef struct {
 contexts_model_details_v1 details;
 uint32_t crc[2],digest[3],committed_digest;
 uint8_t bank_seen,bank_good,bank_present,neural_eligible;
 bool neural_seen,neural_good,known_hold,previous_active;
 bool event_valid,event_ambiguous;
 int event_slot;uint32_t event_score;char event_name[17];
 unsigned quiet;
 uint64_t event_at,segment_at,query_at;
} ct_metadata;
typedef struct {
 st_library library;sn_model neural;st_segmenter segment;st_matcher match;
 uint64_t pair_power[128];uint32_t previous_power[128];
 unsigned pair_count;bool pair_active;
} ct_audio;
typedef struct {
 rt_library library;rn_model neural;rt_segmenter segment;rt_matcher match;
 uint32_t previous_power[128];uint64_t previous_at;bool have_previous;
} ct_radio;
static ct_audio ct_a;
static ct_radio ct_r;
static ct_metadata ct_m[2];
_Static_assert(sizeof(ct_a)+sizeof(ct_r)+sizeof(ct_m)<290000u,"bounded inference-only temporal models");
static uint32_t ct_hash(const void *bytes,uint32_t size){
 uint32_t hash=UINT32_C(2166136261);const uint8_t *p=bytes;
 for(uint32_t i=0;i<size;i++)hash=(hash^p[i])*UINT32_C(16777619);
 return hash;
}
static void ct_reset(unsigned i){
 ct_m[i].known_hold=ct_m[i].previous_active=false;ct_m[i].quiet=0;
 ct_m[i].event_valid=ct_m[i].event_ambiguous=false;ct_m[i].event_slot=-1;ct_m[i].event_name[0]=0;
 ct_m[i].details.event_engine=CONTEXTS_EVENT_NONE;ct_m[i].details.event_age_ms=UINT32_MAX;
 ct_m[i].details.match_pending=false;ct_m[i].details.match_work_units=0;
 if(i==0){st_segment_reset(&ct_a.segment);memset(&ct_a.match,0,sizeof(ct_a.match));ct_a.match.selected=-1;
  memset(ct_a.pair_power,0,sizeof(ct_a.pair_power));memset(ct_a.previous_power,0,sizeof(ct_a.previous_power));ct_a.pair_count=0;ct_a.pair_active=false;
 }else{rt_segment_reset(&ct_r.segment);memset(&ct_r.match,0,sizeof(ct_r.match));ct_r.match.selected=-1;
  memset(ct_r.previous_power,0,sizeof(ct_r.previous_power));ct_r.have_previous=false;ct_r.previous_at=0;
 }
}
static void ct_begin(unsigned i){
 uint32_t generation=ct_m[i].details.temporal_generation,digest=ct_m[i].committed_digest;
 memset(&ct_m[i],0,sizeof(ct_m[i]));
 ct_m[i].details=(contexts_model_details_v1){.struct_size=sizeof(contexts_model_details_v1),.source=i?CONTEXTS_RADIO:CONTEXTS_AUDIO,
  .temporal_generation=generation,.event_age_ms=UINT32_MAX};ct_m[i].committed_digest=digest;
 if(i==0){memset(&ct_a.library,0,sizeof(ct_a.library));memset(&ct_a.neural,0,sizeof(ct_a.neural));}
 else{memset(&ct_r.library,0,sizeof(ct_r.library));memset(&ct_r.neural,0,sizeof(ct_r.neural));}
 ct_reset(i);
}
static void ct_init(void){memset(ct_m,0,sizeof(ct_m));ct_begin(0);ct_begin(1);}
/* Match the canonical trainer's eligible labels and exact held-out identities,
 * without constructing or initializing any training model. */
static uint8_t ct_audio_eligible(uint8_t held[8]){
 uint8_t eligible=0;unsigned labels=0;memset(held,0,8);
 for(unsigned i=0;i<8;i++){
  const st_label *l=&ct_a.library.labels[i];
  if(!l->present||!st_label_valid(l)||st_example_count(l,ST_POSITIVE)<SN_MIN_POSITIVES||st_example_count(l,ST_NEGATIVE)<SN_MIN_NEGATIVES)continue;
  uint32_t positive=0,negative=0;unsigned p=0,n=0;
  for(unsigned j=0;j<ST_EXAMPLES;j++){const st_example *e=&l->examples[j];if(e->kind==ST_POSITIVE&&e->id>positive){positive=e->id;p=j;}else if(e->kind==ST_NEGATIVE&&e->id>negative){negative=e->id;n=j;}}
  held[i]=(uint8_t)((1u<<p)|(1u<<n));eligible|=(uint8_t)(1u<<i);++labels;
 }
 return labels>=2?eligible:0;
}
static uint8_t ct_radio_eligible(uint8_t held[8]){
 uint8_t eligible=0;unsigned labels=0;memset(held,0,8);
 for(unsigned i=0;i<8;i++){
  const rt_label *l=&ct_r.library.labels[i];
  if(!l->present||!rt_label_valid(l)||rt_example_count(l,RT_POSITIVE)<RN_MIN_POSITIVES||rt_example_count(l,RT_NEGATIVE)<RN_MIN_NEGATIVES)continue;
  uint32_t positive=0,negative=0;unsigned p=0,n=0;
  for(unsigned j=0;j<RT_EXAMPLES;j++){const rt_example *e=&l->examples[j];if(e->kind==RT_POSITIVE&&e->id>positive){positive=e->id;p=j;}else if(e->kind==RT_NEGATIVE&&e->id>negative){negative=e->id;n=j;}}
  held[i]=(uint8_t)((1u<<p)|(1u<<n));eligible|=(uint8_t)(1u<<i);++labels;
 }
 return labels>=2?eligible:0;
}
static bool ct_empty_crcs(unsigned i,const rf_capture_identity *identity){
 if(i&&!rf_identity_valid(&ct_r.library.identity))ct_r.library.identity=*identity;
 uint8_t bytes[RT_BANK_MIN];
 for(unsigned bank=0;bank<2;bank++)if(!(ct_m[i].bank_present&(1u<<bank))){
  size_t size=i?rt_bank_encode(&ct_r.library,bank,bytes,sizeof(bytes)):st_bank_encode(&ct_a.library,bank,bytes,sizeof(bytes));
  if(!size)return false;
  ct_m[i].crc[bank]=spectrum_signature_u32(bytes+size-4);
 }
 return true;
}
static bool ct_neural(unsigned i,const uint8_t *bytes,uint32_t size,const rf_capture_identity *identity){
 ct_metadata *m=&ct_m[i];uint8_t held[8];
 if(m->bank_good!=3||!ct_empty_crcs(i,identity)){m->details.neural_error=CONTEXTS_IMPORT_INCOMPLETE;return false;}
 if(i?!rn_record_valid(bytes,size):!sn_record_valid(bytes,size))return false;
 uint8_t eligible=i?ct_radio_eligible(held):ct_audio_eligible(held);
 if(!eligible||bytes[8]!=eligible||memcmp(bytes+9,held,8)){m->details.neural_error=CONTEXTS_IMPORT_STALE;return false;}
 const uint32_t *generation=i?ct_r.library.generation:ct_a.library.generation;
 for(unsigned bank=0;bank<2;bank++)if(spectrum_signature_u32(bytes+20+bank*4)!=m->crc[bank]||spectrum_signature_u32(bytes+28+bank*4)!=generation[bank]){m->details.neural_error=CONTEXTS_IMPORT_STALE;return false;}
 if(i){rf_capture_identity saved;if(!rf_identity_decode(&saved,bytes+64,RF_IDENTITY_SIZE)||!rf_identity_equal(&saved,&ct_r.library.identity)){m->details.neural_error=CONTEXTS_IMPORT_STALE;return false;}}
 void *model=i?(void*)&ct_r.neural:(void*)&ct_a.neural;
 unsigned offset=i?RN_RECORD_HEADER:64u;
 unsigned floats=i?RN_FLOATS:SN_FLOATS;
 for(unsigned n=0;n<floats;n++){uint32_t bits=spectrum_signature_u32(bytes+offset+n*4u);memcpy((uint8_t*)model+n*4u,&bits,4);}
 m->neural_eligible=eligible;return true;
}
static bool ct_record_error(unsigned i,uint32_t kind,uint32_t index,int32_t error){
 ct_metadata *m=&ct_m[i];if(!error)return false;
 if(kind==CONTEXTS_RECORD_TEMPORAL_BANK&&index<2){uint8_t bit=(uint8_t)(1u<<index);m->bank_seen|=bit;m->bank_good&=(uint8_t)~bit;m->details.bank_error[index]=error;return true;}
 if(kind==CONTEXTS_RECORD_NEURAL&&!index){m->neural_seen=true;m->neural_good=false;m->details.neural_error=error;return true;}
 return false;
}
static bool ct_record(unsigned i,uint32_t kind,uint32_t index,const void *bytes,uint32_t size,const rf_capture_identity *identity){
 ct_metadata *m=&ct_m[i];bool absent=!bytes&&!size,ok=false;
 if((!bytes&&size)||(bytes&&!size))return ct_record_error(i,kind,index,CONTEXTS_IMPORT_INVALID)&&false;
 if(kind==CONTEXTS_RECORD_TEMPORAL_BANK&&index<2){
  uint8_t bit=(uint8_t)(1u<<index);bool duplicate=!!(m->bank_seen&bit);m->bank_seen|=bit;
  if(!duplicate)ok=absent||(i?rt_bank_decode(&ct_r.library,index,bytes,size):st_bank_decode(&ct_a.library,index,bytes,size));
  m->details.bank_error[index]=ok?0:CONTEXTS_IMPORT_INVALID;m->details.bank_size[index]=ok?size:0;
  if(ok){m->bank_good|=bit;if(!absent){m->bank_present|=bit;m->crc[index]=spectrum_signature_u32((const uint8_t*)bytes+size-4);}}
  else m->bank_good&=(uint8_t)~bit;
  m->digest[index]=ct_hash(bytes,size);return ok;
 }
 if(kind==CONTEXTS_RECORD_NEURAL&&!index){
  bool duplicate=m->neural_seen;m->neural_seen=true;m->details.neural_error=0;
  ok=!duplicate&&(absent||ct_neural(i,bytes,size,identity));m->neural_good=ok&&!absent;
  if(!ok&&!m->details.neural_error)m->details.neural_error=CONTEXTS_IMPORT_INVALID;
  m->digest[2]=ct_hash(bytes,size);return ok;
 }
 return false;
}
static void ct_finish(unsigned i,bool signatures_ready){
 ct_metadata *m=&ct_m[i];contexts_model_details_v1 *d=&m->details;
 if(!signatures_ready){d->temporal_state=d->neural_state=CONTEXTS_IMPORT_FAILED;return;}
 if(!m->bank_seen&&!m->neural_seen)return; /* Older owner: explicit unsupported suffix. */
 if(m->bank_seen!=3||m->bank_good!=3){
  d->temporal_state=CONTEXTS_IMPORT_FAILED;
  for(unsigned bank=0;bank<2;bank++)if(!(m->bank_seen&(1u<<bank)))d->bank_error[bank]=CONTEXTS_IMPORT_INCOMPLETE;
 }else d->temporal_state=m->bank_present?CONTEXTS_IMPORT_READY:CONTEXTS_IMPORT_MISSING;
 d->neural_state=m->neural_good&&d->temporal_state==CONTEXTS_IMPORT_READY?CONTEXTS_IMPORT_READY:
  d->neural_error||!m->neural_seen?CONTEXTS_IMPORT_FAILED:CONTEXTS_IMPORT_MISSING;
 if(!m->neural_seen)d->neural_error=CONTEXTS_IMPORT_INCOMPLETE;
 if(d->temporal_state==CONTEXTS_IMPORT_READY)for(unsigned slot=0;slot<8;slot++){
  d->positive_examples+=i?rt_example_count(&ct_r.library.labels[slot],RT_POSITIVE):st_example_count(&ct_a.library.labels[slot],ST_POSITIVE);
  d->negative_examples+=i?rt_example_count(&ct_r.library.labels[slot],RT_NEGATIVE):st_example_count(&ct_a.library.labels[slot],ST_NEGATIVE);
 }
 uint32_t digest=ct_hash(m->digest,sizeof(m->digest));
 if(!d->temporal_generation||digest!=m->committed_digest){if(++d->temporal_generation==0)++d->temporal_generation;m->committed_digest=digest;}
}
