#pragma once
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include <string.h>
typedef struct {const risc_runtime_api_v1 *runtime;bool terminal,clock_seen;uint32_t clock_low;uint64_t clock_high;} ble_buttons_owner;
static inline void bbo_retain(ble_buttons_owner *o){
 if(o->terminal)return;
 o->terminal=true;
 if(risc_runtime_get_api(1)==o->runtime)(void)o->runtime->retain_invocation();
}
static inline bool bbo_live(void *opaque){
 ble_buttons_owner *o=opaque;
 if(o->terminal)return false;
 if(risc_runtime_get_api(1)!=o->runtime){o->terminal=true;return false;}
 return true;
}
static inline bool bbo_init(ble_buttons_owner *o){
 memset(o,0,sizeof(*o));o->runtime=risc_runtime_get_api(1);
 const risc_runtime_api_v1 *r=o->runtime;
 return r&&r->api_version==1&&r->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE&&r->acquire&&r->release&&r->retain_invocation&&r->monotonic_ms&&r->yield_ms;
}
static inline bool bbo_empty(const risc_runtime_capability_v1 *g){return g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation;}
static inline bool bbo_acquire(ble_buttons_owner *o,risc_runtime_capability_v1 *g,const char *name,unsigned version,uint64_t instance){
 if(!bbo_live(o))return false;
 if(!bbo_empty(g)){bbo_retain(o);return false;}
 bool ok=o->runtime->acquire(name,version,instance,g);
 if(!bbo_live(o))return false;
 if((!ok&&!bbo_empty(g))||(ok&&(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation))){bbo_retain(o);return false;}
 return ok;
}
static inline bool bbo_release(ble_buttons_owner *o,risc_runtime_capability_v1 *g){
 if(!bbo_live(o))return false;
 if(bbo_empty(g))return true;
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation){bbo_retain(o);return false;}
 bool ok=o->runtime->release(g);
 if(!bbo_live(o))return false;
 if(!ok||!bbo_empty(g)){bbo_retain(o);return false;}
 return true;
}
static inline bool bbo_now(ble_buttons_owner *o,uint64_t *out){
 if(!bbo_live(o))return false;
 uint32_t low=0;bool ok=o->runtime->monotonic_ms(&low);
 if(!bbo_live(o))return false;
 if(!ok){bbo_retain(o);return false;}
 if(o->clock_seen&&low<o->clock_low){if(o->clock_low-low<UINT32_C(0x80000000)){bbo_retain(o);return false;}o->clock_high+=UINT64_C(0x100000000);}
 o->clock_seen=true;o->clock_low=low;*out=o->clock_high+low;return true;
}
static inline bool bbo_kv_status(ble_buttons_owner *o,int rc){
 if(!bbo_live(o))return false;
 if(rc==RISC_KEY_VALUE_CONTEXT||rc>0||rc<RISC_KEY_VALUE_BUSY){bbo_retain(o);return false;}return true;
}
