#ifndef RF_TEMPORAL_STORE_H
#define RF_TEMPORAL_STORE_H
#include "rf_temporal.h"
#include "rf_signature_store.h"
#define RT_BANK_MAX 61252u
#define RT_BANK_MIN 1348u
#define RT_BANK_HEADER 128u
#define RT_FRAME_BYTES 39u
#define RT_LABEL_BYTES 64u
#define RT_EXAMPLE_BYTES 40u
/* Levels are canonical whole dB values: one byte covers -120..+10dB.
 * This reserves two bytes for exact per-observation millisecond deltas, even
 * across long display gaps. The first delta is zero; every later delta is
 *1..65535, based on the first real uint32 timestamp in the example header.
 * Nonmonotonic timestamps and uint32 wrap are invalid. No interpolation or occupied duration is encoded.
 * Two independent collections, four labels each. Variable frame payloads make
 * deletion reclaim committed bytes. No cross-file atomicity or KV migration. */
static inline void rt_put16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static inline unsigned rt_u16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static inline size_t rt_bank_encode(const rt_library *library,unsigned bank,uint8_t *out,size_t capacity){
 if(!library||!out||bank>=2||capacity<RT_BANK_MIN||!rf_identity_valid(&library->identity))return 0;
 size_t size=RT_BANK_MIN;for(unsigned i=bank*4;i<bank*4+4;i++){if(!rt_label_valid(&library->labels[i]))return 0;for(unsigned j=0;j<RT_EXAMPLES;j++)if(library->labels[i].examples[j].id)size+=(size_t)library->labels[i].examples[j].count*RT_FRAME_BYTES;}
 if(size>capacity||size>RT_BANK_MAX)return 0;
 memset(out,0,size);memcpy(out,"RFT1",4);out[4]=1;out[5]=(uint8_t)bank;out[6]=RT_FRAMES;out[7]=RT_EXAMPLES;out[8]=RT_BANDS;out[9]=RT_PRE;out[10]=RT_FRAME_BYTES;out[11]=2;/* uint16 exact millisecond deltas; 1dB levels */
 rf_signature_put32(out+20,library->generation[bank]);rf_signature_put32(out+24,(uint32_t)size);rf_identity_encode(&library->identity,out+32);
 size_t at=RT_BANK_HEADER;
 for(unsigned i=bank*4;i<bank*4+4;i++){const rt_label*l=&library->labels[i];uint8_t *p=out+at;p[0]=l->present;p[1]=l->shift_limit;if(l->present){memcpy(p+4,l->name,strlen(l->name));rf_signature_put32(p+24,l->next_id);}at+=64;
  for(unsigned j=0;j<RT_EXAMPLES;j++){const rt_example*e=&l->examples[j];p=out+at;at+=40;if(!e->id)continue;rf_signature_put32(p,e->id);p[4]=e->count;p[5]=e->pre;p[6]=e->kind;p[7]=e->flags;p[8]=e->onset;p[9]=e->end;p[10]=e->impacts;p[11]=(uint8_t)((e->peak_db+12000)/100);memcpy(p+12,e->impact_at,8);rf_signature_put32(p+20,e->duration_ms);rf_signature_put32(p+24,e->attack_ms);rf_signature_put32(p+28,e->decay_ms);rt_put16(p+32,e->peak_coord);rf_signature_put32(p+34,e->frames[0].timestamp_ms);
   for(unsigned k=0;k<e->count;k++){const rt_frame*f=&e->frames[k];p=out+at;memcpy(p,f->shape,32);p[32]=(uint8_t)((f->level_db+12000)/100);rt_put16(p+33,f->peak_coord);p[35]=f->flux;p[36]=f->flags;rt_put16(p+37,k?f->timestamp_ms-e->frames[k-1u].timestamp_ms:0);at+=RT_FRAME_BYTES;}
  }
 }
 if(at+4!=size)return 0;
 rf_signature_put32(out+at,rf_signature_crc(out,at));return size;
}
/* Two-pass validation keeps all existing labels unchanged on malformed input.
 * The largest stack object is one example, not a complete collection. */
static inline bool rt_bank_decode(rt_library *library,unsigned bank,const uint8_t *in,size_t size){
 if(!library||!in||bank>=2||size<RT_BANK_MIN||size>RT_BANK_MAX||memcmp(in,"RFT1",4)||in[4]!=1||in[5]!=bank||in[6]!=RT_FRAMES||in[7]!=RT_EXAMPLES||in[8]!=RT_BANDS||in[9]!=RT_PRE||in[10]!=RT_FRAME_BYTES||in[11]!=2||rf_signature_u32(in+24)!=size||rf_signature_u32(in+size-4)!=rf_signature_crc(in,size-4))return false;
 for(unsigned i=12;i<20;i++)if(in[i])return false;
 for(unsigned i=28;i<32;i++)if(in[i])return false;
 for(unsigned i=96;i<RT_BANK_HEADER;i++)if(in[i])return false;
 rf_capture_identity identity;if(!rf_identity_decode(&identity,in+32,RF_IDENTITY_SIZE))return false;
 /* Never combine banks recorded at different RF tuning/gain/filter settings. */
 if(rf_identity_valid(&library->identity)&&!rf_identity_equal(&library->identity,&identity))return false;
 for(unsigned pass=0;pass<2;pass++){size_t at=RT_BANK_HEADER;
  for(unsigned slot=bank*4;slot<bank*4+4;slot++){if(at+64>size-4)return false;const uint8_t*p=in+at;bool present=p[0]!=0;uint8_t shift=p[1];char name[17];memcpy(name,p+4,17);uint32_t next=rf_signature_u32(p+24);if(p[0]>1||shift>2||p[2]||p[3]||name[16])return false;for(unsigned i=21;i<24;i++)if(p[i])return false;for(unsigned i=28;i<64;i++)if(p[i])return false;unsigned z=0;while(z<17&&name[z])z++;for(;z<17;z++)if(name[z])return false;if(present?(!rf_signature_name_valid(name)||!next):(name[0]||next||shift))return false;at+=64;uint32_t ids[6]={0};
   if(pass){memset(&library->labels[slot],0,sizeof(rt_label));library->labels[slot].present=present;library->labels[slot].shift_limit=shift;library->labels[slot].next_id=next;memcpy(library->labels[slot].name,name,17);}
   for(unsigned ex=0;ex<6;ex++){if(at+40>size-4)return false;p=in+at;at+=40;rt_example e={0};e.id=rf_signature_u32(p);if(!e.id){for(unsigned k=4;k<40;k++)if(p[k])return false;continue;}
    if(!present||e.id>=next)return false;
    for(unsigned k=0;k<ex;k++)if(ids[k]==e.id)return false;
    ids[ex]=e.id;
    e.count=p[4];e.pre=p[5];e.kind=p[6];e.flags=p[7];e.onset=p[8];e.end=p[9];e.impacts=p[10];if(p[11]>130)return false;e.peak_db=(int16_t)((int)p[11]*100-12000);memcpy(e.impact_at,p+12,8);e.duration_ms=rf_signature_u32(p+20);e.attack_ms=rf_signature_u32(p+24);e.decay_ms=rf_signature_u32(p+28);e.peak_coord=(uint16_t)rt_u16(p+32);uint32_t timestamp=rf_signature_u32(p+34);if(p[38]||p[39])return false;
    if(e.count>64||at+(size_t)e.count*RT_FRAME_BYTES>size-4)return false;
    for(unsigned k=0;k<e.count;k++){p=in+at;rt_frame*f=&e.frames[k];memcpy(f->shape,p,32);if(p[32]>130)return false;f->level_db=(int16_t)((int)p[32]*100-12000);f->peak_coord=(uint16_t)rt_u16(p+33);f->flux=p[35];f->flags=p[36];unsigned delta=rt_u16(p+37);if((k&&!delta)||(!k&&delta)||UINT32_MAX-timestamp<delta)return false;timestamp+=delta;f->timestamp_ms=timestamp;at+=RT_FRAME_BYTES;}
    if(!rt_example_valid(&e))return false;
    if(pass)library->labels[slot].examples[ex]=e;
   }
  }
  if(at!=size-4)return false;
 }
 library->generation[bank]=rf_signature_u32(in+20);library->identity=identity;return true;
}
#endif
