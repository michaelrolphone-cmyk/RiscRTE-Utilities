#ifndef UTILITIES_POINTS_UTC_SCHEDULE_H
#define UTILITIES_POINTS_UTC_SCHEDULE_H
/* POINTS_UTC_PROJECTION_ONLY exposes pure recurrence for read-only adapters using
 * copied, explicitly normalized absolute fields. It does not select record keys,
 * narrow the legacy record range, or authorize persistence of converted records.
 * Pure opt-in hooks for service and future Points writer/face integration.
 * All absolute fields are UTC seconds since 2000, never raw RTC wall time.
 * The caller supplies a resolved rule or explicit conversion callbacks; no implicit I/O. */
#if !defined(ALARM_NATIVE_UTC) && !defined(POINTS_UTC_PROJECTION_ONLY)
#error "PointsUtcSchedule requires a native UTC record or explicit read-only projection profile"
#endif
#include "PointsSchedule.h"
#include "PortableTimeZone.h"
/* Optional pure conversion seam for provider-owned civil history. The caller
 * owns callback lifetime and terminal-error fencing; NULL keeps the existing
 * frozen-rule native-UTC behavior unchanged. Epochs are Unix UTC seconds. */
typedef struct {
    void *context;
    bool (*project)(void *,int64_t,portable_timezone_civil *);
    int32_t (*inverse)(void *,const portable_timezone_civil *,int64_t [2],uint32_t *);
} points_utc_projection_clock;
static inline bool points_utc_local_day_clock(const portable_timezone_rule *rule,uint32_t raw,uint32_t *day,
                                              const points_utc_projection_clock *clock) {
    portable_timezone_civil local;uint32_t seconds;
    if(!day||raw>ALARM_RTC_MAX)return false;
    bool ok=clock&&clock->project?clock->project(clock->context,INT64_C(946684800)+raw,&local):
        rule&&portable_timezone_utc_to_local(rule,INT64_C(946684800)+raw,&local,NULL)==PORTABLE_TIMEZONE_OK;
    if(!ok||local.year<2000||local.year>2099||!alarm_calendar_seconds((uint16_t)local.year,local.month,local.day,0,0,0,&seconds))return false;
    *day=seconds/86400+1;return true;
}
/* Checked domain boundary for future native-UTC writers. The format retains
 * uint32 seconds since 2000, with native Unix INT32_MAX as its upper bound. */
static inline bool points_utc_from_unix(int64_t epoch,uint32_t *out) {
    if(!out||epoch<INT64_C(946684800)||epoch>INT32_MAX)return false;
    *out=(uint32_t)(epoch-INT64_C(946684800));return true;
}
static inline bool points_utc_to_unix(uint32_t seconds,int64_t *out) {
    if(!out||seconds>ALARM_RTC_MAX)return false;
    *out=INT64_C(946684800)+seconds;return true;
}
static inline bool points_utc_local_day(const portable_timezone_rule *rule,uint32_t raw,uint32_t *day) {
    return points_utc_local_day_clock(rule,raw,day,NULL);
}
static inline bool points_utc_notice_enabled(const points_item *p,unsigned edge) {
    if(!p||!p->enabled||edge>=POINTS_EDGE_COUNT)return false;
    if(edge==POINTS_EDGE_START)return true;
    if(edge==POINTS_EDGE_END)return p->duration_minutes&&p->notify_end;
    return p->duration_minutes>=3&&p->warn3;
}
/* End remains a projection edge even when notify_end is disabled. Warning is
 * service-only and occurs exactly three elapsed minutes before the duration end. */
static inline bool points_utc_event_for_day_clock(const portable_timezone_rule *rule,const points_config *c,unsigned slot,uint32_t parent_day,
                                        unsigned edge,points_event *out,uint32_t *flags,uint32_t maximum,const points_utc_projection_clock *clock) {
    if(!c||!out||maximum>ALARM_RTC_MAX||maximum<ALARM_RECOVERY_SECONDS||slot>=POINTS_MAX||edge>=POINTS_EDGE_COUNT||!parent_day||parent_day>36525)return false;
    const points_item *p=&c->points[slot];twatch_rtc_time_v1 civil;
    if(!p->enabled||!points_calendar((parent_day-1)*86400,&civil)||!(p->weekdays&(1u<<civil.weekday))||
       ((edge==POINTS_EDGE_END||edge==POINTS_EDGE_WARNING)&&!p->duration_minutes)||
       (edge==POINTS_EDGE_WARNING&&!p->warn3))return false;
    civil.hour=p->hour;civil.minute=p->minute;civil.second=0;
    portable_timezone_civil local={civil.year,civil.month,civil.day,civil.hour,civil.minute,0,civil.weekday};
    int64_t epochs[2]={0};uint32_t count=0;int result=PORTABLE_TIMEZONE_OK;
    if(clock&&clock->inverse) {
        result=clock->inverse(clock->context,&local,epochs,&count);
        if(result==0)result=count==2?PORTABLE_TIMEZONE_FOLD:count==0?PORTABLE_TIMEZONE_GAP:PORTABLE_TIMEZONE_OK;
    } else {
        portable_timezone_inverse candidates={0};
        result=portable_timezone_local_to_utc(rule,&local,&candidates);
        count=candidates.count;
        if(count==1)epochs[0]=candidates.candidate[0].epoch;
    }
    if(result!=PORTABLE_TIMEZONE_OK||count!=1) {
        if(flags)*flags|=result==PORTABLE_TIMEZONE_FOLD?POINTS_FLAG_FOLD:
            result==PORTABLE_TIMEZONE_GAP?POINTS_FLAG_GAP:POINTS_FLAG_RANGE;
        return false;
    }
    int64_t raw=epochs[0]-INT64_C(946684800);
    if(raw<0||raw>(int64_t)maximum-ALARM_RECOVERY_SECONDS-(uint32_t)p->duration_minutes*60) {
        if(flags)*flags|=POINTS_FLAG_RANGE;
        return false;
    }
    uint32_t start=(uint32_t)raw;
    if(start<=c->created)return false;
    uint32_t deadline=start;
    if(edge==POINTS_EDGE_END)deadline+=(uint32_t)p->duration_minutes*60;
    else if(edge==POINTS_EDGE_WARNING)deadline+=(uint32_t)p->duration_minutes*60-180u;
    *out=(points_event){deadline,parent_day,(uint8_t)slot,(uint8_t)edge,p->kind,p->mode};return true;
}
static inline bool points_utc_event_for_day_bounded(const portable_timezone_rule *rule,const points_config *c,unsigned slot,uint32_t parent_day,
                                        unsigned edge,points_event *out,uint32_t *flags,uint32_t maximum) {
    return points_utc_event_for_day_clock(rule,c,slot,parent_day,edge,out,flags,maximum,NULL);
}
/* Default service profile retains its original absolute upper bound. */
static inline bool points_utc_event_for_day(const portable_timezone_rule *rule,const points_config *c,unsigned slot,uint32_t parent_day,
                                        unsigned edge,points_event *out,uint32_t *flags) {
    return points_utc_event_for_day_bounded(rule,c,slot,parent_day,edge,out,flags,ALARM_RTC_MAX);
}
/* A read-only legacy adapter may supply its provider-interpreted record ceiling.
 * The same ceiling applies to start + elapsed duration + recovery, not just the
 * displayed edge; this preserves range semantics for starts and ends together. */
static inline bool points_utc_project_clock(const portable_timezone_rule *rule,const points_config *c,uint32_t now,points_projection *out,uint32_t maximum,const points_utc_projection_clock *clock) {
    if(!c||!out||maximum>ALARM_RTC_MAX)return false;
    memset(out,0,sizeof(*out));uint32_t today;
    if(!c->revision)return true;
    if(!points_config_valid(c)||now>maximum||!points_utc_local_day_clock(rule,now,&today,clock)){out->flags=POINTS_FLAG_RANGE;return false;}
    for(int offset=-7;offset<=8;offset++) {
        int day=(int)today+offset;if(day<1||day>36525)continue;
        for(unsigned slot=0;slot<POINTS_MAX;slot++)for(unsigned edge=0;edge<2;edge++) {
            points_event e;if(!points_utc_event_for_day_clock(rule,c,slot,(uint32_t)day,edge,&e,offset>=0?&out->flags:NULL,maximum,clock))continue;
            if(e.deadline<=now){if(!out->has_previous||points_event_before(&out->previous,&e)){out->previous=e;out->has_previous=true;}}
            else{unsigned at=0;while(at<out->count&&!points_event_before(&e,&out->next[at]))at++;
                if(at<POINTS_NEXT_COUNT){unsigned n=out->count<POINTS_NEXT_COUNT?out->count++:POINTS_NEXT_COUNT-1;while(n>at){out->next[n]=out->next[n-1];n--;}out->next[at]=e;}}
        }
    }return true;
}
static inline bool points_utc_project_bounded(const portable_timezone_rule *rule,const points_config *c,uint32_t now,points_projection *out,uint32_t maximum) {
    return points_utc_project_clock(rule,c,now,out,maximum,NULL);
}
static inline bool points_utc_project(const portable_timezone_rule *rule,const points_config *c,uint32_t now,points_projection *out) {
    return points_utc_project_bounded(rule,c,now,out,ALARM_RTC_MAX);
}
static inline bool points_utc_latest_for_edge(const portable_timezone_rule *rule,const points_config *c,const points_ledger *l,uint32_t now,
                                          unsigned slot,unsigned edge,points_event *out) {
    uint32_t today;if(!c||!c->revision||!l||!out||slot>=POINTS_MAX||edge>=POINTS_EDGE_COUNT||
       !points_utc_notice_enabled(&c->points[slot],edge)||!points_utc_local_day(rule,now,&today))return false;
    bool found=false;
    for(int offset=-8;offset<=0;offset++) {
        int day=(int)today+offset;if(day<1||points_ledger_handled(l,slot,(uint32_t)day,edge))continue;
        points_event e;if(points_utc_event_for_day(rule,c,slot,(uint32_t)day,edge,&e,NULL)&&e.deadline<=now&&(!found||points_event_before(out,&e))){*out=e;found=true;}
    }return found;
}
static inline bool points_utc_due(const portable_timezone_rule *rule,const points_config *c,const points_ledger *l,uint32_t now,points_event *out) {
    if(!c||!l||!out)return false;
    bool found=false;
    for(unsigned slot=0;slot<POINTS_MAX;slot++)for(unsigned edge=0;edge<POINTS_EDGE_COUNT;edge++) {
        points_event latest;if(points_utc_latest_for_edge(rule,c,l,now,slot,edge,&latest)&&(!found||points_event_before(&latest,out))){*out=latest;found=true;}
    }return found;
}
#endif
