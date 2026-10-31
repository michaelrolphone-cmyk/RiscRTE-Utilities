#ifndef BATTERY_PREFERENCE_ADAPTER_H
#define BATTERY_PREFERENCE_ADAPTER_H
#include <stdbool.h>
#include "RiscPreferenceStoreV1.h"
/* Explicit capability-contract adaptation, never a storage/backend selection.
 * Atomic KV's native-durable copied-worker contract is stronger than the
 * preference controller's copied-input contract. Preserve every callback,
 * token, status, revision and byte; only project proven state flags. */
typedef struct {
 const risc_key_value_atomic_v1 *source;
 risc_preference_store_v1 view;
} battery_preference_adapter;
static inline int32_t battery_preference_state(void*c,risc_key_value_atomic_state_v1*out){
 battery_preference_adapter*a=c;
 int32_t rc=a->source->state(a->source->context,out);
 if(rc==RISC_ATOMIC_OK){
  if(out->struct_size<sizeof(*out)||(out->flags&(RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC))!=(RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC))return RISC_ATOMIC_INVALID;
  out->flags|=RISC_PREFERENCE_STORE_COPIED_INPUT|RISC_PREFERENCE_STORE_ATOMIC_GROUP;
 }
 return rc;
}
static inline int32_t battery_preference_get(void*c,uint64_t r,const char*k,void*b,uint32_t n,uint32_t*z){battery_preference_adapter*a=c;return a->source->get(a->source->context,r,k,b,n,z);}
static inline int32_t battery_preference_submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*i,uint32_t z,uint64_t*t){battery_preference_adapter*a=c;return a->source->submit(a->source->context,r,m,n,i,z,t);}
static inline int32_t battery_preference_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){battery_preference_adapter*a=c;return a->source->collect(a->source->context,t,r);}
static inline int32_t battery_preference_cancel(void*c,uint64_t t){battery_preference_adapter*a=c;return a->source->cancel(a->source->context,t);}
static inline bool battery_preference_adapt(battery_preference_adapter*a,const risc_key_value_atomic_v1*source){
 if(!a||!source||source->api_version!=1||source->struct_size<sizeof(*source)||!source->state||!source->get||!source->submit||!source->collect||!source->cancel)return false;
 a->source=source;a->view=(risc_preference_store_v1){1,sizeof(a->view),a,battery_preference_state,battery_preference_get,battery_preference_submit,battery_preference_collect,battery_preference_cancel};return true;
}
#endif
