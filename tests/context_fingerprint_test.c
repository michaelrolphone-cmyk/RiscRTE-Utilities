#include "../lib/ContextFingerprint/include/ContextFingerprintStore.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
static cf_pipeline pipeline;
static cf_feature first,second;
static cf_bank bank,decoded;
static cf_fusion fusion;
static uint8_t encoded[CF_STORE_MAX];
static void pcm_train(unsigned period,cf_feature *out){
    cf_config c=cf_defaults(CF_AUDIO);c.window_ms=5000;
    assert(cf_init(&pipeline,&c,1));
    int16_t pcm[16];uint32_t power[128]={0};power[8]=1u<<22;power[31]=1u<<21;
    for(unsigned ms=0;ms<5000;ms++){
        for(unsigned j=0;j<16;j++)pcm[j]=(ms%period<8&&ms>=200)?16000:1000;
        assert(cf_pcm(&pipeline,pcm,16,16000,(uint64_t)ms*1000,true));
        if(!(ms%16))assert(cf_spectrum(&pipeline,power,128,1.f/(1u<<30),0,8000));
    }
    assert(cf_take(&pipeline,5000000,out));
}
int main(void){
    cf_config c=cf_defaults(CF_AUDIO);c.window_ms=5000;
    for(unsigned i=1;i<1000000;i+=997)assert(fabsf(cf_sqrt((float)i)-sqrtf((float)i))<.001f);
    pcm_train(100,&first);assert(first.flags&CF_TEMPORAL);assert(first.bins==1&&first.repetition[0].ms==100);
    assert(first.duty_cycle>.07f&&first.duty_cycle<.08f);assert(pipeline.burst_count>40);
    assert(pipeline.bursts[0].duration_us==8000);
    pcm_train(200,&second);assert(second.bins==1&&second.repetition[0].ms==200);
    assert(fabsf(cf_emd(&first,&second)-100.f)<.01f);
    cf_enable(&fusion,CF_AUDIO,true);cf_publish(&fusion,CF_AUDIO,&first,5000000);
    assert(cf_train(&bank,&fusion,0,"Office",CF_ROOM,5000000,10000000,&c,true));
    cf_publish(&fusion,CF_AUDIO,&second,10000000);
    assert(cf_train(&bank,&fusion,1,"Kitchen",CF_ROOM,10000000,10000000,&c,true));
    cf_match m=cf_match_profiles(&bank,&fusion,CF_ROOM,10000000,10000000,&c);assert(m.slot==1&&m.confidence>.99f);
    /* Equal average spectrum/amplitude but different timing is discriminated. */
    second=first;second.repetition[0].ms=150;cf_publish(&fusion,CF_AUDIO,&second,15000000);
    m=cf_match_profiles(&bank,&fusion,CF_ROOM,15000000,10000000,&c);assert(m.slot==-1);
    cf_publish(&fusion,CF_AUDIO,&first,20000000);m=cf_match_profiles(&bank,&fusion,CF_ROOM,20000000,10000000,&c);assert(m.slot==0);
    assert(cf_train(&bank,&fusion,0,"Office",CF_ROOM,20000000,10000000,&c,false));
    assert(!cf_train(&bank,&fusion,0,"Office",CF_ROOM,20000000,10000000,&c,false));
    uint32_t n=cf_encode_source(&bank,CF_AUDIO,encoded,sizeof(encoded));assert(n&&cf_decode_source(&decoded,CF_AUDIO,encoded,n));
    assert(decoded.profiles[0].feature[0].repetition[0].ms==100);
    encoded[n/2]^=1;assert(!cf_decode_source(&decoded,CF_AUDIO,encoded,n));encoded[n/2]^=1;
    cf_enable(&fusion,CF_AUDIO,false);assert(cf_match_profiles(&bank,&fusion,CF_ROOM,20000000,10000000,&c).slot==-1);
    cf_enable(&fusion,CF_AUDIO,true);assert(!fusion.have[0]&&fusion.fresh_windows[0]==0);
    cf_publish(&fusion,CF_AUDIO,&first,30000000);assert(cf_weight(&bank.profiles[0],&fusion,0,10000000,&c)<.11f);
    assert(cf_weight(&bank.profiles[0],&fusion,0,10000000+10*c.stale_after_seconds,&c)<.011f);
    assert(cf_match_profiles(&bank,&fusion,CF_ROOM,50000000,10000000,&c).slot==-1);
    /* Independently captured microsecond RF bursts cannot manufacture 100ms
       repetition fingerprints out of host polling cadence. */
    cf_config rf=cf_defaults(CF_RF);assert(cf_init(&pipeline,&rf,2));uint32_t iq[256],power[128]={0};
    for(unsigned i=0;i<256;i++)iq[i]=100|(100u<<10);
    power[8]=100000;
    for(unsigned ms=0;ms<5000;ms+=100){assert(cf_iq(&pipeline,iq,256,16000000,(uint64_t)ms*1000,false));assert(cf_spectrum(&pipeline,power,128,1.f/(1u<<30),2440000000.f,16000000.f));}
    assert(cf_take(&pipeline,5000000,&second));assert(!(second.flags&CF_TEMPORAL));assert(second.flags&CF_SPECTRAL);
    /* Event labels use the same temporal matcher independently of rooms. */
    cf_publish(&fusion,CF_AUDIO,&first,60000000);assert(cf_train(&bank,&fusion,2,"Door alarm",CF_EVENT,60000000,10000000,&c,true));
    assert(cf_match_profiles(&bank,&fusion,CF_EVENT,60000000,10000000,&c).slot==2);
    c.temporal_only=true;cf_publish(&fusion,CF_AUDIO,&second,61000000);assert(cf_match_profiles(&bank,&fusion,CF_EVENT,61000000,10000000,&c).slot==-1);
    /* Fusion must reject disagreement; disabling one source removes its vote. */
    memset(&bank,0,sizeof(bank));memset(&fusion,0,sizeof(fusion));c.temporal_only=false;
    pcm_train(100,&first);pcm_train(200,&second);
    cf_enable(&fusion,CF_AUDIO,true);cf_enable(&fusion,CF_RF,true);
    cf_publish(&fusion,CF_AUDIO,&first,1000000);cf_publish(&fusion,CF_RF,&first,1000000);
    assert(cf_train(&bank,&fusion,0,"Same room",CF_ROOM,1000000,10000000,&c,true));
    fusion.fresh_windows[0]=fusion.fresh_windows[1]=10;
    fusion.live[1]=second;
    assert(cf_match_profiles(&bank,&fusion,CF_ROOM,1000000,10000000,&c).slot==-1);
    cf_enable(&fusion,CF_RF,false);
    assert(cf_match_profiles(&bank,&fusion,CF_ROOM,1000000,10000000,&c).slot==0);
    cf_enable(&fusion,CF_RF,true);cf_publish(&fusion,CF_RF,&second,1000000);
    assert(cf_match_profiles(&bank,&fusion,CF_ROOM,1000000,10000000,&c).slot==0);
    fusion.fresh_windows[1]=10;
    bank.profiles[0].updated_seconds[1]=1;
    assert(cf_match_profiles(&bank,&fusion,CF_ROOM,1000000,10000000,&c).slot==0);
    /* A matching explicitly taught nonmatch vetoes an event, not a room. */
    cf_enable(&fusion,CF_RF,false);cf_publish(&fusion,CF_AUDIO,&first,2000000);
    assert(cf_train(&bank,&fusion,8,"Alarm",CF_EVENT,2000000,10000000,&c,true));
    cf_publish(&fusion,CF_AUDIO,&first,3000000);
    assert(cf_train(&bank,&fusion,16,"Alarm",CF_NEGATIVE,3000000,10000000,&c,true));
    assert(cf_match_profiles(&bank,&fusion,CF_EVENT,3000000,10000000,&c).slot==-1);
    /* Coarse radio observation supports slow cycles, never polling aliases. */
    for(unsigned period=200;period<=1000;period+=800){
        rf.window_ms=6000;assert(cf_init(&pipeline,&rf,2));
        for(unsigned ms=0;ms<6000;ms+=100){
            unsigned amplitude=ms%period==0?300:20;
            for(unsigned k=0;k<256;k++)iq[k]=amplitude;
            assert(cf_iq_snapshot(&pipeline,iq,256,16000000,(uint64_t)ms*1000));
        }
        assert(cf_take(&pipeline,6000000,&second));assert(second.flags&CF_SPARSE);
        assert(second.resolution_ms==100);
        if(period==1000){assert(second.flags&CF_TEMPORAL);assert(second.bins==1&&second.repetition[0].ms==1000);}
        else assert(!(second.flags&CF_TEMPORAL));
    }
    /* Interval edge, overflow and exact transport distance. */
    memset(&first,0,sizeof(first));memset(&second,0,sizeof(second));first.flags=second.flags=CF_TEMPORAL;first.bins=second.bins=1;first.repetition[0]=(cf_bin){0,1};second.repetition[0]=(cf_bin){10000,1};assert(cf_emd(&first,&second)==10000);
    uint16_t bins=0;for(unsigned i=0;i<CF_HIST_CAPACITY;i++)assert(cf_hist_add(first.repetition,&bins,i,1));assert(!cf_hist_add(first.repetition,&bins,9999,1));
    printf("context fingerprints: IQ/PCM extraction, timing, EMD, unknown, events, fusion lifecycle, training, storage PASS\n");
}
