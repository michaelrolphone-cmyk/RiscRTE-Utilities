#define PORTABLE_CONTEXTS_CLIENT
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "contexts_owner_export.h"
#include <assert.h>
#include <stdio.h>
static uint32_t pending,active,source,result,records,requests,acquires,releases,stops;
static bool fail_get,fail_release,fail_stop,malformed,loaded;
static bool capture_safe(void){stops++;return !fail_stop;}
bool portable_contexts_stop(void){return capture_safe();}
static bool get_status(void *c,contexts_status_v1 *out){(void)c;*out=(contexts_status_v1){.struct_size=sizeof(*out),.export_pending=pending,.export_active=active,.audio={.model_state=loaded?CONTEXTS_MODEL_READY:CONTEXTS_MODEL_EMPTY},.radio={.model_state=loaded?CONTEXTS_MODEL_READY:CONTEXTS_MODEL_EMPTY}};return true;}
static bool request(void *c,uint32_t s){(void)c;requests++;pending|=s;return true;}
static bool begin(void *c,uint32_t s){(void)c;assert((pending&s)&&!active);active=s;source=s;records=0;return true;}
static bool record(void *c,uint32_t s,uint32_t kind,uint32_t index,const void *bytes,uint32_t size) {
    (void)c;assert(s==source&&active==s&&bytes);
    bool valid=false;
    if(kind==CONTEXTS_RECORD_PREFERENCES){assert(index==0&&records==0);if(s==CONTEXTS_AUDIO){spectrum_preferences p;valid=spectrum_preferences_decode(&p,bytes,size);}else{rf_preferences p;valid=rf_preferences_decode(&p,bytes,size);}}
    else{assert(kind==CONTEXTS_RECORD_SIGNATURE&&index==records-1);if(s==CONTEXTS_AUDIO){spectrum_signature sig;valid=spectrum_signature_decode(&sig,bytes,size);}else{rf_signature sig;valid=rf_signature_decode(&sig,bytes,size);}}
    if(valid)records++;
    return valid;
}
static bool finish(void *c,uint32_t s,uint32_t code){(void)c;assert(active==s&&s==source);result=code;active=0;pending&=~s;return true;}
static const contexts_service_v1 service={.api_version=1,.struct_size=sizeof(service),.status=get_status,.request_export=request,.begin_export=begin,.export_record=record,.finish_export=finish};
const contexts_service_v1 *portable_contexts_service(void){return &service;}
static int32_t get(void *c,const char *key,void *bytes,uint32_t capacity,uint32_t *size) {
    (void)c;assert(capacity==RF_SIGNATURE_RECORD_SIZE);*size=0;
    assert(!strncmp(key,source==CONTEXTS_AUDIO?"spectrum_":"rf_",source==CONTEXTS_AUDIO?9:3));
    if(fail_get)return RISC_KEY_VALUE_IO;
    if(malformed){memset(bytes,0,32);*size=32;return RISC_KEY_VALUE_OK;}
    return RISC_KEY_VALUE_NOT_FOUND;
}
static const risc_key_value_v1 kv={2,sizeof(kv),NULL,get,NULL};
static bool acquire(const char *cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *out){assert(!strcmp(cap,"storage.key-value")&&api==2&&instance==(source==CONTEXTS_AUDIO?0:13)&&stops);acquires++;out->api=&kv;return true;}
static bool release(risc_runtime_capability_v1 *grant){assert(grant->api==&kv);releases++;return !fail_release;}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
int main(void) {
    assert(contexts_owner_export(&runtime,CONTEXTS_AUDIO,0)==CONTEXTS_OWNER_NORMAL);assert(!acquires&&!stops);
    pending=CONTEXTS_ALL;assert(contexts_owner_export(&runtime,CONTEXTS_AUDIO,0)==CONTEXTS_OWNER_EXPORTED);assert(records==9&&!result&&pending==CONTEXTS_RADIO&&acquires==1&&releases==1);
    assert(contexts_owner_export(&runtime,CONTEXTS_RADIO,13)==CONTEXTS_OWNER_EXPORTED);assert(records==9&&!result&&!pending&&acquires==2&&releases==2);
    pending=CONTEXTS_AUDIO;malformed=true;assert(contexts_owner_export(&runtime,CONTEXTS_AUDIO,0)==CONTEXTS_OWNER_EXPORTED);assert(result==CONTEXTS_EXPORT_INVALID&&!pending&&!active);
    malformed=false;pending=CONTEXTS_RADIO;fail_get=true;assert(contexts_owner_export(&runtime,CONTEXTS_RADIO,13)==CONTEXTS_OWNER_EXPORTED);assert(result==CONTEXTS_EXPORT_STORAGE&&!pending);
    fail_get=false;pending=CONTEXTS_AUDIO;fail_stop=true;unsigned before=acquires;assert(contexts_owner_export(&runtime,CONTEXTS_AUDIO,0)==CONTEXTS_OWNER_RETAINED);assert(acquires==before&&pending==CONTEXTS_AUDIO&&!active);
    fail_stop=false;fail_release=true;assert(contexts_owner_export(&runtime,CONTEXTS_AUDIO,0)==CONTEXTS_OWNER_RETAINED);assert(active==CONTEXTS_AUDIO&&pending==CONTEXTS_AUDIO);
    fail_release=false;active=pending=0;assert(contexts_owner_refresh(&runtime,CONTEXTS_AUDIO,0));assert(!requests);
    loaded=true;assert(contexts_owner_refresh(&runtime,CONTEXTS_AUDIO,0));assert(requests==1&&records==9&&!result&&!pending);
    pending=CONTEXTS_RADIO;assert(begin(NULL,CONTEXTS_RADIO));assert(contexts_owner_export(&runtime,CONTEXTS_RADIO,13)==CONTEXTS_OWNER_EXPORTED);assert(!active&&!pending&&records==9);
    puts("Contexts owner rendezvous: bounded own-KV reads, virtual defaults, no writes/capture, no normal-launch diversion, invalid/read/retained failures, and explicit refresh PASS");
}
