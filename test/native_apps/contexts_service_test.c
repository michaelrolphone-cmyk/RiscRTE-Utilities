#include "ContextsServiceV1.h"
#include "AudioInputV1.h"
#include "RiscRadioIqV1.h"
#include "RiscPlatformClockV1.h"
#include "spectrum_signature_store.h"
#include "spectrum_store.h"
#include "rf_signature_store.h"
#include "rf_store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static const risc_driver_v2 *driver;
static const contexts_service_v1 *service;
static uint64_t now;
static unsigned opens,reads,closes,bursts,suspends,position;
static bool mic_live,close_fail,open_fail,read_fail,empty_read,bad_format,clocked;
static uint64_t producer_at,produced,consumed,queued;
static unsigned read_delay,overruns;
static unsigned dsp_delay,dsp_clock_countdown;
static bool dsp_regression;
static void advance_producer(void){
    if(!clocked||!mic_live)return;
    uint64_t added=(now-producer_at)*16u;produced+=added;queued+=added;producer_at=now;
    if(queued>512u){overruns++;queued=512u;}
}
static int rf_result;
static unsigned audio_frequency=1250,rf_bin=23;
static uint64_t millis(void *c){
    (void)c;
    if(dsp_clock_countdown&&!--dsp_clock_countdown){now+=dsp_delay;if(dsp_regression)--now;}
    return now;
}
static void pcm(int16_t *p,unsigned start,unsigned n,unsigned hz) {
    for(unsigned i=0;i<n;i++)p[i]=(int16_t)(6000*sin(6.283185307179586*(double)(start+i)*hz/16000.0));
}
static void iq(uint32_t *p,unsigned count,unsigned bin) {
    for(unsigned k=0;k<count;k++) {
        int i=(int)(170*cos(6.283185307179586*bin*k/256.0));
        int q=(int)(170*sin(6.283185307179586*bin*k/256.0));
        p[k]=((uint32_t)i&1023u)|(((uint32_t)q&1023u)<<10);
    }
}
static bool mic_open(void *c,uint32_t rate){(void)c;assert(rate==16000&&!mic_live);opens++;mic_live=true;producer_at=now;queued=produced=consumed=0;return !open_fail;}
static bool mic_read(void *c,int16_t *p,size_t count,size_t *got){
    (void)c;assert(mic_live&&count==256);reads++;*got=empty_read?0:count;
    if(clocked){advance_producer();if(queued<256u)*got=0;queued-=*got;consumed+=*got;}
    if(*got){pcm(p,position,(unsigned)*got,audio_frequency);position+=(unsigned)*got;}
    now+=read_delay;if(*got&&(dsp_delay||dsp_regression))dsp_clock_countdown=2;return !read_fail;
}
static bool mic_close(void *c){(void)c;assert(mic_live);advance_producer();closes++;if(close_fail)return false;mic_live=false;return true;}
static int capture(void *c,uint32_t *p,uint32_t count,const risc_radio_iq_settings_v1 *settings,risc_radio_iq_format_v1 *format) {
    (void)c;assert(count==256&&settings&&format->struct_size==sizeof(*format));bursts++;
    if(rf_result)return rf_result;
    iq(p,count,rf_bin);*format=(risc_radio_iq_format_v1){.struct_size=sizeof(*format),.flags=RISC_RADIO_IQ_FLAG_COHERENT_BURST,
        .center_hz=settings->center_hz,.sample_rate_hz=settings->sample_rate_hz,.bandwidth_hz=settings->bandwidth_hz,.pair_count=256,
        .sample_format=RISC_RADIO_IQ_FORMAT_S10_I0_Q10,.component_bits=bad_format?9:10,.component_full_scale=512};return 0;
}
static bool suspend_radio(void *c){(void)c;suspends++;return !close_fail;}
static const risc_platform_clock_api_v1 clock_table={.api_version=1,.struct_size=sizeof(clock_table),.monotonic_ms=millis};
static const twatch_audio_in_api_v1 microphone={1,sizeof(microphone),NULL,mic_open,mic_read,NULL,mic_close};
static const risc_radio_iq_extended_api_v1 radio={.base={.base={.api_version=1,.struct_size=sizeof(radio),.suspend=suspend_radio}},.capture_configured=capture};
static const risc_provider_dependency_v1 dependencies[]={{"platform.clock",1,&clock_table},{"audio.input",1,&microphone},{"radio.iq",1,&radio}};
static contexts_policy_v1 policy={sizeof(policy),true,true,true,true,CONTEXTS_ALL};
static contexts_status_v1 status(void){contexts_status_v1 s={.struct_size=sizeof(s)};assert(service->status(NULL,&s));return s;}
static void step(unsigned count){while(count--){now+=16;assert(service->step(NULL,&policy));}}
static void audio_profile(spectrum_signature *s,unsigned hz,const char *name) {
    spectrum_signature_analyzer analyzer;spectrum_signature_init(&analyzer);int16_t samples[256];
    pcm(samples,0,256,hz);assert(spectrum_signature_feed(&analyzer,samples,256));
    pcm(samples,256,256,hz);assert(spectrum_signature_feed(&analyzer,samples,256));
    *s=(spectrum_signature){.kind=SPECTRUM_SIGNATURE_ROOM};strcpy(s->name,name);
    for(unsigned k=0;k<64;k++)assert(spectrum_signature_add(s,analyzer.power));
}
static void export_source(unsigned source,bool ambiguous) {
    uint8_t bytes[RF_SIGNATURE_RECORD_SIZE];
    assert(service->request_export(NULL,source));assert(service->begin_export(NULL,source));
    if(source==CONTEXTS_AUDIO) {
        spectrum_preferences prefs=spectrum_preferences_default();spectrum_preferences_encode(&prefs,bytes);
        assert(service->export_record(NULL,source,CONTEXTS_RECORD_PREFERENCES,0,bytes,32));
        for(unsigned k=0;k<8;k++) {
            spectrum_signature s={0};if(k==0)audio_profile(&s,1250,"Study");if(k==1)audio_profile(&s,ambiguous?1250:2500,"Hall");
            if(k==2){audio_profile(&s,3750,"Clink");s.kind=SPECTRUM_SIGNATURE_EVENT;}
            assert(spectrum_signature_encode(&s,bytes));assert(service->export_record(NULL,source,CONTEXTS_RECORD_SIGNATURE,k,bytes,SPECTRUM_SIGNATURE_RECORD_SIZE));
        }
    } else {
        rf_preferences prefs=rf_preferences_default();assert(rf_preferences_encode(&prefs,bytes));
        assert(service->export_record(NULL,source,CONTEXTS_RECORD_PREFERENCES,0,bytes,RF_PREFERENCES_SIZE));
        rf_signature_analyzer analyzer;rf_signature_init(&analyzer);uint32_t samples[256];iq(samples,256,23);
        assert(rf_signature_burst(&analyzer,&prefs.identity,samples,256));
        for(unsigned k=0;k<8;k++) {
            rf_signature s={.identity=prefs.identity};
            if(k==0){s.kind=RF_SIGNATURE_ROOM;strcpy(s.name,"Studio");for(unsigned n=0;n<64;n++)assert(rf_signature_add(&s,analyzer.power));}
            if(k==1){s.kind=RF_SIGNATURE_EVENT;strcpy(s.name,"Burst");iq(samples,256,49);assert(rf_signature_burst(&analyzer,&prefs.identity,samples,256));assert(rf_signature_add(&s,analyzer.power));}
            assert(rf_signature_encode(&s,bytes));assert(service->export_record(NULL,source,CONTEXTS_RECORD_SIGNATURE,k,bytes,RF_SIGNATURE_RECORD_SIZE));
        }
    }
    assert(service->finish_export(NULL,source,CONTEXTS_EXPORT_OK));
}
int main(void) {
    driver=t5_driver_get(2);assert(driver&&!t5_driver_get(1));service=driver->capability;
    assert(!driver->start(dependencies,2));assert(driver->start(dependencies,3));assert(!driver->start(dependencies,3));
    assert(!opens&&!bursts&&status().state==CONTEXTS_OFF);
    step(3);assert(!opens&&!bursts&&status().state==CONTEXTS_UNAVAILABLE);
    assert(service->request_export(NULL,CONTEXTS_ALL));export_source(CONTEXTS_AUDIO,false);step(3);assert(!opens&&!bursts);
    export_source(CONTEXTS_RADIO,false);step(1100);
    contexts_status_v1 s=status();
    assert(s.audio.room_valid&&s.audio.room_slot==0&&!strcmp(s.audio.room_name,"Study"));
    assert(s.radio.room_valid&&s.radio.room_slot==0&&!strcmp(s.radio.room_name,"Studio"));
    assert(s.audio.signatures_ready&&!s.audio.temporal_ready&&!s.audio.neural_ready);
    assert(s.audio.current&&s.radio.current&&s.audio.age_ms<=32&&s.radio.age_ms<=100);
    uint32_t initial_entry=s.audio.room_entry;
    audio_frequency=0;step(4);assert(!status().audio.room_valid);
    audio_frequency=1250;step(2);assert(!status().audio.room_valid);
    step(130);assert(status().audio.room_valid&&status().audio.room_entry==initial_entry);
    audio_frequency=3750;step(4);assert(status().audio.event_valid&&status().audio.event_slot==2&&!strcmp(status().audio.event_name,"Clink"));
    audio_frequency=1250;step(4);assert(status().audio.event_valid);step(200);assert(!status().audio.event_valid);
    rf_bin=49;step(8);assert(status().radio.event_valid&&status().radio.event_slot==1);
    rf_bin=23;step(220);assert(!status().radio.event_valid);
    contexts_label_v1 label;assert(service->label(NULL,CONTEXTS_AUDIO,0,&label)==1&&!strcmp(label.name,"Study"));
    assert(service->label(NULL,CONTEXTS_AUDIO,8,&label)==0);
    assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,0,"wrong",s.audio.model_generation));
    assert(service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Study",s.audio.model_generation));
    assert(service->preset_result(NULL,CONTEXTS_AUDIO,s.audio.model_generation,CONTEXTS_PRESET_PARTIAL));
    assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Study",s.audio.model_generation));
    assert(service->pause(NULL)&&!mic_live&&!status().audio.current);
    step(1100);assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Study",s.audio.model_generation));
    audio_frequency=2500;step(1100);assert(status().audio.room_valid&&status().audio.room_slot==1);
    uint32_t unclaimed_entry=status().audio.room_entry;assert(unclaimed_entry>initial_entry);
    audio_frequency=1250;step(1100);assert(status().audio.room_valid&&status().audio.room_entry>unclaimed_entry);
    assert(service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Study",s.audio.model_generation));
    export_source(CONTEXTS_AUDIO,false);assert(status().audio.model_generation==s.audio.model_generation);
    step(1100);assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Study",s.audio.model_generation));
    audio_frequency=2500;step(1100);s=status();assert(s.audio.room_valid&&s.audio.room_slot==1);
    assert(service->claim_preset(NULL,CONTEXTS_AUDIO,1,"Hall",s.audio.model_generation));
    now+=1000;s=status();assert(!s.audio.current&&!s.audio.room_valid&&!s.radio.current);
    assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,1,"Hall",s.audio.model_generation));
    step(4);close_fail=true;assert(!service->pause(NULL)&&status().cleanup_pending);
    unsigned before_reads=reads,before_bursts=bursts;assert(!service->step(NULL,&policy));assert(reads==before_reads&&bursts==before_bursts);
    close_fail=false;assert(service->step(NULL,&policy)&&!mic_live&&!status().cleanup_pending);assert(reads==before_reads);
    policy.audio_allowed=false;policy.radio_allowed=false;step(10);assert(!mic_live&&bursts==before_bursts);
    policy.audio_allowed=policy.radio_allowed=true;rf_result=RISC_RADIO_IQ_BUSY;unsigned before_suspend=suspends;step(10);assert(suspends==before_suspend&&status().radio.capture_error==RISC_RADIO_IQ_BUSY);
    rf_result=RISC_RADIO_IQ_CLEANUP_RETAINED;now+=5000;assert(!service->step(NULL,&policy)&&status().cleanup_pending);
    close_fail=true;assert(!service->pause(NULL));before_reads=reads;before_bursts=bursts;
    before_suspend=suspends;close_fail=false;assert(service->pause(NULL)&&suspends==before_suspend+1);assert(reads==before_reads&&bursts==before_bursts);
    rf_result=0;policy.awake=false;step(10);assert(!mic_live&&bursts==before_bursts);
    policy.awake=true;export_source(CONTEXTS_AUDIO,true);audio_frequency=1250;now+=5000;step(1100);
    s=status();assert(s.audio.room_ambiguous&&!s.audio.room_valid&&s.audio.model_generation>1);
    assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Study",s.audio.model_generation));
    assert(service->request_export(NULL,CONTEXTS_AUDIO));assert(service->begin_export(NULL,CONTEXTS_AUDIO));
    uint8_t bad[32]={0};assert(!service->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_PREFERENCES,0,bad,sizeof(bad)));
    assert(service->finish_export(NULL,CONTEXTS_AUDIO,CONTEXTS_EXPORT_OK));s=status();assert(!s.export_pending&&s.audio.model_state==CONTEXTS_MODEL_FAILED);
    before_reads=reads;before_bursts=bursts;step(10);assert(reads==before_reads&&bursts>before_bursts&&status().radio.current&&!mic_live);
    assert(service->request_export(NULL,CONTEXTS_AUDIO));assert(service->finish_export(NULL,CONTEXTS_AUDIO,CONTEXTS_EXPORT_STORAGE));assert(!status().export_pending);
    export_source(CONTEXTS_AUDIO,false);assert(status().audio.model_generation>s.audio.model_generation);
    policy.radio_allowed=false;open_fail=true;before_reads=reads;step(1);assert(!mic_live&&reads==before_reads&&status().audio.capture_error==1);
    open_fail=false;now+=5000;step(3);assert(mic_live);read_fail=true;step(1);assert(!mic_live&&status().audio.capture_error==2);
    read_fail=false;now+=5000;step(130);empty_read=true;step(40);assert(!status().audio.current&&!status().audio.room_valid);empty_read=false;step(2);assert(!status().audio.room_valid);
    policy.radio_allowed=true;bad_format=true;now+=5000;step(1);assert(!status().radio.current&&status().radio.capture_error==RISC_RADIO_IQ_BAD_ARGUMENT);bad_format=false;
    assert(service->pause(NULL));policy.sources=CONTEXTS_AUDIO;policy.radio_allowed=false;audio_frequency=1250;clocked=true;overruns=0;
    const unsigned cadences[]={8,16,20,24};
    for(unsigned cadence=0;cadence<4;cadence++) {
        assert(service->pause(NULL));now+=16;assert(service->step(NULL,&policy));
        for(unsigned tick=0;tick<600;tick++){now+=cadences[cadence];assert(service->step(NULL,&policy));advance_producer();assert(consumed+queued==produced&&queued<256u&&!overruns);}
        assert(status().audio.room_valid&&status().audio.current);
    }
    /* Long UI frames are safe only because real capture checkpoints service
     * the two-buffer RX queue while the full policy step is120ms apart. */
    assert(service->pause(NULL));now+=16;assert(service->step(NULL,&policy));
    unsigned prior_opens=opens,prior_bursts=bursts;
    for(unsigned tick=1;tick<=1000;tick++) {
        now+=8;if(tick%15==0)assert(service->step(NULL,&policy));else assert(service->capture_audio(NULL));
        advance_producer();assert(consumed+queued==produced&&queued<256u&&!overruns);
    }
    assert(status().audio.room_valid&&opens==prior_opens&&bursts==prior_bursts);
    now+=40;advance_producer();assert(overruns&&service->capture_audio(NULL)&&!mic_live&&!status().audio.current);
    before_reads=reads;now+=20;assert(service->capture_audio(NULL)&&opens==prior_opens&&reads==before_reads);
    assert(service->step(NULL,&policy)&&opens==prior_opens+1&&!status().audio.room_valid);
    read_delay=9;before_reads=reads;now+=24;assert(service->capture_audio(NULL));assert(reads==before_reads+1&&!status().audio.current&&status().audio.capture_error==5);read_delay=0;
    assert(service->pause(NULL));assert(service->step(NULL,&policy));dsp_delay=24;now+=16;
    assert(service->capture_audio(NULL)&&mic_live&&!status().audio.current&&status().audio.capture_error==5);dsp_delay=0;
    assert(service->pause(NULL));assert(service->step(NULL,&policy));dsp_delay=32;now+=16;
    assert(service->capture_audio(NULL)&&!mic_live&&!status().audio.current&&status().audio.capture_error==5);dsp_delay=0;
    assert(service->step(NULL,&policy));read_delay=2;dsp_regression=true;now+=16;
    assert(service->capture_audio(NULL)&&!mic_live&&!status().audio.current&&status().audio.capture_error==4);read_delay=0;dsp_regression=false;
    assert(service->pause(NULL));before_reads=reads;prior_opens=opens;now+=200;assert(service->capture_audio(NULL)&&reads==before_reads&&opens==prior_opens);
    assert(service->step(NULL,&policy));close_fail=true;now+=40;assert(!service->capture_audio(NULL)&&status().cleanup_pending);before_reads=reads;assert(!service->capture_audio(NULL)&&reads==before_reads);close_fail=false;assert(service->pause(NULL));
    assert(driver->quiesce());driver->stop();assert(!service->step(NULL,&policy));
    printf("Contexts: actual PCM/IQ room inference, copied owner exports, model identity, ambiguity, staleness, once-per-room presets, no training, exclusive pause and retained cleanup PASS\n");
}
