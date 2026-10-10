/* Exercise the real optional service ABI, source-local storage mapping and
 * event windows. Existing canonical peripheral fixtures remain unchanged. */
#define CONTEXTS_TEST_ENTRY existing_contexts_temporal_tests
#include "contexts_temporal_test.c"
#undef CONTEXTS_TEST_ENTRY
static cf_bank saved_bank;
static uint8_t saved_bytes[CF_STORE_MAX];
static void timing_window(unsigned period,bool event){
    int16_t pcm[16];uint32_t power[128]={0};power[8]=1u<<22;
    cf_pipeline*p=event?fp_event_pipeline(CF_AUDIO):fp_pipeline(CF_AUDIO);
    uint64_t began=test_now;
    if(event)fp_event_boundary(CF_AUDIO,true,false,test_now);
    for(unsigned ms=0;ms<1500;ms++){
        test_now=began+ms;
        for(unsigned i=0;i<16;i++)pcm[i]=ms%period<8?16000:1000;
        assert(cf_pcm(p,pcm,16,16000,test_now*1000u,true));
        if(!(ms%16))assert(cf_spectrum(p,power,128,1.f/(1u<<30),0,8000));
    }
    test_now=began+1500;
    if(event)fp_event_boundary(CF_AUDIO,false,true,test_now);
    else{assert(cf_take(p,test_now*1000u,&fp_window));cf_publish(&fp_fusion,CF_AUDIO,&fp_window,test_now*1000u);fp_evaluate(test_now);}
}
int main(void){
    const risc_driver_v2*d=t5_driver_get(2);
    const risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock_table},{"audio.input",1,&input_table},{"radio.iq",1,&receiver_table}};
    assert(d->start(deps,3));provider=d->capability;
    const contexts_fingerprint_service_v1*api=contexts_fingerprint_api(provider);assert(api);
    contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=CONTEXTS_AUDIO,.utc_seconds=1700000000};
    assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIG,&cfg));
    cf_enable(&fp_fusion,0,true);cf_enable(&fp_event_fusion,0,true);
    timing_window(100,false);
    contexts_fingerprint_confirm_v1 confirm={.struct_size=sizeof(confirm),.kind=CF_ROOM,.name="Office"};
    assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIRM,&confirm));
    timing_window(100,true);confirm.kind=CF_EVENT;strcpy(confirm.name,"Alarm");
    assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIRM,&confirm));
    timing_window(100,true);
    contexts_fingerprint_status_v1 state={.struct_size=sizeof(state)};
    assert(api->fingerprint(NULL,CONTEXTS_FP_STATUS,&state));
    assert(state.event_slot>=0&&!strcmp(state.event_name,"Alarm")&&state.event_confidence>90);
    assert(state.dirty_sources==CONTEXTS_AUDIO);
    contexts_fingerprint_record_v1 record={.struct_size=sizeof(record),.source=CONTEXTS_AUDIO,.bytes=saved_bytes,.capacity=sizeof(saved_bytes)};
    assert(api->fingerprint(NULL,CONTEXTS_FP_EXPORT,&record));
    assert(cf_decode_source(&saved_bank,CF_AUDIO,saved_bytes,record.size));
    assert(!api->fingerprint(NULL,CONTEXTS_FP_IMPORT,&record)); /* unsaved edits */
    uint32_t generation=record.generation;record.generation--;
    assert(!api->fingerprint(NULL,CONTEXTS_FP_SAVED,&record));record.generation=generation;
    assert(api->fingerprint(NULL,CONTEXTS_FP_SAVED,&record));
    cfg.sources=0;assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIG,&cfg));
    assert(api->fingerprint(NULL,CONTEXTS_FP_STATUS,&state)&&state.room_slot<0&&state.event_slot<0);
    assert(provider->pause(NULL));assert(d->quiesce());d->stop();
    assert(d->start(deps,3));assert(api->fingerprint(NULL,CONTEXTS_FP_IMPORT,&record));
    saved_bytes[80]^=1;assert(!api->fingerprint(NULL,CONTEXTS_FP_IMPORT,&record));saved_bytes[80]^=1;
    assert(fp_bank.profiles[0].kind==CF_ROOM&&!strcmp(fp_bank.profiles[0].name,"Office"));
    cfg.sources=CONTEXTS_AUDIO;assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIG,&cfg));
    cf_enable(&fp_fusion,0,true);cf_enable(&fp_event_fusion,0,true);
    timing_window(200,true);assert(api->fingerprint(NULL,CONTEXTS_FP_STATUS,&state)&&state.event_slot<0);
    assert(d->quiesce());d->stop();
    puts("Fingerprint service: segmented PCM event training/match/unknown, source disable, dirty checkpoint acknowledgment, restart/import and corruption preservation PASS");
    return 0;
}
