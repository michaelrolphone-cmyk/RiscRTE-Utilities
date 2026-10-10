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
static uint64_t cfa_revision;
static cf_profile cfa_learning;
static unsigned cfa_learning_kind,cfa_learning_samples;
static uint32_t cfa_learning_at;
static bool cfa_learning_saved;
static char cfa_manual_name[17];
static uint32_t cfa_background_bands[128];
static void cfa_learn_observe(const cf_feature*,bool);
static uint32_t cfa_saved_at,cfa_seen,cfa_event_at;
static uint64_t cfa_time_us,cfa_utc,cfa_quiet_us,cfa_onset_us;
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
static bool cfa_timing_only(void){
#ifdef PORTABLE_CONTEXTS_CLIENT
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(portable_contexts_service());
 contexts_fingerprint_status_v1 q={.struct_size=sizeof(q)};
 return f&&f->fingerprint(f->base.context,CONTEXTS_FP_STATUS,&q)&&q.temporal_only;
#else
 return false;
#endif
}
/* Other-source labels are visible, but cannot classify or subtract until this
 * source has been trained. Never fabricate an RF fingerprint from audio. */
static void cfa_shared_names(void){
 for(unsigned i=0;i<8;i++)if(signatures[i].kind==CF_ROOM){
  int at=cf_profile_slot(&cfa_bank,CF_ROOM,signatures[i].name);if(at>=0&&!cfa_bank.profiles[at].kind){cfa_bank.profiles[at].kind=CF_ROOM;memcpy(cfa_bank.profiles[at].name,signatures[i].name,17);}
 }
#ifdef PORTABLE_CONTEXTS_CLIENT
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(portable_contexts_service());if(!f)return;
 for(unsigned i=0;i<CF_PROFILES;i++){
  contexts_profile_v1 q={.struct_size=sizeof(q),.index=i};
  if(!f->fingerprint(f->base.context,CONTEXTS_FP_PROFILE,&q)||q.kind<1||q.kind>2||!q.name[0])continue;
  int at=cf_profile_slot(&cfa_bank,q.kind,q.name);if(at<0)continue;
  cf_profile*p=&cfa_bank.profiles[at];if(!p->kind){p->kind=(uint8_t)q.kind;memcpy(p->name,q.name,17);}
 }
#endif
}
static bool cfa_publish_saved(uint32_t size){
#ifdef PORTABLE_CONTEXTS_CLIENT
 const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(portable_contexts_service());
 if(f){contexts_fingerprint_record_v1 q={.struct_size=sizeof(q),.source=1u<<CFA_SOURCE,.bytes=cfa_bytes,.size=size,.capacity=sizeof(cfa_bytes)};
  return f->fingerprint(f->base.context,CONTEXTS_FP_IMPORT,&q);}
#else
 (void)size;
#endif
 return true;
}
static void cfa_begin(void){
    cfa_config=cf_defaults(cfa_source());cf_init(&cfa_pipeline,&cfa_config,cfa_identity());
    cf_init(&cfa_event_pipeline,&cfa_config,cfa_identity());cfa_event_pipeline.config.window_ms=100;
    cf_enable(&cfa_fusion,cfa_source(),false);cf_enable(&cfa_fusion,cfa_source(),true);
    cf_enable(&cfa_event_fusion,cfa_source(),false);cf_enable(&cfa_event_fusion,cfa_source(),true);
    cfa_config.temporal_only=cfa_timing_only();
    cfa_onset_us=cfa_quiet_us=0;cfa_training_ready=cfa_segment_active=cfa_training_saved=false;cfa_room=(cf_match){.slot=-1};cfa_event=cfa_room;cfa_time_us=(uint64_t)app->millis()*1000u;cfa_seen=0;cfa_have_capture=false;
}
static void cfa_load(void){
    if(cfa_loaded||cfa_failed||!event_files.api)return;
    uint32_t size=0,actual=0;uint64_t revision=0,current=0;
    int32_t rc=event_files.api->stat(event_files.api->context,"context-fingerprints.cfp",&size,&revision);
    if(rc==RISC_APP_DATA_NOT_FOUND){cfa_loaded=true;cfa_revision=0;cfa_shared_names();return;}
    if(!rc&&size<=sizeof(cfa_bytes)&&revision)rc=event_files.api->read(event_files.api->context,"context-fingerprints.cfp",revision,cfa_bytes,sizeof(cfa_bytes),&actual,&current);
    else if(!rc)rc=RISC_APP_DATA_INVALID;
    if(!rc&&(actual!=size||current!=revision||!cf_decode_source(&cfa_read_bank,cfa_source(),cfa_bytes,actual)))rc=RISC_APP_DATA_INVALID;
    if(rc==RISC_APP_DATA_RETAINED)event_files.retained=true;
    if(!rc){cfa_bank=cfa_read_bank;cfa_loaded=true;cfa_revision=revision;cfa_shared_names();}else{cfa_failed=true;notify("FINGERPRINTS UNREAD / RETRY");}
}
static bool cfa_filter_select(unsigned mode,const char*name){
    if(!name)name=cfa_manual_name;
    if(mode>2||!memchr(name,0,17))return false;
    pause_capture();if(temporal_retained())return false;
    uint8_t wanted[32]={0},actual[32]={0};memcpy(wanted,"CFB1",4);wanted[4]=(uint8_t)mode;memcpy(wanted+5,name,17);
    cf_writer w={wanted,28,sizeof(wanted),true};cf_w32(&w,cf_crc(wanted,28));uint32_t size=0;
    bool ok=acquire_storage();
    if(ok){int32_t rc=storage->put(storage->context,"context_filter",wanted,sizeof(wanted));
        ok=(rc==RISC_KEY_VALUE_OK||rc==RISC_KEY_VALUE_IO)&&storage->get(storage->context,"context_filter",actual,sizeof(actual),&size)==RISC_KEY_VALUE_OK&&size==sizeof(actual)&&!memcmp(actual,wanted,size);}
    if(ok){signature_mode=mode;memcpy(cfa_manual_name,name,17);clear_analysis();refresh_analysis();}
    else notify("FILTER SAVE FAILED / RETRY");
    if(!temporal_retained())CFA_RESUME();
    dirty=true;return ok;
}
static void cfa_filter_load(void){
    if(!storage)return;
    uint8_t bytes[32];uint32_t size=0;int32_t rc=storage->get(storage->context,"context_filter",bytes,sizeof(bytes),&size);
    if(rc==RISC_KEY_VALUE_NOT_FOUND)return; /* Preserve original RF preference. */
    cf_reader r={bytes,28,sizeof(bytes),true};
    if(!rc&&size==sizeof(bytes)&&!memcmp(bytes,"CFB1",4)&&bytes[4]<=2&&memchr(bytes+5,0,17)&&cf_r32(&r)==cf_crc(bytes,28)){
        signature_mode=bytes[4];memcpy(cfa_manual_name,bytes+5,17);
    }else{signature_mode=0;notify("FILTER SETTING UNREAD / CHOOSE AGAIN");}
}
static void cfa_restore(void){
    memset(&cfa_bank,0,sizeof(cfa_bank));memset(&cfa_fusion,0,sizeof(cfa_fusion));memset(&cfa_event_fusion,0,sizeof(cfa_event_fusion));cfa_loaded=cfa_changed=cfa_failed=false;
    cfa_room=(cf_match){.slot=-1};cfa_event=cfa_room;cfa_saved_at=app->millis();cfa_utc_at=cfa_saved_at;
    if(!portable_fingerprint_clock(runtime,&cfa_utc)){event_files.retained=true;return;}
    cfa_learning_kind=cfa_learning_samples=0;cfa_learning_saved=false;cfa_manual_name[0]=0;cfa_filter_load();
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!portable_contexts_models_save()){cfa_failed=true;notify("SHARED MODELS / RETRY SAVE");return;}
#endif
    cfa_begin();cfa_load();
}
static int32_t cfa_verify_saved(uint32_t wanted_size,uint32_t wanted_crc,uint64_t *revision){
    uint32_t size=0,actual=0;uint64_t current=0;
    int32_t rc=event_files.api->stat(event_files.api->context,"context-fingerprints.cfp",&size,revision);
    if(!rc&&(size!=wanted_size||!*revision))return RISC_APP_DATA_STALE;
    if(!rc)rc=event_files.api->read(event_files.api->context,"context-fingerprints.cfp",*revision,cfa_bytes,sizeof(cfa_bytes),&actual,&current);
    if(!rc&&(actual!=size||current!=*revision||cf_crc(cfa_bytes,size)!=wanted_crc))rc=RISC_APP_DATA_STALE;
    return rc;
}
static void cfa_save(void){
    if(!cfa_changed||!cfa_loaded||cfa_failed||!event_files.api||temporal_retained()||event_files.pending>=0||signature_pending)return;
    pause_capture();if(temporal_retained())return;
    uint32_t size=0,wanted=cf_encode_source(&cfa_bank,cfa_source(),cfa_bytes,sizeof(cfa_bytes));uint64_t revision=0;
    uint32_t crc=cf_crc(cfa_bytes,wanted);
    int32_t rc=wanted?event_files.api->stat(event_files.api->context,"context-fingerprints.cfp",&size,&revision):RISC_APP_DATA_INVALID;
    if(rc==RISC_APP_DATA_NOT_FOUND){revision=0;rc=0;}
    if(!rc&&revision==cfa_revision)rc=event_files.api->replace(event_files.api->context,"context-fingerprints.cfp",revision,cfa_bytes,wanted);
    /* An uncertain commit or newer revision is accepted only after exact
     * readback. A different writer's profile set is never overwritten. */
    if(rc!=RISC_APP_DATA_RETAINED&&(rc==0||rc==RISC_APP_DATA_COMMIT_UNKNOWN))rc=cfa_verify_saved(wanted,crc,&revision);
    if(rc==RISC_APP_DATA_RETAINED){event_files.retained=true;return;}
    if(!rc){cfa_revision=revision;if(cfa_publish_saved(wanted)){cfa_changed=false;cfa_saved_at=app->millis();}else rc=RISC_APP_DATA_STALE;}
    if(rc)notify(rc==RISC_APP_DATA_STALE?"SHARED DATA CHANGED / RETRY":"FINGERPRINT SAVE FAILED / RETRY");
    CFA_RESUME();
}
static void cfa_reset_window(cf_pipeline*p){cf_window_reset(p,cfa_time_us);}
static void cfa_arm(void){cfa_onset_us=cfa_quiet_us=0;cfa_training_ready=cfa_segment_active=cfa_training_saved=false;cfa_room=(cf_match){.slot=-1};cfa_event=cfa_room;cfa_reset_window(&cfa_event_pipeline);}
static bool cfa_confirm(unsigned slot,const char*name,unsigned kind){
    if(!cfa_loaded||cfa_failed||slot>=CF_PROFILES||!cfa_fusion.have[cfa_source()])return false;
    int shared=cf_profile_slot(&cfa_bank,kind,name);if(shared<0)return false;slot=(unsigned)shared;
    bool ok=cf_train(&cfa_bank,&cfa_fusion,slot,name,kind,cfa_time_us,cfa_seconds(),&cfa_config,true);
    cfa_changed|=ok;return ok;
}
static void cfa_event_boundary(bool training,bool collecting,bool ready){
#if CFA_SOURCE == 0
    /* The envelope segmenter owns live events. A user-reviewed legacy
     * example can close its training window before the quiet tail finishes. */
    if(!training||cfa_training_ready)return;
#else
    if(cfa_event_pipeline.sparse&&cfa_event_pipeline.resolution_ms>0&&cfa_event_pipeline.resolution_ms<20)return;
#endif
    if(collecting&&!cfa_segment_active)cfa_reset_window(&cfa_event_pipeline);
    cfa_segment_active=collecting;
    if(!ready||(training&&cfa_training_ready))return;
    if(!cf_take(&cfa_event_pipeline,cfa_time_us,&cfa_event_window))return;
    if(!(cfa_event_window.flags&(CF_TEMPORAL|CF_SPECTRAL)))return;
    if(training){cfa_training_window=cfa_event_window;cfa_training_ready=true;return;}
    cfa_learn_observe(&cfa_event_window,true);
    cf_publish(&cfa_event_fusion,cfa_source(),&cfa_event_window,cfa_time_us);
    cfa_event_at=app->millis();
    cfa_event=cf_match_profiles(&cfa_bank,&cfa_event_fusion,CF_EVENT,cfa_time_us,cfa_seconds(),&cfa_config);
}
static bool cfa_save_event(unsigned slot,const char*name,bool positive){
    if(cfa_training_saved)return true;
    if(!cfa_training_ready)return false;
    if(!cfa_loaded||cfa_failed)return false;
    cf_publish(&cfa_event_fusion,cfa_source(),&cfa_training_window,cfa_time_us);
    (void)slot;int shared=cf_profile_slot(&cfa_bank,positive?CF_EVENT:CF_NEGATIVE,name);if(shared<0)return false;
    bool ok=cf_train(&cfa_bank,&cfa_event_fusion,(unsigned)shared,name,positive?CF_EVENT:CF_NEGATIVE,cfa_time_us,cfa_seconds(),&cfa_config,true);
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
        cfa_learn_observe(&cfa_window,false);
        cf_publish(&cfa_fusion,s,&cfa_window,cfa_time_us);
        cfa_room=cf_match_profiles(&cfa_bank,&cfa_fusion,CF_ROOM,cfa_time_us,cfa_seconds(),&cfa_config);
        if(cfa_room.slot>=0&&!cfa_learning_kind&&!signature_goal&&!event_armed&&!event_training.ready){
            if(cf_train(&cfa_bank,&cfa_fusion,(unsigned)cfa_room.slot,cfa_bank.profiles[cfa_room.slot].name,CF_ROOM,cfa_time_us,cfa_seconds(),&cfa_config,false))cfa_changed=true;
        }
    }
}
static void cfa_forget(unsigned slot){
    if(slot>=CF_PROFILES||!cfa_loaded)return;
    unsigned kind=slot<8?signatures[slot].kind:slot<16?CF_EVENT:CF_NEGATIVE;
    const char*name=slot<8?signatures[slot].name:event_library.labels[slot%8].name;
    int at=cf_profile_find(&cfa_bank,kind,name);if(at<0)return;
    memset(&cfa_bank.profiles[at],0,sizeof(cf_profile));++cfa_bank.generation;cfa_changed=true;
    cfa_room.slot=cfa_event.slot=-1;
}
static void cfa_rename_event(unsigned slot,const char*name){
    for(unsigned kind=CF_EVENT;kind<=CF_NEGATIVE;kind++){
        int at=cf_profile_find(&cfa_bank,kind,event_library.labels[slot].name);
        if(at>=0&&cf_profile_find(&cfa_bank,kind,name)<0){memcpy(cfa_bank.profiles[at].name,name,17);++cfa_bank.generation;cfa_changed=true;}
    }
}
static void cfa_discard(void){pause_capture();if(temporal_retained())return;cfa_loaded=cfa_failed=cfa_changed=false;memset(&cfa_bank,0,sizeof(cfa_bank));cfa_load();cfa_room.slot=cfa_event.slot=-1;if(!temporal_retained())CFA_RESUME();}
static const char*cfa_name(unsigned kind,const char*fallback){int slot=kind==CF_ROOM?cfa_room.slot:cfa_event.slot;return slot>=0&&slot<(int)CF_PROFILES?cfa_bank.profiles[slot].name:fallback;}
static bool cfa_has_events(void){for(unsigned i=0;i<CF_PROFILES;i++)if(cfa_bank.profiles[i].kind==CF_EVENT)return true;return false;}
static bool cfa_unsaved(void){return cfa_changed;}
static void cfa_retry(void){pause_capture();if(temporal_retained())return;if(cfa_failed){cfa_failed=false;cfa_load();}if(temporal_retained())return;cfa_save();if(!temporal_retained())CFA_RESUME();}
static void cfa_apply(void){
    if((uint32_t)(app->millis()-cfa_event_at)>4000u)cfa_event.slot=-1;
    /* A newly trained fingerprint replaces its old spectral-only answer. */
    bool rooms=false,events=false;
    for(unsigned i=0;i<CF_PROFILES;i++){rooms|=cfa_bank.profiles[i].kind==CF_ROOM&&cfa_bank.profiles[i].present[CFA_SOURCE];events|=cfa_bank.profiles[i].kind==CF_EVENT&&cfa_bank.profiles[i].present[CFA_SOURCE];}
    if(rooms){int local=-1;
        if(cfa_room.slot>=0)for(unsigned i=0;i<8;i++)if(signatures[i].kind==CF_ROOM&&!strcmp(signatures[i].name,cfa_bank.profiles[cfa_room.slot].name)){local=(int)i;break;}
        room_tracker.selected=room_tracker.candidate=local;room_tracker.confidence=(unsigned)(cfa_room.confidence*100.f);room_tracker.ambiguous=cfa_room.ambiguous;room_tracker.misses=cfa_room.slot<0;room_tracker.stable=cfa_room.slot>=0?64:0;}
    if(events){
        event_neural_used=false;
        event_match.running=false;event_match.complete=true;event_match.selected=-1;
        if(cfa_event.slot>=0)for(unsigned i=0;i<8;i++)if(event_library.labels[i].present&&!strcmp(event_library.labels[i].name,cfa_bank.profiles[cfa_event.slot].name)){event_match.selected=(int)i;break;}
        event_match.score=(unsigned)(cfa_event.confidence*1000.f);
        event_match.reason=event_match.selected>=0?CFA_MATCH_REASON:CFA_UNKNOWN_REASON;
        memset(event_match.positive,0,sizeof(event_match.positive));
        if(event_match.selected>=0)event_match.positive[event_match.selected]=event_match.score;
        event_slot=-1;
        event_confidence=(unsigned)(cfa_event.confidence*100.f);
    }
    if(cfa_changed&&!cfa_learning_kind&&!signature_goal&&!event_armed&&!event_training.ready&&(uint32_t)(app->millis()-cfa_saved_at)>=60000u)cfa_save();
}
static void cfa_segment(uint64_t stamp,bool gap){
 if(gap){cfa_onset_us=cfa_quiet_us=0;cfa_event.slot=-1;}
 if(cfa_event_pipeline.active){if(!cfa_onset_us){cfa_onset_us=stamp;cf_window_reset(&cfa_event_pipeline,stamp);}cfa_quiet_us=stamp;}
 if(cfa_onset_us&&(stamp-cfa_quiet_us>=500000u||stamp-cfa_onset_us>=10000000u)){
  if(cf_take(&cfa_event_pipeline,stamp,&cfa_event_window)){
   cfa_learn_observe(&cfa_event_window,true);
   if(event_armed&&!cfa_training_ready){cfa_training_window=cfa_event_window;cfa_training_ready=true;}
   else if(!event_armed){cf_publish(&cfa_event_fusion,cfa_source(),&cfa_event_window,stamp);cfa_event_at=app->millis();cfa_event=cf_match_profiles(&cfa_bank,&cfa_event_fusion,CF_EVENT,stamp,cfa_seconds(),&cfa_config);}
  }
  cfa_onset_us=0;cf_window_reset(&cfa_event_pipeline,stamp);
 }
}
#if CFA_SOURCE == 1
static void cfa_envelope(const risc_radio_iq_envelope_v1*v){
 cfa_time_us=v->timestamp_us;
 bool gap=(v->flags&RISC_RADIO_IQ_ENVELOPE_GAP)!=0;
 cf_envelope_sample(&cfa_pipeline,v->magnitude,v->timestamp_us,v->observed_us,gap);
 cf_envelope_sample(&cfa_event_pipeline,v->magnitude,v->timestamp_us,v->observed_us,gap);
 cfa_segment(v->timestamp_us,gap);
}
static void cfa_iq(const uint32_t*pairs,size_t count,uint32_t rate,uint32_t stamp){
    cfa_capture_clock(stamp);
    cf_iq_snapshot(&cfa_pipeline,pairs,count,rate,cfa_time_us);
    cf_iq_snapshot(&cfa_event_pipeline,pairs,count,rate,cfa_time_us);cfa_observe();
}
#else
static void cfa_pcm(const int16_t*pcm,size_t count,uint32_t rate,bool continuous){
    cfa_capture_clock(app->millis());
    uint64_t start=cfa_pipeline.contiguous&&continuous?cfa_pipeline.expected_us:cfa_time_us;
    if(!rate)return;
    size_t stride=rate/1000u;if(!stride)stride=1;
    for(size_t at=0;at<count;at+=stride){size_t n=count-at<stride?count-at:stride;uint64_t stamp=start+(uint64_t)at*1000000u/rate;
        cf_pcm(&cfa_pipeline,pcm+at,n,rate,stamp,continuous||at!=0);cf_pcm(&cfa_event_pipeline,pcm+at,n,rate,stamp,continuous||at!=0);
        cfa_segment(stamp,at==0&&!continuous);
    }
    uint64_t wall=cfa_time_us;cfa_time_us=start+(uint64_t)count*1000000u/rate;cfa_observe();cfa_time_us=wall;
}
#endif

/* Shared subtraction operates only on matching source/capture identities.
 * It is display-only: all detection and training consume raw canonical power. */
static const uint32_t*cfa_background(void){
    if(!signature_mode)return NULL;
    int at=-1;const char*name=NULL;
    if(signature_mode==1){
        at=cfa_room.slot;
        if(at<0){
            /* Once this source has temporal rooms, unknown must not silently
             * fall back to a different legacy spectral-only room. */
            for(unsigned i=0;i<CF_PROFILES;i++)if(cfa_bank.profiles[i].kind==CF_ROOM&&cfa_bank.profiles[i].present[CFA_SOURCE])return NULL;
            if(room_tracker.selected>=0&&room_tracker.selected<8)name=signatures[room_tracker.selected].name;
        }
    }else if(cfa_manual_name[0])name=cfa_manual_name;
    else if(manual_room>=0&&manual_room<8)name=signatures[manual_room].name;
    if(name)at=cf_profile_find(&cfa_bank,CF_ROOM,name);
    if(at>=0){
        name=cfa_bank.profiles[at].name;
        if(cf_background_power(&cfa_bank.profiles[at],CFA_SOURCE,cfa_identity(),cfa_background_bands))return cfa_background_bands;
    }
    /* Preserve pre-upgrade raw spectra, matched by name rather than slot. */
    if(name)for(unsigned i=0;i<8;i++)if(signatures[i].kind==CF_ROOM&&!strcmp(name,signatures[i].name)){
#if CFA_SOURCE == 1
        if(!rf_identity_equal(&signatures[i].identity,&prefs.identity))continue;
#endif
        return signature_means[i];
    }
    return NULL;
}
static void cfa_learn_observe(const cf_feature*f,bool event){
    if(!cfa_learning_kind||cfa_learning_saved||((cfa_learning_kind==CF_EVENT)!=event)||
       !(f->flags&(CF_TEMPORAL|CF_SPECTRAL))||(f->flags&CF_OVERFLOW))return;
    if(cfa_learning_samples){if(!cf_blend(&cfa_learning.feature[CFA_SOURCE],f,1.f/(float)(cfa_learning_samples+1u)))return;}
    else cfa_learning.feature[CFA_SOURCE]=*f;
    cfa_learning.present[CFA_SOURCE]=1;cfa_learning.weight[CFA_SOURCE]=1;cfa_learning.updated_seconds[CFA_SOURCE]=cfa_seconds();
    ++cfa_learning_samples;dirty=true;
}
static bool cfa_learn_ready(void){return cfa_learning_samples>=3&&(cfa_learning_kind==CF_EVENT||(uint32_t)(app->millis()-cfa_learning_at)>=30000u);}
static bool cfa_learn_save(void){
    if(!cfa_learning_kind||(!cfa_learning_saved&&!cfa_learn_ready()))return false;
    if(!cfa_learning_saved){
        int at=cf_profile_slot(&cfa_bank,cfa_learning_kind,cfa_learning.name);if(at<0){notify("LIBRARY FULL");return false;}
        cf_profile*p=&cfa_bank.profiles[at];
        if(p->present[CFA_SOURCE]){if(!cf_blend(&p->feature[CFA_SOURCE],&cfa_learning.feature[CFA_SOURCE],.05f)){notify("CAPTURE SETTINGS DIFFER / USE NEW NAME");return false;}}
        else p->feature[CFA_SOURCE]=cfa_learning.feature[CFA_SOURCE];
        p->kind=(uint8_t)cfa_learning_kind;memcpy(p->name,cfa_learning.name,17);p->present[CFA_SOURCE]=1;p->weight[CFA_SOURCE]=1;p->updated_seconds[CFA_SOURCE]=cfa_seconds();
        ++cfa_bank.generation;cfa_changed=true;cfa_learning_saved=true;
    }
    cfa_save();if(cfa_changed)return false;
    cfa_learning_kind=0;cfa_learning_saved=false;notify("SAVED TO SHARED LIBRARY");return true;
}
