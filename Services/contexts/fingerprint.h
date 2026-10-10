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
static cf_pipeline *fp_pipeline(unsigned s){return &fp_pipe[CONTEXTS_RF_ONLY?0:s];}
static cf_pipeline *fp_event_pipeline(unsigned s){return &fp_event_pipe[CONTEXTS_RF_ONLY?0:s];}
static uint64_t fp_seconds(uint64_t now){return fp_utc&&now>=fp_utc_at?fp_utc+(now-fp_utc_at)/1000u:0;}
static void fp_reset(unsigned s){
    if(!s&&!CONTEXTS_HAS_AUDIO)return;
    cf_config c=cf_defaults(s);cf_init(fp_pipeline(s),&c,s?1u:16000u);
    cf_enable(&fp_fusion,s,false);cf_enable(&fp_event_fusion,s,false);
    c.window_ms=100;cf_init(fp_event_pipeline(s),&c,s?1u:16000u);fp_collecting[s]=false;
}
static void fp_init(void){
    memset(fp_owner_slot,-1,sizeof(fp_owner_slot));memset(&fp_bank,0,sizeof(fp_bank));memset(&fp_fusion,0,sizeof(fp_fusion));memset(&fp_event_fusion,0,sizeof(fp_event_fusion));
    fp_config=cf_defaults(CF_RF);fp_sources=CONTEXTS_SUPPORTED_SOURCES;fp_dirty=0;fp_utc=fp_utc_at=0;
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
    if(fp_room.slot>=0){
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
    if(cf_take(p,now*1000u,&fp_window)){cf_publish(&fp_fusion,s,&fp_window,now*1000u);fp_evaluate(now);}
}
static void fp_event_boundary(unsigned s,bool collecting,bool ready,uint64_t now){
    cf_pipeline*p=fp_event_pipeline(s);
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
    if(operation==CONTEXTS_FP_CONFIG){
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
        if(q->struct_size>=sizeof(*q)&&memchr(q->name,0,17)&&q->name[0]){
            int slot=-1;for(unsigned i=0;i<CF_PROFILES;i++)if(fp_bank.profiles[i].kind==q->kind&&!strcmp(fp_bank.profiles[i].name,q->name)){slot=(int)i;break;}
            if(slot<0)for(unsigned i=0;i<CF_PROFILES;i++)if(!fp_bank.profiles[i].kind){slot=(int)i;break;}
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
                for(unsigned j=0;j<CF_PROFILES;j++)if(fp_owner_slot[s][j]>=0){unsigned at=(unsigned)fp_owner_slot[s][j];if(fp_bank.profiles[at].present[s]){fp_export.profiles[j]=fp_bank.profiles[at];used[at]=true;}}
                for(unsigned j=0;j<CF_PROFILES;j++)if(fp_bank.profiles[j].present[s]&&!used[j])for(unsigned k=0;k<CF_PROFILES;k++)if(!fp_export.profiles[k].kind){fp_export.profiles[k]=fp_bank.profiles[j];fp_owner_slot[s][k]=(int8_t)j;break;}
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
                    if(slot<0)for(unsigned j=0;j<CF_PROFILES;j++)if(!fp_merged.profiles[j].kind){slot=(int)j;break;}
                    if(slot<0){ok=false;break;}
                    mapping[i]=(int8_t)slot;cf_profile*p=&fp_merged.profiles[slot];p->kind=incoming->kind;memcpy(p->name,incoming->name,17);p->feature[s]=incoming->feature[s];p->present[s]=1;p->weight[s]=incoming->weight[s];p->updated_seconds[s]=incoming->updated_seconds[s];
                }
                if(ok){memcpy(fp_owner_slot[s],mapping,sizeof(mapping));fp_bank=fp_merged;if(++fp_bank.generation==0)++fp_bank.generation;}
            }
        }
    }
    leave();return ok;
}
