#ifdef ALARM_NATIVE_UTC
#include "alarm_native_utc_app.h"
#else
#ifndef UTILITIES_ALARM_APP_SHARED_H
#define UTILITIES_ALARM_APP_SHARED_H
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTime.h"
#ifdef PORTABLE_PAPER_UTILITIES
#include <AlarmServiceV1.h>
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
static const twatch_rtc_api_v1 *rtc;
static const alarm_service_v1 *service;
static risc_runtime_capability_v1 grants[4];
#if defined(DAILY_NOVA_APP) && DAILY_ALARM_KIND == 1
#include "PortableTimeFormat.h"
static const risc_key_value_v1 *alarm_preferences;
static unsigned alarm_time_format;
#endif
static unsigned acquired, values[3];
static alarm_writer writer;
static alarm_status_v1 service_state;
static const char *notice;
static bool service_valid, editing;
static bool read_clock(twatch_rtc_time_v1 *t,uint32_t *seconds) {
    return rtc->read(rtc->context,t)&&t->weekday<=6&&alarm_calendar_seconds(t->year,t->month,t->day,t->hour,t->minute,t->second,seconds);
}
#if DAILY_ALARM_KIND == 1
static bool deadline_time(uint32_t seconds,twatch_rtc_time_v1 *out) {
    if(seconds>ALARM_RTC_MAX)return false;
    uint32_t days=seconds/86400;unsigned year=2000,month=1;
    while(year<2099&&days>=(year%4?365u:366u)){days-=year%4?365u:366u;++year;}
    while(month<12&&days>=watch_month_days(year,month)){days-=watch_month_days(year,month);++month;}
    *out=(twatch_rtc_time_v1){(uint16_t)year,(uint8_t)month,(uint8_t)(days+1),
        (uint8_t)watch_weekday(year,month,days+1),(uint8_t)(seconds/3600%24),(uint8_t)(seconds/60%60),(uint8_t)(seconds%60)};
    return true;
}
static bool next_alarm(const twatch_rtc_time_v1 *raw,uint32_t now,uint32_t *deadline) {
    twatch_rtc_time_v1 local;if(!portable_time_forward(raw,&local))return false;
    for(unsigned i=0;i<2;i++) {
        twatch_rtc_time_v1 civil=local;
        if(i&&!portable_time_add_hours(&local,24,&civil))return false;
        civil.hour=(uint8_t)values[0];civil.minute=(uint8_t)values[1];civil.second=0;
        portable_time_candidate candidate[2];unsigned count=portable_time_inverse(&civil,candidate);
        /* A fold/gap is explicitly rejected, never silently offset-selected. */
        if(count!=1){notice="TIME GAP OR AMBIGUOUS";return false;}
        uint32_t v; twatch_rtc_time_v1 *t=&candidate[0].rtc;
        if(!alarm_calendar_seconds(t->year,t->month,t->day,t->hour,t->minute,t->second,&v))return false;
        if(v>now){*deadline=v;return true;}
    }
    return false;
}
#endif
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
            if(!read_clock(&t,&now)){notice="RTC INVALID - RETRY";return;}
#if DAILY_ALARM_KIND == 1
            if(!next_alarm(&t,now,&deadline)){if(!notice||!notice[0])notice="TIME NOT AVAILABLE";return;}
#else
            uint32_t duration=values[0]*3600u+values[1]*60u+values[2];
            if(!duration||duration>ALARM_COUNTDOWN_MAX){notice="CHOOSE A NONZERO DURATION";return;}
            if(now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS-duration){notice="RTC RANGE LIMIT";return;}
            deadline=now+duration;desired.duration=duration;
#endif
            desired.created=now;desired.deadline=deadline;desired.enabled=1;
        }
        if(!alarm_config_valid(&desired)){notice="TIME OUT OF RANGE";return;}
        if(alarm_writer_commit(&writer,storage,&desired)!=0){notice="SAVE UNCONFIRMED - RETRY";return;}
    }
    editing=false;service->refresh(service->context);notice=writer.saved.enabled?"SAVED - CHECKING SERVICE":"CANCEL SAVED - CHECKING";
}
static bool open_dependencies(void) {
    runtime=risc_runtime_get_api(1);acquired=0;
    if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms)return false;
    const char *names[]={"storage.key-value","rtc.clock",ALARM_SERVICE_CAPABILITY};const uint32_t apis[]={1,2,1};
    for(unsigned i=0;i<3;i++){grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};if(!runtime->acquire(names[i],apis[i],
#ifdef DAILY_NOVA_APP
        i==0?3:0,
#else
        0,
#endif
        &grants[i])){return false;}
        acquired++;
    }
    storage=grants[0].api;rtc=grants[1].api;service=grants[2].api;
#if defined(DAILY_NOVA_APP) && DAILY_ALARM_KIND == 1
    alarm_preferences=NULL;alarm_time_format=PORTABLE_TIME_FORMAT_12;
    grants[3]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[3])};
    if(runtime->acquire("storage.key-value",1,1,&grants[3])) {acquired++;alarm_preferences=grants[3].api;}
    (void)portable_time_format_load(alarm_preferences,&alarm_time_format);
#endif
    return storage&&storage->api_version==1&&storage->struct_size>=sizeof(*storage)&&storage->get&&storage->put&&
        rtc&&rtc->api_version==2&&rtc->struct_size>=sizeof(*rtc)&&rtc->read&&
        service&&service->api_version==1&&service->struct_size>=sizeof(*service)&&service->status&&service->step&&service->refresh&&service->acknowledge&&service->stop_only;
}
static void close_dependencies(void) {
    if(runtime)while(acquired)runtime->release(&grants[--acquired]);
    runtime=NULL;storage=NULL;rtc=NULL;service=NULL;
#if defined(DAILY_NOVA_APP) && DAILY_ALARM_KIND == 1
    alarm_preferences=NULL;
#endif
}
#ifdef DAILY_NOVA_APP
#include "alarm_nova.inc"
#else
static void draw(void) {
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
    if(service_valid&&service_state.state==ALARM_STATE_BLOCKED)state=service_state.error==ALARM_RTC?"RTC CHANGED - RETRY":"SERVICE ERROR - RETRY";
    else if(service_valid&&service_state.state==ALARM_STATE_DISMISSING)state="STOPPING / SAVING DISMISS";
    else if(service_valid&&service_state.occurrence.generation)state=service_state.label;
    app->draw_label(4,141,w-8,state);
    app->draw_label(8,165,w-16,DAILY_ALARM_KIND==1?portable_time_zone():"HH:MM:SS - RTC DEADLINE");
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
    app=t5_app_get_api(1);notice="";editing=false;writer=(alarm_writer){0};service_valid=false;service_state=(alarm_status_v1){0};values[0]=0;values[1]=5;values[2]=0;
    if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label)||!app->poll||!app->millis||!app->screen_width||!app->screen_height||!app->clear||!app->draw_text||!app->draw_label||!app->fill_rect||!app->present)return;
    if(app->screen_width()<160||app->screen_width()>1024||app->screen_height()<240||app->screen_height()>1024)return;
#if defined(PORTABLE_PAPER_UTILITIES)
    up_open(app);
#endif
    if(!open_dependencies()){notice="ALARM SERVICE UNAVAILABLE";draw();for(;;){t5_app_input_t i={0};if(!app->poll(&i,50)) {
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
    volume_load();
#endif
    refresh_status();draw();uint32_t last_draw=app->millis();
    for(;;) {
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
        refresh_status();
#ifndef DAILY_NOVA_APP
        bool alert=service_valid&&service_state.occurrence.generation;
#endif
#if defined(PORTABLE_PAPER_UTILITIES)
        if(input.exit_requested&&!(input.buttons&T5_APP_BUTTON_BACK))break; /* Global Home was queued by adapter. */
        if(utility_paper){up_input(&input);if(input.tapped&&up_hit(input.touch_x,input.touch_y,32,688,alarm_page?416:200,88))input.buttons|=T5_APP_BUTTON_BACK;}
#endif
        if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK)) {
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
                editing=false;restore_saved_fields();service->refresh(service->context);notice="REFRESHING";dirty=true;
            } else if(!alert&&!writer.uncertain&&x>=8&&x<w-8&&((y>=32&&y<63)||(y>=105&&y<131))) {
                unsigned columns=DAILY_ALARM_KIND==1?2:3;unsigned column=(unsigned)(x-8)*columns/(unsigned)(w-16);
                unsigned max=column?59:(DAILY_ALARM_KIND==1?23:99);values[column]=(values[column]+(y<63?1:max))%(max+1);notice="EDITED - NOT SAVED";editing=true;dirty=true;
            }
        #endif
        }
        if(dirty||(uint32_t)(app->millis()-last_draw)>=
#if defined(PORTABLE_PAPER_UTILITIES)
            (utility_paper?1000u:250u)
#else
            250u
#endif
            ){draw();last_draw=app->millis();}
    }
    close_dependencies();
}
#endif

#endif /* ALARM_NATIVE_UTC */
