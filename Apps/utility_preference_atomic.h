#ifndef UTILITY_PREFERENCE_ATOMIC_H
#define UTILITY_PREFERENCE_ATOMIC_H
/* Explicit X4 controller adapter. The System helper and ABI are pinned by the
 * standalone experimental build. No legacy put is reclassified as intent. */
#if !defined(PORTABLE_UTILITIES_ATOMIC_PREFERENCES) || !defined(PORTABLE_UTILITIES_TARGET_X4)
#error "Utility atomic preferences require the explicit X4 experimental profile"
#endif
#include "PortablePreferenceAtomic.h"
#include "RiscRuntimeV1.h"
#include "RiscRuntimeHostControlV1.h"
typedef struct {
 portable_preference_atomic edit;
 risc_runtime_capability_v1 grant;
 const risc_runtime_api_v1 *runtime;
 bool (*alive)(void *);void (*retain)(void *);void *context;
 bool retained;
} utility_preference_atomic;
static inline bool utility_preference_guard(void *context){
 utility_preference_atomic *s=context;
 return s&&!s->retained&&s->alive&&s->alive(s->context)&&risc_runtime_get_api(1)==s->runtime;
}
static inline int utility_preference_result(utility_preference_atomic *s,int rc){
 if(rc==RISC_ATOMIC_RETAINED||!utility_preference_guard(s)){
  if(!s->retained){s->retained=true;if(s->retain)s->retain(s->context);}
  return RISC_ATOMIC_RETAINED;
 }
 return rc;
}
static inline int utility_preference_close(utility_preference_atomic *s){
 if(!utility_preference_guard(s)||s->edit.token)return utility_preference_result(s,RISC_ATOMIC_RETAINED);
 if(s->grant.api){
  risc_runtime_capability_v1 released=s->grant;
  bool ok=s->runtime->release(&released);
  if(!utility_preference_guard(s)||!ok||released.struct_size!=sizeof(released)||released.api||released.slot||released.generation)
   return utility_preference_result(s,RISC_ATOMIC_RETAINED);
  s->grant=(risc_runtime_capability_v1){0};
 }
 return RISC_ATOMIC_OK;
}
static inline int utility_preference_begin(utility_preference_atomic *s,const risc_runtime_api_v1 *rt,const char *key,
 bool (*alive)(void *),void (*retain)(void *),void *context){
 if(s->grant.api||s->edit.token||s->retained)return RISC_ATOMIC_RETAINED;
 memset(s,0,sizeof(*s));s->runtime=rt;s->alive=alive;s->retain=retain;s->context=context;
 if(!rt||!rt->acquire||!rt->release||!rt->yield_ms)return RISC_ATOMIC_INVALID;
 if(!utility_preference_guard(s))return utility_preference_result(s,RISC_ATOMIC_RETAINED);
 s->grant=(risc_runtime_capability_v1){.struct_size=sizeof(s->grant)};
 bool got=rt->acquire(RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,1,&s->grant);
 if(!utility_preference_guard(s))return utility_preference_result(s,RISC_ATOMIC_RETAINED);
 if(!got){
  if(s->grant.api||s->grant.slot||s->grant.generation)return utility_preference_result(s,RISC_ATOMIC_RETAINED);
  return RISC_ATOMIC_INVALID;
 }
 if(s->grant.struct_size!=sizeof(s->grant)||!s->grant.slot||!s->grant.generation||!s->grant.api)
  return utility_preference_result(s,RISC_ATOMIC_RETAINED);
 int rc=portable_preference_atomic_begin(&s->edit,s->grant.api,key,PORTABLE_PREFERENCE_USER,utility_preference_guard,s);
 return utility_preference_result(s,rc);
}
/* Accepted tokens stay in this invocation until a checked terminal collect.
 * Exit requests cancel once but must still collect; a committed cancellation
 * race is success. Busy before acceptance is returned without automatic retry. */
static inline int utility_preference_wait(utility_preference_atomic *s,bool cancel){
 int rc=s->edit.status;
 while(s->edit.token){
  bool exit=cancel||risc_runtime_exit_requested_v1(s->runtime);
  if(!utility_preference_guard(s))return utility_preference_result(s,RISC_ATOMIC_RETAINED);
  rc=exit?portable_preference_atomic_cancel(&s->edit):portable_preference_atomic_collect(&s->edit);
  rc=utility_preference_result(s,rc);if(rc==RISC_ATOMIC_RETAINED)return rc;
  if(s->edit.token){s->runtime->yield_ms(5);if(!utility_preference_guard(s))return utility_preference_result(s,RISC_ATOMIC_RETAINED);}
 }
 return rc;
}
static inline int utility_preference_cancel(utility_preference_atomic *s){
 if(!utility_preference_guard(s))return utility_preference_result(s,RISC_ATOMIC_RETAINED);
 int rc=portable_preference_atomic_cancel(&s->edit);
 if(rc==RISC_ATOMIC_PENDING)rc=utility_preference_wait(s,true);
 rc=utility_preference_result(s,rc);
 return rc==RISC_ATOMIC_RETAINED?rc:utility_preference_close(s)==RISC_ATOMIC_OK?rc:RISC_ATOMIC_RETAINED;
}
static inline int utility_preference_submit(utility_preference_atomic *s,const void *value,uint32_t size){
 int rc=portable_preference_atomic_prepare(&s->edit,value,size);
 if(rc==PORTABLE_PREFERENCE_READY){
  if(risc_runtime_exit_requested_v1(s->runtime))return utility_preference_cancel(s);
  rc=portable_preference_atomic_submit(&s->edit);
 }
 rc=utility_preference_result(s,rc);
 if(rc==RISC_ATOMIC_PENDING)rc=utility_preference_wait(s,false);
 return rc;
}
static inline int utility_preference_finish(utility_preference_atomic *s,int rc){
 if(rc==RISC_ATOMIC_RETAINED)return utility_preference_result(s,rc);
 return utility_preference_close(s)==RISC_ATOMIC_OK?rc:RISC_ATOMIC_RETAINED;
}
#endif
