/* Exercise the real optional service ABI, source-local storage mapping and
 * event windows. Existing canonical peripheral fixtures remain unchanged. */
#define CONTEXTS_TEST_ENTRY existing_contexts_temporal_tests
#include "contexts_temporal_test.c"
#undef CONTEXTS_TEST_ENTRY
static cf_bank saved_bank,radio_shared_bank;
static uint8_t radio_shared_bytes[CF_STORE_MAX];
static uint8_t saved_bytes[CF_STORE_MAX];
static void timing_window(unsigned period,bool event){
    int16_t pcm[16];uint32_t power[128]={0};power[8]=1u<<22;
    cf_pipeline*p=event?fp_event_pipeline(CF_AUDIO):fp_pipeline(CF_AUDIO);
    uint64_t began=test_now;
    if(event)cf_window_reset(p,test_now*1000u);
    for(unsigned ms=0;ms<1500;ms++){
        test_now=began+ms;
        for(unsigned i=0;i<16;i++)pcm[i]=ms%period<8?16000:1000;
        assert(cf_pcm(p,pcm,16,16000,test_now*1000u,true));
        if(!(ms%16))assert(cf_spectrum(p,power,128,1.f/(1u<<30),0,8000));
        if(event)fp_temporal_event(CF_AUDIO,test_now);
    }
    test_now=began+1500;
    if(event){for(unsigned ms=0;ms<600;ms++){for(unsigned i=0;i<16;i++)pcm[i]=1000;assert(cf_pcm(p,pcm,16,16000,test_now*1000u,true));fp_temporal_event(CF_AUDIO,test_now++);}}
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
    /* RF owner slot6 and Audio slot0 are the same named place. Preserve
     * each source's background spectrum through merge/export/import. */
    radio_shared_bank.profiles[6]=saved_bank.profiles[0];
    cf_profile *rp=&radio_shared_bank.profiles[6];rp->feature[1]=rp->feature[0];rp->feature[1].identity=99;
    rp->present[0]=0;rp->present[1]=1;rp->weight[1]=1;
    uint32_t radio_size=cf_encode_source(&radio_shared_bank,CF_RF,radio_shared_bytes,sizeof(radio_shared_bytes));assert(radio_size);
    contexts_fingerprint_record_v1 rr={.struct_size=sizeof(rr),.source=CONTEXTS_RADIO,.size=radio_size,.capacity=sizeof(radio_shared_bytes),.bytes=radio_shared_bytes};
    assert(api->fingerprint(NULL,CONTEXTS_FP_IMPORT,&rr));
    int shared=cf_profile_find(&fp_bank,CF_ROOM,"Office");assert(shared>=0&&fp_bank.profiles[shared].present[0]&&fp_bank.profiles[shared].present[1]);
    uint32_t bg[128];assert(cf_background_power(&fp_bank.profiles[shared],CF_RF,99,bg)&&bg[8]==1u<<22);
    assert(api->fingerprint(NULL,CONTEXTS_FP_EXPORT,&rr)&&cf_decode_source(&radio_shared_bank,CF_RF,radio_shared_bytes,rr.size));
    assert(radio_shared_bank.profiles[6].present[1]&&!strcmp(radio_shared_bank.profiles[6].name,"Office"));
    cfg.sources=0;assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIG,&cfg));
    assert(api->fingerprint(NULL,CONTEXTS_FP_STATUS,&state)&&state.room_slot<0&&state.event_slot<0);
    assert(provider->pause(NULL));assert(d->quiesce());d->stop();
    assert(d->start(deps,3));assert(api->fingerprint(NULL,CONTEXTS_FP_IMPORT,&record));
    saved_bytes[80]^=1;assert(!api->fingerprint(NULL,CONTEXTS_FP_IMPORT,&record));saved_bytes[80]^=1;
    assert(fp_bank.profiles[0].kind==CF_ROOM&&!strcmp(fp_bank.profiles[0].name,"Office"));
    cfg.sources=CONTEXTS_AUDIO;assert(api->fingerprint(NULL,CONTEXTS_FP_CONFIG,&cfg));
    cf_enable(&fp_fusion,0,true);cf_enable(&fp_event_fusion,0,true);
    timing_window(200,true);assert(api->fingerprint(NULL,CONTEXTS_FP_STATUS,&state)&&state.event_slot<0);
    contexts_learning_v1 learn={.struct_size=sizeof(learn),.operation=CONTEXTS_LEARN_BEGIN,.kind=CF_EVENT,.name="Doorbell"};
    assert(api->fingerprint(NULL,CONTEXTS_FP_LEARN,&learn));
    for(unsigned take=0;take<3;take++){
        timing_window(100,true);learn.operation=CONTEXTS_LEARN_POLL;
        assert(api->fingerprint(NULL,CONTEXTS_FP_LEARN,&learn));
        assert(learn.samples==take+1);
        if(take<2)assert(learn.state!=CONTEXTS_LEARN_READY);
    }
    assert(learn.state==CONTEXTS_LEARN_READY);learn.operation=CONTEXTS_LEARN_SAVE;
    assert(api->fingerprint(NULL,CONTEXTS_FP_LEARN,&learn));
    bool learned=false;for(unsigned i=8;i<16;i++)if(!strcmp(fp_bank.profiles[i].name,"Doorbell"))learned=fp_bank.profiles[i].kind==CF_EVENT;
    assert(learned);
    assert(d->quiesce());d->stop();
    puts("Fingerprint service: segmented PCM event training/match/unknown, source disable, dirty checkpoint acknowledgment, restart/import and corruption preservation PASS");
    return 0;
}
