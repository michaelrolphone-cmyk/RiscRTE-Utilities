#define PORTABLE_CONTEXTS_CLIENT
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "contexts_owner_export.h"
#include <assert.h>
#include <stdio.h>
static uint32_t pending,active,source,result,records,requests,acquires,releases,stops;
static bool fail_get,fail_release,fail_stop,malformed,loaded;
static bool data_mode,storage_retained,fenced,data_release_fail;
static unsigned data_acquires,data_releases,data_stats,data_reads,model_records,model_errors,finishes,fences,live_grants,max_grants;
static int data_failure;
static uint8_t model_buffer[RT_BANK_MAX],stored_bank[2][RT_BANK_MAX];
static uint32_t stored_size[2];
static void safe(void){assert(!storage_retained&&!fenced);}
static bool capture_safe(void){stops++;return !fail_stop;}
bool portable_contexts_stop(void){return capture_safe();}
static bool get_status(void *c,contexts_status_v1 *out){(void)c;safe();*out=(contexts_status_v1){.struct_size=sizeof(*out),.export_pending=pending,.export_active=active,.audio={.model_state=loaded?CONTEXTS_MODEL_READY:CONTEXTS_MODEL_EMPTY},.radio={.model_state=loaded?CONTEXTS_MODEL_READY:CONTEXTS_MODEL_EMPTY}};return true;}
static bool request(void *c,uint32_t s){(void)c;requests++;pending|=s;return true;}
static bool begin(void *c,uint32_t s){(void)c;assert((pending&s)&&!active);active=s;source=s;records=0;return true;}
static bool record(void *c,uint32_t s,uint32_t kind,uint32_t index,const void *bytes,uint32_t size) {
    (void)c;safe();assert(s==source&&active==s);
    if(kind>=CONTEXTS_RECORD_TEMPORAL_BANK){
        assert(data_mode);model_records++;
        if(!bytes){assert(!size);return true;}
        assert(kind==CONTEXTS_RECORD_TEMPORAL_BANK&&index<2&&size==stored_size[index]);
        assert(!memcmp(bytes,stored_bank[index],size));return true;
    }
    assert(bytes);
    bool valid=false;
    if(kind==CONTEXTS_RECORD_PREFERENCES){assert(index==0&&records==0);if(s==CONTEXTS_AUDIO){spectrum_preferences p;valid=spectrum_preferences_decode(&p,bytes,size);}else{rf_preferences p;valid=rf_preferences_decode(&p,bytes,size);}}
    else{assert(kind==CONTEXTS_RECORD_SIGNATURE&&index==records-1);if(s==CONTEXTS_AUDIO){spectrum_signature sig;valid=spectrum_signature_decode(&sig,bytes,size);}else{rf_signature sig;valid=rf_signature_decode(&sig,bytes,size);}}
    if(valid)records++;
    return valid;
}
static bool finish(void *c,uint32_t s,uint32_t code){(void)c;safe();finishes++;assert(active==s&&s==source);result=code;active=0;pending&=~s;return true;}
static bool model_error(void *c,uint32_t s,uint32_t kind,uint32_t index,int32_t error){(void)c;safe();assert(s==source&&kind>=3&&index<2&&error);model_errors++;return true;}
static contexts_service_v1 service={.api_version=1,.struct_size=sizeof(service),.status=get_status,.request_export=request,.begin_export=begin,.export_record=record,.finish_export=finish,.export_model_error=model_error};
const contexts_service_v1 *portable_contexts_service(void){return &service;}
static int32_t get(void *c,const char *key,void *bytes,uint32_t capacity,uint32_t *size) {
    (void)c;assert(capacity==RF_SIGNATURE_RECORD_SIZE);*size=0;
    assert(!strncmp(key,source==CONTEXTS_AUDIO?"spectrum_":"rf_",source==CONTEXTS_AUDIO?9:3));
    if(fail_get)return RISC_KEY_VALUE_IO;
    if(malformed){memset(bytes,0,32);*size=32;return RISC_KEY_VALUE_OK;}
    return RISC_KEY_VALUE_NOT_FOUND;
}
static const risc_key_value_v1 kv={2,sizeof(kv),NULL,get,NULL};
static unsigned file_index(const char *name){
    const char *expected[2][3]={{"spectrum-events-a.sqt","spectrum-events-b.sqt","spectrum-neural.snn"},{"rf-events-a.rft","rf-events-b.rft","rf-neural.rnn"}};
    for(unsigned i=0;i<3;i++)if(!strcmp(name,expected[source==CONTEXTS_AUDIO?0:1][i]))return i;
    assert(!"wrong owner file");return 0;
}
static int32_t data_stat(void *c,const char *name,uint32_t *size,uint64_t *revision){
    (void)c;safe();data_stats++;unsigned i=file_index(name);*size=0;*revision=0;
    if(data_failure==1&&i==0){storage_retained=true;return RISC_APP_DATA_RETAINED;}
    if(data_failure==4&&i==0)return RISC_APP_DATA_IO;
    if(i==2||!stored_size[i])return RISC_APP_DATA_NOT_FOUND;
    *size=data_failure==5?UINT32_MAX:stored_size[i];*revision=17;return 0;
}
static int32_t data_read(void *c,const char *name,uint64_t revision,void *bytes,uint32_t capacity,uint32_t *size,uint64_t *current){
    (void)c;safe();data_reads++;unsigned i=file_index(name);assert(i<2&&revision==17&&capacity>=stored_size[i]);*size=0;*current=0;
    if(data_failure==2&&i==0){storage_retained=true;return RISC_APP_DATA_RETAINED;}
    if(data_failure==3&&i==0)return RISC_APP_DATA_STALE;
    memcpy(bytes,stored_bank[i],stored_size[i]);*size=stored_size[i];*current=data_failure==6?18:17;return 0;
}
static const risc_app_data_v1 data_api={1,sizeof(data_api),NULL,data_stat,data_read,NULL};
static bool acquire(const char *cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *out){
    safe();assert(stops);if(!strcmp(cap,RISC_APP_DATA_CAPABILITY)){
        assert(data_mode&&api==1&&instance==(source==CONTEXTS_AUDIO?2u:3u)&&!live_grants);data_acquires++;out->api=&data_api;
    }else{assert(!strcmp(cap,"storage.key-value")&&api==2&&instance==(source==CONTEXTS_AUDIO?0u:13u));acquires++;out->api=&kv;}
    if(data_mode){live_grants++;if(live_grants>max_grants)max_grants=live_grants;}return true;
}
static bool release(risc_runtime_capability_v1 *grant){
    safe();if(grant->api==&data_api){data_releases++;if(data_release_fail)return false;}else{assert(grant->api==&kv);releases++;if(fail_release)return false;}
    if(data_mode){assert(live_grants);live_grants--;}return true;
}
static bool fence(void){assert(storage_retained||data_release_fail);fences++;fenced=true;return true;}
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
    static struct {risc_runtime_api_v1 base;bool (*confirm)(void);bool (*retain)(void);} extended;
    extended.base=runtime;extended.base.struct_size=sizeof(extended);extended.retain=fence;data_mode=true;
    static st_library audio_library;static rt_library radio_library;
    radio_library.identity=rf_identity_default();
    for(unsigned which=0;which<2;which++){
        source=which?CONTEXTS_RADIO:CONTEXTS_AUDIO;
        for(unsigned bank=0;bank<2;bank++)stored_size[bank]=(uint32_t)(which?rt_bank_encode(&radio_library,bank,stored_bank[bank],sizeof(stored_bank[bank])):st_bank_encode(&audio_library,bank,stored_bank[bank],sizeof(stored_bank[bank])));
        pending=source;unsigned prior=model_records;
        assert(contexts_owner_export_models(&extended.base,source,which?13:0,which?3:2,model_buffer,sizeof(model_buffer))==CONTEXTS_OWNER_EXPORTED);
        assert(model_records==prior+3&&!live_grants&&max_grants==1&&!result);
    }
    unsigned before_data=data_acquires;service.struct_size=CONTEXTS_SERVICE_V1_SIZE;pending=CONTEXTS_RADIO;
    assert(contexts_owner_export_models(&extended.base,CONTEXTS_RADIO,13,3,model_buffer,sizeof(model_buffer))==CONTEXTS_OWNER_EXPORTED&&data_acquires==before_data);
    service.struct_size=sizeof(service);pending=CONTEXTS_RADIO;unsigned before_errors=model_errors;
    assert(contexts_owner_export_models(&runtime,CONTEXTS_RADIO,13,3,model_buffer,sizeof(model_buffer))==CONTEXTS_OWNER_EXPORTED&&data_acquires==before_data&&model_errors==before_errors+3);
    for(data_failure=3;data_failure<=6;data_failure++){
        pending=CONTEXTS_RADIO;before_errors=model_errors;
        assert(contexts_owner_export_models(&extended.base,CONTEXTS_RADIO,13,3,model_buffer,sizeof(model_buffer))==CONTEXTS_OWNER_EXPORTED);
        assert(!result&&model_errors>before_errors&&!live_grants);
    }
    for(data_failure=1;data_failure<=2;data_failure++){
        pending=CONTEXTS_RADIO;unsigned before_finish=finishes,before_release=data_releases;
        assert(contexts_owner_export_models(&extended.base,CONTEXTS_RADIO,13,3,model_buffer,sizeof(model_buffer))==CONTEXTS_OWNER_FENCED);
        assert(fenced&&storage_retained&&finishes==before_finish&&data_releases==before_release&&active==CONTEXTS_RADIO&&pending==CONTEXTS_RADIO&&live_grants==1);
        /* Simulated restart: only the next invocation may resolve abandonment. */
        fenced=storage_retained=false;live_grants=0;assert(finish(NULL,CONTEXTS_RADIO,CONTEXTS_EXPORT_UNSUPPORTED));
    }
    data_failure=0;data_release_fail=true;pending=CONTEXTS_RADIO;unsigned before_finish=finishes;
    assert(contexts_owner_export_models(&extended.base,CONTEXTS_RADIO,13,3,model_buffer,sizeof(model_buffer))==CONTEXTS_OWNER_FENCED);
    assert(fenced&&finishes==before_finish&&live_grants==1&&fences==3);
    puts("Contexts owner rendezvous: bounded own-KV reads, virtual defaults, no writes/capture, no normal-launch diversion, invalid/read/retained failures, and explicit refresh PASS");
}
