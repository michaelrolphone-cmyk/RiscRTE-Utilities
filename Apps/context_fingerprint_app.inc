/* Shared by the watch and paper RF app and by the audio Spectrogram. The
 * fourth app-data file keeps temporal profiles independent of legacy records. */
#include "../lib/ContextFingerprint/include/ContextFingerprintStore.h"
#if defined(__has_include)
# if __has_include("PortableFingerprintClock.h")
#  include "PortableFingerprintClock.h"
#  define CFA_CLOCK_SDK 1
# endif
#endif
#ifndef CFA_CLOCK_SDK
/* Older adapters remain buildable; only date-based profile aging is absent. */
static bool portable_fingerprint_clock(const risc_runtime_api_v1*rt,uint64_t*out){(void)rt;*out=0;return true;}
#endif
static cf_pipeline cfa_pipeline,cfa_event_pipeline;
static cf_bank cfa_bank,cfa_read_bank;
static cf_fusion cfa_fusion,cfa_event_fusion;
static cf_feature cfa_window,cfa_event_window,cfa_training_window;
static cf_config cfa_config;
static cf_match cfa_room,cfa_event;
static uint8_t cfa_bytes[CF_STORE_MAX];
static bool cfa_loaded,cfa_changed,cfa_failed,cfa_training_ready,cfa_segment_active,cfa_training_saved;
static uint32_t cfa_saved_at,cfa_seen,cfa_event_at;
static uint64_t cfa_time_us,cfa_utc;
static uint32_t cfa_utc_at,cfa_last_capture_ms;
static bool cfa_have_capture;
static uint64_t cfa_seconds(void){return cfa_utc?cfa_utc+(uint32_t)(app->millis()-cfa_utc_at)/1000u:0;}
static void cfa_capture_clock(uint32_t stamp){
    if(cfa_have_capture){
        uint32_t gap=stamp-cfa_last_capture_ms;cfa_time_us+=(uint64_t)gap*1000u;
        if(gap>(CFA_SOURCE?500u:64u)){
            cf_gap(&cfa_pipeline);cf_gap(&cfa_event_pipeline);
            if(!CFA_SOURCE){cf_window_reset(&cfa_pipeline,cfa_time_us);cf_window_reset(&cfa_event_pipeline,cfa_time_us);}
            cfa_fusion.have[CFA_SOURCE]=false;cfa_event_fusion.have[CFA_SOURCE]=false;
            cfa_room.slot=cfa_event.slot=-1;
        }
    }else{cfa_have_capture=true;cfa_time_us=(uint64_t)stamp*1000u;}
    cfa_last_capture_ms=stamp;
}
static unsigned cfa_source(void){return CFA_SOURCE;}
static uint32_t cfa_identity(void){
#if CFA_SOURCE == 1
    const rf_capture_identity*i=&prefs.identity;
    const uint32_t v[]={i->lo_hz,i->sample_rate_hz,i->width_hz,i->raw_gain,i->rf_gain,i->bb_gain,i->filter,i->dc[0],i->dc[1],i->dc[2],i->dc[3],i->iq_correction};
    return cf_crc((const uint8_t*)v,sizeof(v));
#else
    return sample_rate;
#endif
}
static void cfa_begin(void){
    cfa_config=cf_defaults(cfa_source());cf_init(&cfa_pipeline,&cfa_config,cfa_identity());
    cf_init(&cfa_event_pipeline,&cfa_config,cfa_identity());cfa_event_pipeline.config.window_ms=100;
    cf_enable(&cfa_fusion,cfa_source(),false);cf_enable(&cfa_fusion,cfa_source(),true);
    cf_enable(&cfa_event_fusion,cfa_source(),false);cf_enable(&cfa_event_fusion,cfa_source(),true);
    cfa_training_ready=cfa_segment_active=cfa_training_saved=false;cfa_room=(cf_match){.slot=-1};cfa_event=cfa_room;cfa_time_us=(uint64_t)app->millis()*1000u;cfa_seen=0;cfa_have_capture=false;
}
static void cfa_load(void){
    if(cfa_loaded||cfa_failed||!event_files.api)return;
    uint32_t size=0,actual=0;uint64_t revision=0,current=0;
    int32_t rc=event_files.api->stat(event_files.api->context,"context-fingerprints.cfp",&size,&revision);
    if(rc==RISC_APP_DATA_NOT_FOUND){cfa_loaded=true;return;}
    if(!rc&&size<=sizeof(cfa_bytes)&&revision)rc=event_files.api->read(event_files.api->context,"context-fingerprints.cfp",revision,cfa_bytes,sizeof(cfa_bytes),&actual,&current);
    else if(!rc)rc=RISC_APP_DATA_INVALID;
    if(!rc&&(actual!=size||current!=revision||!cf_decode_source(&cfa_read_bank,cfa_source(),cfa_bytes,actual)))rc=RISC_APP_DATA_INVALID;
    if(rc==RISC_APP_DATA_RETAINED)event_files.retained=true;
    if(!rc){cfa_bank=cfa_read_bank;cfa_loaded=true;}else{cfa_failed=true;notify("FINGERPRINTS UNREAD / RETRY");}
}
static void cfa_restore(void){
    memset(&cfa_bank,0,sizeof(cfa_bank));memset(&cfa_fusion,0,sizeof(cfa_fusion));memset(&cfa_event_fusion,0,sizeof(cfa_event_fusion));cfa_loaded=cfa_changed=cfa_failed=false;
    cfa_room=(cf_match){.slot=-1};cfa_event=cfa_room;cfa_saved_at=app->millis();cfa_utc_at=cfa_saved_at;
    if(!portable_fingerprint_clock(runtime,&cfa_utc)){event_files.retained=true;return;}
    cfa_begin();cfa_load();
}
static void cfa_save(void){
    if(!cfa_changed||!cfa_loaded||cfa_failed||!event_files.api||temporal_retained()||event_files.pending>=0||signature_pending)return;
    pause_capture();if(temporal_retained())return;
    uint32_t size=0;uint64_t revision=0;
    int32_t rc=event_files.api->stat(event_files.api->context,"context-fingerprints.cfp",&size,&revision);
    if(rc==RISC_APP_DATA_NOT_FOUND){revision=0;rc=0;}
    if(!rc){size=cf_encode_source(&cfa_bank,cfa_source(),cfa_bytes,sizeof(cfa_bytes));rc=size?event_files.api->replace(event_files.api->context,"context-fingerprints.cfp",revision,cfa_bytes,size):RISC_APP_DATA_INVALID;}
    if(rc==RISC_APP_DATA_RETAINED){event_files.retained=true;return;}
    if(rc){notify("FINGERPRINT SAVE FAILED / RETRY");}
    else{cfa_changed=false;cfa_saved_at=app->millis();}
    CFA_RESUME();
}
static void cfa_reset_window(cf_pipeline*p){cf_window_reset(p,cfa_time_us);}
static void cfa_arm(void){cfa_training_ready=cfa_segment_active=cfa_training_saved=false;cfa_room=(cf_match){.slot=-1};cfa_event=cfa_room;cfa_reset_window(&cfa_event_pipeline);}
static bool cfa_confirm(unsigned slot,const char*name,unsigned kind){
    if(!cfa_loaded||cfa_failed||slot>=CF_PROFILES||!cfa_fusion.have[cfa_source()])return false;
    cf_profile*p=&cfa_bank.profiles[slot];
    if(p->kind&&(p->kind!=kind||strcmp(p->name,name)))memset(p,0,sizeof(*p));
    bool ok=cf_train(&cfa_bank,&cfa_fusion,slot,name,kind,cfa_time_us,cfa_seconds(),&cfa_config,true);
    cfa_changed|=ok;return ok;
}
static void cfa_event_boundary(bool training,bool collecting,bool ready){
    if(collecting&&!cfa_segment_active)cfa_reset_window(&cfa_event_pipeline);
    cfa_segment_active=collecting;
    if(!ready||(training&&cfa_training_ready))return;
    if(!cf_take(&cfa_event_pipeline,cfa_time_us,&cfa_event_window))return;
    if(!(cfa_event_window.flags&(CF_TEMPORAL|CF_SPECTRAL)))return;
    if(training){cfa_training_window=cfa_event_window;cfa_training_ready=true;return;}
    cf_publish(&cfa_event_fusion,cfa_source(),&cfa_event_window,cfa_time_us);
    cfa_event_at=app->millis();
    cfa_event=cf_match_profiles(&cfa_bank,&cfa_event_fusion,CF_EVENT,cfa_time_us,cfa_seconds(),&cfa_config);
}
static bool cfa_save_event(unsigned slot,const char*name,bool positive){
    if(cfa_training_saved)return true;
    if(!cfa_training_ready)return false;
    if(!cfa_loaded||cfa_failed)return false;
    cf_publish(&cfa_event_fusion,cfa_source(),&cfa_training_window,cfa_time_us);
    bool ok=cf_train(&cfa_bank,&cfa_event_fusion,(positive?8u:16u)+slot,name,positive?CF_EVENT:CF_NEGATIVE,cfa_time_us,cfa_seconds(),&cfa_config,true);
    if(ok){cfa_changed=true;cfa_training_saved=true;}
    return ok;
}
static void cfa_observe(void){
    unsigned s=cfa_source();
    if(cfa_pipeline.identity!=cfa_identity())cfa_begin();
    if(signature_audio.transforms!=cfa_seen){
        cfa_seen=signature_audio.transforms;
#if CFA_SOURCE == 1
        float origin=(float)rf_identity_low_hz(&prefs.identity),span=(float)prefs.identity.sample_rate_hz;
#else
        float origin=0,span=(float)sample_rate*.5f;
#endif
        cf_spectrum(&cfa_pipeline,signature_audio.power,128,1.f/1073741824.f,origin,span);
        cf_spectrum(&cfa_event_pipeline,signature_audio.power,128,1.f/1073741824.f,origin,span);
    }
    if(cf_take(&cfa_pipeline,cfa_time_us,&cfa_window)){
        cf_publish(&cfa_fusion,s,&cfa_window,cfa_time_us);
        cfa_room=cf_match_profiles(&cfa_bank,&cfa_fusion,CF_ROOM,cfa_time_us,cfa_seconds(),&cfa_config);
        if(cfa_room.slot>=0&&!signature_goal&&!event_armed&&!event_training.ready){
            if(cf_train(&cfa_bank,&cfa_fusion,(unsigned)cfa_room.slot,cfa_bank.profiles[cfa_room.slot].name,CF_ROOM,cfa_time_us,cfa_seconds(),&cfa_config,false))cfa_changed=true;
        }
    }
}
static void cfa_forget(unsigned slot){
    if(slot>=CF_PROFILES||!cfa_loaded)return;
    memset(&cfa_bank.profiles[slot],0,sizeof(cf_profile));++cfa_bank.generation;cfa_changed=true;
    cfa_room.slot=cfa_event.slot=-1;
}
static void cfa_rename_event(unsigned slot,const char*name){
    for(unsigned offset=8;offset<=16;offset+=8)if(cfa_bank.profiles[offset+slot].kind){
        memcpy(cfa_bank.profiles[offset+slot].name,name,17);++cfa_bank.generation;cfa_changed=true;
    }
}
static void cfa_discard(void){pause_capture();if(temporal_retained())return;cfa_loaded=cfa_failed=cfa_changed=false;memset(&cfa_bank,0,sizeof(cfa_bank));cfa_load();cfa_room.slot=cfa_event.slot=-1;if(!temporal_retained())CFA_RESUME();}
static bool cfa_has_events(void){for(unsigned i=0;i<CF_PROFILES;i++)if(cfa_bank.profiles[i].kind==CF_EVENT)return true;return false;}
static bool cfa_unsaved(void){return cfa_changed;}
static void cfa_retry(void){pause_capture();if(temporal_retained())return;if(cfa_failed){cfa_failed=false;cfa_load();}if(temporal_retained())return;cfa_save();if(!temporal_retained())CFA_RESUME();}
static void cfa_apply(void){
    if((uint32_t)(app->millis()-cfa_event_at)>4000u)cfa_event.slot=-1;
    /* A newly trained fingerprint replaces its old spectral-only answer. */
    bool rooms=false,events=false;
    for(unsigned i=0;i<CF_PROFILES;i++){rooms|=cfa_bank.profiles[i].kind==CF_ROOM;events|=cfa_bank.profiles[i].kind==CF_EVENT;}
    if(rooms){room_tracker.selected=room_tracker.candidate=cfa_room.slot;room_tracker.confidence=(unsigned)(cfa_room.confidence*100.f);room_tracker.ambiguous=cfa_room.ambiguous;room_tracker.misses=cfa_room.slot<0;room_tracker.stable=cfa_room.slot>=0?64:0;}
    if(events){
        event_neural_used=false;
        event_match.running=false;event_match.complete=true;event_match.selected=cfa_event.slot>=8&&cfa_event.slot<16?cfa_event.slot-8:-1;
        event_match.score=(unsigned)(cfa_event.confidence*1000.f);
        event_match.reason=event_match.selected>=0?CFA_MATCH_REASON:CFA_UNKNOWN_REASON;
        memset(event_match.positive,0,sizeof(event_match.positive));
        if(event_match.selected>=0)event_match.positive[event_match.selected]=event_match.score;
        event_slot=cfa_event.slot>=0&&cfa_event.slot<8?cfa_event.slot:-1;
        event_confidence=(unsigned)(cfa_event.confidence*100.f);
    }
    if(cfa_changed&&!signature_goal&&!event_armed&&!event_training.ready&&(uint32_t)(app->millis()-cfa_saved_at)>=60000u)cfa_save();
}
#if CFA_SOURCE == 1
static void cfa_iq(const uint32_t*pairs,size_t count,uint32_t rate,uint32_t stamp){
    cfa_capture_clock(stamp);
    cf_iq_snapshot(&cfa_pipeline,pairs,count,rate,cfa_time_us);
    cf_iq_snapshot(&cfa_event_pipeline,pairs,count,rate,cfa_time_us);cfa_observe();
}
#else
static void cfa_pcm(const int16_t*pcm,size_t count,uint32_t rate,bool continuous){
    cfa_capture_clock(app->millis());
    uint64_t start=cfa_pipeline.contiguous&&continuous?cfa_pipeline.expected_us:cfa_time_us;
    cf_pcm(&cfa_pipeline,pcm,count,rate,start,continuous);cf_pcm(&cfa_event_pipeline,pcm,count,rate,start,continuous);
    uint64_t wall=cfa_time_us;cfa_time_us=start+(uint64_t)count*1000000u/rate;cfa_observe();cfa_time_us=wall;
}
#endif
