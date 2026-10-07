/* Ordinary singleton ELF. All policy/persistence/UI-facing state belongs here;
 * Runtime only resolves opaque dependencies. No poll callback, task or ISR. */
#include "AlarmRecords.h"
#ifdef ALARM_DND_CONTROL
#include "AlarmDnd.h"
#endif
#ifdef ALARM_VOLUME_CONTROL
#include "AlarmVolume.h"
#endif
#ifdef POINTS_IN_TIME_SERVICE
#include "PointsSchedule.h"
#endif
#include "AlarmOutputV1.h"
#include "PortableRtcClock.h"
#include "RiscProviderV2.h"
#include "RiscPlatformClockV1.h"
#include "RiscBoundKeyValueV1.h"
#include <string.h>
#include <limits.h>

typedef enum { LOAD_ALARM,LOAD_TIMER,LOAD_MODE,LOAD_ALARM_OCC,LOAD_TIMER_OCC,
               READ_RTC,EVALUATE,IDLE,WRITE_OCC,VERIFY_OCC,ACTIVATE_RTC,START_AUDIO,
               START_HAPTIC,PLAYING,CLEAN_HAPTIC,CLEAN_SILENCE,CLEAN_AUDIO,
               BLOCKED
#ifdef ALARM_VOLUME_CONTROL
               ,LOAD_VOLUME,SET_AUDIO_GAIN
#endif
#ifdef ALARM_DND_CONTROL
               ,LOAD_DND
#endif
#ifdef POINTS_IN_TIME_SERVICE
               ,LOAD_POINTS_CFG,LOAD_POINTS_OCC
#endif
               } phase_t;
static const risc_bound_key_value_v1 *kv;
static const risc_platform_clock_api_v1 *clock_api;
static const twatch_rtc_api_v1 *rtc;
static const twatch_haptic_api_v1 *haptic;
static const twatch_audio_out_api_v1 *audio;
static alarm_config config[2], staging[2];
static alarm_occurrence occurrences[2], staged_occ[2], desired;
#ifdef POINTS_IN_TIME_SERVICE
static points_config points_configured,points_staging;
static points_ledger points_occ,points_staged_occ,points_desired;
#endif
static alarm_status_v1 view;
static phase_t phase;
static unsigned selected;
#ifdef ALARM_VOLUME_CONTROL
static unsigned staged_volume;
static bool volume_sound_enabled(void) {
    return staged_volume!=0;
}
#endif
static uint32_t staged_mode, seconds, previous_seconds, sleep_snapshot, alert_limit_ms;
static uint64_t previous_ms, sample_ms, next_reconcile, alert_started, next_pulse;
static bool started, in_call, refreshed, anchored, sleep_waiting;
static bool sleep_ticket_valid;
static alarm_sleep_v1 sleep_ticket;
static bool active, dismiss, haptic_uncertain, audio_uncertain, cleanup_failed;
static bool persistence_pending, foreground_failed;
#ifdef ALARM_DND_CONTROL
static bool staged_dnd,muting_active;
static int32_t read_dnd(bool *enabled) {
    uint8_t b=0;uint32_t n=0;int32_t r=kv->get(kv->context,ALARM_DND_KEY,&b,1,&n);
    if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){*enabled=false;return ALARM_OK;}
    if(r!=RISC_BOUND_KEY_VALUE_OK||n!=1||b>1)return ALARM_STORAGE;
    *enabled=b!=0;return ALARM_OK;
}
#endif

static int32_t error;
static const char *const cfg_keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY};
static const char *const occ_keys[]={ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY};
static uint64_t now_ms(void) { return clock_api->monotonic_ms(clock_api->context); }
static alarm_token_v1 token(const alarm_occurrence *o) {
    return (alarm_token_v1){o->kind,o->revision,o->deadline,o->generation};
}
static void update_view(void) {
    view.api_version=1;view.struct_size=sizeof(view);view.error=error;
    view.rtc_seconds=seconds;view.mode=staged_mode;
    view.output_uncertain=haptic_uncertain||audio_uncertain;
#ifdef POINTS_IN_TIME_SERVICE
    bool point_cue=active&&selected==2;
#else
    bool point_cue=false;
#endif
    view.state=phase==BLOCKED?ALARM_STATE_BLOCKED:active&&!point_cue?(dismiss?ALARM_STATE_DISMISSING:ALARM_STATE_ALERT):
        point_cue?ALARM_STATE_CUE:phase==IDLE?ALARM_STATE_READY:ALARM_STATE_LOADING;
    for(unsigned i=0;i<2;i++) {
        alarm_schedule_status_v1 *s=&view.schedules[i];
        s->revision=config[i].revision;s->deadline=config[i].deadline;s->state=ALARM_SCHEDULE_OFF;
        if(config[i].enabled) {
            s->state=config[i].deadline<=seconds?ALARM_SCHEDULE_DUE:ALARM_SCHEDULE_ARMED;
            if(alarm_same_occurrence(&occurrences[i],&config[i])) {
                if(occurrences[i].state==ALARM_OCC_ACKED)s->state=ALARM_SCHEDULE_DISMISSED;
                if(occurrences[i].state==ALARM_OCC_EXPIRED)s->state=ALARM_SCHEDULE_EXPIRED;
            }
        }
    }
    if(active&&!point_cue) {
        view.occurrence=token(&desired);view.mode=desired.mode;view.recovery_until=desired.recovery_until;
        uint64_t elapsed=now_ms()-alert_started;
        view.remaining_ms=elapsed<alert_limit_ms?(uint32_t)(alert_limit_ms-elapsed):0;
        const char *name=desired.kind==ALARM_KIND_ALARM?"ALARM":"COUNTDOWN FINISHED";
        #ifdef POINTS_IN_TIME_SERVICE
        if(selected==2)name=points_label(points_configured.points[points_desired.slot].kind,points_desired.edge);
#endif
        memset(view.label,0,sizeof(view.label));memcpy(view.label,name,strlen(name));
    } else {
        memset(&view.occurrence,0,sizeof(view.occurrence));view.recovery_until=view.remaining_ms=0;
        memset(view.label,0,sizeof(view.label));
    }
}
static int32_t fail(int32_t e) { error=e;phase=BLOCKED;sleep_waiting=sleep_ticket_valid=false;update_view();return e; }
static const void *dependency(const risc_provider_dependency_v1 *d,size_t n,const char *name,uint32_t v,size_t size) {
    const void *out=NULL;
    for(size_t i=0;i<n;i++)if(d[i].capability_id&&!strcmp(d[i].capability_id,name)) {
        const uint32_t *head=d[i].api;
        if(out||!head||d[i].api_version!=v||head[0]!=v||head[1]<size)return NULL;
        out=head;
    }
    return out;
}
static bool read_config(unsigned i) {
    uint8_t b[ALARM_RECORD_SIZE];uint32_t n=0;
    int32_t r=kv->get(kv->context,cfg_keys[i],b,sizeof(b),&n);
    if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){staging[i]=(alarm_config){.kind=(uint8_t)(i+1)};return true;}
    return r==RISC_BOUND_KEY_VALUE_OK&&alarm_config_decode(&staging[i],b,n,(uint8_t)(i+1));
}
static bool read_occurrence(unsigned i) {
    uint8_t b[ALARM_RECORD_SIZE];uint32_t n=0;
    int32_t r=kv->get(kv->context,occ_keys[i],b,sizeof(b),&n);
    if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){staged_occ[i]=(alarm_occurrence){0};return true;}
    return r==RISC_BOUND_KEY_VALUE_OK&&alarm_occurrence_decode(&staged_occ[i],b,n,(uint8_t)(i+1));
}
static void begin_reconcile(void) {
    sleep_ticket_valid=false;
    persistence_pending=false;foreground_failed=false;
    phase=LOAD_ALARM;refreshed=false;error=0;memset(staging,0,sizeof(staging));memset(staged_occ,0,sizeof(staged_occ));
}
static void begin_cleanup(void) {
    phase=CLEAN_HAPTIC;cleanup_failed=false;
    /* Call both outputs independently even if open/effect reported failure.
       A backend may have acquired resources before returning false. */
}
static void prepare_occurrence(unsigned i,bool replay) {
    selected=i;
    desired=(alarm_occurrence){.kind=(uint8_t)(i+1),.state=ALARM_OCC_PENDING,.mode=(uint8_t)staged_mode,
        .revision=config[i].revision,.deadline=config[i].deadline,.recovery_until=config[i].deadline+ALARM_RECOVERY_SECONDS};
    if(occurrences[i].generation==UINT32_MAX){fail(ALARM_EXHAUSTED);return;}
    desired.generation=occurrences[i].generation+1;
    if(replay)desired.mode=occurrences[i].mode;
    desired.silenced=replay?occurrences[i].silenced:0;
#ifdef ALARM_DND_CONTROL
    desired.silenced|=staged_dnd;
#endif
    if(seconds>=desired.recovery_until)desired.state=ALARM_OCC_EXPIRED;
    persistence_pending=true;phase=WRITE_OCC;
}
#ifdef POINTS_IN_TIME_SERVICE
static bool read_points_config(void) {
    uint8_t b[POINTS_RECORD_SIZE];uint32_t n=0;
    int32_t r=kv->get(kv->context,POINTS_CONFIG_KEY,b,sizeof(b),&n);
    if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){points_staging=points_default_config();return true;}
    return r==RISC_BOUND_KEY_VALUE_OK&&points_config_decode(&points_staging,b,n);
}
static bool read_points_occ(void) {
    uint8_t b[POINTS_RECORD_SIZE];uint32_t n=0;
    int32_t r=kv->get(kv->context,POINTS_OCCURRENCE_KEY,b,sizeof(b),&n);
    if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){points_staged_occ=(points_ledger){0};return true;}
    return r==RISC_BOUND_KEY_VALUE_OK&&points_ledger_decode(&points_staged_occ,b,n);
}
static bool reconcile_points(void) {
    uint8_t a[POINTS_RECORD_SIZE],b[POINTS_RECORD_SIZE];
    if(points_staging.revision<points_configured.revision||points_staged_occ.generation<points_occ.generation)return false;
    if(points_staging.revision&&points_staging.revision==points_configured.revision) {
        points_config_encode(&points_staging,a);points_config_encode(&points_configured,b);if(memcmp(a,b,sizeof(a)))return false;
    }
    if(points_staged_occ.revision>points_staging.revision||points_staged_occ.revision<points_occ.revision)return false;
    if(points_staged_occ.revision==points_occ.revision)
        for(unsigned i=0;i<POINTS_MAX;i++) {
            if(points_staged_occ.day[i]<points_occ.day[i])return false;
            if(points_staged_occ.day[i]==points_occ.day[i]&&
               (points_staged_occ.delivered[i]&points_occ.delivered[i])!=points_occ.delivered[i])return false;
        }
    if(points_staged_occ.generation&&points_staged_occ.generation==points_occ.generation) {
        points_ledger_encode(&points_staged_occ,a);points_ledger_encode(&points_occ,b);if(memcmp(a,b,sizeof(a)))return false;
    }
    points_configured=points_staging;points_occ=points_staged_occ;
    if(points_occ.revision==points_configured.revision) {
        for(unsigned slot=0;slot<POINTS_MAX;slot++)if(points_occ.day[slot]) {
            for(unsigned edge=0;edge<POINTS_EDGE_COUNT;edge++)if(points_occ.delivered[slot]&(1u<<edge)) {
                points_event e={0};if(!points_event_for_day(&points_configured,slot,points_occ.day[slot],edge,&e,NULL))return false;
            }
        }
    }
    if(points_occ.revision==points_configured.revision&&points_occ.state) {
        points_event e={0};uint8_t configured_mode=points_configured.points[points_occ.slot].mode;
        if(configured_mode&&points_occ.mode!=configured_mode)return false;
        if(!points_event_for_day(&points_configured,points_occ.slot,points_occ.day[points_occ.slot],points_occ.edge,&e,NULL)||
           e.deadline!=points_occ.deadline)return false;
    }
    return true;
}
static points_ledger points_current_ledger(void) {
    if(points_occ.revision==points_configured.revision)return points_occ;
    return (points_ledger){.revision=points_configured.revision,.generation=points_occ.generation};
}
static void prepare_point(const points_event *e,bool replay) {
    selected=2;points_desired=points_current_ledger();
    if(points_desired.generation==UINT32_MAX){fail(ALARM_EXHAUSTED);return;}
    points_desired.generation++;points_desired.slot=e->slot;points_desired.edge=e->edge;
    points_desired.deadline=e->deadline;points_desired.recovery_until=e->deadline+ALARM_RECOVERY_SECONDS;
    points_desired.state=seconds>=points_desired.recovery_until?ALARM_OCC_EXPIRED:ALARM_OCC_PENDING;
    points_desired.mode=replay?points_occ.mode:e->mode?e->mode:(uint8_t)staged_mode;
    points_desired.silenced=replay?points_occ.silenced:0;
#ifdef ALARM_DND_CONTROL
    points_desired.silenced|=staged_dnd;
#endif
    if(!points_ledger_mark(&points_desired,e->slot,e->parent_day,e->edge)){fail(ALARM_STORAGE);return;}
    desired=(alarm_occurrence){.revision=points_desired.revision,.deadline=e->deadline,.generation=points_desired.generation,
        .recovery_until=points_desired.recovery_until,.kind=(uint8_t)points_token_kind(e->slot,e->edge),
        .state=points_desired.state,.mode=points_desired.mode,.silenced=points_desired.silenced};
    persistence_pending=true;phase=WRITE_OCC;
}
#endif
static int32_t evaluate(void) {
    for(unsigned i=0;i<2;i++) {
        if(staging[i].revision<config[i].revision)return fail(ALARM_STORAGE);
        if(staging[i].revision&&staging[i].revision==config[i].revision&&
           (staging[i].deadline!=config[i].deadline||staging[i].created!=config[i].created||
            staging[i].duration!=config[i].duration||staging[i].enabled!=config[i].enabled))return fail(ALARM_STORAGE);
        if(staged_occ[i].generation<occurrences[i].generation)return fail(ALARM_STORAGE);
        if(staged_occ[i].generation&&staged_occ[i].generation==occurrences[i].generation) {
            uint8_t old_bytes[ALARM_RECORD_SIZE],new_bytes[ALARM_RECORD_SIZE];
            alarm_occurrence_encode(&occurrences[i],old_bytes);alarm_occurrence_encode(&staged_occ[i],new_bytes);
            if(memcmp(old_bytes,new_bytes,sizeof(old_bytes)))return fail(ALARM_STORAGE);
        }
    }
#ifdef POINTS_IN_TIME_SERVICE
    if(!reconcile_points())return fail(ALARM_STORAGE);
    if(points_configured.revision) {
        uint32_t local_day;
        if(seconds<points_configured.created||!points_local_day(seconds,&local_day))return fail(ALARM_RTC);
    }
#endif
    memcpy(config,staging,sizeof(config));memcpy(occurrences,staged_occ,sizeof(occurrences));
    if(view.snapshot==UINT32_MAX)return fail(ALARM_EXHAUSTED);
    ++view.snapshot;
    int candidate=-1;
    for(unsigned i=0;i<2;i++) {
        if(occurrences[i].state && (occurrences[i].revision>config[i].revision ||
           (occurrences[i].revision==config[i].revision&&occurrences[i].deadline!=config[i].deadline)))return fail(ALARM_STORAGE);
        if(!config[i].enabled)continue;
        if(seconds<config[i].created)return fail(ALARM_RTC);
        if(config[i].deadline>seconds)continue;
        if(alarm_same_occurrence(&occurrences[i],&config[i])&&occurrences[i].state!=ALARM_OCC_PENDING)continue;
        if(candidate<0||config[i].deadline<config[candidate].deadline)candidate=(int)i;
    }
#ifdef POINTS_IN_TIME_SERVICE
    if(points_configured.revision) {
        points_ledger ledger=points_current_ledger();points_event event={0};bool replay=false,have=false;
        if(ledger.state==ALARM_OCC_PENDING) {
            have=points_event_for_day(&points_configured,ledger.slot,ledger.day[ledger.slot],ledger.edge,&event,NULL)&&event.deadline<=seconds;replay=true;
        } else {
            bool compacted=false;
            /* Compact every expired edge in one atomic ledger write. A long
             * outage must not require one reconciliation for each missed edge. */
            for(unsigned slot=0;slot<POINTS_MAX;slot++)for(unsigned edge=0;edge<POINTS_EDGE_COUNT;edge++) {
                points_event expired={0};
                if(points_latest_for_edge(&points_configured,&ledger,seconds,slot,edge,&expired)&&seconds>=expired.deadline+ALARM_RECOVERY_SECONDS) {
                    if(!points_ledger_mark(&ledger,slot,expired.parent_day,edge))return fail(ALARM_STORAGE);
                    compacted=true;
                }
            }
            if(compacted) {
                if(ledger.generation==UINT32_MAX)return fail(ALARM_EXHAUSTED);
                ledger.generation++;ledger.state=ledger.slot=ledger.edge=ledger.mode=ledger.silenced=0;
                ledger.deadline=ledger.recovery_until=0;points_desired=ledger;
                desired=(alarm_occurrence){0};selected=2;persistence_pending=true;phase=WRITE_OCC;return ALARM_OK;
            }
            have=points_due(&points_configured,&ledger,seconds,&event);
        }
        if(have&&(candidate<0||event.deadline<config[candidate].deadline)){prepare_point(&event,replay);return error?error:ALARM_OK;}
    }
#endif
    if(candidate>=0)prepare_occurrence((unsigned)candidate,alarm_same_occurrence(&occurrences[candidate],&config[candidate]));
    else {phase=IDLE;next_reconcile=now_ms()+1000;}
    return error?error:ALARM_OK;
}
static int32_t do_step(void) {
    if(foreground_failed)return error?error:ALARM_FOREGROUND;
    uint64_t now=now_ms();
    if(active&&(phase==ACTIVATE_RTC||phase==START_AUDIO||phase==START_HAPTIC||phase==PLAYING)&&
       (dismiss||now-alert_started>=alert_limit_ms))begin_cleanup();
#ifdef ALARM_DND_CONTROL
    /* The foreground is serialized. Check immediately before any new output;
     * an active alert stays visual while its outputs are stopped and its mute
     * marker is durably verified. Disabling DND cannot replay this occurrence. */
    if(active&&!desired.silenced&&(phase==START_AUDIO||
#ifdef ALARM_VOLUME_CONTROL
       phase==SET_AUDIO_GAIN||
#endif
       phase==START_HAPTIC||phase==PLAYING)) {
        bool muted=false;int32_t result=read_dnd(&muted);
        if(result!=ALARM_OK||muted) {
            desired.silenced=1;
#ifdef POINTS_IN_TIME_SERVICE
            if(selected==2)points_desired.silenced=1;
#endif
            muting_active=true;error=result;begin_cleanup();
        }
    }
#endif
    switch(phase) {
    case LOAD_ALARM: if(!read_config(0))return fail(ALARM_STORAGE);phase=LOAD_TIMER;break;
    case LOAD_TIMER: if(!read_config(1))return fail(ALARM_STORAGE);phase=LOAD_MODE;break;
    case LOAD_MODE: {
        uint8_t b=0;uint32_t n=0;int32_t r=kv->get(kv->context,ALARM_MODE_KEY,&b,1,&n);
        if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND)staged_mode=ALARM_MODE_VIBRATE;
        else if(r!=RISC_BOUND_KEY_VALUE_OK||n!=1||b<1||b>3)return fail(ALARM_STORAGE);
        else staged_mode=b;
#ifdef ALARM_VOLUME_CONTROL
        phase=LOAD_VOLUME;break;
    }
    case LOAD_VOLUME: {
        uint8_t b=0;uint32_t n=0;int32_t r=kv->get(kv->context,ALARM_VOLUME_KEY,&b,1,&n);
        if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND)staged_volume=ALARM_VOLUME_DEFAULT;
        else if(r!=RISC_BOUND_KEY_VALUE_OK||!alarm_volume_decode(&b,n,&staged_volume))return fail(ALARM_STORAGE);
#endif
        phase=
#ifdef ALARM_DND_CONTROL
            LOAD_DND;
#else
            LOAD_ALARM_OCC;
#endif
        break;
    }
#ifdef ALARM_DND_CONTROL
    case LOAD_DND:if(read_dnd(&staged_dnd)!=ALARM_OK)return fail(ALARM_STORAGE);phase=LOAD_ALARM_OCC;break;
#endif
    case LOAD_ALARM_OCC:if(!read_occurrence(0))return fail(ALARM_STORAGE);phase=LOAD_TIMER_OCC;break;
    case LOAD_TIMER_OCC:if(!read_occurrence(1))return fail(ALARM_STORAGE);
#ifdef POINTS_IN_TIME_SERVICE
        phase=LOAD_POINTS_CFG;break;
    case LOAD_POINTS_CFG:if(!read_points_config())return fail(ALARM_STORAGE);phase=LOAD_POINTS_OCC;break;
    case LOAD_POINTS_OCC:if(!read_points_occ())return fail(ALARM_STORAGE);phase=READ_RTC;break;
#else
        phase=READ_RTC;break;
#endif
    case READ_RTC: {
        twatch_rtc_time_v1 t;uint32_t current;
        if(!rtc->read(rtc->context,&t)||t.weekday>6||!alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,&current))return fail(ALARM_RTC);
        uint64_t at=now_ms();
        if(anchored) {
            if(at<previous_ms||at-previous_ms>UINT32_MAX)return fail(ALARM_RTC);
            uint32_t elapsed=(uint32_t)(at-previous_ms)/1000;
            uint64_t delta=current>=previous_seconds?current-previous_seconds:UINT64_MAX;
            if(delta==UINT64_MAX||(delta>elapsed?delta-elapsed:elapsed-delta)>2)return fail(ALARM_RTC);
        }
        seconds=previous_seconds=current;sample_ms=previous_ms=at;anchored=true;phase=EVALUATE;break;
    }
    case EVALUATE:return evaluate();
    case IDLE:if(refreshed||now>=next_reconcile)begin_reconcile();break;
    case WRITE_OCC: {
#ifdef POINTS_IN_TIME_SERVICE
        if(selected==2) {
            uint8_t b[POINTS_RECORD_SIZE];points_desired.state=desired.state;points_ledger_encode(&points_desired,b);
            (void)kv->put(kv->context,POINTS_OCCURRENCE_KEY,b,sizeof(b));phase=VERIFY_OCC;break;
        }
#endif
        uint8_t b[ALARM_RECORD_SIZE];alarm_occurrence_encode(&desired,b);
        /* Even IO may have persisted. Always verify before deciding next action. */
        (void)kv->put(kv->context,occ_keys[selected],b,sizeof(b));phase=VERIFY_OCC;break;
    }
    case VERIFY_OCC: {
#ifdef POINTS_IN_TIME_SERVICE
        if(selected==2) {
            uint8_t expected[POINTS_RECORD_SIZE],actual[POINTS_RECORD_SIZE];uint32_t n=0;points_ledger_encode(&points_desired,expected);
            int32_t r=kv->get(kv->context,POINTS_OCCURRENCE_KEY,actual,sizeof(actual),&n);
            if(r!=RISC_BOUND_KEY_VALUE_OK||n!=sizeof(actual)||memcmp(actual,expected,sizeof(actual)))return fail(ALARM_STORAGE);
            points_occ=points_desired;persistence_pending=false;
        } else {
#endif
        uint8_t expected[ALARM_RECORD_SIZE],actual[ALARM_RECORD_SIZE];uint32_t n=0;
        alarm_occurrence_encode(&desired,expected);
        int32_t r=kv->get(kv->context,occ_keys[selected],actual,sizeof(actual),&n);
        if(r!=RISC_BOUND_KEY_VALUE_OK||n!=sizeof(actual)||memcmp(actual,expected,sizeof(actual)))return fail(ALARM_STORAGE);
        occurrences[selected]=desired;persistence_pending=false;
#ifdef POINTS_IN_TIME_SERVICE
        }
#endif
#ifdef ALARM_DND_CONTROL
        if(muting_active) {
            muting_active=false;
            if(error)return fail(error);
            phase=PLAYING;break; /* Keep the original visual timeout/token. */
        }
#endif
        if(desired.state==ALARM_OCC_PENDING) {
            active=true;dismiss=false;alert_started=now_ms();
#ifdef POINTS_IN_TIME_SERVICE
            alert_limit_ms=selected==2?350u:ALARM_INVOCATION_MS;
#else
            alert_limit_ms=ALARM_INVOCATION_MS;
#endif
            next_pulse=alert_started;phase=ACTIVATE_RTC;
        } else {
            active=false;dismiss=false;begin_reconcile();
        }
        break;
    }
    case ACTIVATE_RTC: {
        twatch_rtc_time_v1 t;uint32_t current;
        if(!rtc->read(rtc->context,&t)||t.weekday>6||!alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,&current))return fail(ALARM_RTC);
        uint64_t at=now_ms();
        if(at<sample_ms||at-sample_ms>UINT32_MAX)return fail(ALARM_RTC);
        uint32_t elapsed=(uint32_t)(at-sample_ms)/1000;
        uint64_t delta=current>=seconds?current-seconds:UINT64_MAX;
        if(current<
#ifdef POINTS_IN_TIME_SERVICE
           (selected==2?points_configured.created:config[selected].created)||
#else
           config[selected].created||
#endif
           delta==UINT64_MAX||(delta>elapsed?delta-elapsed:elapsed-delta)>2)return fail(ALARM_RTC);
        seconds=previous_seconds=current;sample_ms=previous_ms=now_ms();
        if(current>=desired.recovery_until||now_ms()-alert_started>=alert_limit_ms)begin_cleanup();
        else {
            uint32_t window_ms=(desired.recovery_until-current)*1000;
            uint32_t elapsed_ms=(uint32_t)(now_ms()-alert_started);
            if(window_ms<alert_limit_ms-elapsed_ms)alert_limit_ms=elapsed_ms+window_ms;
            phase=START_AUDIO;
        }
        break;
    }
    case START_AUDIO: {
        if(dismiss){begin_cleanup();break;}
        /* Cancellation linearizes at this read. It and the first output call
           are one serialized phase, so no foreground writer can interleave.
           This phase performs two dependency calls, plus the optional DND
           preflight read before any new output. */
#ifdef POINTS_IN_TIME_SERVICE
        if(selected==2) {
            uint8_t expected[POINTS_RECORD_SIZE],actual[POINTS_RECORD_SIZE];uint32_t n=0;
            points_config_encode(&points_configured,expected);
            int32_t r=kv->get(kv->context,POINTS_CONFIG_KEY,actual,sizeof(actual),&n);points_config latest;
            if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){latest=points_default_config();points_config_encode(&latest,actual);}
            else if(r!=RISC_BOUND_KEY_VALUE_OK||!points_config_decode(&latest,actual,n))return fail(ALARM_STORAGE);
            if(memcmp(expected,actual,sizeof(actual))){active=false;begin_reconcile();break;}
        } else {
#endif
        uint8_t expected[ALARM_RECORD_SIZE],actual[ALARM_RECORD_SIZE];uint32_t n=0;
        alarm_config_encode(&config[selected],expected);
        int32_t r=kv->get(kv->context,cfg_keys[selected],actual,sizeof(actual),&n);
        if(r!=RISC_BOUND_KEY_VALUE_OK||n!=sizeof(actual))return fail(ALARM_STORAGE);
        alarm_config latest;
        if(!alarm_config_decode(&latest,actual,n,(uint8_t)(selected+1)))return fail(ALARM_STORAGE);
        if(memcmp(expected,actual,sizeof(actual))) {active=false;begin_reconcile();break;}
#ifdef POINTS_IN_TIME_SERVICE
        }
#endif
        if(now_ms()-alert_started>=alert_limit_ms){begin_cleanup();break;}
        if(desired.silenced){phase=PLAYING;break;}
        if((desired.mode&ALARM_MODE_SOUND)
#ifdef ALARM_VOLUME_CONTROL
           &&volume_sound_enabled()
#endif
          ) {
            audio_uncertain=true;
            if(!audio->open(audio->context,8000,1)){error=ALARM_OUTPUT;begin_cleanup();break;}
#if defined(POINTS_IN_TIME_SERVICE) && !defined(ALARM_VOLUME_CONTROL)
            if(selected==2) {
                int16_t pcm[256];for(unsigned i=0;i<256;i++)pcm[i]=(i&4)?1600:-1600;
                if(!audio->write(audio->context,pcm,256)){error=ALARM_OUTPUT;begin_cleanup();break;}
            }
#endif
#ifdef ALARM_VOLUME_CONTROL
            phase=SET_AUDIO_GAIN;break;
#endif
            phase=desired.mode&ALARM_MODE_VIBRATE?START_HAPTIC:PLAYING;
        } else {
            if(desired.mode&ALARM_MODE_VIBRATE) {
                haptic_uncertain=true;
                if(!haptic->effect(haptic->context,47)){error=ALARM_OUTPUT;begin_cleanup();break;}
            }
            phase=PLAYING;
        }
        break;
    }
#ifdef ALARM_VOLUME_CONTROL
    case SET_AUDIO_GAIN:
        if(dismiss||now-alert_started>=alert_limit_ms){begin_cleanup();break;}
        /* Speaker gain survives close/open. Keep the shared device at unity;
         * alarm volume scales full-range source PCM, never another app's gain. */
        if(!audio->set_gain(audio->context,
                           100,100)){error=ALARM_OUTPUT;begin_cleanup();break;}
#ifdef POINTS_IN_TIME_SERVICE
        if(selected==2) {
            int16_t peak=(int16_t)(ALARM_VOLUME_PCM_PEAK*staged_volume/100u);
            int16_t pcm[256];for(unsigned i=0;i<256;i++)pcm[i]=(i&4)?peak:-peak;
            if(!audio->write(audio->context,pcm,256)){error=ALARM_OUTPUT;begin_cleanup();break;}
        }
#endif
        phase=desired.mode&ALARM_MODE_VIBRATE?START_HAPTIC:PLAYING;break;
#endif
    case START_HAPTIC:
        if(dismiss){begin_cleanup();break;}
        if(desired.silenced){phase=PLAYING;break;}
        if(desired.mode&ALARM_MODE_VIBRATE) {
            haptic_uncertain=true;
            if(!haptic->effect(haptic->context,47)){error=ALARM_OUTPUT;begin_cleanup();break;}
        }
        phase=PLAYING;break;
    case PLAYING:
        if(dismiss||now-alert_started>=alert_limit_ms){begin_cleanup();break;}
        if(desired.silenced)break;
#ifdef POINTS_IN_TIME_SERVICE
        if(selected==2)break;
#endif
        if(now>=next_pulse) {
            next_pulse=now+500;
            if((desired.mode&ALARM_MODE_SOUND)
#ifdef ALARM_VOLUME_CONTROL
           &&volume_sound_enabled()
#endif
          ) {
                int16_t pcm[256];for(unsigned i=0;i<256;i++)pcm[i]=(i&4)?
#ifdef ALARM_VOLUME_CONTROL
                    (int16_t)(ALARM_VOLUME_PCM_PEAK*staged_volume/100u):-(int16_t)(ALARM_VOLUME_PCM_PEAK*staged_volume/100u);
#else
                    1600:-1600;
#endif
                if(!audio->write(audio->context,pcm,256)){error=ALARM_OUTPUT;begin_cleanup();}
                else if(desired.mode&ALARM_MODE_VIBRATE)phase=START_HAPTIC;
            } else if(desired.mode&ALARM_MODE_VIBRATE) {
                if(!haptic->effect(haptic->context,47)){error=ALARM_OUTPUT;begin_cleanup();}
            }
        }
        break;
    case CLEAN_HAPTIC:
        if(haptic->stop(haptic->context))haptic_uncertain=false;
        else {haptic_uncertain=true;cleanup_failed=true;}
        phase=CLEAN_SILENCE;break;
    case CLEAN_SILENCE:
        /* silence is advisory; only close confirms physical ownership ended. */
        (void)audio->silence(audio->context);phase=CLEAN_AUDIO;break;
    case CLEAN_AUDIO:
        if(audio->close(audio->context))audio_uncertain=false;
        else {audio_uncertain=true;cleanup_failed=true;}
        if(cleanup_failed)return fail(ALARM_OUTPUT);
        if(error==ALARM_OUTPUT&&!dismiss)return fail(ALARM_OUTPUT);
#ifdef ALARM_DND_CONTROL
        if(muting_active&&!dismiss){persistence_pending=true;phase=WRITE_OCC;break;}
        muting_active=false;
#endif
        error=0;
#ifdef POINTS_IN_TIME_SERVICE
        desired.state=selected==2?ALARM_OCC_ACKED:(dismiss?ALARM_OCC_ACKED:ALARM_OCC_EXPIRED);
#else
        desired.state=dismiss?ALARM_OCC_ACKED:ALARM_OCC_EXPIRED;
#endif
        persistence_pending=true;phase=WRITE_OCC;break;
    case BLOCKED:return error;
    }
    return ALARM_OK;
}
static int32_t step(void *c) {
    (void)c;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
    in_call=true;sleep_ticket_valid=false;int32_t result=do_step();update_view();in_call=false;return result;
}
static int32_t status(void *c,alarm_status_v1 *out) {
    (void)c;if(!started||!out||out->struct_size<sizeof(*out))return ALARM_INVALID;
    if(in_call)return ALARM_BUSY;
    in_call=true;update_view();*out=view;in_call=false;return ALARM_OK;
}
static int32_t refresh(void *c) {
    (void)c;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
    sleep_ticket_valid=false;
    if(!haptic_uncertain&&!audio_uncertain&&(!active||
       (!dismiss&&desired.state==ALARM_OCC_PENDING&&(phase==ACTIVATE_RTC||phase==START_AUDIO||phase==BLOCKED)))) {
        /* No physical output began. Reconcile the latest durable schedule,
           including a writer reset that persisted before its refresh hint. */
        if(error==ALARM_RTC)anchored=false;
        active=false;begin_reconcile();
    } else if(phase==BLOCKED) {
        if(foreground_failed&&!dismiss)return ALARM_OUTPUT;
        foreground_failed=false;begin_cleanup();
    }
    else refreshed=true;
    return ALARM_PENDING;
}
static int32_t acknowledge(void *c,const alarm_token_v1 *value) {
    (void)c;if(!started||!value)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
    sleep_ticket_valid=false;
    if(!active) {
        for(unsigned i=0;i<2;i++)if(occurrences[i].state==ALARM_OCC_ACKED&&alarm_same_occurrence(&occurrences[i],&config[i])) {
            alarm_token_v1 confirmed=token(&occurrences[i]);
            if(alarm_token_equal(value,&confirmed))return ALARM_OK;
        }
#ifdef POINTS_IN_TIME_SERVICE
        if(points_occ.state==ALARM_OCC_ACKED&&points_occ.revision==points_configured.revision) {
            alarm_token_v1 confirmed={points_token_kind(points_occ.slot,points_occ.edge),points_occ.revision,points_occ.deadline,points_occ.generation};
            if(alarm_token_equal(value,&confirmed))return ALARM_OK;
        }
#endif
        return ALARM_STALE;
    }
    alarm_token_v1 current=token(&desired);
    if(!alarm_token_equal(value,&current))return ALARM_STALE;
    dismiss=true;foreground_failed=false;if(phase==BLOCKED){error=0;begin_cleanup();}return ALARM_PENDING;
}
static int32_t prepare_sleep_internal(alarm_sleep_v1 *out) {
    if(active||haptic_uncertain||audio_uncertain||phase==BLOCKED)return error?error:ALARM_PENDING;
    if(!sleep_waiting){sleep_snapshot=view.snapshot;sleep_waiting=true;begin_reconcile();return ALARM_PENDING;}
    if(phase!=IDLE||view.snapshot==sleep_snapshot)return ALARM_PENDING;
    if(now_ms()-sample_ms>100){begin_reconcile();return ALARM_PENDING;}
    uint32_t deadline=0;
    for(unsigned i=0;i<2;i++)if(config[i].enabled&&config[i].deadline>seconds&&
        (!alarm_same_occurrence(&occurrences[i],&config[i])||occurrences[i].state==ALARM_OCC_PENDING)) {
        if(!deadline||config[i].deadline<deadline)deadline=config[i].deadline;
    }
#ifdef POINTS_IN_TIME_SERVICE
    if(points_configured.revision) {
        points_ledger ledger=points_current_ledger();uint32_t day;
        if(ledger.state==ALARM_OCC_PENDING&&ledger.deadline>seconds&&(!deadline||ledger.deadline<deadline))deadline=ledger.deadline;
        if(!points_local_day(seconds,&day))return ALARM_RTC;
        for(unsigned slot=0;slot<POINTS_MAX;slot++)for(unsigned edge=0;edge<POINTS_EDGE_COUNT;edge++) {
            if(!points_notice_enabled(&points_configured.points[slot],edge))continue;
            uint32_t first=day>1?day-1:day;
            for(unsigned offset=0;offset<=8;offset++) {
                uint32_t parent=first+offset;if(points_ledger_handled(&ledger,slot,parent,edge))continue;
                points_event event={0};if(points_event_for_day(&points_configured,slot,parent,edge,&event,NULL)&&
                   event.deadline>seconds&&(!deadline||event.deadline<deadline))deadline=event.deadline;
            }
        }
    }
#endif
    *out=(alarm_sleep_v1){sizeof(*out),view.snapshot,seconds,deadline};
    sleep_ticket=*out;sleep_ticket_valid=true;sleep_waiting=false;return ALARM_OK;
}
static int32_t prepare_sleep(void *c,alarm_sleep_v1 *out) {
    (void)c;if(!started||!out||out->struct_size<sizeof(*out))return ALARM_INVALID;
    if(in_call)return ALARM_BUSY;
    in_call=true;sleep_ticket_valid=false;int32_t result=prepare_sleep_internal(out);in_call=false;return result;
}
static int32_t resume_sleep(void *c,const alarm_sleep_v1 *decision) {
    (void)c;if(!started||!decision||decision->struct_size<sizeof(*decision))return ALARM_INVALID;
    if(in_call)return ALARM_BUSY;
    if(!sleep_ticket_valid||phase!=IDLE||active||haptic_uncertain||audio_uncertain||
       decision->snapshot!=sleep_ticket.snapshot||decision->rtc_seconds!=sleep_ticket.rtc_seconds||
       decision->deadline!=sleep_ticket.deadline)return ALARM_STALE;
    in_call=true;sleep_ticket_valid=false;
    twatch_rtc_time_v1 t;uint32_t current;int32_t result=ALARM_OK;
    if(!rtc->read(rtc->context,&t)||t.weekday>6||
       !alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,&current))result=fail(ALARM_RTC);
    else {
        uint64_t at=now_ms();
        /* Light sleep preserves this service but may estimate elapsed monotonic
           time from a different oscillator. Only this explicit successful-sleep
           boundary discards their cross-sleep rate comparison. Calendar bounds
           and backward RTC/monotonic rejection remain mandatory. */
        if(!anchored||current<sleep_ticket.rtc_seconds||at<previous_ms)result=fail(ALARM_RTC);
        else {
            seconds=previous_seconds=current;sample_ms=previous_ms=at;
            begin_reconcile();update_view();
        }
    }
    in_call=false;return result;
}
static int32_t stop_only_internal(void) {
    if(!active&&!haptic_uncertain&&!audio_uncertain) {
        phase=BLOCKED;error=ALARM_FOREGROUND;foreground_failed=true;return ALARM_OK;
    }
    if(!foreground_failed) {
        foreground_failed=true;begin_cleanup();error=ALARM_FOREGROUND;
    }
    switch(phase) {
    case CLEAN_HAPTIC:
        if(haptic->stop(haptic->context))haptic_uncertain=false;
        else {haptic_uncertain=true;cleanup_failed=true;}
        phase=CLEAN_SILENCE;return ALARM_PENDING;
    case CLEAN_SILENCE:
        (void)audio->silence(audio->context);phase=CLEAN_AUDIO;return ALARM_PENDING;
    case CLEAN_AUDIO:
        if(audio->close(audio->context))audio_uncertain=false;
        else {audio_uncertain=true;cleanup_failed=true;}
        phase=BLOCKED;
        error=cleanup_failed?ALARM_OUTPUT:ALARM_FOREGROUND;
        return cleanup_failed?ALARM_OUTPUT:ALARM_OK;
    default:
        return haptic_uncertain||audio_uncertain?ALARM_OUTPUT:ALARM_OK;
    }
}
static int32_t stop_only(void *c) {
    (void)c;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
    in_call=true;sleep_ticket_valid=false;int32_t result=stop_only_internal();update_view();in_call=false;return result;
}
static bool quiesce(void) {
    if(in_call)return false;
    in_call=true;started=false;sleep_ticket_valid=false;
    bool h=!haptic||haptic->stop(haptic->context);
    if(audio)(void)audio->silence(audio->context);
    bool a=!audio||audio->close(audio->context);
    haptic_uncertain=!h;audio_uncertain=!a;in_call=false;
    if(!h||!a)return false;
    /* Storage authority has already been revoked. Never write during cleanup. */
    started=false;kv=NULL;rtc=NULL;haptic=NULL;audio=NULL;clock_api=NULL;return true;
}
static bool start(const risc_provider_dependency_v1 *deps,size_t count) {
    if(started||in_call||haptic_uncertain||audio_uncertain||!deps||count!=5)return false;
    kv=dependency(deps,count,"storage.key-value.bound",1,sizeof(*kv));
    clock_api=dependency(deps,count,"platform.clock",1,sizeof(*clock_api));
    rtc=dependency(deps,count,"rtc.clock",2,sizeof(*rtc));
    haptic=dependency(deps,count,"haptic.effect",1,sizeof(*haptic));
    audio=dependency(deps,count,"audio.output",1,sizeof(*audio));
    if(!kv||!kv->get||!kv->put||!clock_api||!clock_api->monotonic_ms||!rtc||!rtc->read||
       !haptic||!haptic->effect||!haptic->stop||!audio||!audio->open||!audio->write||!audio->silence||!audio->close
#ifdef ALARM_VOLUME_CONTROL
       ||!audio->set_gain
#endif
       ) {
        kv=NULL;clock_api=NULL;rtc=NULL;haptic=NULL;audio=NULL;return false;
    }
#ifdef POINTS_IN_TIME_SERVICE
    memset(&points_configured,0,sizeof(points_configured));memset(&points_occ,0,sizeof(points_occ));
#endif
    memset(config,0,sizeof(config));memset(occurrences,0,sizeof(occurrences));memset(&view,0,sizeof(view));
    memset(&desired,0,sizeof(desired));
    active=dismiss=anchored=sleep_waiting=sleep_ticket_valid=persistence_pending=false;
    memset(&sleep_ticket,0,sizeof(sleep_ticket));
#ifdef ALARM_DND_CONTROL
    staged_dnd=muting_active=false;
#endif
    seconds=0;staged_mode=ALARM_MODE_VIBRATE;started=true;begin_reconcile();return true;
}
static void stop(void) {(void)quiesce();}
static const alarm_service_sleep_v1 api={{1,sizeof(api),NULL,status,step,refresh,acknowledge,prepare_sleep,stop_only},resume_sleep};
static const risc_driver_v2 driver={2,sizeof(driver),"alarm-service",ALARM_SERVICE_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi) {return abi==2?&driver:NULL;}
