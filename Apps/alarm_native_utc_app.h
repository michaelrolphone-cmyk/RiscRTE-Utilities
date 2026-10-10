#ifndef UTILITIES_ALARM_NATIVE_UTC_APP_H
#define UTILITIES_ALARM_NATIVE_UTC_APP_H
#include "T5AppApi.h"
#include "AlarmServiceV2.h"
#ifndef ALARM_SERVICE_TAGGED_V2
#error Native UTC apps require alarm.service API 2
#endif
#include "RiscRuntimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "PortableRealtimeClient.h"
#include "PortableTimeZonePreference.h"
#include "PortableNativeCustody.h"
#include "PortableNativeTimeToolbar.h"
#include "PointsUtcSchedule.h"
#if !defined(PORTABLE_ALARM_CLIENT) || !defined(PORTABLE_APP_LAUNCH_GUARD) || !defined(PORTABLE_NATIVE_TIME_TOOLBAR)
#error Native UTC apps require shared alarm ownership and launch guard
#endif
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#error Native UTC apps require the shared native custody fence
#endif
#ifndef ALARM_RETAINED
#define ALARM_RETAINED (-9)
#endif
#include "alarm_writer.h"
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#include "daily_draw.h"
#include <stddef.h>
#include <stdio.h>
#ifndef DAILY_ALARM_KIND
#error DAILY_ALARM_KIND must select the one-shot alarm or countdown writer
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_key_value_v1 *storage;

static const alarm_service_v1 *service;
static risc_runtime_capability_v1 grants[4];
#include "PortableTimeFormat.h"
static const risc_key_value_v1 *alarm_preferences;
static unsigned alarm_time_format;
static unsigned acquired, values[3];
static alarm_writer writer;
static alarm_status_v1 service_state;
static const char *notice;
static bool service_valid, editing;
#include "alarm_native_utc_io.h"
__attribute__((visibility("hidden"))) bool portable_app_before_launch(const char *destination) {
    (void)destination;if(writer.uncertain){notice="SAVE UNCONFIRMED - RETRY";return false;}
    return !native_retained;
}

#if DAILY_ALARM_KIND == 1
static bool deadline_time(uint32_t seconds,twatch_rtc_time_v1 *out) {
    int64_t epoch;portable_timezone_civil utc;
    if(!points_utc_to_unix(seconds,&epoch)||portable_timezone_epoch_to_civil(epoch,&utc)!=PORTABLE_TIMEZONE_OK)return false;
    *out=(twatch_rtc_time_v1){(uint16_t)utc.year,utc.month,utc.day,utc.weekday,utc.hour,utc.minute,utc.second};return true;
}
static bool next_alarm(const twatch_rtc_time_v1 *raw,uint32_t now,uint32_t *deadline) {
    (void)raw;portable_timezone_civil local;int64_t epoch;
    if(!native_zone_valid||!points_utc_to_unix(now,&epoch)||portable_timezone_utc_to_local(&native_zone,epoch,&local,NULL)!=PORTABLE_TIMEZONE_OK)return false;
    for(unsigned i=0;i<2;i++) {
        portable_timezone_civil civil=local;
        if(i) {int64_t civil_epoch;if(portable_timezone_civil_to_epoch(&local,&civil_epoch)!=PORTABLE_TIMEZONE_OK||portable_timezone_epoch_to_civil(civil_epoch+86400,&civil)!=PORTABLE_TIMEZONE_OK)return false;}
        civil.hour=(uint8_t)values[0];civil.minute=(uint8_t)values[1];civil.second=0;
        portable_timezone_inverse candidate;int status=portable_timezone_local_to_utc(&native_zone,&civil,&candidate);
        /* Preserve the existing explicit rejection of either DST gap or fold. */
        if(status==PORTABLE_TIMEZONE_FOLD||status==PORTABLE_TIMEZONE_GAP){notice="TIME GAP OR AMBIGUOUS";return false;}
        uint32_t v;if(status!=PORTABLE_TIMEZONE_OK||candidate.count!=1||!points_utc_from_unix(candidate.candidate[0].epoch,&v))return false;
        if(v>now){*deadline=v;return true;}
    }
    return false;
}
#endif
/* Rendering uses the same explicit zone as local alarm planning. */
#define portable_time_forward(raw,local) native_local(raw,local)
#define portable_time_zone() native_zone_name
static void restore_saved_fields(void) {
    if(!writer.loaded||writer.uncertain||!writer.saved.enabled)return;
#if DAILY_ALARM_KIND == 1
    twatch_rtc_time_v1 raw,local;
    if(deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)) {
        values[0]=local.hour;values[1]=local.minute;
    }
#else
    values[0]=writer.saved.duration/3600;values[1]=writer.saved.duration/60%60;values[2]=writer.saved.duration%60;
#endif
}
static void refresh_status(void) {
    service_state=(alarm_status_v1){.struct_size=sizeof(service_state)};
    service_valid=service->status(service->context,&service_state)==ALARM_OK;
}
static void save_action(bool cancel) {
    if(native_retained)return;
    notice="";
    if(!cancel&&!writer.uncertain) {
        bool zone_loaded=native_load_zone();if(native_retained)return;
        if(DAILY_ALARM_KIND==1&&!zone_loaded){notice="TIME ZONE UNAVAILABLE";return;}
    }
    if(cancel&&writer.uncertain){notice="SAVE UNCERTAIN - RETRY FIRST";return;}
    if(writer.uncertain) {
        if(alarm_writer_commit(&writer,storage,&writer.pending)!=0){notice="SAVE UNCONFIRMED - RETRY";return;}
    } else {
        if(!writer.loaded){notice="SAVED DATA INVALID - RETRY";return;}
        if(writer.saved.revision==UINT32_MAX){notice="REVISION LIMIT";return;}
        alarm_config desired={.revision=writer.saved.revision+1,.kind=DAILY_ALARM_KIND,.created=writer.saved.created};
        if(!cancel) {
            if(!service_valid||service_state.state==ALARM_STATE_BLOCKED){notice="SERVICE BLOCKED - RETRY";return;}
            twatch_rtc_time_v1 t;uint32_t now,deadline;
            if(!read_clock(&t,&now)){notice="NATIVE TIME UNAVAILABLE";return;}
#if DAILY_ALARM_KIND == 1
            if(!next_alarm(&t,now,&deadline)){if(!notice||!notice[0])notice="TIME NOT AVAILABLE";return;}
#else
            uint32_t duration=values[0]*3600u+values[1]*60u+values[2];
            if(!duration||duration>ALARM_COUNTDOWN_MAX){notice="CHOOSE A NONZERO DURATION";return;}
            if(now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS-duration){notice="NATIVE TIME RANGE LIMIT";return;}
            deadline=now+duration;desired.duration=duration;
#endif
            desired.created=now;desired.deadline=deadline;desired.enabled=1;
        }
        if(!alarm_config_valid(&desired)){notice="TIME OUT OF RANGE";return;}
        if(alarm_writer_commit(&writer,storage,&desired)!=0){notice="SAVE UNCONFIRMED - RETRY";return;}
    }
    if(native_retained)return;
    editing=false;service->refresh(service->context);notice=writer.saved.enabled?"SAVED - CHECKING SERVICE":"CANCEL SAVED - CHECKING";
}
#ifdef DAILY_NOVA_APP
#include "alarm_nova.inc"
#else
static void draw(void) {
    if(native_retained)return;
    int w=app->screen_width(),h=app->screen_height();app->clear();app->draw_text(8,16,"BACK");
    app->draw_label(52,16,w-104,DAILY_ALARM_KIND==1?"ALARM":"COUNTDOWN");
    unsigned columns=DAILY_ALARM_KIND==1?2:3;int cell=(w-16)/(int)columns;
    char value[12];
    unsigned shown[3]={values[0],values[1],values[2]};
    if(DAILY_ALARM_KIND==2&&!editing&&service_valid&&writer.saved.enabled&&
       service_state.schedules[1].revision==writer.saved.revision&&service_state.schedules[1].state==ALARM_SCHEDULE_ARMED) {
        uint32_t left=writer.saved.deadline>service_state.rtc_seconds?writer.saved.deadline-service_state.rtc_seconds:0;
        shown[0]=left/3600;shown[1]=left/60%60;shown[2]=left%60;
    }
#if DAILY_ALARM_KIND == 1
    if(!editing&&!writer.uncertain&&writer.loaded&&writer.saved.enabled) {
        twatch_rtc_time_v1 raw,local;
        if(deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)) {
            shown[0]=local.hour;shown[1]=local.minute;
        }
    }
#endif
#ifdef PORTABLE_UNPADDED_HOURS
    if(DAILY_ALARM_KIND==1)snprintf(value,sizeof(value),"%u:%02u",shown[0],shown[1]);
#else
    if(DAILY_ALARM_KIND==1)snprintf(value,sizeof(value),"%02u:%02u",shown[0],shown[1]);
#endif
#ifdef PORTABLE_UNPADDED_HOURS
    else snprintf(value,sizeof(value),"%u:%02u:%02u",shown[0],shown[1],shown[2]);
#else
    else snprintf(value,sizeof(value),"%02u:%02u:%02u",shown[0],shown[1],shown[2]);
#endif
    if(DAILY_ALARM_KIND==1&&!native_zone_valid&&!editing)snprintf(value,sizeof(value),"--:--");
    int scale=w>=220?3:2;daily_draw_text(app,(w-daily_draw_width(value,scale))/2,71,value,scale);
    for(unsigned i=0;i<columns;i++){app->draw_label(8+(int)i*cell,46,cell,"+");app->draw_label(8+(int)i*cell,117,cell,"-");}
    const char *state=notice?notice:"";
    if(!editing&&!writer.uncertain&&service_valid&&service_state.schedules[DAILY_ALARM_KIND-1].revision==writer.saved.revision) {
        switch(service_state.schedules[DAILY_ALARM_KIND-1].state){
        case ALARM_SCHEDULE_ARMED:state=DAILY_ALARM_KIND==1?"ONE-SHOT ARMED":"COUNTDOWN RUNNING";break;
        case ALARM_SCHEDULE_EXPIRED:state="EXPIRED - NOT REPLAYED";break;
        case ALARM_SCHEDULE_DISMISSED:state="DISMISSED";break;
        default:break;}
    }
    if(service_valid&&service_state.state==ALARM_STATE_BLOCKED)state=service_state.error==ALARM_RTC?"NATIVE TIME UNAVAILABLE":"SERVICE ERROR - RETRY";
    else if(service_valid&&service_state.state==ALARM_STATE_DISMISSING)state="STOPPING / SAVING DISMISS";
    else if(service_valid&&service_state.occurrence.generation)state=service_state.label;
    app->draw_label(4,141,w-8,state);
    app->draw_label(8,165,w-16,DAILY_ALARM_KIND==1?(native_zone_valid?portable_time_zone():"TIME ZONE UNAVAILABLE"):"HH:MM:SS - UTC DEADLINE");
    bool alert=service_valid&&service_state.occurrence.generation;
    app->draw_label(8,h-45,w/2-12,alert?"DISMISS":writer.uncertain?"RETRY SAVE":DAILY_ALARM_KIND==1?"ARM NEXT":"START");
    app->draw_label(w/2+4,h-45,w/2-12,writer.uncertain?"WAIT":"CANCEL");
    app->draw_label(8,h-15,w-16,"RETRY / REFRESH");app->present(false);
}
#endif
void app_main(void) {
#ifdef DAILY_NOVA_APP
    alarm_page=alarm_field=0;
#if DAILY_ALARM_KIND == 1
    volume_uncertain=false;volume_message="";
#endif
#endif
    native_retained=false;native_client=(portable_realtime_client){0};native_zone_valid=false;
    app=t5_app_get_api(1);notice="";editing=false;writer=(alarm_writer){0};service_valid=false;service_state=(alarm_status_v1){0};values[0]=0;values[1]=5;values[2]=0;
    if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label)||!app->poll||!app->millis||!app->screen_width||!app->screen_height||!app->clear||!app->draw_text||!app->draw_label||!app->fill_rect||!app->present)return;
    if(app->screen_width()<160||app->screen_width()>1024||app->screen_height()<240||app->screen_height()>1024)return;
#if defined(PORTABLE_PAPER_UTILITIES)
    up_open(app);
#endif
    if(!open_dependencies()){if(native_retained)return;notice="ALARM SERVICE UNAVAILABLE";draw();for(;;){t5_app_input_t i={0};if(!app->poll(&i,50)) {
#if defined(PORTABLE_PAPER_UTILITIES) && defined(PORTABLE_ALARM_CLIENT)
        if(portable_app_sleep_retained())return;
#endif
        break;
    }
    if(i.exit_requested&&!(i.buttons&T5_APP_BUTTON_BACK))break;
#if defined(PORTABLE_PAPER_UTILITIES)
        if(utility_paper){up_input(&i);if(i.tapped&&up_hit(i.touch_x,i.touch_y,32,688,200,88))i.buttons|=T5_APP_BUTTON_BACK;}
        if(i.buttons&T5_APP_BUTTON_BACK){if(up_return())break;}
#else
        if(i.buttons&T5_APP_BUTTON_BACK)break;
#endif
}close_dependencies();return;}
    (void)alarm_writer_load(&writer,storage,DAILY_ALARM_KIND);
    if(native_retained)return;
    if(!writer.loaded)notice="SAVED DATA INVALID - RETRY";
#if DAILY_ALARM_KIND == 1
    twatch_rtc_time_v1 raw,local;uint32_t seconds;
    if(writer.loaded&&writer.saved.enabled&&deadline_time(writer.saved.deadline,&raw)&&portable_time_forward(&raw,&local)) {
        values[0]=local.hour;values[1]=local.minute;
    } else if(read_clock(&raw,&seconds)&&portable_time_forward(&raw,&local)) {
        values[0]=local.hour;values[1]=(local.minute+1)%60;if(!values[1])values[0]=(values[0]+1)%24;
    }
#else
    if(writer.saved.duration){values[0]=writer.saved.duration/3600;values[1]=writer.saved.duration/60%60;values[2]=writer.saved.duration%60;}
#endif
#if defined(DAILY_NOVA_APP) && DAILY_ALARM_KIND == 1
    /* Native visual profile has no volume authority. */
#endif
    if(native_retained)return;
    refresh_status();if(native_retained)return;draw();uint32_t last_draw=app->millis();
    for(;;) {
        if(native_retained)return;
        t5_app_input_t input={0};bool poll_ok=app->poll(&input,20);
        if(!poll_ok) {
#ifdef PORTABLE_ALARM_CLIENT
            /* The shared adapter already owns the settled/error boundary and
               bounded output cleanup; never repeat or race it here. */
            if(portable_app_sleep_retained())return;
            break;
#else
            /* A false poll can mean an outstanding/failed presentation. Only
               independent output cleanup is allowed, never normal service I/O. */
            int32_t stopped=ALARM_PENDING;
            for(unsigned i=0;i<3&&stopped==ALARM_PENDING;i++) {
                stopped=service->stop_only(service->context);
                if(stopped==ALARM_PENDING)runtime->yield_ms(1);
            }
            if(stopped!=ALARM_OK) {
                if(runtime->diagnostic)runtime->diagnostic("ALARM foreground-failed output-stop-unconfirmed; invocation retained");
                /* Do not replay unsafe I/O, release grants or queue a handoff.
                   Only a device reset/recovery can resolve this failed view. */
                for(;;)runtime->yield_ms(50);
            }
            break;
#endif
        }
        /* Successful poll/present is the staged client's safe point. The next
           shared adapter must expose its settled/error barrier explicitly. */
#ifndef PORTABLE_ALARM_CLIENT
        (void)service->step(service->context);
#endif
        if(native_retained)return;
        refresh_status();if(native_retained)return;
#ifndef DAILY_NOVA_APP
        bool alert=service_valid&&service_state.occurrence.generation;
#endif
#if defined(PORTABLE_PAPER_UTILITIES)
        if(input.exit_requested&&!(input.buttons&T5_APP_BUTTON_BACK)&&portable_app_before_launch(NULL))break; /* Global Home was queued by adapter. */
        if(utility_paper){up_input(&input);if(input.tapped&&up_hit(input.touch_x,input.touch_y,32,688,alarm_page?416:200,88))input.buttons|=T5_APP_BUTTON_BACK;}
#endif
        if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK)) {
            if(writer.uncertain){notice="SAVE UNCONFIRMED - RETRY";draw();continue;}
#ifdef DAILY_NOVA_APP
            if(alarm_back())break;
            draw();continue;
#else
            if(!alert)break;
            notice="DISMISS ALERT BEFORE BACK";draw();
            continue;
#endif
        }
        bool dirty=false;
        if(input.tapped) {
#ifdef DAILY_NOVA_APP
            dirty=alarm_tap(input.touch_x,input.touch_y);
#else
            int x=input.touch_x,y=input.touch_y,w=app->screen_width(),h=app->screen_height();
            if(x>=8&&x<w-8&&y>=h-62&&y<h-28) {
                if(x<w/2-4){if(alert)service->acknowledge(service->context,&service_state.occurrence);else save_action(false);}
                else if(x>=w/2+4&&!alert)save_action(true);
                dirty=true;
            } else if(x>=8&&x<w-8&&y>=h-27&&y<h) {
                if(writer.uncertain)save_action(false);else (void)alarm_writer_load(&writer,storage,DAILY_ALARM_KIND);
                if(native_retained)return;
                (void)native_load_zone();if(native_retained)return;
                editing=false;restore_saved_fields();service->refresh(service->context);notice="REFRESHING";dirty=true;
            } else if(!alert&&!writer.uncertain&&x>=8&&x<w-8&&((y>=32&&y<63)||(y>=105&&y<131))) {
                unsigned columns=DAILY_ALARM_KIND==1?2:3;unsigned column=(unsigned)(x-8)*columns/(unsigned)(w-16);
                unsigned max=column?59:(DAILY_ALARM_KIND==1?23:99);values[column]=(values[column]+(y<63?1:max))%(max+1);notice="EDITED - NOT SAVED";editing=true;dirty=true;
            }
        #endif
        }
        if(native_retained)return;
        if(dirty||(uint32_t)(app->millis()-last_draw)>=
#if defined(PORTABLE_PAPER_UTILITIES)
            (utility_paper?1000u:250u)
#else
            250u
#endif
            ){draw();last_draw=app->millis();}
    }
    if(!native_retained)close_dependencies();
}
#endif
