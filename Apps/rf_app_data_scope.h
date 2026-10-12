#pragma once
#include "RiscAppDataV1.h"
#include "RiscRuntimeV1.h"
#include <stdbool.h>
#include <string.h>
/* Runtime has one app/shared-data lease per invocation. Keep a stable app-owned
 * table for the broadcast fence, but borrow the native table only for one call.
 * Its opaque revisions remain valid across release/reacquire (AppData ABI).
 * Failed cleanup keeps the exact grant and blocks every subsequent operation. */
typedef struct {
 risc_app_data_v1 api;
 const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 grant;
 uint64_t instance;
 bool retained;
} rf_app_data_scope;
static inline int32_t rf_data_finish(rf_app_data_scope *s,int32_t result){
 if(result==RISC_APP_DATA_RETAINED){s->retained=true;return result;}
 if(!s->runtime->release(&s->grant)){s->retained=true;return RISC_APP_DATA_RETAINED;}
 s->grant=(risc_runtime_capability_v1){0};return result;
}
static inline int32_t rf_data_begin(rf_app_data_scope *s,const risc_app_data_v1 **source){
 *source=NULL;
 if(s->retained)return RISC_APP_DATA_RETAINED;
 s->grant=(risc_runtime_capability_v1){.struct_size=sizeof(s->grant)};
 if(!s->runtime->acquire(RISC_APP_DATA_CAPABILITY,1,s->instance,&s->grant)){
  if(s->grant.struct_size!=sizeof(s->grant)||s->grant.api||s->grant.slot||s->grant.generation){s->retained=true;return RISC_APP_DATA_RETAINED;}
  return RISC_APP_DATA_UNAVAILABLE;
 }
 const risc_app_data_v1 *p=s->grant.api;
 if(!p||p->api_version!=1||p->struct_size<sizeof(*p)||!p->stat||!p->read||!p->replace)return rf_data_finish(s,RISC_APP_DATA_INVALID);
 *source=p;return RISC_APP_DATA_OK;
}
static int32_t rf_data_stat(void *context,const char *name,uint32_t *size,uint64_t *revision){
 rf_app_data_scope *s=context;const risc_app_data_v1 *p;
 if(size)*size=0;
 if(revision)*revision=0;
 int32_t result=rf_data_begin(s,&p);if(result)return result;
 result=rf_data_finish(s,p->stat(p->context,name,size,revision));
 if(result==RISC_APP_DATA_RETAINED){if(size)*size=0;
 if(revision)*revision=0;}
 return result;
}
static int32_t rf_data_read(void *context,const char *name,uint64_t expected,void *bytes,uint32_t capacity,uint32_t *size,uint64_t *revision){
 rf_app_data_scope *s=context;const risc_app_data_v1 *p;
 if(size)*size=0;
 if(revision)*revision=0;
 int32_t result=rf_data_begin(s,&p);if(result)return result;
 result=rf_data_finish(s,p->read(p->context,name,expected,bytes,capacity,size,revision));
 if(result==RISC_APP_DATA_RETAINED){if(size)*size=0;
 if(revision)*revision=0;}
 return result;
}
static int32_t rf_data_replace(void *context,const char *name,uint64_t expected,const void *bytes,uint32_t size){
 rf_app_data_scope *s=context;const risc_app_data_v1 *p;
 int32_t result=rf_data_begin(s,&p);if(result)return result;
 return rf_data_finish(s,p->replace(p->context,name,expected,bytes,size));
}
static inline const risc_app_data_v1 *rf_data_bind(rf_app_data_scope *s,const risc_runtime_api_v1 *runtime,uint64_t instance){
 *s=(rf_app_data_scope){.runtime=runtime,.instance=instance,.api={1,sizeof(risc_app_data_v1),s,rf_data_stat,rf_data_read,rf_data_replace}};
 return &s->api;
}
