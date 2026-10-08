/* Signature inference in an ordinary ELF. No thread, poll suffix, file/KV
 * authority, automatic training, persistent model copy or settings mutation. */
#include "ContextsServiceV1.h"
#include "AudioInputV1.h"
#include "RiscRadioIqV1.h"
#include "RiscPlatformClockV1.h"
#include "spectrum_signature_store.h"
#include "spectrum_background.h"
#include "spectrum_store.h"
#include "rf_signature_store.h"
#include "rf_background.h"
#include "rf_store.h"
#include <string.h>
#define FRESH_MS 500u
#define EVENT_HOLD_MS 2000u
#define RETRY_MS 5000u
#define RF_INTERVAL_MS 100u
typedef struct {
    spectrum_signature signatures[8];
    uint32_t means[8][128];
    spectrum_signature_analyzer analyzer;
    spectrum_background background;
    spectrum_room_tracker room;
    spectrum_preferences prefs;
} audio_models;
typedef struct {
    rf_signature signatures[8];
    uint32_t means[8][128];
    rf_signature_analyzer analyzer;
    rf_background background;
    rf_room_tracker room;
    rf_preferences prefs;
} radio_models;
static audio_models a;
static radio_models r;
_Static_assert(sizeof(a)+sizeof(r)<40000u,"bounded copied models and DSP state");
static const twatch_audio_in_api_v1 *microphone;
static const risc_radio_iq_extended_api_v1 *receiver;
static const risc_platform_clock_api_v1 *clock_api;
static contexts_status_v1 view;
static bool started,busy,audio_owned,radio_retained,audio_active,radio_active;
static bool have_observation[2];
static uint16_t exported[2];
static bool export_invalid[2];
static uint32_t record_digest[2][9],model_digest[2];
static char claimed_name[CONTEXTS_NAME_SIZE];
static struct {int32_t slot;uint32_t generation;char name[17];bool known;} room_identity[2];
static uint64_t observed[2],event_at[2],retry_at[2],radio_at;
static uint64_t last_pcm;
static uint32_t foreground[128],residual[8][128],pairs[256];
static contexts_source_status_v1 *source_status(uint32_t source) {
    return source==CONTEXTS_AUDIO?&view.audio:source==CONTEXTS_RADIO?&view.radio:NULL;
}
static bool enter(void) {if(!started||busy)return false;busy=true;return true;}
static void leave(void) {busy=false;}
static void invalidate(contexts_source_status_v1 *s) {
    s->current=s->room_valid=s->event_valid=false;
}
static void reset_audio(void) {
    spectrum_signature_init(&a.analyzer);spectrum_background_reset(&a.background);
    spectrum_room_reset(&a.room);audio_active=false;have_observation[0]=false;invalidate(&view.audio);
}
static void reset_radio(void) {
    rf_signature_init(&r.analyzer);rf_background_reset(&r.background);
    rf_room_reset(&r.room);radio_active=false;have_observation[1]=false;invalidate(&view.radio);
}
static bool close_audio(void) {
    if(audio_owned&&!microphone->close(microphone->context)) {
        view.cleanup_pending=true;view.state=CONTEXTS_RETAINED;return false;
    }
    audio_owned=false;reset_audio();return true;
}
static bool close_radio(void) {
    /* BUSY is another owner's refusal, never authority to suspend it. */
    if(radio_retained&&!receiver->base.base.suspend(receiver->base.base.context)) {
        view.cleanup_pending=true;view.state=CONTEXTS_RETAINED;return false;
    }
    radio_retained=false;reset_radio();return true;
}
static bool pause_internal(void) {
    invalidate(&view.audio);invalidate(&view.radio);
    if(!close_audio()||!close_radio())return false;
    view.cleanup_pending=false;view.state=CONTEXTS_PAUSED;return true;
}
static bool pause_service(void *context) {
    (void)context;if(!enter())return false;
    bool ok=pause_internal();leave();return ok;
}
static void update_room(contexts_source_status_v1 *s,int slot,unsigned confidence,
        bool ambiguous,bool confirmed,const char *name) {
    s->room_slot=slot;s->room_confidence=confidence;s->room_ambiguous=ambiguous;
    s->room_valid=confirmed&&slot>=0&&!ambiguous;
    memset(s->room_name,0,sizeof(s->room_name));
    if(name)memcpy(s->room_name,name,sizeof(s->room_name));
    if(s->room_valid) {
        unsigned index=s->source==CONTEXTS_AUDIO?0u:1u;
        if(!room_identity[index].known||room_identity[index].slot!=slot||
           room_identity[index].generation!=s->model_generation||strcmp(room_identity[index].name,s->room_name)) {
            if(++s->room_entry==0)++s->room_entry;
            room_identity[index].slot=slot;room_identity[index].generation=s->model_generation;
            memcpy(room_identity[index].name,s->room_name,17);room_identity[index].known=true;
        }
    }
}
static void update_event(contexts_source_status_v1 *s,unsigned index,int slot,
        unsigned confidence,bool ambiguous,const char *name,uint64_t now) {
    s->event_ambiguous=ambiguous;
    if(slot>=0&&!ambiguous) {
        s->event_slot=slot;s->event_confidence=confidence;s->event_valid=true;
        memcpy(s->event_name,name,sizeof(s->event_name));event_at[index]=now;
    } else if(ambiguous||now<event_at[index]||now-event_at[index]>EVENT_HOLD_MS) {
        s->event_valid=false;s->event_slot=-1;s->event_name[0]=0;
    }
}
static void audio_observe(uint64_t now) {
    spectrum_background_observe(&a.background,a.analyzer.power);
    if(a.background.ready)spectrum_room_observe(&a.room,a.analyzer.power,a.signatures,a.means);
    int room=a.room.selected;
    update_room(&view.audio,room,a.room.confidence,a.room.ambiguous,
        a.background.ready&&!a.room.misses&&a.room.candidate==room&&a.room.stable>=64,
        room>=0?a.signatures[room].name:NULL);
    uint64_t excess=spectrum_background_salient(&a.background,foreground);
    for(unsigned i=0;i<8;i++)for(unsigned b=0;b<128;b++)
        residual[i][b]=spectrum_signature_residual(a.means[i][b],a.background.slow[b]);
    unsigned confidence=0;bool ambiguous=false;
    int event=a.background.ready&&a.background.foreground&&
        spectrum_background_db(excess)>=a.prefs.threshold_db*100?
        spectrum_signature_best(foreground,a.signatures,residual,SPECTRUM_SIGNATURE_EVENT,&confidence,&ambiguous):-1;
    update_event(&view.audio,0,event,confidence,ambiguous,event>=0?a.signatures[event].name:NULL,now);
    if(view.audio.samples<UINT32_MAX)++view.audio.samples;
    observed[0]=now;have_observation[0]=true;view.audio.current=true;view.audio.capture_error=0;
}
static void radio_observe(uint64_t now,const rf_capture_identity *identity) {
    rf_background_observe(&r.background,r.analyzer.power);
    if(r.background.ready)rf_room_observe(&r.room,r.analyzer.power,r.signatures,r.means,identity);
    int room=r.room.selected;
    update_room(&view.radio,room,r.room.confidence,r.room.ambiguous,
        r.background.ready&&!r.room.misses&&r.room.candidate==room&&r.room.stable>=64,
        room>=0?r.signatures[room].name:NULL);
    uint64_t excess=rf_background_salient(&r.background,foreground);
    for(unsigned i=0;i<8;i++)for(unsigned b=0;b<128;b++)
        residual[i][b]=rf_signature_residual(r.means[i][b],r.background.slow[b]);
    unsigned confidence=0;bool ambiguous=false;
    int event=r.background.ready&&r.background.foreground&&
        rf_background_db(excess)>=r.prefs.threshold_db*100?
        rf_signature_best(foreground,r.signatures,residual,RF_SIGNATURE_EVENT,&confidence,&ambiguous,identity):-1;
    update_event(&view.radio,1,event,confidence,ambiguous,event>=0?r.signatures[event].name:NULL,now);
    if(view.radio.samples<UINT32_MAX)++view.radio.samples;
    observed[1]=now;have_observation[1]=true;view.radio.current=true;view.radio.capture_error=0;
}
static bool audio_step(uint64_t now) {
    if(retry_at[0]&&now<retry_at[0]&&retry_at[0]-now<=RETRY_MS)return true;
    if(!audio_owned) {
        audio_owned=true;
        if(!microphone->open(microphone->context,16000u)) {
            view.audio.capture_error=1;retry_at[0]=now+RETRY_MS;return close_audio();
        }
        audio_active=true;retry_at[0]=0;observed[0]=last_pcm=now;
    }
    int16_t pcm[256];size_t got=0;
    if(!microphone->read(microphone->context,pcm,256,&got)||got>256) {
        view.audio.capture_error=2;retry_at[0]=now+RETRY_MS;return close_audio();
    }
    if(!got) {
        if(now<observed[0]||now-observed[0]>FRESH_MS){reset_audio();audio_active=true;}
        return true;
    }
    if(now<last_pcm||now-last_pcm>=128u) {
        a.analyzer.used=0;spectrum_background_interrupt(&a.background);
        invalidate(&view.audio);view.audio.event_slot=-1;view.audio.event_name[0]=0;
    }
    last_pcm=now;
    uint32_t before=a.analyzer.transforms;
    if(!spectrum_signature_feed(&a.analyzer,pcm,got)) {
        view.audio.capture_error=3;retry_at[0]=now+RETRY_MS;return close_audio();
    }
    if(a.analyzer.transforms!=before)audio_observe(now);
    return true;
}
static bool radio_step(uint64_t now) {
    if(retry_at[1]&&now<retry_at[1]&&retry_at[1]-now<=RETRY_MS)return true;
    if(radio_active&&now>=radio_at&&now-radio_at<RF_INTERVAL_MS)return true;
    radio_active=true;radio_at=now;
    const rf_capture_identity *i=&r.prefs.identity;
    risc_radio_iq_settings_v1 settings={sizeof(settings),i->lo_hz,i->sample_rate_hz,i->width_hz,i->raw_gain,
        i->rf_gain,i->bb_gain,i->filter,{i->dc[0],i->dc[1],i->dc[2],i->dc[3]},i->iq_correction};
    risc_radio_iq_format_v1 format={.struct_size=sizeof(format)};
    int result=receiver->capture_configured(receiver->base.base.context,pairs,256,&settings,&format);
    if(result!=RISC_RADIO_IQ_OK) {
        view.radio.capture_error=result;retry_at[1]=now+RETRY_MS;
        radio_retained=result==RISC_RADIO_IQ_CLEANUP_RETAINED;
        if(radio_retained){view.cleanup_pending=true;view.state=CONTEXTS_RETAINED;return false;}
        reset_radio();return true;
    }
    if(format.struct_size<sizeof(format)||format.component_bits!=10||format.component_full_scale!=512||
       format.pair_count!=256||format.sample_rate_hz!=i->sample_rate_hz||format.bandwidth_hz!=i->width_hz||
       format.sample_format!=RISC_RADIO_IQ_FORMAT_S10_I0_Q10||!(format.flags&RISC_RADIO_IQ_FLAG_COHERENT_BURST)) {
        view.radio.capture_error=RISC_RADIO_IQ_BAD_ARGUMENT;retry_at[1]=now+RETRY_MS;reset_radio();return true;
    }
    rf_capture_identity actual=*i;actual.lo_hz=format.center_hz;
    if(!rf_identity_valid(&actual)||!rf_signature_burst(&r.analyzer,&actual,pairs,256)) {
        view.radio.capture_error=RISC_RADIO_IQ_BAD_ARGUMENT;retry_at[1]=now+RETRY_MS;reset_radio();return true;
    }
    radio_observe(now,&actual);retry_at[1]=0;return true;
}
static bool step(void *context,const contexts_policy_v1 *p) {
    (void)context;if(!p||p->struct_size<sizeof(*p)||(p->sources&~CONTEXTS_ALL)||!enter())return false;
    if(view.cleanup_pending){bool ok=pause_internal();leave();return ok;}
    uint64_t now=clock_api->monotonic_ms(clock_api->context);
    if(!p->enabled||!p->awake||!p->sources||now==UINT64_MAX) {
        bool ok=pause_internal();if(ok&&!p->enabled)view.state=CONTEXTS_OFF;leave();return ok;
    }
    bool pending=view.export_active||view.export_pending;
    bool audio_ready=(p->sources&CONTEXTS_AUDIO)&&view.audio.model_state==CONTEXTS_MODEL_READY;
    bool radio_ready=(p->sources&CONTEXTS_RADIO)&&view.radio.model_state==CONTEXTS_MODEL_READY;
    if(pending||(!audio_ready&&!radio_ready)) {
        bool ok=pause_internal();if(ok)view.state=pending?CONTEXTS_LOADING:CONTEXTS_UNAVAILABLE;
        leave();return ok;
    }
    bool audio=p->audio_allowed&&audio_ready;
    bool radio=p->radio_allowed&&radio_ready;
    if(!audio&&!close_audio()){leave();return false;}
    if(!radio&&!close_radio()){leave();return false;}
    if(audio&&!audio_step(now)){leave();return false;}
    if(radio&&!radio_step(now)){leave();return false;}
    view.state=audio||radio?CONTEXTS_LIVE:CONTEXTS_PAUSED;
    leave();return true;
}
static bool status(void *context,contexts_status_v1 *out) {
    (void)context;if(!out||out->struct_size<sizeof(*out)||!enter())return false;
    *out=view;out->struct_size=sizeof(*out);
    if(view.cleanup_pending){invalidate(&out->audio);invalidate(&out->radio);leave();return true;}
    uint64_t now=clock_api->monotonic_ms(clock_api->context);
    contexts_source_status_v1 *sources[]={&out->audio,&out->radio};
    for(unsigned i=0;i<2;i++) {
        contexts_source_status_v1 *s=sources[i];
        uint64_t age=now>=observed[i]?now-observed[i]:UINT64_MAX;
        s->age_ms=!have_observation[i]||age>UINT32_MAX?UINT32_MAX:(uint32_t)age;
        if(now==UINT64_MAX||s->age_ms>FRESH_MS)invalidate(s);
        if(now<event_at[i]||now-event_at[i]>EVENT_HOLD_MS)s->event_valid=false;
    }
    leave();return true;
}
static bool request_export(void *context,uint32_t sources) {
    (void)context;if(!sources||(sources&~CONTEXTS_ALL)||!enter())return false;
    if(!pause_internal()){leave();return false;}
    if(view.export_active){leave();return false;}
    view.export_pending|=sources;
    if(sources&CONTEXTS_AUDIO){view.audio.model_state=CONTEXTS_MODEL_REQUESTED;view.audio.signatures_ready=false;}
    if(sources&CONTEXTS_RADIO){view.radio.model_state=CONTEXTS_MODEL_REQUESTED;view.radio.signatures_ready=false;}
    view.state=CONTEXTS_LOADING;leave();return true;
}
static bool begin_export(void *context,uint32_t source) {
    (void)context;if(!source_status(source)||!enter())return false;
    if(view.export_active||!(view.export_pending&source)||!pause_internal()){leave();return false;}
    unsigned index=source==CONTEXTS_AUDIO?0u:1u;
    exported[index]=0;export_invalid[index]=false;view.export_active=source;
    contexts_source_status_v1 *s=source_status(source);s->model_state=CONTEXTS_MODEL_LOADING;s->model_error=0;
    s->room_slot=s->event_slot=-1;s->room_name[0]=s->event_name[0]=0;s->samples=0;s->age_ms=UINT32_MAX;
    view.state=CONTEXTS_LOADING;leave();return true;
}
static bool export_record(void *context,uint32_t source,uint32_t kind,uint32_t index,const void *bytes,uint32_t size) {
    (void)context;if(!bytes||!source_status(source)||!enter())return false;
    if(view.export_active!=source){leave();return false;}
    unsigned which=source==CONTEXTS_AUDIO?0u:1u;bool ok=false;uint16_t bit=0;
    if(kind==CONTEXTS_RECORD_PREFERENCES&&index==0) {
        bit=256u;
        ok=source==CONTEXTS_AUDIO?spectrum_preferences_decode(&a.prefs,bytes,size):rf_preferences_decode(&r.prefs,bytes,size);
    } else if(kind==CONTEXTS_RECORD_SIGNATURE&&index<8) {
        bit=(uint16_t)(1u<<index);
        ok=source==CONTEXTS_AUDIO?spectrum_signature_decode(&a.signatures[index],bytes,size):rf_signature_decode(&r.signatures[index],bytes,size);
        if(ok) {
            if(source==CONTEXTS_AUDIO)spectrum_signature_mean(&a.signatures[index],a.means[index]);
            else rf_signature_mean(&r.signatures[index],r.means[index]);
        }
    }
    if(!ok||(exported[which]&bit))export_invalid[which]=true;
    else {
        exported[which]|=bit;
        /* CRC32 over a record INCLUDING its CRC has a constant residue.
         * Use an independent byte fingerprint for semantic change detection. */
        uint32_t hash=UINT32_C(2166136261);const uint8_t *record=bytes;
        for(uint32_t i=0;i<size;i++)hash=(hash^record[i])*UINT32_C(16777619);
        record_digest[which][kind==CONTEXTS_RECORD_PREFERENCES?8:index]=hash;
    }
    leave();return ok&&!export_invalid[which];
}
static bool finish_export(void *context,uint32_t source,uint32_t result) {
    (void)context;if(!source_status(source)||result>CONTEXTS_EXPORT_UNSUPPORTED||!enter())return false;
    if(view.export_active!=source&&!(view.export_pending&source)){leave();return false;}
    unsigned index=source==CONTEXTS_AUDIO?0u:1u;
    if(result==CONTEXTS_EXPORT_OK&&(view.export_active!=source||export_invalid[index]||exported[index]!=511u))result=CONTEXTS_EXPORT_INVALID;
    contexts_source_status_v1 *s=source_status(source);
    s->model_error=result;s->signatures_ready=result==CONTEXTS_EXPORT_OK;
    s->model_state=result==CONTEXTS_EXPORT_OK?CONTEXTS_MODEL_READY:CONTEXTS_MODEL_FAILED;
    if(result==CONTEXTS_EXPORT_OK) {
        s->capture_error=0;
        uint32_t digest=spectrum_signature_crc((const uint8_t *)record_digest[index],sizeof(record_digest[index]));
        if(!s->model_generation||digest!=model_digest[index]) {
            if(++s->model_generation==0)++s->model_generation;
            model_digest[index]=digest;
        }
    }
    view.export_pending&=~source;if(view.export_active==source)view.export_active=0;
    view.state=view.export_pending?CONTEXTS_LOADING:CONTEXTS_PAUSED;leave();return true;
}
static int32_t label(void *context,uint32_t source,uint32_t slot,contexts_label_v1 *out) {
    (void)context;if(!source_status(source)||!out||!enter())return -1;
    if(source_status(source)->model_state!=CONTEXTS_MODEL_READY){leave();return -1;}
    if(slot>=8){leave();return 0;}
    *out=(contexts_label_v1){.source=source,.slot=slot};
    if(source==CONTEXTS_AUDIO){out->kind=a.signatures[slot].kind;memcpy(out->name,a.signatures[slot].name,17);}
    else{out->kind=r.signatures[slot].kind;memcpy(out->name,r.signatures[slot].name,17);}
    leave();return 1;
}
static bool claim_preset(void *context,uint32_t source,uint32_t slot,const char *name,uint32_t generation) {
    (void)context;if(!source_status(source)||slot>=8||!name||!memchr(name,0,17)||!enter())return false;
    contexts_source_status_v1 *s=source_status(source);
    if(view.cleanup_pending){leave();return false;}
    unsigned index=source==CONTEXTS_AUDIO?0u:1u;
    uint64_t now=clock_api->monotonic_ms(clock_api->context);
    bool valid=!view.cleanup_pending&&s->model_state==CONTEXTS_MODEL_READY&&s->current&&s->room_valid&&
        !s->room_ambiguous&&s->room_slot==(int32_t)slot&&s->model_generation==generation&&
        !strcmp(s->room_name,name)&&now!=UINT64_MAX&&now>=observed[index]&&now-observed[index]<=FRESH_MS;
    bool same=s->room_entry==s->preset_entry;
    if(valid&&!same) {
        view.preset_source=source;view.preset_slot=(int32_t)slot;view.preset_generation=generation;
        view.preset_result=CONTEXTS_PRESET_CLAIMED;memcpy(claimed_name,s->room_name,17);
        s->preset_entry=s->room_entry;
    }
    leave();return valid&&!same;
}
static bool preset_result(void *context,uint32_t source,uint32_t generation,uint32_t result) {
    (void)context;if(result<CONTEXTS_PRESET_APPLIED||result>CONTEXTS_PRESET_PARTIAL||!enter())return false;
    bool ok=source==view.preset_source&&generation==view.preset_generation&&view.preset_result==CONTEXTS_PRESET_CLAIMED;
    if(ok)view.preset_result=result;
    leave();return ok;
}
static bool start(const risc_provider_dependency_v1 *deps,size_t count) {
    if(started||busy||audio_owned||radio_retained||!deps||count!=3)return false;
    const risc_platform_clock_api_v1 *c=NULL;const twatch_audio_in_api_v1 *m=NULL;
    const risc_radio_iq_extended_api_v1 *q=NULL;
    for(size_t i=0;i<count;i++) {
        if(!deps[i].capability_id||deps[i].api_version!=1||!deps[i].api)return false;
        if(!strcmp(deps[i].capability_id,"platform.clock")&&!c)c=deps[i].api;
        else if(!strcmp(deps[i].capability_id,"audio.input")&&!m)m=deps[i].api;
        else if(!strcmp(deps[i].capability_id,"radio.iq")&&!q)q=deps[i].api;
        else return false;
    }
    if(!c||c->api_version!=1||c->struct_size<sizeof(*c)||!c->monotonic_ms||
       !m||m->api_version!=1||m->struct_size<sizeof(*m)||!m->open||!m->read||!m->close||
       !q||q->base.base.api_version!=1||q->base.base.struct_size<sizeof(*q)||
       !q->base.base.suspend||!q->capture_configured)return false;
    clock_api=c;microphone=m;receiver=q;
    memset(&a,0,sizeof(a));memset(&r,0,sizeof(r));memset(exported,0,sizeof(exported));
    memset(export_invalid,0,sizeof(export_invalid));memset(observed,0,sizeof(observed));
    memset(record_digest,0,sizeof(record_digest));memset(model_digest,0,sizeof(model_digest));
    memset(claimed_name,0,sizeof(claimed_name));
    memset(room_identity,0,sizeof(room_identity));
    memset(event_at,0,sizeof(event_at));memset(retry_at,0,sizeof(retry_at));radio_at=0;
    view=(contexts_status_v1){.struct_size=sizeof(view),.state=CONTEXTS_OFF,
        .audio={.source=CONTEXTS_AUDIO,.room_slot=-1,.event_slot=-1,.age_ms=UINT32_MAX},
        .radio={.source=CONTEXTS_RADIO,.room_slot=-1,.event_slot=-1,.age_ms=UINT32_MAX}};
    reset_audio();reset_radio();started=true;return true;
}
static bool quiesce(void) {
    if(!started)return true;
    if(busy)return false;
    busy=true;bool ok=pause_internal();busy=false;return ok;
}
static void stop(void) {
    if(busy||audio_owned||radio_retained)return;
    started=false;microphone=NULL;receiver=NULL;clock_api=NULL;
}
static const contexts_service_v1 api={1,sizeof(api),NULL,step,pause_service,status,
    request_export,begin_export,export_record,finish_export,label,claim_preset,preset_result};
static const risc_driver_v2 driver={2,sizeof(driver),"contexts-service",CONTEXTS_SERVICE_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
