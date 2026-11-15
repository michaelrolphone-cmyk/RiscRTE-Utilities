#pragma once
#include "ble_buttons_owner.h"
#include "RiscPowerKeyV1.h"
#include "RiscResidentIdleV1.h"
typedef struct {uint64_t activity;uint32_t idle_ms;bool neutral,idle_known,idle_supported;} ble_buttons_policy;
static inline bool bbpolicy_load(ble_buttons_policy *p,ble_buttons_owner *o,uint64_t now){
 p->activity=now;p->neutral=false;p->idle_known=false;p->idle_ms=60000;
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!bbo_acquire(o,&g,RISC_KEY_VALUE_CAPABILITY,1,1))return bbo_live(o);
 const risc_key_value_v1 *kv=g.api;
 if(kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get){
  uint8_t b[5]={0};uint32_t n=0;int rc=kv->get(kv->context,"sleep_idle",b,sizeof(b),&n);
  if(!bbo_kv_status(o,rc))return false;
  if(rc==RISC_KEY_VALUE_NOT_FOUND&&n==0)p->idle_known=true;
  if(rc==0&&n==5&&b[0]==0x54&&b[1]==1&&b[4]==(uint8_t)(b[2]^b[3]^0xa5)){
   unsigned seconds=(unsigned)b[2]|((unsigned)b[3]<<8);if(seconds>=5&&seconds<=3600){p->idle_ms=seconds*1000;p->idle_known=true;}
  }
 }
 return bbo_release(o,&g);
}
/* Caller owns semantic Back. A new host event can never be mapped to a
 * different manual action by an ordinary POLL: idle uses negotiated key-free
 * reasons only, after every native owner and UI has settled. */
static inline bool bbpolicy_key(ble_buttons_policy *p,ble_buttons_owner *o,const risc_power_key_api_v1 *key,bool *back,uint64_t now){
 *back=false;if(!key)return true;if(!bbo_live(o))return false;
 uint32_t bits=0;bool ok=key->events(key->context,&bits);if(!bbo_live(o))return false;
 if(!ok||(bits&~3u)){bbo_retain(o);return false;}
 if(!bits){p->neutral=true;return true;}
 *back=p->neutral&&bits==RISC_POWER_KEY_SHORT_PRESS;p->neutral=false;p->activity=now;return true;
}
static inline bool bbpolicy_due(const ble_buttons_policy *p,uint64_t now){return p->idle_known&&now>=p->activity&&now-p->activity>=p->idle_ms;}
