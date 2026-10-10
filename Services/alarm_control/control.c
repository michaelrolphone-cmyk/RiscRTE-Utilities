#include "AlarmControlV1.h"
#include "AlarmRecords.h"
#include "AlarmServiceV2.h"
#include "AlarmVolume.h"
#include "RiscProviderV2.h"
#include "RiscBoundKeyValueV1.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef ALARM_NATIVE_UTC
#include "RiscPlatformRealtimeV1.h"
#include "PortableTimeZonePreference.h"
#define CLOCK_POLICY 2u
#define SERVICE_VERSION 2u
#else
#include "PortableRtcClock.h"
#include "PortableTime.h"
#define CLOCK_POLICY 1u
#define SERVICE_VERSION 1u
#endif
#ifndef ALARM_CONTROL_ID
#define ALARM_CONTROL_ID "alarm-control"
#endif
static const risc_bound_key_value_v1 *store;
static const alarm_service_v1 *scheduler;
#ifdef ALARM_NATIVE_UTC
static const risc_platform_realtime_api_v1 *realtime;
static portable_timezone_rule zone_rule;
#else
static const twatch_rtc_api_v1 *rtc;
#endif
static bool started,retained;
static uint32_t hash(const uint8_t *b,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++)h=(h^b[i])*16777619u;return h;}
static int32_t fence(void){retained=true;return ALARM_CONTROL_RETAINED;}
static int32_t valid_call(void){return retained?ALARM_CONTROL_RETAINED:started?ALARM_CONTROL_OK:ALARM_CONTROL_INVALID;}
static int32_t get(const char *key,void *bytes,uint32_t capacity,uint32_t *size){
    if(retained)return RISC_BOUND_KEY_VALUE_CONTEXT;
    int32_t rc=store->get(store->context,key,bytes,capacity,size);
    if(rc==RISC_BOUND_KEY_VALUE_CONTEXT)retained=true;
    return rc;
}
static int32_t service_status(alarm_status_v1 *out){
    *out=(alarm_status_v1){.struct_size=sizeof(*out)};
    int32_t rc=scheduler->status(scheduler->context,out);
    if(rc==ALARM_RETAINED||out->error==ALARM_RETAINED)return fence();
    if(rc!=ALARM_OK||out->api_version!=1||out->struct_size!=sizeof(*out))return ALARM_CONTROL_BLOCKED;
    return ALARM_CONTROL_OK;
}
static int32_t read_config(alarm_config *config,uint8_t bytes[32],uint32_t *size){
    memset(bytes,0,32);*size=0;
    int32_t rc=get(ALARM_CONFIG_KEY,bytes,32,size);
    if(retained)return ALARM_CONTROL_RETAINED;
    if(rc==RISC_BOUND_KEY_VALUE_NOT_FOUND){*config=(alarm_config){.kind=ALARM_KIND_ALARM};*size=0;return ALARM_CONTROL_OK;}
    if(rc!=RISC_BOUND_KEY_VALUE_OK||!alarm_config_decode(config,bytes,*size,ALARM_KIND_ALARM))return ALARM_CONTROL_STORAGE;
    return ALARM_CONTROL_OK;
}
#ifndef ALARM_NATIVE_UTC
static bool from_seconds(uint32_t seconds,twatch_rtc_time_v1 *out){
    if(seconds>ALARM_RTC_MAX)return false;
    uint32_t days=seconds/86400;unsigned year=2000,month=1;
    while(year<2099&&days>=(year%4?365u:366u)){days-=year%4?365u:366u;++year;}
    while(month<12&&days>=watch_month_days(year,month)){days-=watch_month_days(year,month);++month;}
    *out=(twatch_rtc_time_v1){(uint16_t)year,(uint8_t)month,(uint8_t)(days+1),(uint8_t)watch_weekday(year,month,days+1),
        (uint8_t)(seconds/3600%24),(uint8_t)(seconds/60%60),(uint8_t)(seconds%60)};return true;
}
#else
static int32_t timezone_get(void *ctx,const char *key,void *buffer,uint32_t capacity,uint32_t *size){(void)ctx;return get(key,buffer,capacity,size);}
#endif
static int32_t clock_read(uint32_t *seconds,uint32_t *minutes,char zone[40]){
#ifdef ALARM_NATIVE_UTC
    risc_realtime_snapshot_v1 s={.struct_size=sizeof(s)};
    int32_t rc=realtime->read(realtime->context,&s);
    if(rc==RISC_REALTIME_CONTEXT)return fence();
    if(rc!=RISC_REALTIME_OK||s.validity!=RISC_REALTIME_VALID||s.epoch_seconds<INT64_C(946684800)||s.epoch_seconds>INT32_MAX||s.nanoseconds>=1000000000u)return ALARM_CONTROL_CLOCK;
    const risc_key_value_v1 kv={1,sizeof(kv),NULL,timezone_get,NULL};
    int zr=portable_timezone_preference_load(&kv,zone);
    if(retained)return ALARM_CONTROL_RETAINED;
    if((zr!=PORTABLE_TIMEZONE_LOADED&&zr!=PORTABLE_TIMEZONE_MISSING)||portable_timezone_resolve(zone,40,&zone_rule)!=PORTABLE_TIMEZONE_OK)return ALARM_CONTROL_CLOCK;
    portable_timezone_civil local;
    if(portable_timezone_utc_to_local(&zone_rule,s.epoch_seconds,&local,NULL)!=PORTABLE_TIMEZONE_OK)return ALARM_CONTROL_CLOCK;
    *seconds=(uint32_t)(s.epoch_seconds-INT64_C(946684800));*minutes=(uint32_t)local.hour*60u+local.minute;
#else
    twatch_rtc_time_v1 raw,local;
    if(!rtc->read(rtc->context,&raw)||raw.weekday>6||!alarm_calendar_seconds(raw.year,raw.month,raw.day,raw.hour,raw.minute,raw.second,seconds)||
       !portable_time_forward(&raw,&local))return ALARM_CONTROL_CLOCK;
    *minutes=(uint32_t)local.hour*60u+local.minute;
    snprintf(zone,40,"%s",portable_time_zone());
#endif
    return ALARM_CONTROL_OK;
}
static int32_t local_minutes(uint32_t seconds,uint32_t *minutes){
#ifdef ALARM_NATIVE_UTC
    portable_timezone_civil local;
    if(portable_timezone_utc_to_local(&zone_rule,INT64_C(946684800)+seconds,&local,NULL)!=PORTABLE_TIMEZONE_OK)return ALARM_CONTROL_CLOCK;
#else
    twatch_rtc_time_v1 raw,local;if(!from_seconds(seconds,&raw)||!portable_time_forward(&raw,&local))return ALARM_CONTROL_CLOCK;
#endif
    *minutes=(uint32_t)local.hour*60u+local.minute;return ALARM_CONTROL_OK;
}
static int32_t next_deadline(uint32_t now,uint32_t minutes,uint32_t *deadline){
    if(minutes>1439)return ALARM_CONTROL_INVALID;
#ifdef ALARM_NATIVE_UTC
    portable_timezone_civil today;
    if(portable_timezone_utc_to_local(&zone_rule,INT64_C(946684800)+now,&today,NULL)!=PORTABLE_TIMEZONE_OK)return ALARM_CONTROL_CLOCK;
    /* Add a civil day, not 24 UTC hours across a DST transition. */
    today.hour=0;today.minute=0;today.second=0;int64_t civil_midnight;
    if(portable_timezone_civil_to_epoch(&today,&civil_midnight)!=PORTABLE_TIMEZONE_OK)return ALARM_CONTROL_CLOCK;
    for(unsigned day=0;day<2;day++){
        portable_timezone_civil candidate;
        if(portable_timezone_epoch_to_civil(civil_midnight+(int64_t)day*86400,&candidate)!=PORTABLE_TIMEZONE_OK)return ALARM_CONTROL_CLOCK;
        candidate.hour=(uint8_t)(minutes/60);candidate.minute=(uint8_t)(minutes%60);candidate.second=0;
        portable_timezone_inverse inverse;int rc=portable_timezone_local_to_utc(&zone_rule,&candidate,&inverse);
        if(rc==PORTABLE_TIMEZONE_GAP||rc==PORTABLE_TIMEZONE_FOLD)return ALARM_CONTROL_AMBIGUOUS;
        if(rc!=PORTABLE_TIMEZONE_OK||inverse.count!=1)return ALARM_CONTROL_CLOCK;
        int64_t v=inverse.candidate[0].epoch-INT64_C(946684800);
        if(v>(int64_t)now&&v<=ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS){*deadline=(uint32_t)v;return ALARM_CONTROL_OK;}
    }
#else
    twatch_rtc_time_v1 raw,local;
    if(!from_seconds(now,&raw)||!portable_time_forward(&raw,&local))return ALARM_CONTROL_CLOCK;
    for(unsigned day=0;day<2;day++){
        twatch_rtc_time_v1 civil=local;
        if(day&&!portable_time_add_hours(&local,24,&civil))return ALARM_CONTROL_CLOCK;
        civil.hour=(uint8_t)(minutes/60);civil.minute=(uint8_t)(minutes%60);civil.second=0;
        portable_time_candidate candidate[2];unsigned count=portable_time_inverse(&civil,candidate);
        if(count!=1)return ALARM_CONTROL_AMBIGUOUS;
        uint32_t v;const twatch_rtc_time_v1 *t=&candidate[0].rtc;
        if(!alarm_calendar_seconds(t->year,t->month,t->day,t->hour,t->minute,t->second,&v))return ALARM_CONTROL_CLOCK;
        if(v>now&&v<=ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS){*deadline=v;return ALARM_CONTROL_OK;}
    }
#endif
    return ALARM_CONTROL_CLOCK;
}
static bool has_volume(void){
#ifdef ALARM_NATIVE_UTC
    const alarm_service_descriptor_v2 *d=alarm_service_descriptor(scheduler);
    return d&&(d->output_modes&ALARM_MODE_SOUND);
#else
    return true; /* This explicit API1 compatibility profile requires sound. */
#endif
}
static int32_t read_volume(uint8_t *volume,uint8_t bytes[32],uint32_t *size){
    memset(bytes,0,32);*size=0;
    int32_t rc=get(ALARM_VOLUME_KEY,bytes,1,size);
    if(retained)return ALARM_CONTROL_RETAINED;
    if(rc==RISC_BOUND_KEY_VALUE_NOT_FOUND){*volume=ALARM_VOLUME_DEFAULT;*size=0;return ALARM_CONTROL_OK;}
    unsigned value;
    if(rc!=RISC_BOUND_KEY_VALUE_OK||!alarm_volume_decode(bytes,*size,&value))return ALARM_CONTROL_STORAGE;
    *volume=(uint8_t)value;return ALARM_CONTROL_OK;
}
static int32_t read_snapshot(void *ctx,alarm_control_snapshot_v1 *out){
    (void)ctx;int32_t rc=valid_call();if(rc)return rc;
    if(!out||out->struct_size!=sizeof(*out))return ALARM_CONTROL_INVALID;
    alarm_control_snapshot_v1 s={.api_version=1,.struct_size=sizeof(s),.volume=ALARM_VOLUME_DEFAULT};
    alarm_status_v1 status;rc=service_status(&status);if(rc==ALARM_CONTROL_RETAINED)return rc;
    if(rc){s.error=rc;snprintf(s.status,sizeof(s.status),"SCHEDULER UNAVAILABLE");*out=s;return rc;}
    if(status.state!=ALARM_STATE_BLOCKED)s.flags|=ALARM_CONTROL_SERVICE_READY;
    s.confirmed_revision=status.schedules[0].revision;s.schedule_state=status.schedules[0].state;
    s.occurrence=(alarm_control_occurrence_v1){status.occurrence.kind,status.occurrence.revision,status.occurrence.deadline,status.occurrence.generation};
    if(status.occurrence.generation)s.flags|=ALARM_CONTROL_ALERT;
    alarm_config saved;uint8_t bytes[32];uint32_t size;
    rc=read_config(&saved,bytes,&size);if(rc==ALARM_CONTROL_RETAINED)return rc;
    if(!rc){s.flags|=ALARM_CONTROL_CONFIG_VALID;s.revision=saved.revision;if(saved.enabled)s.flags|=ALARM_CONTROL_ENABLED;}
    else s.error=rc;
    uint32_t now=0,minute=0;int32_t cr=clock_read(&now,&minute,s.zone);if(cr==ALARM_CONTROL_RETAINED)return cr;
    if(!cr){s.flags|=ALARM_CONTROL_CLOCK_VALID;s.minutes=(minute+1)%1440;
        if(!rc&&saved.enabled&&local_minutes(saved.deadline,&s.minutes)!=ALARM_CONTROL_OK)s.flags&=~ALARM_CONTROL_CLOCK_VALID;
    }else if(!s.error)s.error=cr;
    if(has_volume()){
        s.flags|=ALARM_CONTROL_HAS_VOLUME;uint8_t value;
        int32_t vr=read_volume(&value,bytes,&size);if(vr==ALARM_CONTROL_RETAINED)return vr;
        if(!vr){s.flags|=ALARM_CONTROL_VOLUME_VALID;s.volume=value;}else if(!s.error)s.error=vr;
    }
    const char *message="READY";
    if(!(s.flags&ALARM_CONTROL_CONFIG_VALID))message="SAVED ALARM INVALID - RETRY";
    else if(!(s.flags&ALARM_CONTROL_CLOCK_VALID))message="CLOCK OR TIME ZONE INVALID";
    else if(!(s.flags&ALARM_CONTROL_SERVICE_READY)){message="SCHEDULER BLOCKED - RETRY";s.error=ALARM_CONTROL_BLOCKED;}
    else if(s.flags&ALARM_CONTROL_ALERT)message="ALARM ACTIVE";
    else if((s.flags&ALARM_CONTROL_ENABLED)&&s.confirmed_revision!=s.revision)message="SAVED - AWAITING SCHEDULER";
    else if(s.schedule_state==ALARM_SCHEDULE_ARMED&&(s.flags&ALARM_CONTROL_ENABLED))message="ONE-SHOT ARMED";
    else if(s.schedule_state==ALARM_SCHEDULE_EXPIRED)message="EXPIRED - NOT REPLAYED";
    else if(s.schedule_state==ALARM_SCHEDULE_DISMISSED)message="DISMISSED";
    else if(!(s.flags&ALARM_CONTROL_ENABLED))message="NO ALARM ARMED";
    snprintf(s.status,sizeof(s.status),"%s",message);*out=s;return s.error;
}
static void seal_command(alarm_control_command_v1 *cmd){alarm_write32(cmd->bytes+92,hash(cmd->bytes,92));}
static bool zeroes(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static bool valid_command(const alarm_control_command_v1 *cmd){
    if(!cmd||cmd->struct_size!=sizeof(*cmd)||memcmp(cmd->bytes,"ACC1",4)||
       alarm_read32(cmd->bytes+4)!=CLOCK_POLICY||alarm_read32(cmd->bytes+92)!=hash(cmd->bytes,92)||!zeroes(cmd->bytes+84,8))return false;
    uint32_t op=alarm_read32(cmd->bytes+8),before=alarm_read32(cmd->bytes+12),after=alarm_read32(cmd->bytes+80);
    if(before>32||after>32||!zeroes(cmd->bytes+16+before,32-before)||!zeroes(cmd->bytes+48+after,32-after))return false;
    if(op==ALARM_CONTROL_VOLUME)return (before==0||before==1)&&after==1&&cmd->bytes[48]<=100&&(!before||cmd->bytes[16]<=100);
    if((op!=ALARM_CONTROL_ARM&&op!=ALARM_CONTROL_CANCEL)||(before!=0&&before!=32)||after!=32)return false;
    alarm_config old={.kind=1},desired;
    if(before&&!alarm_config_decode(&old,cmd->bytes+16,before,1))return false;
    return alarm_config_decode(&desired,cmd->bytes+48,after,1)&&old.revision!=UINT32_MAX&&
        desired.revision==old.revision+1&&desired.enabled==(op==ALARM_CONTROL_ARM)&&
        (op!=ALARM_CONTROL_CANCEL||desired.created==old.created);
}
static int32_t prepare(void *ctx,uint32_t op,uint32_t value,uint32_t expected,alarm_control_command_v1 *out){
    (void)ctx;int32_t rc=valid_call();if(rc)return rc;
    if(!out||out->struct_size!=sizeof(*out))return ALARM_CONTROL_INVALID;
    alarm_control_command_v1 cmd={.struct_size=sizeof(cmd)};
    memcpy(cmd.bytes,"ACC1",4);alarm_write32(cmd.bytes+4,CLOCK_POLICY);alarm_write32(cmd.bytes+8,op);
    if(op==ALARM_CONTROL_VOLUME){
        if(!has_volume()||value>100)return ALARM_CONTROL_INVALID;
        uint8_t old;uint32_t size;rc=read_volume(&old,cmd.bytes+16,&size);if(rc)return rc;
        if(expected!=old)return ALARM_CONTROL_STALE;
        alarm_write32(cmd.bytes+12,size);cmd.bytes[48]=(uint8_t)value;alarm_write32(cmd.bytes+80,1);
    }else {
        if(op!=ALARM_CONTROL_ARM&&op!=ALARM_CONTROL_CANCEL)return ALARM_CONTROL_INVALID;
        alarm_config old;uint32_t size;rc=read_config(&old,cmd.bytes+16,&size);if(rc)return rc;
        if(old.revision!=expected)return ALARM_CONTROL_STALE;
        if(old.revision==UINT32_MAX)return ALARM_CONTROL_INVALID;
        alarm_config desired={.revision=old.revision+1,.kind=1,.created=old.created};
        if(op==ALARM_CONTROL_ARM){
            alarm_status_v1 state;rc=service_status(&state);if(rc)return rc;
            if(state.state==ALARM_STATE_BLOCKED)return ALARM_CONTROL_BLOCKED;
            uint32_t now,minutes,deadline;char zone[40];rc=clock_read(&now,&minutes,zone);if(rc)return rc;
            rc=next_deadline(now,value,&deadline);if(rc)return rc;
            desired.enabled=1;desired.created=now;desired.deadline=deadline;
        }
        if(!alarm_config_valid(&desired))return ALARM_CONTROL_INVALID;
        alarm_write32(cmd.bytes+12,size);alarm_config_encode(&desired,cmd.bytes+48);alarm_write32(cmd.bytes+80,32);
    }
    seal_command(&cmd);*out=cmd;return ALARM_CONTROL_OK;
}
static int32_t refresh_scheduler(void){
    int32_t rc=scheduler->refresh(scheduler->context);
    if(rc==ALARM_RETAINED)return fence();
    return rc==ALARM_OK||rc==ALARM_PENDING?ALARM_CONTROL_OK:ALARM_CONTROL_BLOCKED;
}
static int32_t apply(void *ctx,const alarm_control_command_v1 *cmd){
    (void)ctx;int32_t rc=valid_call();if(rc)return rc;
    if(!valid_command(cmd))return ALARM_CONTROL_INVALID;
    uint32_t op=alarm_read32(cmd->bytes+8),before=alarm_read32(cmd->bytes+12),after=alarm_read32(cmd->bytes+80),size=0;
    const char *key=op==ALARM_CONTROL_VOLUME?ALARM_VOLUME_KEY:ALARM_CONFIG_KEY;
    if(op==ALARM_CONTROL_VOLUME&&!has_volume())return ALARM_CONTROL_INVALID;
    uint8_t actual[32]={0};rc=get(key,actual,32,&size);if(retained)return ALARM_CONTROL_RETAINED;
    if(rc!=RISC_BOUND_KEY_VALUE_OK&&rc!=RISC_BOUND_KEY_VALUE_NOT_FOUND)return ALARM_CONTROL_STORAGE;
    if(rc==RISC_BOUND_KEY_VALUE_NOT_FOUND)size=0;
    if(size==after&&!memcmp(actual,cmd->bytes+48,after))return refresh_scheduler();
    if(size!=before||memcmp(actual,cmd->bytes+16,before))return ALARM_CONTROL_STALE;
    if(op==ALARM_CONTROL_ARM){
        alarm_config desired;
        if(!alarm_config_decode(&desired,cmd->bytes+48,32,1))return ALARM_CONTROL_INVALID;
        uint32_t now,minute;char zone[40];rc=clock_read(&now,&minute,zone);if(rc)return rc;
        if(now<desired.created)return ALARM_CONTROL_CLOCK;
        if(now>=desired.deadline)return ALARM_CONTROL_EXPIRED;
    }
    rc=store->put(store->context,key,cmd->bytes+48,after);
    if(rc==RISC_BOUND_KEY_VALUE_CONTEXT)return fence();
    /* IO may have committed. Resolve by exact readback, never recompute now. */
    memset(actual,0,sizeof(actual));size=0;int32_t readback=get(key,actual,sizeof(actual),&size);
    if(retained)return ALARM_CONTROL_RETAINED;
    if(readback!=RISC_BOUND_KEY_VALUE_OK||size!=after||memcmp(actual,cmd->bytes+48,after))return ALARM_CONTROL_UNCERTAIN;
    return refresh_scheduler();
}
static int32_t step(void *ctx){
    (void)ctx;int32_t rc=valid_call();if(rc)return rc;
    rc=scheduler->step(scheduler->context);if(rc==ALARM_RETAINED)return fence();
    alarm_status_v1 state;int32_t sr=service_status(&state);if(sr)return sr;
    if(rc==ALARM_PENDING)return ALARM_CONTROL_PENDING;
    return rc==ALARM_OK?ALARM_CONTROL_OK:ALARM_CONTROL_BLOCKED;
}
static int32_t dismiss(void *ctx,const alarm_control_occurrence_v1 *token){
    (void)ctx;int32_t rc=valid_call();if(rc)return rc;if(!token||!token->generation)return ALARM_CONTROL_INVALID;
    alarm_status_v1 state;rc=service_status(&state);if(rc)return rc;
    alarm_token_v1 t={token->kind,token->revision,token->deadline,token->generation};
    if(!alarm_token_equal(&t,&state.occurrence))return ALARM_CONTROL_STALE;
    rc=scheduler->acknowledge(scheduler->context,&t);if(rc==ALARM_RETAINED)return fence();
    return rc==ALARM_OK?ALARM_CONTROL_OK:rc==ALARM_PENDING?ALARM_CONTROL_PENDING:ALARM_CONTROL_BLOCKED;
}
static const alarm_control_api_v1 api={1,sizeof(api),NULL,step,read_snapshot,prepare,apply,dismiss};
static bool start(const risc_provider_dependency_v1 *deps,size_t count){
    if(started||retained||!deps||count!=3)return false;
    store=NULL;scheduler=NULL;
#ifdef ALARM_NATIVE_UTC
    realtime=NULL;
#else
    rtc=NULL;
#endif
    for(size_t i=0;i<count;i++){
        if(!deps[i].capability_id||!deps[i].api)return false;
        for(size_t j=0;j<i;j++)if(!strcmp(deps[i].capability_id,deps[j].capability_id))return false;
        if(!strcmp(deps[i].capability_id,RISC_BOUND_KEY_VALUE_CAPABILITY)&&deps[i].api_version==1)store=deps[i].api;
        else if(!strcmp(deps[i].capability_id,ALARM_SERVICE_CAPABILITY)&&deps[i].api_version==SERVICE_VERSION)scheduler=deps[i].api;
#ifdef ALARM_NATIVE_UTC
        else if(!strcmp(deps[i].capability_id,"platform.realtime")&&deps[i].api_version==1)realtime=deps[i].api;
#else
        else if(!strcmp(deps[i].capability_id,"rtc.clock")&&deps[i].api_version==2)rtc=deps[i].api;
#endif
        else return false;
    }
    if(!store||store->api_version!=1||store->struct_size<sizeof(*store)||!store->get||!store->put||
       !scheduler||scheduler->api_version!=SERVICE_VERSION||scheduler->struct_size<sizeof(*scheduler)||
       !scheduler->status||!scheduler->step||!scheduler->refresh||!scheduler->acknowledge)return false;
#ifdef ALARM_NATIVE_UTC
    if(!alarm_service_descriptor(scheduler)||!realtime||realtime->api_version!=1||realtime->struct_size<sizeof(*realtime)||!realtime->read)return false;
#else
    if(!rtc||rtc->api_version!=2||rtc->struct_size<sizeof(*rtc)||!rtc->read)return false;
#endif
    started=true;return true;
}
static bool quiesce(void){return !retained;}
static void stop(void){if(retained)return;started=false;store=NULL;scheduler=NULL;}
static const risc_driver_v2 driver={2,sizeof(driver),ALARM_CONTROL_ID,ALARM_CONTROL_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
