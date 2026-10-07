#ifdef PORTABLE_RETURN_APP
#error Nested Nova app requires ALARM_RETURN_APP, never generic eager return
#endif
#include "PortableNovaUi.h"
static unsigned alarm_page,alarm_field;
#if DAILY_ALARM_KIND == 1
#include "AlarmVolume.h"
static unsigned volume_value,volume_choice;
static bool volume_valid,volume_missing,volume_uncertain;
static const char *volume_message;
static void volume_load(void) {
    if(volume_uncertain)return;
    volume_value=volume_choice=ALARM_VOLUME_DEFAULT;volume_valid=volume_missing=false;volume_message="";
    if(!alarm_preferences || !alarm_preferences->get){volume_message="Storage unavailable";return;}
    uint8_t data=0;uint32_t size=0;int32_t rc=alarm_preferences->get(alarm_preferences->context,ALARM_VOLUME_KEY,&data,1,&size);
    if(rc==RISC_KEY_VALUE_NOT_FOUND){volume_valid=volume_missing=true;return;}
    if(rc!=RISC_KEY_VALUE_OK || !alarm_volume_decode(&data,size,&volume_value)){volume_message="Invalid volume - choose a value";return;}
    volume_choice=volume_value;volume_valid=true;
}
static void volume_save(void) {
    if(!alarm_preferences || !alarm_preferences->put || !alarm_preferences->get || !volume_valid || volume_choice>100){volume_message="Choose a valid volume";return;}
    const uint8_t value=(uint8_t)volume_choice;uint8_t actual=255;uint32_t size=0;unsigned confirmed;
    int32_t rc=alarm_preferences->put(alarm_preferences->context,ALARM_VOLUME_KEY,&value,1);
    if((rc==RISC_KEY_VALUE_OK || rc==RISC_KEY_VALUE_IO) && alarm_preferences->get(alarm_preferences->context,ALARM_VOLUME_KEY,&actual,1,&size)==RISC_KEY_VALUE_OK && alarm_volume_decode(&actual,size,&confirmed) && confirmed==volume_choice) {
        volume_value=confirmed;volume_missing=volume_uncertain=false;volume_message="Volume saved";notice="Volume saved";alarm_page=0;
        (void)service->refresh(service->context);return;
    }
    volume_uncertain=true;volume_message="Save unconfirmed - retry";
}
#endif
static const char *const alarm_fields[]={"Hour","Minute","Second"};
static bool alarm_24h(void) {
#if DAILY_ALARM_KIND == 1
    return alarm_time_format==PORTABLE_TIME_FORMAT_24;
#else
    return true; /* An elapsed duration has no AM/PM interpretation. */
#endif
}
static void alarm_value_text(char *out,size_t cap,const unsigned *shown) {
    if(DAILY_ALARM_KIND==2)snprintf(out,cap,"%02u:%02u:%02u",shown[0],shown[1],shown[2]);
    else if(alarm_24h())snprintf(out,cap,"%02u:%02u",shown[0],shown[1]);
    else snprintf(out,cap,"%u:%02u %s",shown[0]%12?shown[0]%12:12,shown[1],shown[0]<12?"AM":"PM");
}
static const char *alarm_state_text(void) {
    if(writer.uncertain)return "Save unconfirmed - retry";
    if(!writer.loaded)return "Saved data invalid - retry";
    if(service_valid && service_state.state==ALARM_STATE_BLOCKED)return service_state.error==ALARM_RTC?"RTC changed - retry":"Service error - retry";
    if(service_valid && service_state.state==ALARM_STATE_DISMISSING)return "Stopping / saving dismiss";
    if(service_valid && service_state.occurrence.generation)return service_state.label;
    if(!editing && service_valid && service_state.schedules[DAILY_ALARM_KIND-1].revision==writer.saved.revision) {
        switch(service_state.schedules[DAILY_ALARM_KIND-1].state) {
        case ALARM_SCHEDULE_ARMED:return DAILY_ALARM_KIND==1?"Once - scheduled":"Countdown running";
        case ALARM_SCHEDULE_EXPIRED:return "Expired - will not repeat";
        case ALARM_SCHEDULE_DISMISSED:return "Dismissed - finished";
        default:break;
        }
    }
    return notice && notice[0]?notice:"Not scheduled";
}
static void alarm_retry(void) {
    if(writer.uncertain)save_action(false);else (void)alarm_writer_load(&writer,storage,DAILY_ALARM_KIND);
    editing=false;restore_saved_fields();service->refresh(service->context);notice="Refreshing";
#if DAILY_ALARM_KIND == 1
    (void)portable_time_format_load(alarm_preferences,&alarm_time_format);volume_load();
#endif
}
#ifdef PORTABLE_PAPER_UTILITIES
#if DAILY_ALARM_KIND == 2
#include "countdown_paper.inc"
#else
#include "alarms_paper.inc"
#endif
#endif
static void draw(void) {
#ifdef PORTABLE_PAPER_UTILITIES
    if(utility_paper){
#if DAILY_ALARM_KIND == 2
        countdown_paper_draw();
#else
        alarms_paper_draw();
#endif
        return;}
#endif
    char value[40],caption[40];unsigned shown[3]={values[0],values[1],values[2]};
    portable_nova_begin();portable_nova_header(DAILY_ALARM_KIND==1?"ALARMS":"COUNTDOWN");
    if(alarm_page==0) {
        snprintf(caption,sizeof(caption),"%s",DAILY_ALARM_KIND==1?"Once (does not repeat)":"Duration  HH:MM:SS");
        if(DAILY_ALARM_KIND==2 && !editing && service_valid && writer.saved.enabled && service_state.schedules[1].revision==writer.saved.revision && service_state.schedules[1].state==ALARM_SCHEDULE_ARMED) {
            uint32_t left=writer.saved.deadline>service_state.rtc_seconds?writer.saved.deadline-service_state.rtc_seconds:0;
            shown[0]=left/3600;shown[1]=left/60%60;shown[2]=left%60;
        }
#if DAILY_ALARM_KIND == 1
        if(!editing && !writer.uncertain && writer.loaded && writer.saved.enabled) {
            twatch_rtc_time_v1 raw,local;
            if(deadline_time(writer.saved.deadline,&raw) && portable_time_forward(&raw,&local)) {
                static const char *const months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
                shown[0]=local.hour;shown[1]=local.minute;
                snprintf(caption,sizeof(caption),"Once / %u %s %u",local.day,months[local.month-1],local.year);
            }
        }
#endif
        alarm_value_text(value,sizeof(value),shown);
        portable_nova_row(16,56,208,54,caption,value,false);
#if DAILY_ALARM_KIND == 1
        if(!volume_valid)snprintf(value,sizeof(value),"Invalid - choose a value");
        else if(!volume_value)snprintf(value,sizeof(value),"Muted (0%%)");
        else snprintf(value,sizeof(value),"%u%%%s",volume_value,volume_missing?" (default)":"");
        portable_nova_row(16,116,208,48,"Alarm volume",value,false);
        portable_nova_center(3,16,169,208,alarm_state_text(),NOVA_CAP);
        portable_nova_button(184,4,48,44,"Retry",false);
#else
        portable_nova_row(16,116,208,54,"Refresh status",alarm_state_text(),false);
#endif

        bool alert=service_valid && service_state.occurrence.generation;
        portable_nova_button(16,184,100,44,alert?"Dismiss":writer.uncertain?"Retry save":DAILY_ALARM_KIND==1?"Arm once":"Start",false);
        portable_nova_button(124,184,100,44,writer.uncertain?"Wait":"Cancel",false);
#if DAILY_ALARM_KIND == 1
    } else if(alarm_page==3) {
        portable_nova_center(1,16,62,208,"Alarm volume (%)",NOVA_CAP);
        snprintf(value,sizeof(value),"%u",volume_choice);portable_nova_center(4,72,94,96,volume_valid?value:"--",NOVA_CYAN);
        portable_nova_button(16,94,48,48,"-",false);portable_nova_button(176,94,48,48,"+",false);
        portable_nova_center(2,16,151,208,volume_message && volume_message[0]?volume_message:volume_choice?"Vibration is unchanged":"Sound muted; vibration unchanged",NOVA_CAP);
        portable_nova_button(16,184,100,44,volume_uncertain?"Wait":"Cancel",false);portable_nova_button(124,184,100,44,volume_uncertain?"Retry save":"Save",false);
#endif
    } else if(alarm_page==1) {
        unsigned count=DAILY_ALARM_KIND==1?2:3;
        for(unsigned i=0;i<count;i++) {
            if(i==0 && !alarm_24h())snprintf(value,sizeof(value),"Hour: %u %s",values[0]%12?values[0]%12:12,values[0]<12?"AM":"PM");
            else snprintf(value,sizeof(value),"%s: %02u",alarm_fields[i],values[i]);
            portable_nova_button(20,58+(int)i*50,200,44,value,false);
        }
        portable_nova_center(2,16,215,208,
#if DAILY_ALARM_KIND == 2
            "Draft only. Back, then Start.",
#else
            "Draft only. Back, then Arm / Start.",
#endif
            NOVA_CAP);
    } else {
        portable_nova_center(1,16,62,208,alarm_fields[alarm_field],NOVA_CAP);
        unsigned shown_value=values[alarm_field];if(alarm_field==0 && !alarm_24h())shown_value=shown_value%12?shown_value%12:12;
        snprintf(value,sizeof(value),"%02u",shown_value);
        portable_nova_button(16,94,48,48,"-",false);portable_nova_button(176,94,48,48,"+",false);
        portable_nova_center(4,72,94,96,value,NOVA_CYAN);
        if(alarm_field==0 && !alarm_24h()) {
            portable_nova_button(16,184,100,44,values[0]<12?"AM selected":"PM selected",true);
            portable_nova_button(124,184,100,44,"Done",false);
        } else portable_nova_button(24,184,192,44,"Done",false);
    }
    app->present(false);
}
static bool alarm_back(void) {
#if DAILY_ALARM_KIND == 1
    if(volume_uncertain){volume_message="Retry save before leaving";return false;}
#endif
    if(service_valid && service_state.occurrence.generation){notice="Dismiss alert before Back";return false;}
    if(alarm_page){alarm_page=alarm_page==2?1:0;return false;}
#ifdef ALARM_RETURN_APP
    if(!runtime || !runtime->request_launch || !runtime->request_launch(ALARM_RETURN_APP)) {notice="Return unavailable - retry";return false;}
#endif
    return true;
}
static bool alarm_tap(int x,int y) {
#ifdef PORTABLE_PAPER_UTILITIES
    if(utility_paper) {
#if DAILY_ALARM_KIND == 2
        return countdown_paper_tap(x,y);
#else
        return alarms_paper_tap(x,y);
#endif
    }
#endif
    bool alert=service_valid && service_state.occurrence.generation;
    if(alarm_page==0) {
#if DAILY_ALARM_KIND == 1
        if(portable_nova_hit(x,y,184,4,48,44)){if(!alert)alarm_retry();return true;}
#endif
        if(portable_nova_hit(x,y,16,184,100,44)) {
            if(alert)service->acknowledge(service->context,&service_state.occurrence);else save_action(false);
        } else if(portable_nova_hit(x,y,124,184,100,44)) {if(!alert)save_action(true);}
        else if(portable_nova_hit(x,y,16,116,208,
#if DAILY_ALARM_KIND == 1
            48)){if(!alert){volume_load();alarm_page=3;}}
#else
            54)){if(!alert)alarm_retry();}
#endif
        else if(portable_nova_hit(x,y,16,56,208,54) && !alert && !writer.uncertain){alarm_page=1;editing=true;notice="Draft only - not saved";}
        else return false;
    } else if(alarm_page==1) {
        for(unsigned i=0;i<(DAILY_ALARM_KIND==1?2u:3u);i++)if(portable_nova_hit(x,y,20,58+(int)i*50,200,44)){alarm_field=i;alarm_page=2;return true;}
        return false;
#if DAILY_ALARM_KIND == 1
    } else if(alarm_page==3) {
        if(!volume_uncertain && portable_nova_hit(x,y,16,94,48,48)){volume_choice=volume_choice>=10?volume_choice-10:0;volume_valid=true;volume_message="Draft only - use Save";}
        else if(!volume_uncertain && portable_nova_hit(x,y,176,94,48,48)){volume_choice=volume_choice<=90?volume_choice+10:100;volume_valid=true;volume_message="Draft only - use Save";}
        else if(portable_nova_hit(x,y,124,184,100,44))volume_save();
        else if(!volume_uncertain && portable_nova_hit(x,y,16,184,100,44))alarm_page=0;
        else return false;
#endif
    } else {
        if(portable_nova_hit(x,y,16,94,48,48) || portable_nova_hit(x,y,176,94,48,48)) {
            bool up=x>=176;unsigned max=alarm_field?59:DAILY_ALARM_KIND==1?23:99;
            if(alarm_field==0 && !alarm_24h())values[0]=(values[0]/12)*12+(values[0]%12+(up?1:11))%12;
            else values[alarm_field]=(values[alarm_field]+(up?1:max))%(max+1);
        } else if(alarm_field==0 && !alarm_24h() && portable_nova_hit(x,y,16,184,100,44))values[0]=(values[0]+12)%24;
        else if(portable_nova_hit(x,y,alarm_field==0 && !alarm_24h()?124:24,184,alarm_field==0 && !alarm_24h()?100:192,44))alarm_page=1;
        else return false;
    }
    return true;
}
