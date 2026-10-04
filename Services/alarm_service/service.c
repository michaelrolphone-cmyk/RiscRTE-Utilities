/* Ordinary singleton ELF. All policy/persistence/UI-facing state belongs here;
 * Runtime only resolves opaque dependencies. No poll callback, task or ISR. */
#include "AlarmRecords.h"
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
               BLOCKED } phase_t;
static const risc_bound_key_value_v1 *kv;
static const risc_platform_clock_api_v1 *clock_api;
static const twatch_rtc_api_v1 *rtc;
static const twatch_haptic_api_v1 *haptic;
static const twatch_audio_out_api_v1 *audio;
static alarm_config config[2], staging[2];
static alarm_occurrence occurrences[2], staged_occ[2], desired;
static alarm_status_v1 view;
static phase_t phase;
static uint32_t staged_mode, seconds, previous_seconds, sleep_snapshot, alert_limit_ms;
static uint64_t previous_ms, sample_ms, next_reconcile, alert_started, next_pulse;
static bool started, in_call, refreshed, anchored, sleep_waiting;
static bool active, dismiss, haptic_uncertain, audio_uncertain, cleanup_failed;
static bool persistence_pending, foreground_failed;
static unsigned selected;
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
    view.state=phase==BLOCKED?ALARM_STATE_BLOCKED:active?(dismiss?ALARM_STATE_DISMISSING:ALARM_STATE_ALERT):
        phase==IDLE?ALARM_STATE_READY:ALARM_STATE_LOADING;
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
    if(active) {
        view.occurrence=token(&desired);view.mode=desired.mode;view.recovery_until=desired.recovery_until;
        uint64_t elapsed=now_ms()-alert_started;
        view.remaining_ms=elapsed<alert_limit_ms?(uint32_t)(alert_limit_ms-elapsed):0;
        const char *name=desired.kind==ALARM_KIND_ALARM?"ALARM":"COUNTDOWN FINISHED";
        memset(view.label,0,sizeof(view.label));memcpy(view.label,name,strlen(name));
    } else {
        memset(&view.occurrence,0,sizeof(view.occurrence));view.recovery_until=view.remaining_ms=0;
        memset(view.label,0,sizeof(view.label));
    }
}
static int32_t fail(int32_t e) { error=e;phase=BLOCKED;sleep_waiting=false;update_view();return e; }
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
    if(seconds>=desired.recovery_until)desired.state=ALARM_OCC_EXPIRED;
    persistence_pending=true;phase=WRITE_OCC;
}
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
    if(candidate>=0)prepare_occurrence((unsigned)candidate,alarm_same_occurrence(&occurrences[candidate],&config[candidate]));
    else {phase=IDLE;next_reconcile=now_ms()+1000;}
    return error?error:ALARM_OK;
}
static int32_t do_step(void) {
    if(foreground_failed)return error?error:ALARM_FOREGROUND;
    uint64_t now=now_ms();
    if(active&&(phase==ACTIVATE_RTC||phase==START_AUDIO||phase==START_HAPTIC||phase==PLAYING)&&
       (dismiss||now-alert_started>=alert_limit_ms))begin_cleanup();
    switch(phase) {
    case LOAD_ALARM: if(!read_config(0))return fail(ALARM_STORAGE);phase=LOAD_TIMER;break;
    case LOAD_TIMER: if(!read_config(1))return fail(ALARM_STORAGE);phase=LOAD_MODE;break;
    case LOAD_MODE: {
        uint8_t b=0;uint32_t n=0;int32_t r=kv->get(kv->context,ALARM_MODE_KEY,&b,1,&n);
        if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND)staged_mode=ALARM_MODE_VIBRATE;
        else if(r!=RISC_BOUND_KEY_VALUE_OK||n!=1||b<1||b>3)return fail(ALARM_STORAGE);
        else staged_mode=b;
        phase=LOAD_ALARM_OCC;break;
    }
    case LOAD_ALARM_OCC:if(!read_occurrence(0))return fail(ALARM_STORAGE);phase=LOAD_TIMER_OCC;break;
    case LOAD_TIMER_OCC:if(!read_occurrence(1))return fail(ALARM_STORAGE);phase=READ_RTC;break;
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
        uint8_t b[ALARM_RECORD_SIZE];alarm_occurrence_encode(&desired,b);
        /* Even IO may have persisted. Always verify before deciding next action. */
        (void)kv->put(kv->context,occ_keys[selected],b,sizeof(b));phase=VERIFY_OCC;break;
    }
    case VERIFY_OCC: {
        uint8_t expected[ALARM_RECORD_SIZE],actual[ALARM_RECORD_SIZE];uint32_t n=0;
        alarm_occurrence_encode(&desired,expected);
        int32_t r=kv->get(kv->context,occ_keys[selected],actual,sizeof(actual),&n);
        if(r!=RISC_BOUND_KEY_VALUE_OK||n!=sizeof(actual)||memcmp(actual,expected,sizeof(actual)))return fail(ALARM_STORAGE);
        occurrences[selected]=desired;persistence_pending=false;
        if(desired.state==ALARM_OCC_PENDING) {
            active=true;dismiss=false;alert_started=now_ms();alert_limit_ms=ALARM_INVOCATION_MS;next_pulse=alert_started;phase=ACTIVATE_RTC;
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
        if(current<config[selected].created||delta==UINT64_MAX||(delta>elapsed?delta-elapsed:elapsed-delta)>2)return fail(ALARM_RTC);
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
           This phase alone may perform two dependency calls. */
        uint8_t expected[ALARM_RECORD_SIZE],actual[ALARM_RECORD_SIZE];uint32_t n=0;
        alarm_config_encode(&config[selected],expected);
        int32_t r=kv->get(kv->context,cfg_keys[selected],actual,sizeof(actual),&n);
        if(r!=RISC_BOUND_KEY_VALUE_OK||n!=sizeof(actual))return fail(ALARM_STORAGE);
        alarm_config latest;
        if(!alarm_config_decode(&latest,actual,n,(uint8_t)(selected+1)))return fail(ALARM_STORAGE);
        if(memcmp(expected,actual,sizeof(actual))) {active=false;begin_reconcile();break;}
        if(now_ms()-alert_started>=alert_limit_ms){begin_cleanup();break;}
        if(desired.mode&ALARM_MODE_SOUND) {
            audio_uncertain=true;
            if(!audio->open(audio->context,8000,1)){error=ALARM_OUTPUT;begin_cleanup();break;}
            phase=desired.mode&ALARM_MODE_VIBRATE?START_HAPTIC:PLAYING;
        } else {
            haptic_uncertain=true;
            if(!haptic->effect(haptic->context,47)){error=ALARM_OUTPUT;begin_cleanup();break;}
            phase=PLAYING;
        }
        break;
    }
    case START_HAPTIC:
        if(dismiss){begin_cleanup();break;}
        if(desired.mode&ALARM_MODE_VIBRATE) {
            haptic_uncertain=true;
            if(!haptic->effect(haptic->context,47)){error=ALARM_OUTPUT;begin_cleanup();break;}
        }
        phase=PLAYING;break;
    case PLAYING:
        if(dismiss||now-alert_started>=alert_limit_ms){begin_cleanup();break;}
        if(now>=next_pulse) {
            next_pulse=now+500;
            if(desired.mode&ALARM_MODE_SOUND) {
                int16_t pcm[256];for(unsigned i=0;i<256;i++)pcm[i]=(i&4)?1600:-1600;
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
        error=0;desired.state=dismiss?ALARM_OCC_ACKED:ALARM_OCC_EXPIRED;
        persistence_pending=true;phase=WRITE_OCC;break;
    case BLOCKED:return error;
    }
    return ALARM_OK;
}
static int32_t step(void *c) {
    (void)c;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
    in_call=true;int32_t result=do_step();update_view();in_call=false;return result;
}
static int32_t status(void *c,alarm_status_v1 *out) {
    (void)c;if(!started||!out||out->struct_size<sizeof(*out))return ALARM_INVALID;
    if(in_call)return ALARM_BUSY;
    in_call=true;update_view();*out=view;in_call=false;return ALARM_OK;
}
static int32_t refresh(void *c) {
    (void)c;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
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
    if(!active) {
        for(unsigned i=0;i<2;i++)if(occurrences[i].state==ALARM_OCC_ACKED&&alarm_same_occurrence(&occurrences[i],&config[i])) {
            alarm_token_v1 confirmed=token(&occurrences[i]);
            if(alarm_token_equal(value,&confirmed))return ALARM_OK;
        }
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
    *out=(alarm_sleep_v1){sizeof(*out),view.snapshot,seconds,deadline};sleep_waiting=false;return ALARM_OK;
}
static int32_t prepare_sleep(void *c,alarm_sleep_v1 *out) {
    (void)c;if(!started||!out||out->struct_size<sizeof(*out))return ALARM_INVALID;
    if(in_call)return ALARM_BUSY;
    in_call=true;int32_t result=prepare_sleep_internal(out);in_call=false;return result;
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
    in_call=true;int32_t result=stop_only_internal();update_view();in_call=false;return result;
}
static bool quiesce(void) {
    if(in_call)return false;
    in_call=true;started=false;
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
       !haptic||!haptic->effect||!haptic->stop||!audio||!audio->open||!audio->write||!audio->silence||!audio->close) {
        kv=NULL;clock_api=NULL;rtc=NULL;haptic=NULL;audio=NULL;return false;
    }
    memset(config,0,sizeof(config));memset(occurrences,0,sizeof(occurrences));memset(&view,0,sizeof(view));
    memset(&desired,0,sizeof(desired));
    active=dismiss=anchored=sleep_waiting=persistence_pending=false;
    seconds=0;staged_mode=ALARM_MODE_VIBRATE;started=true;begin_reconcile();return true;
}
static void stop(void) {(void)quiesce();}
static const alarm_service_v1 api={1,sizeof(api),NULL,status,step,refresh,acknowledge,prepare_sleep,stop_only};
static const risc_driver_v2 driver={2,sizeof(driver),"alarm-service",ALARM_SERVICE_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi) {return abi==2?&driver:NULL;}
