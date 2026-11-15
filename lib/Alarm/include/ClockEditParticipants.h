#ifndef UTILITIES_CLOCK_EDIT_PARTICIPANTS_H
#define UTILITIES_CLOCK_EDIT_PARTICIPANTS_H
/* Opt-in mixed editor persistence. Original ACE2 profiles remain unchanged.
 * Exact participant bytes, never native structs or collision-prone identities.
 * Decoding is transactional: output views are published only after all checks. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define CLOCK_PARTICIPANT_MAX_BYTES 2048u
#define CLOCK_PARTICIPANT_MAX_ROWS 16u
#define CLOCK_PARTICIPANT_HEADER 44u
#define CLOCK_PARTICIPANT_ROW_HEADER 32u
#define CLOCK_PARTICIPANT_INTENT 1u
#define CLOCK_PARTICIPANT_VERIFIED 2u
typedef struct {
 uint32_t kind,version,config_bytes,history_bytes;
 bool (*validate)(const uint8_t *,const uint8_t *,uint32_t,uint32_t,uint32_t,uint32_t,bool);
} clock_participant_type;
typedef struct {
 uint32_t group,kind,version,floor,zone,provenance;
 const uint8_t *config,*history;
} clock_participant_row;
typedef struct {
 uint32_t phase,generation,requested,previous,prior_phase,prior_requested,prior_previous,count;
 clock_participant_row rows[CLOCK_PARTICIPANT_MAX_ROWS];
} clock_participant_record;
static inline uint32_t clock_participant_read(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline void clock_participant_write(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static inline uint32_t clock_participant_checksum(const uint8_t *p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static inline const clock_participant_type *clock_participant_find(const clock_participant_type *types,size_t count,uint32_t kind,uint32_t version){
 for(size_t i=0;i<count;i++)if(types[i].kind==kind&&types[i].version==version)return &types[i];
 return NULL;
}
static inline bool clock_participant_registry_valid(const clock_participant_type *types,size_t count){
 if(!types||!count||count>CLOCK_PARTICIPANT_MAX_ROWS/2)return false;
 for(size_t i=0;i<count;i++){
  if(!types[i].kind||!types[i].version||!types[i].config_bytes||!types[i].validate||
     types[i].config_bytes>CLOCK_PARTICIPANT_MAX_BYTES||types[i].history_bytes>CLOCK_PARTICIPANT_MAX_BYTES)return false;
  for(size_t j=0;j<i;j++)if(types[j].kind==types[i].kind)return false;
 }return true;
}
static inline bool clock_participant_shape(const clock_participant_record *r,const clock_participant_type *types,size_t count,uint32_t max_time){
 if(!r||!clock_participant_registry_valid(types,count)||r->phase>CLOCK_PARTICIPANT_VERIFIED||!r->generation||
    r->requested>max_time||r->previous>max_time||r->prior_phase>CLOCK_PARTICIPANT_VERIFIED||
    r->prior_requested>max_time||r->prior_previous>max_time||r->count>CLOCK_PARTICIPANT_MAX_ROWS)return false;
 if(r->phase!=CLOCK_PARTICIPANT_INTENT&&(r->prior_phase||r->prior_requested||r->prior_previous))return false;
 if(!r->prior_phase&&(r->prior_requested||r->prior_previous))return false;
 if(!r->phase)return !r->count&&!r->requested&&!r->previous;
 /* Each selected family is explicit even when it has no saved configuration.
  * Its validator distinguishes confirmed absence from virtual/default state. */
 size_t groups=(r->phase==CLOCK_PARTICIPANT_INTENT&&r->prior_phase)?2u:1u;
 if(r->count!=count*groups)return false;
 for(size_t i=0;i<r->count;i++){
  const clock_participant_row *row=&r->rows[i];
  const clock_participant_type *t=clock_participant_find(types,count,row->kind,row->version);
  if(!t||row->group>=groups||!row->config||(!row->history&&t->history_bytes)||row->floor>max_time||
     !t->validate(row->config,row->history,row->floor,row->zone,row->provenance,row->group?r->prior_requested:r->requested,row->group==0))return false;
  for(size_t j=0;j<i;j++)if(r->rows[j].group==row->group&&r->rows[j].kind==row->kind)return false;
 }
 return true;
}
static inline bool clock_participant_decode(clock_participant_record *out,const uint8_t *bytes,size_t size,const clock_participant_type *types,size_t count,uint32_t max_time){
 if(!out||!bytes||size<CLOCK_PARTICIPANT_HEADER+4||size>CLOCK_PARTICIPANT_MAX_BYTES||
    memcmp(bytes,"ACM2",4)||clock_participant_read(bytes+4)!=1||clock_participant_read(bytes+8)!=size||
    clock_participant_read(bytes+size-4)!=clock_participant_checksum(bytes,size-4)||!clock_participant_registry_valid(types,count))return false;
 clock_participant_record r={0};
 uint32_t *fields[]={&r.phase,&r.generation,&r.requested,&r.previous,&r.prior_phase,&r.prior_requested,&r.prior_previous,&r.count};
 for(unsigned i=0;i<8;i++)*fields[i]=clock_participant_read(bytes+12+i*4);
 if(r.count>CLOCK_PARTICIPANT_MAX_ROWS)return false;
 size_t offset=CLOCK_PARTICIPANT_HEADER;
 for(size_t i=0;i<r.count;i++){
  if(offset>size-4||size-4-offset<CLOCK_PARTICIPANT_ROW_HEADER)return false;
  const uint8_t *p=bytes+offset;clock_participant_row *row=&r.rows[i];
  row->group=clock_participant_read(p);row->kind=clock_participant_read(p+4);row->version=clock_participant_read(p+8);
  uint32_t config_size=clock_participant_read(p+12),history_size=clock_participant_read(p+16);
  row->floor=clock_participant_read(p+20);row->zone=clock_participant_read(p+24);row->provenance=clock_participant_read(p+28);
  const clock_participant_type *t=clock_participant_find(types,count,row->kind,row->version);
  if(!t||config_size!=t->config_bytes||history_size!=t->history_bytes)return false;
  offset+=CLOCK_PARTICIPANT_ROW_HEADER;
  if(config_size>size-4-offset)return false;
  row->config=bytes+offset;offset+=config_size;
  if(history_size>size-4-offset)return false;
  row->history=bytes+offset;offset+=history_size;
 }
 if(offset!=size-4||!clock_participant_shape(&r,types,count,max_time))return false;
 *out=r;return true;
}
static inline bool clock_participant_overlap(const void *a,size_t an,const void *b,size_t bn){
 uintptr_t av=(uintptr_t)a,bv=(uintptr_t)b;
 return an&&bn&&(av<=bv?bv-av<an:av-bv<bn);
}
static inline bool clock_participant_encode_scratch(uint8_t *out,size_t capacity,size_t *written,const clock_participant_record *r,const clock_participant_type *types,size_t count,uint32_t max_time,uint8_t *scratch,size_t scratch_capacity){
 if(!out||!written||!clock_participant_shape(r,types,count,max_time))return false;
 size_t size=CLOCK_PARTICIPANT_HEADER+4;
 for(size_t i=0;i<r->count;i++){
  const clock_participant_type *t=clock_participant_find(types,count,r->rows[i].kind,r->rows[i].version);
  size_t extra=CLOCK_PARTICIPANT_ROW_HEADER+(size_t)t->config_bytes+t->history_bytes;
  if(extra>CLOCK_PARTICIPANT_MAX_BYTES-size)return false;
  size+=extra;
 }
 if(size>capacity||!scratch||scratch_capacity<size||clock_participant_overlap(written,sizeof(*written),out,size)||
    clock_participant_overlap(scratch,size,out,size)||clock_participant_overlap(scratch,size,r,sizeof(*r))||
    clock_participant_overlap(scratch,size,types,count*sizeof(*types))||clock_participant_overlap(scratch,size,written,sizeof(*written)))return false;
 for(size_t i=0;i<r->count;i++){
  const clock_participant_type *t=clock_participant_find(types,count,r->rows[i].kind,r->rows[i].version);
  if(clock_participant_overlap(scratch,size,r->rows[i].config,t->config_bytes)||clock_participant_overlap(scratch,size,r->rows[i].history,t->history_bytes))return false;
 }
 /* Stage before touching output: permits input views into a previous record. */
 uint8_t *bytes=scratch;memset(bytes,0,size);memcpy(bytes,"ACM2",4);
 clock_participant_write(bytes+4,1);clock_participant_write(bytes+8,(uint32_t)size);
 const uint32_t fields[]={r->phase,r->generation,r->requested,r->previous,r->prior_phase,r->prior_requested,r->prior_previous,r->count};
 for(unsigned i=0;i<8;i++)clock_participant_write(bytes+12+i*4,fields[i]);
 size_t offset=CLOCK_PARTICIPANT_HEADER;
 for(size_t i=0;i<r->count;i++){
  const clock_participant_row *row=&r->rows[i];const clock_participant_type *t=clock_participant_find(types,count,row->kind,row->version);
  const uint32_t header[]={row->group,row->kind,row->version,t->config_bytes,t->history_bytes,row->floor,row->zone,row->provenance};
  for(unsigned j=0;j<8;j++)clock_participant_write(bytes+offset+j*4,header[j]);
  offset+=CLOCK_PARTICIPANT_ROW_HEADER;memcpy(bytes+offset,row->config,t->config_bytes);offset+=t->config_bytes;
  if(t->history_bytes)memcpy(bytes+offset,row->history,t->history_bytes);
  offset+=t->history_bytes;
 }
 clock_participant_write(bytes+size-4,clock_participant_checksum(bytes,size-4));memcpy(out,bytes,size);*written=size;return true;
}
static inline bool clock_participant_encode(uint8_t *out,size_t capacity,size_t *written,const clock_participant_record *r,const clock_participant_type *types,size_t count,uint32_t max_time){
 uint8_t scratch[CLOCK_PARTICIPANT_MAX_BYTES];
 return clock_participant_encode_scratch(out,capacity,written,r,types,count,max_time,scratch,sizeof(scratch));
}
#endif
