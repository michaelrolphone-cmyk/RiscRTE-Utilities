#ifndef SPECTRUM_TEMPORAL_STORE_H
#define SPECTRUM_TEMPORAL_STORE_H
#include "spectrum_temporal.h"
#include "spectrum_signature_store.h"
#define ST_BANK_MAX 59652u
#define ST_BANK_MIN 1284u
#define ST_BANK_HEADER 64u
#define ST_LABEL_BYTES 64u
#define ST_EXAMPLE_BYTES 40u
/* Two independent collections, four labels each. Variable frame payloads make
 * deletion reclaim committed bytes. No cross-file atomicity or KV migration. */
static inline void st_put16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static inline unsigned st_u16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static inline size_t st_bank_encode(const st_library *library,unsigned bank,uint8_t *out,size_t capacity){
 if(!library||!out||bank>=2||capacity<ST_BANK_MIN)return 0;
 size_t size=ST_BANK_MIN;for(unsigned i=bank*4;i<bank*4+4;i++){if(!st_label_valid(&library->labels[i]))return 0;for(unsigned j=0;j<ST_EXAMPLES;j++)if(library->labels[i].examples[j].id)size+=(size_t)library->labels[i].examples[j].count*38;}
 if(size>capacity||size>ST_BANK_MAX)return 0;
 memset(out,0,size);memcpy(out,"SQT2",4);out[4]=1;out[5]=(uint8_t)bank;out[6]=64;out[7]=6;spectrum_signature_put32(out+8,16000);st_put16(out+12,512);out[14]=SPECTRUM_DSP_HANN;out[15]=64;out[16]=ST_PRE;out[17]=3;out[18]=2;out[19]=1;spectrum_signature_put32(out+20,library->generation[bank]);spectrum_signature_put32(out+24,(uint32_t)size);
 size_t at=64;
 for(unsigned i=bank*4;i<bank*4+4;i++){const st_label*l=&library->labels[i];uint8_t *p=out+at;p[0]=l->present;p[1]=l->shift_limit;if(l->present){memcpy(p+4,l->name,strlen(l->name));spectrum_signature_put32(p+24,l->next_id);}at+=64;
  for(unsigned j=0;j<ST_EXAMPLES;j++){const st_example*e=&l->examples[j];p=out+at;at+=40;if(!e->id)continue;spectrum_signature_put32(p,e->id);p[4]=e->count;p[5]=e->pre;p[6]=e->kind;p[7]=e->flags;p[8]=e->onset;p[9]=e->end;p[10]=e->impacts;memcpy(p+12,e->impact_at,8);st_put16(p+20,e->duration_ms);st_put16(p+22,e->attack_ms);st_put16(p+24,e->decay_ms);st_put16(p+26,e->peak_hz);st_put16(p+28,(unsigned)(e->peak_db+12000));
   for(unsigned k=0;k<e->count;k++){const st_frame*f=&e->frames[k];p=out+at;memcpy(p,f->shape,32);st_put16(p+32,(unsigned)(f->level_db+12000));st_put16(p+34,f->peak_hz);p[36]=f->flux;p[37]=f->flags;at+=38;}
  }
 }
 if(at+4!=size)return 0;
 spectrum_signature_put32(out+at,spectrum_signature_crc(out,at));return size;
}
/* Two-pass validation keeps all existing labels unchanged on malformed input.
 * The largest stack object is one example, not a complete collection. */
static inline bool st_bank_decode(st_library *library,unsigned bank,const uint8_t *in,size_t size){
 if(!library||!in||bank>=2||size<ST_BANK_MIN||size>ST_BANK_MAX||memcmp(in,"SQT2",4)||in[4]!=1||in[5]!=bank||in[6]!=64||in[7]!=6||spectrum_signature_u32(in+8)!=16000||st_u16(in+12)!=512||in[14]!=SPECTRUM_DSP_HANN||in[15]!=64||in[16]!=ST_PRE||in[17]!=3||in[18]!=2||in[19]!=1||spectrum_signature_u32(in+24)!=size||spectrum_signature_u32(in+size-4)!=spectrum_signature_crc(in,size-4))return false;
 for(unsigned i=28;i<64;i++)if(in[i])return false;
 for(unsigned pass=0;pass<2;pass++){size_t at=64;
  for(unsigned slot=bank*4;slot<bank*4+4;slot++){if(at+64>size-4)return false;const uint8_t*p=in+at;bool present=p[0]!=0;uint8_t shift=p[1];char name[17];memcpy(name,p+4,17);uint32_t next=spectrum_signature_u32(p+24);if(p[0]>1||shift>2||p[2]||p[3]||name[16])return false;for(unsigned i=21;i<24;i++)if(p[i])return false;for(unsigned i=28;i<64;i++)if(p[i])return false;unsigned z=0;while(z<17&&name[z])z++;for(;z<17;z++)if(name[z])return false;if(present?(!spectrum_signature_name_valid(name)||!next):(name[0]||next||shift))return false;at+=64;uint32_t ids[6]={0};
   if(pass){memset(&library->labels[slot],0,sizeof(st_label));library->labels[slot].present=present;library->labels[slot].shift_limit=shift;library->labels[slot].next_id=next;memcpy(library->labels[slot].name,name,17);}
   for(unsigned ex=0;ex<6;ex++){if(at+40>size-4)return false;p=in+at;at+=40;st_example e={0};e.id=spectrum_signature_u32(p);if(!e.id){for(unsigned k=4;k<40;k++)if(p[k])return false;continue;}
    if(!present||e.id>=next)return false;
    for(unsigned k=0;k<ex;k++)if(ids[k]==e.id)return false;
    ids[ex]=e.id;
    e.count=p[4];e.pre=p[5];e.kind=p[6];e.flags=p[7];e.onset=p[8];e.end=p[9];e.impacts=p[10];if(p[11])return false;memcpy(e.impact_at,p+12,8);e.duration_ms=(uint16_t)st_u16(p+20);e.attack_ms=(uint16_t)st_u16(p+22);e.decay_ms=(uint16_t)st_u16(p+24);e.peak_hz=(uint16_t)st_u16(p+26);if(st_u16(p+28)>13000)return false;e.peak_db=(int16_t)((int)st_u16(p+28)-12000);for(unsigned k=30;k<40;k++)if(p[k])return false;
    if(e.count>64||at+(size_t)e.count*38>size-4)return false;
    for(unsigned k=0;k<e.count;k++){p=in+at;st_frame*f=&e.frames[k];memcpy(f->shape,p,32);if(st_u16(p+32)>13000)return false;f->level_db=(int16_t)((int)st_u16(p+32)-12000);f->peak_hz=(uint16_t)st_u16(p+34);f->flux=p[36];f->flags=p[37];at+=38;}
    if(!st_example_valid(&e))return false;
    if(pass)library->labels[slot].examples[ex]=e;
   }
  }
  if(at!=size-4)return false;
 }
 library->generation[bank]=spectrum_signature_u32(in+20);return true;
}
#endif
