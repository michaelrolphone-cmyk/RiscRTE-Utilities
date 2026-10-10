/* Reuse the production PCM/IQ fixture, but admit only clock + real RF. */
#define main contexts_full_profile_fixture
#include "contexts_service_test.c"
#undef main
#ifdef CONTEXTS_TEST_OWNER
#define PORTABLE_CONTEXTS_CLIENT
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "contexts_owner_export.h"
static unsigned owner_stops,owner_acquires;
const contexts_service_v1 *portable_contexts_service(void){return service;}
bool portable_contexts_stop(void){owner_stops++;return service->pause(NULL);}
static bool no_owner_acquire(const char*c,uint32_t v,uint64_t n,risc_runtime_capability_v1*g){(void)c;(void)v;(void)n;(void)g;owner_acquires++;return false;}
static const risc_runtime_api_v1 owner_runtime={.api_version=1,.struct_size=sizeof(owner_runtime),.acquire=no_owner_acquire};
#endif
static unsigned clock_reads;
static uint64_t counted_clock(void*c){clock_reads++;return millis(c);}
static const risc_platform_clock_api_v1 rf_clock={.api_version=1,.struct_size=sizeof(rf_clock),.monotonic_ms=counted_clock};
static const risc_provider_dependency_v1 rf_dependencies[]={{"radio.iq",1,&radio},{"platform.clock",1,&rf_clock}};
static void absent_audio(void){
    contexts_status_v1 s=status();
    assert(s.audio.source==CONTEXTS_AUDIO&&s.audio.model_state==CONTEXTS_MODEL_UNAVAILABLE);
    assert(s.audio.model_error==CONTEXTS_EXPORT_UNSUPPORTED&&!s.audio.signatures_ready&&!s.audio.temporal_ready&&!s.audio.neural_ready);
    assert(!s.audio.current&&!s.audio.room_valid&&!s.audio.event_valid&&!s.audio.samples&&s.audio.age_ms==UINT32_MAX);
    assert(!(s.export_pending&CONTEXTS_AUDIO)&&s.export_active!=CONTEXTS_AUDIO);
    assert(!opens&&!reads&&!closes&&!mic_live);
}
int main(void){
    driver=t5_driver_get(2);assert(driver);service=driver->capability;
    assert(!driver->start(dependencies,3)); /* Full manifest cannot bind this profile. */
    assert(!driver->start(dependencies,2)); /* clock + audio is not an RF provider. */
    assert(!driver->start(rf_dependencies,1));
    risc_provider_dependency_v1 bad[2]={rf_dependencies[0],rf_dependencies[1]};
    bad[1]=bad[0];assert(!driver->start(bad,2));
    bad[1]=rf_dependencies[1];bad[0].api_version=2;assert(!driver->start(bad,2));
    risc_radio_iq_extended_api_v1 incomplete=radio;incomplete.capture_configured=NULL;bad[0]=rf_dependencies[0];bad[0].api=&incomplete;assert(!driver->start(bad,2));
    incomplete=radio;incomplete.base.base.struct_size=sizeof(incomplete)-1;assert(!driver->start(bad,2));
    assert(driver->start(rf_dependencies,2)&&!driver->start(rf_dependencies,2));absent_audio();
    contexts_model_details_v1 d={.struct_size=sizeof(d)};assert(service->model_details(NULL,CONTEXTS_AUDIO,&d));
    assert(d.source==CONTEXTS_AUDIO&&d.temporal_state==CONTEXTS_IMPORT_UNAVAILABLE&&d.neural_state==CONTEXTS_IMPORT_UNAVAILABLE);
    assert(d.bank_error[0]==CONTEXTS_IMPORT_UNSUPPORTED&&d.bank_error[1]==CONTEXTS_IMPORT_UNSUPPORTED&&d.neural_error==CONTEXTS_IMPORT_UNSUPPORTED);
    contexts_status_v1 before=status();unsigned ticks=clock_reads;
    contexts_label_v1 label;uint8_t bytes[32]={0};
    assert(!service->request_export(NULL,CONTEXTS_AUDIO)&&!service->request_export(NULL,0)&&!service->request_export(NULL,7));
    assert(!service->begin_export(NULL,CONTEXTS_AUDIO));
    assert(!service->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_PREFERENCES,0,bytes,sizeof(bytes)));
    assert(!service->export_model_error(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,0,CONTEXTS_IMPORT_INVALID));
    assert(!service->finish_export(NULL,CONTEXTS_AUDIO,CONTEXTS_EXPORT_OK));
    assert(service->label(NULL,CONTEXTS_AUDIO,0,&label)==-1);
    assert(!service->claim_preset(NULL,CONTEXTS_AUDIO,0,"Absent",1));
    assert(!service->preset_result(NULL,CONTEXTS_AUDIO,1,CONTEXTS_PRESET_APPLIED));
    for(unsigned i=0;i<100;i++)assert(service->capture_audio(NULL));
    assert(clock_reads==ticks&&!bursts&&!suspends);contexts_status_v1 after=status();assert(!memcmp(&before,&after,sizeof(before)));
    policy.sources=CONTEXTS_AUDIO;step(3);assert(status().state==CONTEXTS_UNAVAILABLE&&!bursts);absent_audio();
    policy.sources=CONTEXTS_ALL;assert(service->request_export(NULL,CONTEXTS_ALL));
    assert(status().export_pending==CONTEXTS_RADIO&&status().radio.model_state==CONTEXTS_MODEL_REQUESTED);absent_audio();
#ifdef CONTEXTS_TEST_OWNER
    assert(contexts_owner_export(&owner_runtime,CONTEXTS_AUDIO,2)==CONTEXTS_OWNER_NORMAL);
    assert(contexts_owner_refresh(&owner_runtime,CONTEXTS_AUDIO,2));
    assert(!owner_acquires&&!owner_stops); /* No Audio storage export or owner capture. */
#endif
    assert(service->begin_export(NULL,CONTEXTS_RADIO));before=status();ticks=clock_reads;
    assert(!service->request_export(NULL,CONTEXTS_AUDIO)&&!service->begin_export(NULL,CONTEXTS_AUDIO)&&!service->finish_export(NULL,CONTEXTS_AUDIO,CONTEXTS_EXPORT_OK));
    assert(clock_reads==ticks);after=status();assert(!memcmp(&before,&after,sizeof(before)));
    assert(service->finish_export(NULL,CONTEXTS_RADIO,CONTEXTS_EXPORT_STORAGE));
    export_source(CONTEXTS_RADIO,false);step(1100);contexts_status_v1 s=status();
    assert(s.state==CONTEXTS_LIVE&&s.radio.room_valid&&s.radio.room_slot==0&&!strcmp(s.radio.room_name,"Studio"));absent_audio();
    ticks=clock_reads;unsigned captures=bursts;assert(!service->request_export(NULL,CONTEXTS_AUDIO));assert(service->capture_audio(NULL)&&clock_reads==ticks&&bursts==captures);
    assert(service->label(NULL,CONTEXTS_RADIO,0,&label)==1&&!strcmp(label.name,"Studio"));
    assert(service->claim_preset(NULL,CONTEXTS_RADIO,0,"Studio",s.radio.model_generation));
    assert(service->preset_result(NULL,CONTEXTS_RADIO,s.radio.model_generation,CONTEXTS_PRESET_APPLIED));
    rf_bin=49;step(8);assert(status().radio.event_valid&&status().radio.event_slot==1);
    rf_bin=23;policy.radio_allowed=false;captures=bursts;step(20);assert(bursts==captures&&!status().radio.current);
    policy.radio_allowed=true;rf_result=RISC_RADIO_IQ_BUSY;unsigned cleanups=suspends;now+=5000;step(1);assert(suspends==cleanups&&status().radio.capture_error==RISC_RADIO_IQ_BUSY);
    rf_result=RISC_RADIO_IQ_CLEANUP_RETAINED;now+=5000;assert(!service->step(NULL,&policy)&&status().cleanup_pending);
    ticks=clock_reads;captures=bursts;cleanups=suspends;
    assert(!service->capture_audio(NULL)&&clock_reads==ticks&&bursts==captures&&suspends==cleanups);
#ifdef CONTEXTS_TEST_OWNER
    assert(contexts_owner_export(&owner_runtime,CONTEXTS_AUDIO,2)==CONTEXTS_OWNER_RETAINED);
    assert(!contexts_owner_refresh(&owner_runtime,CONTEXTS_AUDIO,2));assert(!owner_acquires&&!owner_stops&&clock_reads==ticks);
#endif
    close_fail=true;assert(!service->pause(NULL)&&!driver->quiesce());driver->stop();
    assert(status().cleanup_pending&&!service->request_export(NULL,CONTEXTS_ALL));
    assert(!service->capture_audio(NULL)&&bursts==captures);close_fail=false;
    assert(service->pause(NULL)&&!status().cleanup_pending);rf_result=0;
    assert(service->request_export(NULL,CONTEXTS_ALL)&&service->finish_export(NULL,CONTEXTS_RADIO,CONTEXTS_EXPORT_STORAGE));
    step(10);assert(status().radio.model_state==CONTEXTS_MODEL_FAILED&&!status().export_pending);absent_audio();
    export_source(CONTEXTS_RADIO,false);now+=5000;step(1100);assert(status().radio.room_valid);
    now+=1000;assert(!status().radio.current);assert(driver->quiesce());driver->stop();
    assert(!service->capture_audio(NULL)&&!service->step(NULL,&policy));
    assert(driver->start(rf_dependencies,2));absent_audio();assert(status().radio.model_state==CONTEXTS_MODEL_EMPTY);assert(driver->quiesce());driver->stop();
    puts("RF-only Contexts: exact admission, unavailable Audio, filtered exports, real IQ inference, no Audio owner I/O, retained RF fence and restart PASS");
}
