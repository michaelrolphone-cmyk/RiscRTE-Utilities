#pragma once
#ifdef PORTABLE_CONTEXTS_CLIENT
#include "ContextsServiceV1.h"
#include "PortableBackgroundServices.h"
#include "spectrum_store.h"
#include "spectrum_signature_store.h"
#include "rf_store.h"
#include "rf_signature_store.h"
/* Borrow the common adapter's grant: a duplicate acquisition can exceed the
 * existing 16-grant bound while this owner reads its private KV namespace. */
const contexts_service_v1 *portable_contexts_service(void);
enum { CONTEXTS_OWNER_RETAINED=-1,CONTEXTS_OWNER_NORMAL=0,CONTEXTS_OWNER_EXPORTED=1 };
static int contexts_owner_export(const risc_runtime_api_v1 *runtime,uint32_t source,uint64_t instance) {
    const contexts_service_v1 *service=portable_contexts_service();
    if(!service)return CONTEXTS_OWNER_NORMAL;
    contexts_status_v1 status={.struct_size=sizeof(status)};
    if(!service->status(service->context,&status))return CONTEXTS_OWNER_RETAINED;
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
        if(!runtime->release(&grant))return CONTEXTS_OWNER_RETAINED;
    }
    if(!service->finish_export(service->context,source,result))return CONTEXTS_OWNER_RETAINED;
    return CONTEXTS_OWNER_EXPORTED;
}
/* A normal foreground editor may have saved/deleted/renamed its own models.
 * Refresh only a source already requested in this boot session, after the
 * app has released capture and its original storage grants. No UI handoff. */
static bool contexts_owner_refresh(const risc_runtime_api_v1 *runtime,uint32_t source,uint64_t instance) {
    const contexts_service_v1 *service=portable_contexts_service();
    if(!service)return true;
    contexts_status_v1 status={.struct_size=sizeof(status)};
    if(!service->status(service->context,&status))return false;
    const contexts_source_status_v1 *s=source==CONTEXTS_AUDIO?&status.audio:&status.radio;
    if(s->model_state==CONTEXTS_MODEL_EMPTY)return true;
    return service->request_export(service->context,source)&&
        contexts_owner_export(runtime,source,instance)!=CONTEXTS_OWNER_RETAINED;
}
#endif
