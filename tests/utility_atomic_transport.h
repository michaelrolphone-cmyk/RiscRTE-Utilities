#ifndef UTILITY_ATOMIC_TRANSPORT_H
#define UTILITY_ATOMIC_TRANSPORT_H
#include "PortablePreferenceAtomic.h"
#include <assert.h>
typedef struct {char key[16];uint8_t value[64];uint32_t size,kind;} uat_record;
static struct {
 uat_record records[12],pending[4];unsigned count,pending_count;
 uint8_t intent[512];uint32_t intent_size;
 uint64_t revision,operation,token;
 unsigned submits,commits,collects,cancels,yields,release_count,user_intents,rollback_intents,automatic_intents;
 char keys[32][16];unsigned boundaries[32];unsigned fail_on_submit;
 int submit_error,collect_error,receipt_error,cancel_error;
 bool exit_on_submit,change_after_commit,revoked,revoke_on_exit_committed;
 unsigned pending_polls;bool cancelled,cancel_race,exit_pending,release_bad,dirty_submit,bad_receipt,direct;
} uat;
static inline void uat_reset(void){memset(&uat,0,sizeof(uat));uat.revision=1;uat.direct=true;}
static inline int uat_find(const char *key){for(unsigned i=0;i<uat.count;i++)if(!strcmp(uat.records[i].key,key))return (int)i;return -1;}
static inline void uat_store(const char *key,const void *value,uint32_t size){int i=uat_find(key);if(i<0){assert(uat.count<12);i=(int)uat.count++;}strcpy(uat.records[i].key,key);assert(size<=64);memcpy(uat.records[i].value,value,size);uat.records[i].size=size;}
static inline int32_t uat_read(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size){(void)ctx;int i=uat_find(key);*size=0;if(i<0)return RISC_ATOMIC_NOT_FOUND;*size=uat.records[i].size;if(cap<*size)return RISC_ATOMIC_BUFFER_SMALL;memcpy(out,uat.records[i].value,*size);return 0;}
static int32_t uat_state(void *ctx,risc_key_value_atomic_state_v1 *s){(void)ctx;*s=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*s),.flags=3,.namespace_id=1,.max_mutations=8,.max_value_bytes=64,.max_group_bytes=4096,.max_intent_bytes=512,.revision=uat.revision};return 0;}
static int32_t uat_get(void *ctx,uint64_t revision,const char *key,void *out,uint32_t cap,uint32_t *size){if(revision!=uat.revision)return RISC_ATOMIC_STALE;return uat_read(ctx,key,out,cap,size);}
static int32_t uat_submit(void *ctx,uint64_t revision,const risc_key_value_atomic_mutation_v1 *m,uint32_t count,const void *intent,uint32_t size,uint64_t *token){
 (void)ctx;uat.submits++;assert(uat.submits<=32);strcpy(uat.keys[uat.submits-1],m[0].key);uat.boundaries[uat.submits-1]=((const uint8_t*)intent)[11];*token=0;if(revision!=uat.revision)return RISC_ATOMIC_STALE;
 if(uat.fail_on_submit==uat.submits)return RISC_ATOMIC_IO;
 if(uat.submit_error&&!uat.dirty_submit)return uat.submit_error;
 assert(!uat.token&&count&&count<=4&&size<=sizeof(uat.intent));
 memcpy(uat.intent,intent,size);uat.intent_size=size;if(uat.intent[8]==1)uat.user_intents++;else if(uat.intent[8]==3)uat.rollback_intents++;else if(uat.intent[8]==2)uat.automatic_intents++;else assert(0);uat.pending_count=count;
 for(unsigned i=0;i<count;i++){assert((m[i].kind==RISC_ATOMIC_PUT||m[i].kind==RISC_ATOMIC_DELETE)&&m[i].size<=64);uat.pending[i].kind=m[i].kind;strcpy(uat.pending[i].key,m[i].key);if(m[i].size)memcpy(uat.pending[i].value,m[i].value,m[i].size);uat.pending[i].size=m[i].size;}
 *token=uat.token=77;if(uat.exit_on_submit)uat.exit_pending=true;return uat.submit_error;
}
static int32_t uat_collect(void *ctx,uint64_t token,risc_key_value_atomic_receipt_v1 *receipt){
 (void)ctx;assert(token==uat.token&&token);uat.collects++;
 if(uat.collect_error)return uat.collect_error;
 if(uat.pending_polls){uat.pending_polls--;return RISC_ATOMIC_PENDING;}
 int status=uat.receipt_error;
 if(!status&&uat.cancelled&&!uat.cancel_race)status=RISC_ATOMIC_CANCELLED;
 if(!status){for(unsigned i=0;i<uat.pending_count;i++){if(uat.pending[i].kind==RISC_ATOMIC_DELETE){int at=uat_find(uat.pending[i].key);if(at>=0)uat.records[at]=uat.records[--uat.count];}else uat_store(uat.pending[i].key,uat.pending[i].value,uat.pending[i].size);}uat.revision++;uat.operation++;uat.commits++;}
 *receipt=(risc_key_value_atomic_receipt_v1){.struct_size=sizeof(*receipt),.status=status,.revision=uat.revision,.operation_id=status?0:uat.operation};
 if(uat.bad_receipt)receipt->operation_id=0;
 if(uat.change_after_commit&&!status){uat.change_after_commit=false;uat.revision++;}
 uat.token=0;return 0;
}
static int32_t uat_cancel(void *ctx,uint64_t token){(void)ctx;assert(token&&token==uat.token);uat.cancels++;if(uat.cancel_error)return uat.cancel_error;uat.cancelled=true;return 0;}
static const risc_key_value_atomic_v1 uat_api={1,sizeof(uat_api),NULL,uat_state,uat_get,uat_submit,uat_collect,uat_cancel};
static inline void uat_assert_user(const char *key,const void *value,unsigned size,uint64_t revision){
 assert(uat.intent_size==176&&!memcmp(uat.intent,"PFI0001",7)&&uat.intent[8]==PORTABLE_PREFERENCE_USER&&uat.intent[9]==RISC_ATOMIC_PUT);
 assert(!strcmp((const char*)uat.intent+12,key));uint32_t n=0,r=0;for(unsigned i=0;i<4;i++){n|=(uint32_t)uat.intent[32+i]<<(8*i);r|=(uint32_t)uat.intent[36+i]<<(8*i);}
 assert(n==size&&r==revision&&!memcmp(uat.intent+108,value,size));
 assert(portable_preference_crc(uat.intent,172)==((uint32_t)uat.intent[172]|(uint32_t)uat.intent[173]<<8|(uint32_t)uat.intent[174]<<16|(uint32_t)uat.intent[175]<<24));
}
#endif
