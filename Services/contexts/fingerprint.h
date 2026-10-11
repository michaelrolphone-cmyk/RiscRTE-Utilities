#pragma once
#include "../../lib/ContextFingerprint/include/ContextFingerprintStore.h"
#include "ContextFingerprintService.h"
static cf_pipeline fp_pipe[CONTEXTS_RF_ONLY?1:2],fp_event_pipe[CONTEXTS_RF_ONLY?1:2];
static bool fp_collecting[2];
static cf_bank fp_bank,fp_merged;
static union {cf_bank incoming,outgoing;} fp_scratch;
#define fp_import fp_scratch.incoming
#define fp_export fp_scratch.outgoing
static int8_t fp_owner_slot[2][CF_PROFILES];
static cf_fusion fp_fusion,fp_event_fusion;
static cf_feature fp_window;
static cf_config fp_config;
static uint32_t fp_sources,fp_dirty;
static bool fp_configured;
static uint64_t fp_utc,fp_utc_at;
static cf_match fp_room,fp_event;
static cf_pipeline *fp_event_pipeline(unsigned s);
static void fp_evaluate(uint64_t now);
static cr_store fp_rules;
static cr_engine fp_rules_engine;
static bool fp_rules_available;
static uint8_t fp_workflow_next[CR_RULES];
static cf_profile fp_learning;
static unsigned fp_learning_state,fp_learning_kind;
static uint64_t fp_learning_at,fp_learning_seen[2],fp_event_quiet[2],fp_event_started[2];
static uint32_t fp_learning_samples[2];
static void fp_learn_observe(unsigned s,const cf_feature *f,uint64_t now,bool event){
 if(!fp_learning_state||fp_learning_state==CONTEXTS_LEARN_READY||((fp_learning_kind==CF_EVENT)!=event))return;
 if(!(f->flags&(CF_TEMPORAL|CF_SPECTRAL))||f->flags&CF_OVERFLOW||fp_learning_seen[s]==now)return;
 fp_learning_seen[s]=now;
 if(fp_learning.present[s]){if(!cf_blend(&fp_learning.feature[s],f,1.f/(float)(fp_learning_samples[s]+1u)))return;}
 else fp_learning.feature[s]=*f;
 fp_learning.present[s]=1;fp_learning.weight[s]=1;fp_learning.updated_seconds[s]=0;
 ++fp_learning_samples[s];fp_learning_state=CONTEXTS_LEARN_RECORDING;
}
static void fp_temporal_event(unsigned s,uint64_t now){
 cf_pipeline*p=fp_event_pipeline(s);
 if(p->active){
  if(!fp_event_started[s]){fp_event_started[s]=now?now:1;cf_window_reset(p,now*1000u);}
  fp_event_quiet[s]=now;
 }
 if(fp_event_started[s]&&((now>=fp_event_quiet[s]+500u)||(now>=fp_event_started[s]+10000u))){
  if(cf_take(p,now*1000u,&fp_window)){
   cf_publish(&fp_event_fusion,s,&fp_window,now*1000u);fp_learn_observe(s,&fp_window,now,true);fp_evaluate(now);
  }
  fp_event_started[s]=0;cf_window_reset(p,now*1000u);
 }
}
static cf_pipeline *fp_pipeline(unsigned s){return &fp_pipe[CONTEXTS_RF_ONLY?0:s];}
static cf_pipeline *fp_event_pipeline(unsigned s){return &fp_event_pipe[CONTEXTS_RF_ONLY?0:s];}
static uint64_t fp_seconds(uint64_t now){return fp_utc&&now>=fp_utc_at?fp_utc+(now-fp_utc_at)/1000u:0;}
static void fp_reset(unsigned s){
    if(!s&&!CONTEXTS_HAS_AUDIO)return;
    cf_config c=cf_defaults(s);cf_init(fp_pipeline(s),&c,s?1u:16000u);
    cf_enable(&fp_fusion,s,false);cf_enable(&fp_event_fusion,s,false);
    c.window_ms=100;cf_init(fp_event_pipeline(s),&c,s?1u:16000u);fp_collecting[s]=false;fp_event_started[s]=fp_event_quiet[s]=0;
}
static void fp_init(void){
    memset(fp_owner_slot,-1,sizeof(fp_owner_slot));memset(&fp_bank,0,sizeof(fp_bank));memset(&fp_fusion,0,sizeof(fp_fusion));memset(&fp_event_fusion,0,sizeof(fp_event_fusion));
    fp_config=cf_defaults(CF_RF);fp_sources=CONTEXTS_SUPPORTED_SOURCES;fp_dirty=0;fp_utc=fp_utc_at=0;
    cr_init(&fp_rules);cr_engine_init(&fp_rules_engine);fp_rules_available=false;memset(fp_workflow_next,0,sizeof(fp_workflow_next));fp_learning_state=0;
    fp_configured=false;fp_room=(cf_match){.slot=-1};fp_event=fp_room;fp_reset(CF_AUDIO);fp_reset(CF_RF);
}
static uint32_t fp_identity(const rf_capture_identity*i){
    /* Explicit fields exclude padding and unstable calibration metadata. */
    const uint32_t v[]={i->lo_hz,i->sample_rate_hz,i->width_hz,i->raw_gain,i->rf_gain,i->bb_gain,i->filter,i->dc[0],i->dc[1],i->dc[2],i->dc[3],i->iq_correction};
    return cf_crc((const uint8_t*)v,sizeof(v));
}
static void fp_evaluate(uint64_t now){
    fp_room=cf_match_profiles(&fp_bank,&fp_fusion,CF_ROOM,now*1000u,fp_seconds(now),&fp_config);
    for(unsigned s=0;s<2;s++)if(fp_event_fusion.have[s]&&(now*1000u<fp_event_fusion.observed_us[s]||now*1000u-fp_event_fusion.observed_us[s]>4000000u))fp_event_fusion.have[s]=false;
    fp_event=cf_match_profiles(&fp_bank,&fp_event_fusion,CF_EVENT,now*1000u,fp_seconds(now),&fp_config);
    if(fp_room.slot>=0&&!fp_learning_state){
        uint32_t before=fp_bank.generation;
        cf_train(&fp_bank,&fp_fusion,(unsigned)fp_room.slot,fp_bank.profiles[fp_room.slot].name,CF_ROOM,now*1000u,fp_seconds(now),&fp_config,false);
        if(before!=fp_bank.generation)fp_dirty|=fp_sources;
    }
}
static void fp_observe(unsigned s,const uint32_t power[128],uint64_t now,const rf_capture_identity*identity){
    cf_pipeline*p=fp_pipeline(s);
    if(s&&p->identity!=fp_identity(identity)){cf_config c=cf_defaults(s);cf_init(p,&c,fp_identity(identity));}
    cf_spectrum(p,power,128,1.f/1073741824.f,s?(float)rf_identity_low_hz(identity):0,s?(float)identity->sample_rate_hz:8000.f);
    cf_spectrum(fp_event_pipeline(s),power,128,1.f/1073741824.f,s?(float)rf_identity_low_hz(identity):0,s?(float)identity->sample_rate_hz:8000.f);
    fp_temporal_event(s,now);
    if(cf_take(p,now*1000u,&fp_window)){cf_publish(&fp_fusion,s,&fp_window,now*1000u);fp_learn_observe(s,&fp_window,now,false);fp_evaluate(now);}
}
static void fp_event_boundary(unsigned s,bool collecting,bool ready,uint64_t now){
    cf_pipeline*p=fp_event_pipeline(s);
    if(fp_configured)return; /* Independent envelope segmenter owns CFP events. */
    if(collecting&&!fp_collecting[s])cf_window_reset(p,now*1000u);
    fp_collecting[s]=collecting;
    if(ready&&cf_take(p,now*1000u,&fp_window)){
        cf_publish(&fp_event_fusion,s,&fp_window,now*1000u);fp_evaluate(now);
    }
}
static void fp_publish(contexts_source_status_v1*s,unsigned index,uint64_t now){
    if(!fp_configured)return;
    fp_evaluate(now);
    if(!fp_fusion.enabled[index]){s->room_valid=s->event_valid=false;return;}
    /* Saved legacy spectra are still used for background subtraction. They
     * cannot stand in for newly captured room/event temporal training. */
    s->room_valid=fp_room.slot>=0;s->room_ambiguous=fp_room.ambiguous;s->room_confidence=(uint32_t)(fp_room.confidence*100.f);
    s->event_valid=fp_event.slot>=0;s->event_ambiguous=fp_event.ambiguous;s->event_confidence=(uint32_t)(fp_event.confidence*100.f);
    s->room_slot=s->event_slot=-1;s->room_name[0]=s->event_name[0]=0;
    if(s->room_valid){
        const char*name=fp_bank.profiles[fp_room.slot].name;memcpy(s->room_name,name,17);
        for(unsigned i=0;i<8;i++)if(!strcmp(name,index?r.signatures[i].name:a.signatures[i].name)){s->room_slot=(int32_t)i;break;}
    }
    if(s->room_valid&&s->room_slot>=0)update_room(s,s->room_slot,s->room_confidence,s->room_ambiguous,true,fp_bank.profiles[fp_room.slot].name);
    if(s->event_valid)memcpy(s->event_name,fp_bank.profiles[fp_event.slot].name,17);
}
static bool fingerprint(void*context,uint32_t operation,void*request){
    (void)context;if(!request||!enter())return false;
    uint64_t now=clock_api->monotonic_ms(clock_api->context);bool ok=false;
    if(view.cleanup_pending||now==UINT64_MAX){leave();return false;}
    if(operation==CONTEXTS_FP_PROFILE){
        contexts_profile_v1*q=request;
        if(q->struct_size>=sizeof(*q)&&q->index<CF_PROFILES){
            cf_profile*p=&fp_bank.profiles[q->index];ok=true;
            if(q->operation==CONTEXTS_PROFILE_RENAME){
                ok=p->kind&&cr_name(q->new_name,17);
                for(unsigned i=0;i<CF_PROFILES&&ok;i++)if(i!=q->index&&fp_bank.profiles[i].kind==p->kind&&!strcmp(fp_bank.profiles[i].name,q->new_name))ok=false;
                if(ok){memcpy(p->name,q->new_name,17);fp_dirty|=(p->present[0]?1u:0u)|(p->present[1]?2u:0u);++fp_bank.generation;}
            }else if(q->operation==CONTEXTS_PROFILE_DELETE){fp_dirty|=(p->present[0]?1u:0u)|(p->present[1]?2u:0u);memset(p,0,sizeof(*p));++fp_bank.generation;}
            else if(q->operation!=CONTEXTS_PROFILE_READ)ok=false;
            if(ok){q->kind=p->kind;q->sources=0;memcpy(q->name,p->name,17);for(unsigned i=0;i<2;i++){if(p->present[i])q->sources|=1u<<i;q->samples[i]=p->present[i]?p->feature[i].windows:0;q->flags[i]=p->present[i]?p->feature[i].flags:0;q->updated[i]=p->updated_seconds[i];}}
        }
    }else if(operation==CONTEXTS_FP_LEARN){
        contexts_learning_v1*q=request;
        if(q->struct_size>=sizeof(*q)){
            ok=true;
            if(q->operation==CONTEXTS_LEARN_BEGIN){
                ok=(q->kind==CF_ROOM||q->kind==CF_EVENT)&&cr_name(q->name,17)&&fp_sources;
                if(ok){memset(&fp_learning,0,sizeof(fp_learning));memcpy(fp_learning.name,q->name,17);fp_learning.kind=(uint8_t)q->kind;fp_learning_kind=q->kind;fp_learning_at=now;fp_learning_state=CONTEXTS_LEARN_WAITING;memset(fp_learning_samples,0,sizeof(fp_learning_samples));for(unsigned i=0;i<2;i++){fp_learning_seen[i]=now;fp_event_started[i]=0;if(i||CONTEXTS_HAS_AUDIO)cf_window_reset(fp_event_pipeline(i),now*1000u);}}
            }else if(q->operation==CONTEXTS_LEARN_CANCEL){fp_learning_state=0;memset(&fp_learning,0,sizeof(fp_learning));}
            else if(q->operation==CONTEXTS_LEARN_SAVE){
                ok=fp_learning_state==CONTEXTS_LEARN_READY;int slot=-1;
                for(unsigned i=0;i<CF_PROFILES;i++)if(fp_bank.profiles[i].kind==fp_learning_kind&&!strcmp(fp_bank.profiles[i].name,fp_learning.name)){slot=(int)i;break;}
                if(slot<0)for(unsigned i=fp_learning_kind==CF_ROOM?0u:8u;i<(fp_learning_kind==CF_ROOM?8u:16u);i++)if(!fp_bank.profiles[i].kind){slot=(int)i;break;}
                if(slot<0)ok=false;
                if(ok){cf_profile staged=fp_bank.profiles[slot];cf_profile*p=&staged;uint32_t dirty=0;for(unsigned i=0;i<2;i++)if(fp_learning.present[i]){if(p->present[i]){if(!cf_blend(&p->feature[i],&fp_learning.feature[i],.05f)){ok=false;break;}}else p->feature[i]=fp_learning.feature[i];p->present[i]=1;p->weight[i]=1;p->updated_seconds[i]=fp_seconds(now);dirty|=1u<<i;}if(ok){p->kind=fp_learning.kind;memcpy(p->name,fp_learning.name,17);fp_bank.profiles[slot]=staged;fp_dirty|=dirty;++fp_bank.generation;fp_learning_state=0;}}
            }else if(q->operation!=CONTEXTS_LEARN_POLL)ok=false;
            unsigned samples=fp_learning_samples[0]>fp_learning_samples[1]?fp_learning_samples[0]:fp_learning_samples[1];
            bool event=fp_learning_kind==CF_EVENT;
            if(fp_learning_state&&((event&&samples>=3)||(!event&&now>=fp_learning_at+30000u&&samples>=3)))fp_learning_state=CONTEXTS_LEARN_READY;
            q->state=fp_learning_state;q->samples=samples;q->sources=(fp_learning.present[0]?1u:0u)|(fp_learning.present[1]?2u:0u);
            q->progress=event?(samples>=3?100:samples*100/3):(uint32_t)((now-fp_learning_at)/300u);if(q->progress>100)q->progress=100;
        }
    }else if(operation==CONTEXTS_FP_RULES){
        contexts_rules_v1*q=request;if(q->struct_size>=sizeof(*q)){ok=true;if(q->set){ok=cr_valid(&q->store);if(ok){for(unsigned i=0;i<CR_RULES;i++)if(memcmp(&fp_rules.rules[i],&q->store.rules[i],sizeof(cr_rule))){fp_rules_engine.pending&=~(1u<<i);fp_workflow_next[i]=0;}fp_rules=q->store;fp_rules_available=q->available;}}else{q->store=fp_rules;q->available=fp_rules_available;}}
    }else if(operation==CONTEXTS_FP_EVALUATE){
        contexts_evaluate_v1*q=request;if(q->struct_size>=sizeof(*q)){
            fp_evaluate(now);q->observation.now_ms=now;q->observation.enabled&=fp_rules_available;
            q->observation.room=fp_room.slot>=0?fp_bank.profiles[fp_room.slot].name:NULL;
            q->observation.event=fp_event.slot>=0?fp_bank.profiles[fp_event.slot].name:NULL;
            q->observation.signal=NULL;
            uint64_t latest=fp_fusion.observed_us[0]>fp_fusion.observed_us[1]?fp_fusion.observed_us[0]:fp_fusion.observed_us[1];
            q->observation.classification_pending=fp_sources&&latest&&now*1000u>=latest&&now*1000u-latest<12000000u&&!(fp_fusion.have[0]||fp_fusion.have[1]);
            q->result=cr_evaluate(&fp_rules_engine,&fp_rules,&q->observation);
            for(unsigned i=0;i<CR_RULES;i++)if(q->result.entered&(1u<<i))fp_workflow_next[i]=0;
            q->observation.room=q->observation.event=q->observation.signal=NULL;ok=true;
        }
    }else if(operation==CONTEXTS_FP_WORKFLOW){
        contexts_workflow_v1*q=request;if(q->struct_size>=sizeof(*q)){
            ok=true;q->available=false;
            if(q->result&&q->rule<CR_RULES)cr_record(&fp_rules_engine,&fp_rules,q->rule,q->result,now);
            if(q->claim)for(unsigned i=0;i<CR_RULES;i++)if(fp_rules_engine.pending&(1u<<i)){
                unsigned j=fp_workflow_next[i];if(j>=fp_rules.rules[i].steps){fp_rules_engine.pending&=~(1u<<i);continue;}
                q->rule=i;q->index=j;q->step=fp_rules.rules[i].workflow[j];q->available=true;++fp_workflow_next[i];break;
            }
        }
    }else if(operation==CONTEXTS_FP_LOG){
        contexts_log_v1*q=request;if(q->struct_size>=sizeof(*q)){q->count=fp_rules_engine.log_count;if(q->index<q->count){q->entry=fp_rules_engine.logs[(fp_rules_engine.log_next+CR_LOGS-1-q->index)%CR_LOGS];ok=true;}}
    }else if(operation==CONTEXTS_FP_CONFIG){
        contexts_fingerprint_config_v1*q=request;
        if(q->struct_size>=sizeof(*q)&&!(q->sources&~CONTEXTS_ALL)){
            uint32_t old_sources=fp_sources;fp_configured=true;fp_sources=q->sources&CONTEXTS_SUPPORTED_SOURCES;fp_config.temporal_only=q->temporal_only;
            if(q->utc_seconds){fp_utc=q->utc_seconds;fp_utc_at=now;}
            for(unsigned s=0;s<2;s++)if(!(fp_sources&(1u<<s))&&(old_sources&(1u<<s)))fp_reset(s);
            ok=true;
        }
    }else if(operation==CONTEXTS_FP_STATUS){
        contexts_fingerprint_status_v1*q=request;
        if(q->struct_size>=sizeof(*q)){
            fp_evaluate(now);*q=(contexts_fingerprint_status_v1){.struct_size=sizeof(*q),.sources=fp_sources,.generation=fp_bank.generation,.dirty_sources=fp_dirty,.room_slot=fp_room.slot,.event_slot=fp_event.slot,.room_confidence=(uint32_t)(fp_room.confidence*100.f),.event_confidence=(uint32_t)(fp_event.confidence*100.f),.temporal_only=fp_config.temporal_only,.room_ambiguous=fp_room.ambiguous,.event_ambiguous=fp_event.ambiguous};
            for(unsigned s=0;s<2;s++){q->feature_flags[s]=fp_fusion.have[s]?fp_fusion.live[s].flags:0;q->fresh_windows[s]=fp_fusion.fresh_windows[s];}
            if(fp_room.slot>=0)memcpy(q->room_name,fp_bank.profiles[fp_room.slot].name,17);
            if(fp_event.slot>=0)memcpy(q->event_name,fp_bank.profiles[fp_event.slot].name,17);
            ok=true;
        }
    }else if(operation==CONTEXTS_FP_CONFIRM){
        contexts_fingerprint_confirm_v1*q=request;
        if(q->struct_size>=sizeof(*q)&&q->kind>=CF_ROOM&&q->kind<=CF_NEGATIVE&&cr_name(q->name,17)){
            int slot=-1;for(unsigned i=0;i<CF_PROFILES;i++)if(fp_bank.profiles[i].kind==q->kind&&!strcmp(fp_bank.profiles[i].name,q->name)){slot=(int)i;break;}
            if(slot<0)for(unsigned i=(q->kind-1u)*8u;i<q->kind*8u;i++)if(!fp_bank.profiles[i].kind){slot=(int)i;break;}
            if(slot>=0)ok=cf_train(&fp_bank,q->kind==CF_ROOM?&fp_fusion:&fp_event_fusion,(unsigned)slot,q->name,q->kind,now*1000u,fp_seconds(now),&fp_config,true);
            if(ok)fp_dirty|=fp_sources;
        }
    }else{
        contexts_fingerprint_record_v1*q=request;
        if(q->struct_size>=sizeof(*q)&&(q->source==CONTEXTS_AUDIO||q->source==CONTEXTS_RADIO)){
            unsigned s=q->source==CONTEXTS_AUDIO?0:1;
            if(operation==CONTEXTS_FP_EXPORT&&q->bytes){
                memset(&fp_export,0,sizeof(fp_export));fp_export.generation=fp_bank.generation;
                bool used[CF_PROFILES]={0};
                for(unsigned j=0;j<CF_PROFILES;j++)if(fp_owner_slot[s][j]>=0){unsigned at=(unsigned)fp_owner_slot[s][j];if(!used[at]&&fp_bank.profiles[at].present[s]){fp_export.profiles[j]=fp_bank.profiles[at];used[at]=true;}}
                for(unsigned j=0;j<CF_PROFILES;j++)if(fp_bank.profiles[j].present[s]&&!used[j])for(unsigned k=0;k<CF_PROFILES;k++)if(!fp_export.profiles[k].kind){unsigned target=!fp_export.profiles[j].kind?j:k;fp_export.profiles[target]=fp_bank.profiles[j];fp_owner_slot[s][target]=(int8_t)j;break;}
                q->size=cf_encode_source(&fp_export,s,q->bytes,q->capacity);q->generation=fp_bank.generation;ok=q->size!=0;
            }
            else if(operation==CONTEXTS_FP_SAVED){if(q->generation==fp_bank.generation){fp_dirty&=~q->source;ok=true;}}
            else if(operation==CONTEXTS_FP_IMPORT&&!(fp_dirty&q->source)&&q->bytes&&cf_decode_source(&fp_import,s,q->bytes,q->size)){
                /* Validate all incoming names/capacity before changing live state. */
                fp_merged=fp_bank;
                for(unsigned i=0;i<CF_PROFILES;i++){fp_merged.profiles[i].present[s]=0;if(!fp_merged.profiles[i].present[1-s])memset(&fp_merged.profiles[i],0,sizeof(cf_profile));}
                int8_t mapping[CF_PROFILES];memset(mapping,-1,sizeof(mapping));ok=true;
                for(unsigned i=0;i<CF_PROFILES&&ok;i++)if(fp_import.profiles[i].present[s]){
                    cf_profile*incoming=&fp_import.profiles[i];int slot=-1;
                    for(unsigned j=0;j<CF_PROFILES;j++)if(fp_merged.profiles[j].kind==incoming->kind&&!strcmp(fp_merged.profiles[j].name,incoming->name)){slot=(int)j;break;}
                    if(slot<0)for(unsigned j=(incoming->kind-1u)*8u;j<incoming->kind*8u;j++)if(!fp_merged.profiles[j].kind){slot=(int)j;break;}
                    if(slot<0){ok=false;break;}
                    mapping[i]=(int8_t)slot;cf_profile*p=&fp_merged.profiles[slot];p->kind=incoming->kind;memcpy(p->name,incoming->name,17);p->feature[s]=incoming->feature[s];p->present[s]=1;p->weight[s]=incoming->weight[s];p->updated_seconds[s]=incoming->updated_seconds[s];
                }
                if(ok){memcpy(fp_owner_slot[s],mapping,sizeof(mapping));fp_bank=fp_merged;if(++fp_bank.generation==0)++fp_bank.generation;}
            }
        }
    }
    leave();return ok;
}
