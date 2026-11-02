#ifndef SCANNER_PREFERENCES_H
#define SCANNER_PREFERENCES_H
/* Shared explicit Scanner settings controller. No device/backend selection.
 * Alias retries retain captured expected bytes AND revision, never rebase.
 * Only explicit enable commits radio intent; physical failure compensates only
 * against the confirmed operation's revision. Pending tokens own their grants. */
#include "RiscRuntimeV1.h"
#include "PortableRadioPolicy.h"
#include "PortableBluetoothControl.h"
#ifndef PORTABLE_PREFERENCE_STORE
#define PORTABLE_PREFERENCE_STORE
#endif
#include "PortablePreferenceRollback.h"
#include "battery_preference_adapter.h"
#include "ble_sensor_names.h"
enum { SCANNER_PREF_OK=0, SCANNER_PREF_UNAVAILABLE=1, SCANNER_PREF_ERROR=2,
 SCANNER_PREF_PENDING=3, SCANNER_PREF_RETAINED=4, SCANNER_PREF_UNCERTAIN=5,
 SCANNER_PREF_CONFLICT=6 };
typedef struct scanner_preferences {
 const risc_runtime_api_v1 *runtime;
 bool (*alive)(void*); void (*retain)(void*); void *context;
 risc_runtime_capability_v1 preference,radio,storage;
 battery_preference_adapter adapter;
 const risc_preference_store_v1 *api,*source_api;
 risc_preference_store_v1 checked_api;
 portable_preference_atomic alias,enable;
 portable_preference_rollback rollback;
 uint8_t alias_type,alias_address[6],radio_prior;
 bool retained,in_call,alias_active,alias_prepared,alias_done;
 unsigned phase; /* 0 idle, 1 alias, 2 radio commit, 3 compensation */
 int last;
} scanner_preferences;
static inline bool scanner_preferences_guard(void *context){
 scanner_preferences*s=context;
 return s&&!s->retained&&!s->in_call&&s->runtime&&s->alive&&s->alive(s->context)&&risc_runtime_get_api(1)==s->runtime;
}
static inline int scanner_preferences_retain(scanner_preferences*s){
 if(s&&!s->retained){s->retained=true;if(s->retain)s->retain(s->context);}
 return SCANNER_PREF_RETAINED;
}
static inline int scanner_preferences_acquire(scanner_preferences*s,const char*name,uint32_t instance,risc_runtime_capability_v1*g){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(g->api){if(g->struct_size!=sizeof(*g)||!g->slot||!g->generation)return scanner_preferences_retain(s);return SCANNER_PREF_OK;}
 if(g->slot||g->generation)return scanner_preferences_retain(s);
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 s->in_call=true;bool ok=s->runtime->acquire(name,1,instance,g);s->in_call=false;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(g->struct_size!=sizeof(*g)||(!ok&&(g->api||g->slot||g->generation))||(ok&&(!g->api||!g->slot||!g->generation)))return scanner_preferences_retain(s);
 return ok?SCANNER_PREF_OK:SCANNER_PREF_UNAVAILABLE;
}
static inline int scanner_preferences_release(scanner_preferences*s,risc_runtime_capability_v1*g){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(!g->api)return g->slot||g->generation?scanner_preferences_retain(s):SCANNER_PREF_OK;
 if(g->struct_size!=sizeof(*g)||!g->slot||!g->generation)return scanner_preferences_retain(s);
 risc_runtime_capability_v1 copy=*g;
 s->in_call=true;bool ok=s->runtime->release(&copy);s->in_call=false;
 if(!scanner_preferences_guard(s)||!ok||copy.struct_size!=sizeof(copy)||copy.api||copy.slot||copy.generation)return scanner_preferences_retain(s);
 *g=(risc_runtime_capability_v1){0};return SCANNER_PREF_OK;
}
/* Checked forwarding also rejects recursive application entry from providers. */
#define SCANNER_PREF_FORWARD(call) \
 if(!scanner_preferences_guard(s))return RISC_ATOMIC_RETAINED; \
 s->in_call=true;int rc=(call);s->in_call=false; \
 return scanner_preferences_guard(s)?rc:RISC_ATOMIC_RETAINED
static inline int32_t scanner_pref_state(void*c,risc_key_value_atomic_state_v1*out){scanner_preferences*s=c;SCANNER_PREF_FORWARD(s->source_api->state(s->source_api->context,out));}
static inline int32_t scanner_pref_get(void*c,uint64_t r,const char*k,void*b,uint32_t n,uint32_t*z){scanner_preferences*s=c;SCANNER_PREF_FORWARD(s->source_api->get(s->source_api->context,r,k,b,n,z));}
static inline int32_t scanner_pref_submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*i,uint32_t z,uint64_t*t){scanner_preferences*s=c;SCANNER_PREF_FORWARD(s->source_api->submit(s->source_api->context,r,m,n,i,z,t));}
static inline int32_t scanner_pref_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){scanner_preferences*s=c;SCANNER_PREF_FORWARD(s->source_api->collect(s->source_api->context,t,r));}
static inline int32_t scanner_pref_cancel(void*c,uint64_t t){scanner_preferences*s=c;SCANNER_PREF_FORWARD(s->source_api->cancel(s->source_api->context,t));}
#undef SCANNER_PREF_FORWARD
static inline int scanner_preferences_open(scanner_preferences*s){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(s->preference.api){
  if(s->preference.struct_size!=sizeof(s->preference)||!s->preference.slot||!s->preference.generation||s->api!=&s->checked_api||!portable_preference_atomic_api_valid(s->source_api))return scanner_preferences_retain(s);
  return SCANNER_PREF_OK;
 }
 int rc=scanner_preferences_acquire(s,RISC_PREFERENCE_STORE_CAPABILITY,1,&s->preference);
 if(rc==SCANNER_PREF_UNAVAILABLE){
  rc=scanner_preferences_acquire(s,RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,&s->preference);
  if(rc)return rc;
  if(!battery_preference_adapt(&s->adapter,s->preference.api))return scanner_preferences_retain(s);
  s->source_api=&s->adapter.view;
 }else if(!rc)s->source_api=s->preference.api;
 if(rc)return rc;
 if(!portable_preference_atomic_api_valid(s->source_api))return scanner_preferences_retain(s);
 s->checked_api=(risc_preference_store_v1){1,sizeof(s->checked_api),s,scanner_pref_state,scanner_pref_get,scanner_pref_submit,scanner_pref_collect,scanner_pref_cancel};s->api=&s->checked_api;
 return SCANNER_PREF_OK;
}
static inline int scanner_preferences_result(scanner_preferences*s,int rc){
 if(!scanner_preferences_guard(s)||rc==RISC_ATOMIC_RETAINED)return scanner_preferences_retain(s);
 if(rc==RISC_ATOMIC_PENDING||rc==PORTABLE_PREFERENCE_READY||rc==RISC_ATOMIC_BUSY)return SCANNER_PREF_PENDING;
 if(rc==RISC_ATOMIC_BUFFER_SMALL||rc==RISC_ATOMIC_NOT_FOUND)return SCANNER_PREF_ERROR;
 if(!portable_preference_atomic_terminal(rc))return scanner_preferences_retain(s);
 if(rc==RISC_ATOMIC_OK)return SCANNER_PREF_OK;
 if(rc==RISC_ATOMIC_UNCERTAIN)return SCANNER_PREF_UNCERTAIN;
 if(rc==RISC_ATOMIC_CONFLICT||rc==RISC_ATOMIC_STALE)return SCANNER_PREF_CONFLICT;
 return SCANNER_PREF_ERROR;
}
static inline int scanner_preferences_init(scanner_preferences*s,const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*context){
 if(!s)return SCANNER_PREF_ERROR;
 memset(s,0,sizeof(*s));s->runtime=rt;s->alive=alive;s->retain=retain;s->context=context;
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 return SCANNER_PREF_OK;
}
static inline int scanner_preferences_close_grants(scanner_preferences*s){
 if(s->alias.token||s->enable.token||s->rollback.edit.token)return SCANNER_PREF_PENDING;
 int rc=scanner_preferences_release(s,&s->radio);if(rc)return rc;
 rc=scanner_preferences_release(s,&s->storage);if(rc)return rc;
 rc=scanner_preferences_release(s,&s->preference);if(!rc)s->api=NULL;
 return rc;
}
static inline int scanner_alias_begin(scanner_preferences*s,uint8_t type,const uint8_t address[6],char prior[BLE_ALIAS_MAX+1]){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(type>3||!address||!prior)return SCANNER_PREF_ERROR;
 if(s->phase)return SCANNER_PREF_PENDING;
 if(s->alias_active){
  if(type!=s->alias_type||memcmp(address,s->alias_address,6))return SCANNER_PREF_CONFLICT;
 }else{
  int rc=scanner_preferences_open(s);if(rc)return rc;
  char key[16];ble_alias_key(type,address,key);
  int result=portable_preference_atomic_begin(&s->alias,s->api,key,PORTABLE_PREFERENCE_USER,scanner_preferences_guard,s);
  if(result!=PORTABLE_PREFERENCE_READY){
   rc=scanner_preferences_result(s,result);
   if(rc==SCANNER_PREF_RETAINED)return rc;
   if(scanner_preferences_close_grants(s))return scanner_preferences_retain(s);
   return rc;
  }
  s->alias_active=true;s->alias_type=type;memcpy(s->alias_address,address,6);
  if(scanner_preferences_close_grants(s))return scanner_preferences_retain(s);
 }
 prior[0]=0;
 const uint8_t *bytes=s->alias_done?s->alias.desired:s->alias.expected;
 uint32_t size=s->alias_done?s->alias.desired_size:s->alias.expected_size;
 /* Explicit repair starts blank but keeps the corrupt raw expected record. */
 if(s->alias.present||s->alias_done)(void)ble_alias_decode(type,address,bytes,size,prior);
 return SCANNER_PREF_OK;
}
static inline int scanner_preferences_step(scanner_preferences*s);
static inline int scanner_alias_save(scanner_preferences*s,const char*text){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(!s->alias_active||!text)return SCANNER_PREF_ERROR;
 if(s->phase)return s->phase==1?scanner_preferences_step(s):SCANNER_PREF_PENDING;
 unsigned length=0;while(length<=BLE_ALIAS_MAX&&text[length]){if((unsigned char)text[length]<32||(unsigned char)text[length]>126)return SCANNER_PREF_ERROR;length++;}
 if(length>BLE_ALIAS_MAX)return SCANNER_PREF_ERROR;
 unsigned first=0,last=length;while(first<last&&text[first]==' ')first++;while(last>first&&text[last-1]==' ')last--;
 char clean[BLE_ALIAS_MAX+1]={0};memcpy(clean,text+first,last-first);uint8_t bytes[40];
 if(!ble_alias_encode(s->alias_type,s->alias_address,clean,bytes))return SCANNER_PREF_ERROR;
 if(s->alias_prepared&&memcmp(bytes,s->alias.desired,40))return SCANNER_PREF_CONFLICT;
 if(s->alias_done)return SCANNER_PREF_OK;
 int rc=scanner_preferences_open(s);if(rc)return rc;
 s->alias.api=s->api;
 if(!s->alias_prepared){
  int prepared=portable_preference_atomic_prepare(&s->alias,bytes,sizeof(bytes));
  if(prepared!=PORTABLE_PREFERENCE_READY)return scanner_preferences_retain(s);
  s->alias_prepared=true;
 }
 /* Explicit retry reuses exact PFI bytes and captured revision. */
 s->alias.submitted=false;s->alias.cancelled=false;s->alias.status=PORTABLE_PREFERENCE_READY;s->phase=1;
 return scanner_preferences_step(s);
}
/* Read-only list population never changes a captured editor intent. Missing
 * and valid empty tombstones return OK with an empty output; corrupt is ERROR. */
static inline int scanner_alias_read(scanner_preferences*s,uint8_t type,const uint8_t address[6],char out[BLE_ALIAS_MAX+1]){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(type>3||!address||!out)return SCANNER_PREF_ERROR;
 out[0]=0;if(s->phase)return SCANNER_PREF_PENDING;
 int rc=scanner_preferences_acquire(s,RISC_KEY_VALUE_CAPABILITY,1,&s->storage);if(rc)return rc;
 const risc_key_value_v1*kv=s->storage.api;
 if(kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get)return scanner_preferences_retain(s);
 char key[16];uint8_t bytes[64];uint32_t n=0;ble_alias_key(type,address,key);
 s->in_call=true;int status=kv->get(kv->context,key,bytes,sizeof(bytes),&n);s->in_call=false;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(status!=RISC_KEY_VALUE_OK&&status!=RISC_KEY_VALUE_NOT_FOUND&&status!=RISC_KEY_VALUE_BUSY&&status!=RISC_KEY_VALUE_IO&&status!=RISC_KEY_VALUE_INVALID&&status!=RISC_KEY_VALUE_BUFFER_SMALL)return scanner_preferences_retain(s);
 if(status==RISC_KEY_VALUE_NOT_FOUND&&n)return scanner_preferences_retain(s);
 bool valid=status==RISC_KEY_VALUE_NOT_FOUND||(status==RISC_KEY_VALUE_OK&&n<=sizeof(bytes)&&ble_alias_decode(type,address,bytes,n,out));
 rc=scanner_preferences_release(s,&s->storage);if(rc)return rc;
 return status==RISC_KEY_VALUE_BUSY?SCANNER_PREF_PENDING:valid?SCANNER_PREF_OK:SCANNER_PREF_ERROR;
}

/* Existing namespace-1 TV1 timer. Ordinary missing/corrupt records use the
 * published 60s default; uncertainty and invalid custody still fail closed. */
static inline int scanner_idle_timeout(scanner_preferences*s,uint32_t*out){
 if(!out)return SCANNER_PREF_ERROR;
 *out=60000u;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(s->phase)return SCANNER_PREF_PENDING;
 int rc=scanner_preferences_acquire(s,RISC_KEY_VALUE_CAPABILITY,1,&s->storage);if(rc)return rc;
 const risc_key_value_v1*kv=s->storage.api;
 if(kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get)return scanner_preferences_retain(s);
 uint8_t bytes[5]={0};uint32_t n=0;s->in_call=true;
 int status=kv->get(kv->context,"sleep_idle",bytes,sizeof(bytes),&n);s->in_call=false;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(status!=RISC_KEY_VALUE_OK&&status!=RISC_KEY_VALUE_NOT_FOUND&&status!=RISC_KEY_VALUE_BUSY&&status!=RISC_KEY_VALUE_IO&&status!=RISC_KEY_VALUE_INVALID&&status!=RISC_KEY_VALUE_BUFFER_SMALL)return scanner_preferences_retain(s);
 if(status==RISC_KEY_VALUE_NOT_FOUND&&n)return scanner_preferences_retain(s);
 uint32_t seconds=(uint32_t)bytes[2]|((uint32_t)bytes[3]<<8);
 if(status==RISC_KEY_VALUE_OK&&n==5&&bytes[0]==0x54&&bytes[1]==1&&bytes[4]==(uint8_t)(bytes[2]^bytes[3]^0xa5)&&seconds>=5&&seconds<=3600)*out=seconds*1000u;
 rc=scanner_preferences_release(s,&s->storage);if(rc)return rc;
 return SCANNER_PREF_OK;
}

static inline int scanner_radio_allowed(scanner_preferences*s,uint8_t*out){
 if(!out)return SCANNER_PREF_ERROR;
 *out=0;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(s->phase)return SCANNER_PREF_PENDING;
 int rc=scanner_preferences_acquire(s,RISC_KEY_VALUE_CAPABILITY,1,&s->storage);if(rc)return rc;
 const risc_key_value_v1*kv=s->storage.api;
 if(!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get)return scanner_preferences_retain(s);
 uint8_t bytes[4];uint32_t n=0;s->in_call=true;int status=kv->get(kv->context,PORTABLE_RADIO_KEY,bytes,sizeof(bytes),&n);s->in_call=false;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(status!=RISC_KEY_VALUE_OK&&status!=RISC_KEY_VALUE_NOT_FOUND&&status!=RISC_KEY_VALUE_BUSY&&status!=RISC_KEY_VALUE_IO&&status!=RISC_KEY_VALUE_INVALID&&status!=RISC_KEY_VALUE_BUFFER_SMALL)return scanner_preferences_retain(s);
 if(status==RISC_KEY_VALUE_NOT_FOUND&&n)return scanner_preferences_retain(s);
 bool valid=status==RISC_KEY_VALUE_NOT_FOUND||(status==RISC_KEY_VALUE_OK&&portable_radio_decode(bytes,n,out));
 if(status==RISC_KEY_VALUE_NOT_FOUND)*out=PORTABLE_RADIO_WIFI;
 rc=scanner_preferences_release(s,&s->storage);if(rc)return rc;
 if(status==RISC_KEY_VALUE_BUSY)return SCANNER_PREF_PENDING;
 return valid?SCANNER_PREF_OK:SCANNER_PREF_ERROR;
}
static inline int scanner_preferences_finish(scanner_preferences*s,int result){
 int rc=scanner_preferences_result(s,result);if(rc==SCANNER_PREF_RETAINED)return rc;
 if(rc==SCANNER_PREF_PENDING)return rc;
 s->phase=0;s->last=rc;
 int closed=scanner_preferences_close_grants(s);return closed?closed:rc;
}
static inline int scanner_enable_bluetooth(scanner_preferences*s){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(s->phase)return SCANNER_PREF_PENDING;
 int rc=scanner_preferences_open(s);if(rc)return rc;
 int result=portable_preference_atomic_begin(&s->enable,s->api,PORTABLE_RADIO_KEY,PORTABLE_PREFERENCE_USER,scanner_preferences_guard,s);
 if(result!=PORTABLE_PREFERENCE_READY)return scanner_preferences_finish(s,result);
 uint8_t flags=PORTABLE_RADIO_WIFI;
 if((s->enable.present&&!portable_radio_decode(s->enable.expected,s->enable.expected_size,&flags))||(flags&PORTABLE_RADIO_AIRPLANE))return scanner_preferences_finish(s,RISC_ATOMIC_INVALID);
 rc=scanner_preferences_acquire(s,"bluetooth.hci",16,&s->radio);
 if(rc){if(rc==SCANNER_PREF_RETAINED)return rc;int closed=scanner_preferences_close_grants(s);return closed?closed:rc;}
 const portable_bluetooth_control_v1 *api=s->radio.api;
 if(api->api_version!=1||api->struct_size<sizeof(*api)||!api->status||!api->set_enabled)return scanner_preferences_retain(s);
 uint8_t physical=255;s->in_call=true;bool ok=api->status(api->context,&physical);s->in_call=false;
 if(!scanner_preferences_guard(s)||!ok||physical>PORTABLE_BLUETOOTH_ON)return scanner_preferences_retain(s);
 s->radio_prior=physical;
 uint8_t desired=(uint8_t)(flags|PORTABLE_RADIO_BLUETOOTH);
 uint8_t bytes[4]={0x51,1,desired,(uint8_t)(desired^0xa5)};
 result=portable_preference_atomic_prepare(&s->enable,bytes,sizeof(bytes));
 if(result!=PORTABLE_PREFERENCE_READY)return scanner_preferences_retain(s);
 s->phase=2;return scanner_preferences_step(s);
}
static inline int scanner_preferences_step(scanner_preferences*s){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 if(!s->phase)return s->last;
 portable_preference_atomic *edit=s->phase==1?&s->alias:s->phase==2?&s->enable:&s->rollback.edit;
 int result=edit->token?portable_preference_atomic_collect(edit):s->phase==3?portable_preference_rollback_submit(&s->rollback):portable_preference_atomic_submit(edit);
 int rc=scanner_preferences_result(s,result);
 if(rc==SCANNER_PREF_PENDING||rc==SCANNER_PREF_RETAINED)return rc;
 if(s->phase==1){s->alias_done=result==RISC_ATOMIC_OK;return scanner_preferences_finish(s,result);}
 if(s->phase==3){
  if(result!=RISC_ATOMIC_OK)return scanner_preferences_retain(s);
  risc_key_value_atomic_state_v1 current={.struct_size=sizeof(current)};
  int status=s->api->state(s->api->context,&current);
  if(!scanner_preferences_guard(s)||status!=RISC_ATOMIC_OK||!portable_preference_atomic_state_valid(&current)||current.revision!=s->rollback.edit.receipt.revision)return scanner_preferences_retain(s);
  const portable_bluetooth_control_v1 *control=s->radio.api;
  s->in_call=true;bool restored=control->set_enabled(control->context,s->radio_prior==PORTABLE_BLUETOOTH_ON);s->in_call=false;
  if(!scanner_preferences_guard(s)||!restored)return scanner_preferences_retain(s);
  uint8_t actual=255;s->in_call=true;bool observed=control->status(control->context,&actual);s->in_call=false;
  if(!scanner_preferences_guard(s)||!observed||actual!=s->radio_prior)return scanner_preferences_retain(s);
  return scanner_preferences_finish(s,RISC_ATOMIC_IO);
 }
 if(result==RISC_ATOMIC_UNCERTAIN)return scanner_preferences_retain(s);
 if(result!=RISC_ATOMIC_OK)return scanner_preferences_finish(s,result);
 risc_key_value_atomic_state_v1 confirmed={.struct_size=sizeof(confirmed)};
 int checked=s->api->state(s->api->context,&confirmed);
 if(!scanner_preferences_guard(s)||checked!=RISC_ATOMIC_OK||!portable_preference_atomic_state_valid(&confirmed)||confirmed.revision!=s->enable.receipt.revision)return scanner_preferences_retain(s);
 const portable_bluetooth_control_v1 *api=s->radio.api;
 s->in_call=true;bool enabled=api->set_enabled(api->context,true);s->in_call=false;
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 uint8_t physical=255;s->in_call=true;bool observed=api->status(api->context,&physical);s->in_call=false;
 if(!scanner_preferences_guard(s)||!observed||physical>PORTABLE_BLUETOOTH_ON)return scanner_preferences_retain(s);
 if(enabled&&physical==PORTABLE_BLUETOOTH_ON)return scanner_preferences_finish(s,RISC_ATOMIC_OK);
 result=portable_preference_rollback_begin(&s->rollback,&s->enable,scanner_preferences_guard,s);
 if(result!=PORTABLE_PREFERENCE_READY)return scanner_preferences_retain(s);
 s->phase=3;return SCANNER_PREF_PENDING;
}
static inline int scanner_preferences_cleanup(scanner_preferences*s){
 if(!scanner_preferences_guard(s))return scanner_preferences_retain(s);
 /* Radio cleanup must finish physical verification/compensation after commit,
  * never abandon a pending successful intent between settings and hardware. */
 if(s->phase==2||s->phase==3)return scanner_preferences_step(s);
 if(s->phase==1){
  int rc=s->alias.token?portable_preference_atomic_cancel(&s->alias):RISC_ATOMIC_CANCELLED;
  if(rc==RISC_ATOMIC_OK)s->alias_done=true;
  return scanner_preferences_finish(s,rc);
 }
 return scanner_preferences_close_grants(s);
}
static inline int scanner_alias_cancel(scanner_preferences*s){
 int rc=scanner_preferences_cleanup(s);
 if(rc==SCANNER_PREF_PENDING||rc==SCANNER_PREF_RETAINED)return rc;
 s->alias=(portable_preference_atomic){0};s->alias_active=s->alias_prepared=s->alias_done=false;
 return SCANNER_PREF_OK;
}
#endif
