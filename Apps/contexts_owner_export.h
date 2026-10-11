#pragma once
#ifdef PORTABLE_CONTEXTS_CLIENT
#include "ContextsServiceV1.h"
#include "ContextFingerprintService.h"
static uint8_t contexts_owner_fingerprint_bytes[72000];
#include "PortableBackgroundServices.h"
#include "spectrum_store.h"
#include "spectrum_signature_store.h"
#include "rf_store.h"
#include "rf_signature_store.h"
#include "spectrum_temporal_store.h"
#include "spectrum_neural_store.h"
#include "rf_temporal_store.h"
#include "rf_neural_store.h"
#include "RiscAppDataV1.h"
#include "app_retained_fence.h"
/* Borrow the common adapter's grant: a duplicate acquisition can exceed the
 * existing 16-grant bound while this owner reads its private KV namespace. */
const contexts_service_v1 *portable_contexts_service(void);
bool portable_contexts_models_save(void);
enum { CONTEXTS_OWNER_FENCED=-2,CONTEXTS_OWNER_RETAINED=-1,CONTEXTS_OWNER_NORMAL=0,CONTEXTS_OWNER_EXPORTED=1 };
static int contexts_owner_fence(const risc_runtime_api_v1 *runtime){
    if(app_retained_fence(runtime))return CONTEXTS_OWNER_FENCED;
    /* Retained native custody cannot safely return to an older finalizer. */
    for(;;)runtime->yield_ms(50);
}
static bool contexts_owner_model_error(const contexts_service_v1 *service,uint32_t source,uint32_t kind,uint32_t index,int32_t error){
    return service->export_model_error(service->context,source,kind,index,error);
}
static int contexts_owner_models(const risc_runtime_api_v1 *runtime,const contexts_service_v1 *service,
        uint32_t source,uint64_t instance,void *buffer,uint32_t capacity){
    if(service->struct_size<CONTEXTS_MODEL_IMPORT_V1_SIZE||!service->export_model_error)return CONTEXTS_OWNER_EXPORTED;
    int32_t unavailable=CONTEXTS_IMPORT_UNSUPPORTED;
    risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    const risc_app_data_v1 *api=NULL;
    bool acquired=false;
    uint32_t bank_max=source==CONTEXTS_AUDIO?ST_BANK_MAX:RT_BANK_MAX,bank_min=source==CONTEXTS_AUDIO?ST_BANK_MIN:RT_BANK_MIN;
    uint32_t neural_size=source==CONTEXTS_AUDIO?SN_RECORD_SIZE:RN_RECORD_SIZE;
    if(buffer&&capacity>=bank_max&&app_retained_fence_method(runtime)){
        unavailable=RISC_APP_DATA_UNAVAILABLE;
        if(runtime->acquire(RISC_APP_DATA_CAPABILITY,1,instance,&grant)){
            acquired=true;
            api=grant.api;
            if(!api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->stat||!api->read)api=NULL;
        }
    }
    for(unsigned record=0;record<3;record++){
        uint32_t kind=record<2?CONTEXTS_RECORD_TEMPORAL_BANK:CONTEXTS_RECORD_NEURAL,index=record<2?record:0;
        const char *name=source==CONTEXTS_AUDIO?(record==0?"spectrum-events-a.sqt":record==1?"spectrum-events-b.sqt":"spectrum-neural.snn"):
            (record==0?"rf-events-a.rft":record==1?"rf-events-b.rft":"rf-neural.rnn");
        uint32_t size=0,actual=0;uint64_t revision=0,current=0;
        int32_t rc=api?api->stat(api->context,name,&size,&revision):unavailable;
        if(rc==RISC_APP_DATA_RETAINED)return contexts_owner_fence(runtime);
        if(rc==RISC_APP_DATA_NOT_FOUND){
            (void)service->export_record(service->context,source,kind,index,NULL,0);
            continue;
        }
        if(!rc&&(!revision||(record<2?(size<bank_min||size>bank_max):size!=neural_size)))rc=CONTEXTS_IMPORT_INVALID;
        if(!rc){
            rc=api->read(api->context,name,revision,buffer,capacity,&actual,&current);
            if(rc==RISC_APP_DATA_RETAINED)return contexts_owner_fence(runtime);
            if(!rc&&(actual!=size||current!=revision))rc=CONTEXTS_IMPORT_INVALID;
        }
        /* The provider records precise format versus stale-model failures;
         * do not replace that result with a generic owner-side error. */
        if(!rc)(void)service->export_record(service->context,source,kind,index,buffer,size);
        if(rc&&!contexts_owner_model_error(service,source,kind,index,rc))return contexts_owner_fence(runtime);
    }
    const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(service);
    if(api&&fp){
        uint32_t size=0,actual=0;uint64_t revision=0,current=0;
        int32_t rc=api->stat(api->context,"context-fingerprints.cfp",&size,&revision);
        if(rc==RISC_APP_DATA_RETAINED)return contexts_owner_fence(runtime);
        if(!rc&&size<=sizeof(contexts_owner_fingerprint_bytes)&&revision){
            rc=api->read(api->context,"context-fingerprints.cfp",revision,contexts_owner_fingerprint_bytes,sizeof(contexts_owner_fingerprint_bytes),&actual,&current);
            if(rc==RISC_APP_DATA_RETAINED)return contexts_owner_fence(runtime);
            if(!rc&&actual==size&&current==revision){
                contexts_fingerprint_record_v1 record={.struct_size=sizeof(record),.source=source,.size=size,.capacity=sizeof(contexts_owner_fingerprint_bytes),.bytes=contexts_owner_fingerprint_bytes};
                (void)fp->fingerprint(fp->base.context,CONTEXTS_FP_IMPORT,&record);
            }
        }
    }
    if(acquired&&!runtime->release(&grant))return contexts_owner_fence(runtime);
    return CONTEXTS_OWNER_EXPORTED;
}
static int contexts_owner_export_models(const risc_runtime_api_v1 *runtime,uint32_t source,uint64_t instance,
        uint64_t data_instance,void *buffer,uint32_t capacity) {
    const contexts_service_v1 *service=portable_contexts_service();
    if(!service)return CONTEXTS_OWNER_NORMAL;
    contexts_status_v1 status={.struct_size=sizeof(status)};
    if(!service->status(service->context,&status)||status.cleanup_pending)return CONTEXTS_OWNER_RETAINED;
    if(!(status.export_pending&source))return CONTEXTS_OWNER_NORMAL;
    if(!portable_background_stop())return CONTEXTS_OWNER_RETAINED;
    if(status.export_active!=source&&!service->begin_export(service->context,source))return CONTEXTS_OWNER_RETAINED;
    risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    uint32_t result=CONTEXTS_EXPORT_STORAGE;
    if(runtime->acquire("storage.key-value",2,instance,&grant)) {
        const risc_key_value_v1 *kv=grant.api;
        if(kv&&kv->api_version==2&&kv->struct_size>=sizeof(*kv)&&kv->get) {
            uint8_t bytes[RF_SIGNATURE_RECORD_SIZE];uint32_t size=0;
            int32_t rc=kv->get(kv->context,source==CONTEXTS_AUDIO?"spectrum_cfg":"rf_cfg",bytes,sizeof(bytes),&size);
            if(rc==RISC_KEY_VALUE_NOT_FOUND) {
                if(source==CONTEXTS_AUDIO){spectrum_preferences p=spectrum_preferences_default();spectrum_preferences_encode(&p,bytes);size=SPECTRUM_PREFERENCES_SIZE;}
                else{rf_preferences p=rf_preferences_default();(void)rf_preferences_encode(&p,bytes);size=RF_PREFERENCES_SIZE;}
                rc=RISC_KEY_VALUE_OK;
            }
            result=rc==RISC_KEY_VALUE_OK?CONTEXTS_EXPORT_OK:CONTEXTS_EXPORT_STORAGE;
            if(!result&&!service->export_record(service->context,source,CONTEXTS_RECORD_PREFERENCES,0,bytes,size))result=CONTEXTS_EXPORT_INVALID;
            for(unsigned slot=0;!result&&slot<8;slot++) {
                char key[16];const char *prefix=source==CONTEXTS_AUDIO?"spectrum_s":"rf_s";
                size_t length=strlen(prefix);memcpy(key,prefix,length);key[length]=(char)('0'+slot);key[length+1]=0;
                size=0;rc=kv->get(kv->context,key,bytes,sizeof(bytes),&size);
                if(rc==RISC_KEY_VALUE_NOT_FOUND) {
                    if(source==CONTEXTS_AUDIO){spectrum_signature empty={0};(void)spectrum_signature_encode(&empty,bytes);size=SPECTRUM_SIGNATURE_RECORD_SIZE;}
                    else{rf_signature empty={.identity=rf_identity_default()};(void)rf_signature_encode(&empty,bytes);size=RF_SIGNATURE_RECORD_SIZE;}
                    rc=RISC_KEY_VALUE_OK;
                }
                if(rc!=RISC_KEY_VALUE_OK)result=CONTEXTS_EXPORT_STORAGE;
                else if(!service->export_record(service->context,source,CONTEXTS_RECORD_SIGNATURE,slot,bytes,size))result=CONTEXTS_EXPORT_INVALID;
            }
        }
        if(!runtime->release(&grant))return app_retained_fence_method(runtime)?contexts_owner_fence(runtime):CONTEXTS_OWNER_RETAINED;
    }
    if(!result&&buffer){int model_result=contexts_owner_models(runtime,service,source,data_instance,buffer,capacity);if(model_result<0)return model_result;}
    if(!service->finish_export(service->context,source,result))return CONTEXTS_OWNER_RETAINED;
    return CONTEXTS_OWNER_EXPORTED;
}
static inline int contexts_owner_export(const risc_runtime_api_v1 *runtime,uint32_t source,uint64_t instance){
    return contexts_owner_export_models(runtime,source,instance,0,NULL,0);
}
/* A normal foreground editor may have saved/deleted/renamed its own models.
 * Refresh only a source already requested in this boot session, after the
 * app has released capture and its original storage grants. No UI handoff. */
static int contexts_owner_refresh_models(const risc_runtime_api_v1 *runtime,uint32_t source,uint64_t instance,
        uint64_t data_instance,void *buffer,uint32_t capacity) {
    const contexts_service_v1 *service=portable_contexts_service();
    if(!service)return CONTEXTS_OWNER_NORMAL;
    contexts_status_v1 status={.struct_size=sizeof(status)};
    if(!service->status(service->context,&status)||status.cleanup_pending)return CONTEXTS_OWNER_RETAINED;
    const contexts_source_status_v1 *s=source==CONTEXTS_AUDIO?&status.audio:&status.radio;
    if(s->model_state==CONTEXTS_MODEL_EMPTY||s->model_state==CONTEXTS_MODEL_UNAVAILABLE)return CONTEXTS_OWNER_NORMAL;
    if(!service->request_export(service->context,source))return CONTEXTS_OWNER_RETAINED;
    return contexts_owner_export_models(runtime,source,instance,data_instance,buffer,capacity);
}
static inline bool contexts_owner_refresh(const risc_runtime_api_v1 *runtime,uint32_t source,uint64_t instance){
    return contexts_owner_refresh_models(runtime,source,instance,0,NULL,0)>=0;
}
#endif
