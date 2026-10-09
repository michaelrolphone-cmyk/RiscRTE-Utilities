#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "PortableNovaUi.h"
#include "PortableContextsClient.h"
#include "PortableBackgroundServices.h"
#include "RiscAppDataV1.h"
#include <stdio.h>
#include <string.h>
unsigned portable_contexts_face_count(void);
const char *portable_contexts_face_name(unsigned id);
enum { CT_HOME,CT_PRESETS,CT_ROOMS,CT_EDIT,CT_FIELD,CT_DISCARD,CT_MODELS };
static const t5_app_api_v1 *ctx_app;
static const risc_runtime_api_v1 *ctx_runtime;
static const contexts_service_v1 *ctx_service;
static portable_context_preset ctx_presets[8],ctx_draft;
static int32_t ctx_results[8];
static contexts_status_v1 ctx_status;
static contexts_model_details_v1 ctx_models[2];
static bool ctx_model_valid[2];
static unsigned ctx_model_source;
static contexts_label_v1 ctx_rooms[16];
static unsigned ctx_page,ctx_selected,ctx_list,ctx_room_scroll,ctx_room_count,ctx_field;
static bool ctx_enabled,ctx_enabled_valid,ctx_dirty,ctx_retained,ctx_exit,ctx_status_valid;
static bool ctx_draft_dirty,ctx_save_uncertain;
static bool ctx_room_return_edit;
static uint32_t ctx_refresh_at;
static const char *ctx_message;
static const uint16_t ctx_bits[]={PORTABLE_CONTEXT_SLEEP,PORTABLE_CONTEXT_IDLE,PORTABLE_CONTEXT_DEEP,
    PORTABLE_CONTEXT_BRIGHTNESS,PORTABLE_CONTEXT_VOLUME,PORTABLE_CONTEXT_DND,PORTABLE_CONTEXT_ALERT,PORTABLE_CONTEXT_FACE};
static const char *ctx_fields[]={"SLEEP MODE","IDLE SLEEP","DEEP SLEEP","BRIGHTNESS","VOLUME","DO NOT DISTURB","NOTIFICATIONS","WATCH FACE"};
static void ctx_note(const char *message){ctx_message=message;ctx_dirty=true;}
static bool ctx_storage_open(risc_runtime_capability_v1 *grant) {
    *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
    if(!portable_background_stop()){ctx_retained=true;return false;}
    if(!ctx_runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,grant)){ctx_note("SETTINGS UNAVAILABLE");return false;}
    return true;
}
static bool ctx_storage_close(risc_runtime_capability_v1 *grant) {
    if(grant->api&&!ctx_runtime->release(grant)){ctx_retained=true;return false;}
    return true;
}
static void ctx_load(void) {
    risc_runtime_capability_v1 grant;ctx_enabled_valid=false;
    for(unsigned i=0;i<8;i++){portable_context_preset_init(&ctx_presets[i]);ctx_results[i]=RISC_KEY_VALUE_CONTEXT;}
    if(!ctx_storage_open(&grant))return;
    const risc_key_value_v1 *kv=grant.api;
    ctx_enabled_valid=portable_context_enabled_load(kv,&ctx_enabled);
    bool unread=!ctx_enabled_valid;
    for(unsigned i=0;i<8;i++) {
        ctx_results[i]=portable_context_preset_load(kv,i,&ctx_presets[i]);
        if(ctx_results[i]!=RISC_KEY_VALUE_OK&&ctx_results[i]!=RISC_KEY_VALUE_NOT_FOUND)unread=true;
    }
    if(!ctx_storage_close(&grant))return;
    ctx_note(unread?"SAVED SETTINGS UNREAD":"");
}
static bool ctx_read_status(void) {
    contexts_status_v1 next={.struct_size=sizeof(next)};
    bool valid=ctx_service&&ctx_service->status(ctx_service->context,&next);
    bool changed=valid!=ctx_status_valid||(valid&&memcmp(&next,&ctx_status,sizeof(next)));
    ctx_status_valid=valid;if(valid)ctx_status=next;
    bool extended=ctx_service&&ctx_service->struct_size>=CONTEXTS_MODEL_DETAILS_V1_SIZE&&ctx_service->model_details;
    for(unsigned i=0;i<2;i++){
        contexts_model_details_v1 detail={.struct_size=sizeof(detail)};
        bool readable=extended&&ctx_service->model_details(ctx_service->context,i?CONTEXTS_RADIO:CONTEXTS_AUDIO,&detail);
        changed|=readable!=ctx_model_valid[i]||(readable&&memcmp(&detail,&ctx_models[i],sizeof(detail)));
        ctx_model_valid[i]=readable;if(readable)ctx_models[i]=detail;
    }
    return changed;
}
static void ctx_collect_rooms(void) {
    ctx_room_return_edit=ctx_page==CT_EDIT;
    ctx_room_count=0;ctx_room_scroll=0;
    if(!ctx_service){ctx_note("MONITOR UNAVAILABLE");return;}
    for(unsigned source=CONTEXTS_AUDIO;source<=CONTEXTS_RADIO;source++)for(unsigned slot=0;slot<8;slot++) {
        contexts_label_v1 label={0};int32_t result=ctx_service->label(ctx_service->context,source,slot,&label);
        if(result!=1)break;
        if(label.kind==1&&label.source==source&&label.slot==slot&&memchr(label.name,0,sizeof(label.name)))ctx_rooms[ctx_room_count++]=label;
    }
    ctx_page=CT_ROOMS;ctx_note(ctx_room_count?"CHOOSE A SAVED ROOM":"LOAD SAVED ROOM MODELS FIRST");
}
static void ctx_open_preset(unsigned slot) {
    if(slot>=8)return;
    if(ctx_results[slot]!=RISC_KEY_VALUE_OK&&ctx_results[slot]!=RISC_KEY_VALUE_NOT_FOUND){ctx_note("UNREAD PRESET / TAP RETRY");return;}
    ctx_selected=slot;ctx_draft=ctx_presets[slot];ctx_draft_dirty=ctx_save_uncertain=false;ctx_field=0;
    if(!ctx_draft.source)ctx_collect_rooms();else{ctx_page=CT_EDIT;ctx_note("");}
}
static void ctx_save(void) {
    if(ctx_save_uncertain){/* Retry exactly the same frozen draft. */}
    if(!ctx_draft.source){ctx_note("CHOOSE A ROOM FIRST");return;}
    if(ctx_draft.enabled&&!ctx_draft.actions){ctx_note("CHOOSE AN ACTION FIRST");return;}
    if((ctx_draft.actions&PORTABLE_CONTEXT_FACE)&&ctx_draft.face>=portable_contexts_face_count()){ctx_note("WATCH FACE UNAVAILABLE");return;}
    risc_runtime_capability_v1 grant;
    if(!ctx_storage_open(&grant))return;
    /* Never replace unread authoritative records on the strength of a stale
     * launch default. Re-read the selected slot before any first write. */
    portable_context_preset actual;int32_t loaded=portable_context_preset_load(grant.api,ctx_selected,&actual);
    bool readable=loaded==RISC_KEY_VALUE_OK||loaded==RISC_KEY_VALUE_NOT_FOUND;
    bool saved=readable&&portable_context_preset_save(grant.api,ctx_selected,&ctx_draft);
    if(!ctx_storage_close(&grant))return;
    if(!saved){ctx_save_uncertain=readable;ctx_note(readable?"SAVE UNCONFIRMED / RETRY":"PRESET UNREAD / RETRY LOAD");return;}
    ctx_presets[ctx_selected]=ctx_draft;ctx_results[ctx_selected]=RISC_KEY_VALUE_OK;
    ctx_draft_dirty=ctx_save_uncertain=false;ctx_page=CT_PRESETS;ctx_note("PRESET SAVED");
}
static void ctx_discard(void) {
    /* A failed put may already have committed. Reload before describing the
     * saved state; discard applies only to the local draft. */
    ctx_draft_dirty=ctx_save_uncertain=false;ctx_page=CT_PRESETS;ctx_load();
}
static void ctx_change(int direction) {
    if(ctx_save_uncertain){ctx_note("RETRY SAVE OR DISCARD");return;}
    unsigned n;
    switch(ctx_field) {
      case 0:ctx_draft.sleep=(uint8_t)((ctx_draft.sleep+(direction<0?2u:1u))%3u);break;
      case 1:n=ctx_draft.idle_ms/1000u;ctx_draft.idle_ms=(direction<0?(n>10?n-5:5):(n<3595?n+5:3600))*1000u;break;
      case 2:n=ctx_draft.deep_ms/60000u;ctx_draft.deep_ms=(direction<0?(n>1?n-1:1):(n<60?n+1:60))*60000u;break;
      case 3:n=ctx_draft.brightness;ctx_draft.brightness=(uint8_t)(direction<0?(n>20?n-10:10):(n<90?n+10:100));break;
      case 4:n=ctx_draft.volume;ctx_draft.volume=(uint8_t)(direction<0?(n>=10?n-10:0):(n<=90?n+10:100));break;
      case 5:ctx_draft.dnd=!ctx_draft.dnd;break;
      case 6:ctx_draft.alert=(uint8_t)(1u+(ctx_draft.alert-1u+(direction<0?2u:1u))%3u);break;
      case 7:n=portable_contexts_face_count();if(!n||n>256){ctx_note("WATCH FACES UNAVAILABLE");return;}ctx_draft.face=(uint8_t)((ctx_draft.face+(direction<0?n-1u:1u))%n);break;
    }
    ctx_draft_dirty=true;ctx_note("");
}
static void ctx_back(void) {
    if(ctx_page==CT_HOME){ctx_exit=true;return;}
    if(ctx_page==CT_PRESETS){ctx_page=CT_HOME;ctx_note("");return;}
    if(ctx_page==CT_MODELS){ctx_page=CT_HOME;ctx_note("");return;}
    if(ctx_page==CT_FIELD){ctx_page=CT_EDIT;ctx_note("");return;}
    if(ctx_page==CT_ROOMS&&ctx_room_return_edit){ctx_page=CT_EDIT;ctx_note("");return;}
    if(ctx_page==CT_DISCARD){ctx_page=CT_EDIT;ctx_note("");return;}
    if(ctx_draft_dirty||ctx_save_uncertain){ctx_page=CT_DISCARD;ctx_note("DISCARD LOCAL CHANGES?");return;}
    ctx_page=CT_PRESETS;ctx_note("");
}
static void ctx_load_models(void) {
    if(!ctx_enabled_valid||!ctx_enabled){ctx_note("TURN MONITORING ON FIRST");return;}
    if(!ctx_service){ctx_note("MONITOR UNAVAILABLE");return;}
    if(!portable_background_stop()){ctx_retained=true;return;}
    if(!ctx_service->request_export(ctx_service->context,CONTEXTS_ALL)){ctx_note("MODELS BUSY / RETRY");return;}
    ctx_exit=true; /* Clock owns the bounded owner-app rendezvous. */
}
static bool ctx_hit(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
static void ctx_tap(int x,int y) {
    if(x<0||y<0||x>=240||y>=240)return;
    if(ctx_hit(x,y,4,0,40,40)){ctx_back();return;}
    if(ctx_page==CT_HOME) {
        if(ctx_hit(x,y,12,42,216,36)) {
            if(!ctx_enabled_valid){ctx_load();return;}
            bool requested=!ctx_enabled;
            if(!portable_background_stop()){ctx_retained=true;return;}
            if(portable_contexts_enable(requested)){ctx_enabled=requested;ctx_note(requested?"ON / LOAD MODELS TO BEGIN":"MONITORING OFF");}
            else{ctx_enabled_valid=false;ctx_note("SAVE UNCONFIRMED / RETRY");}
        } else if(ctx_hit(x,y,12,80,216,49)){ctx_model_source=0;ctx_page=CT_MODELS;ctx_note("");}
        else if(ctx_hit(x,y,12,132,216,49)){ctx_model_source=1;ctx_page=CT_MODELS;ctx_note("");}
        else if(ctx_hit(x,y,12,188,104,44))ctx_load_models();
        else if(ctx_hit(x,y,124,188,104,44)){ctx_page=CT_PRESETS;ctx_list=0;ctx_note("");}
    } else if(ctx_page==CT_MODELS){
        if(ctx_hit(x,y,12,188,104,44))ctx_load_models();
        else if(ctx_hit(x,y,124,188,104,44))ctx_back();
    } else if(ctx_page==CT_PRESETS) {
        for(unsigned row=0;row<3;row++)if(ctx_hit(x,y,12,43+(int)row*46,216,42)&&ctx_list+row<8){ctx_open_preset(ctx_list+row);return;}
        if(ctx_hit(x,y,12,188,44,44)){if(ctx_list)ctx_list--;ctx_dirty=true;}
        else if(ctx_hit(x,y,64,188,44,44)){if(ctx_list+3<8)ctx_list++;ctx_dirty=true;}
        else if(ctx_hit(x,y,120,188,108,44))ctx_load();
    } else if(ctx_page==CT_ROOMS) {
        for(unsigned row=0;row<3;row++)if(ctx_hit(x,y,12,43+(int)row*46,216,42)&&ctx_room_scroll+row<ctx_room_count) {
            const contexts_label_v1 *room=&ctx_rooms[ctx_room_scroll+row];
            ctx_draft.source=(uint8_t)room->source;ctx_draft.slot=(uint8_t)room->slot;memcpy(ctx_draft.name,room->name,17);
            ctx_draft_dirty=true;ctx_page=CT_EDIT;ctx_note("CHOOSE ACTIONS, THEN SAVE");return;
        }
        if(ctx_hit(x,y,12,188,44,44)){if(ctx_room_scroll)ctx_room_scroll--;ctx_dirty=true;}
        else if(ctx_hit(x,y,64,188,44,44)){if(ctx_room_scroll+3<ctx_room_count)ctx_room_scroll++;ctx_dirty=true;}
        else if(ctx_hit(x,y,120,188,108,44))ctx_back();
    } else if(ctx_page==CT_EDIT) {
        if(ctx_hit(x,y,12,43,216,38)){if(ctx_save_uncertain)ctx_note("RETRY SAVE OR DISCARD");else ctx_collect_rooms();}
        else if(ctx_hit(x,y,12,87,216,32)){if(ctx_save_uncertain)ctx_note("RETRY SAVE OR DISCARD");else{ctx_draft.enabled=!ctx_draft.enabled;ctx_draft_dirty=true;ctx_note("");}}
        else if(ctx_hit(x,y,12,125,216,36)){ctx_page=CT_FIELD;ctx_note("");}
        else if(ctx_hit(x,y,12,188,104,44))ctx_save();
        else if(ctx_hit(x,y,124,188,104,44)){ctx_page=CT_DISCARD;ctx_note("DISCARD LOCAL CHANGES?");}
    } else if(ctx_page==CT_FIELD) {
        if(ctx_hit(x,y,12,80,216,36)) {
            if(ctx_save_uncertain)ctx_note("RETRY SAVE OR DISCARD");
            else if(ctx_field==7&&!portable_contexts_face_count())ctx_note("WATCH FACES UNAVAILABLE");
            else{ctx_draft.actions^=ctx_bits[ctx_field];ctx_draft_dirty=true;ctx_note("");}
        } else if(ctx_hit(x,y,12,128,44,44))ctx_change(-1);
        else if(ctx_hit(x,y,184,128,44,44))ctx_change(1);
        else if(ctx_hit(x,y,12,188,44,44)){ctx_field=(ctx_field+7)%8;ctx_note("");}
        else if(ctx_hit(x,y,64,188,44,44)){ctx_field=(ctx_field+1)%8;ctx_note("");}
        else if(ctx_hit(x,y,120,188,108,44)){ctx_page=CT_EDIT;ctx_note("");}
    } else if(ctx_page==CT_DISCARD) {
        if(ctx_hit(x,y,12,174,104,44))ctx_discard();
        else if(ctx_hit(x,y,124,174,104,44)){ctx_page=CT_EDIT;ctx_note("");}
    }
}
static const char *ctx_source_caption(const contexts_source_status_v1 *s) {
    if(s->model_state==CONTEXTS_MODEL_UNAVAILABLE)return "SOURCE UNAVAILABLE";
    if(s->model_state==CONTEXTS_MODEL_EMPTY)return "LOAD SAVED MODELS";
    if(s->model_state==CONTEXTS_MODEL_REQUESTED||s->model_state==CONTEXTS_MODEL_LOADING)return "LOADING MODELS";
    if(s->model_state==CONTEXTS_MODEL_FAILED)return "MODEL LOAD FAILED / RETRY";
    if(s->capture_error)return "INPUT UNAVAILABLE";
    if(s->room_ambiguous&&s->current)return "ROOM AMBIGUOUS";
    if(!s->current)return s->room_name[0]?"PAUSED / LAST ROOM":"PAUSED / NO CURRENT ROOM";
    return s->room_valid?"ROOM MATCH":"LEARNING / UNKNOWN ROOM";
}
static void ctx_draw_source(const char *title,const contexts_source_status_v1 *s,int y) {
    contexts_source_status_v1 shown=*s;
    if(!ctx_enabled_valid||!ctx_enabled){shown.current=shown.room_valid=shown.event_valid=false;s=&shown;}
    char line[40];portable_nova_text(1,14,y,210,title,NOVA_CYAN);
    if(!ctx_status_valid){portable_nova_text(2,14,y+17,212,"MONITOR UNAVAILABLE",NOVA_CAP);return;}
    portable_nova_text(2,70,y+1,156,ctx_source_caption(s),NOVA_CAP);
    snprintf(line,sizeof(line),"ROOM: %.16s",s->room_name[0]?s->room_name:"--");portable_nova_text(2,14,y+17,212,line,s->room_valid&&s->current?NOVA_WHITE:NOVA_CAP);
    unsigned i=s->source==CONTEXTS_RADIO?1u:0u;
    const char *engine=ctx_model_valid[i]&&s->event_valid&&s->current?
        ctx_models[i].event_engine==CONTEXTS_EVENT_NEURAL?"NEURAL":ctx_models[i].event_engine==CONTEXTS_EVENT_TEMPORAL?"TEMPORAL":"EVENT":"EVENT";
    snprintf(line,sizeof(line),"%s: %.16s",engine,s->event_valid&&s->current?s->event_name:"--");portable_nova_text(2,14,y+31,212,line,NOVA_TEXT);
}
static const char *ctx_import_caption(uint32_t state,int32_t error,bool neural){
    if(state==CONTEXTS_IMPORT_UNAVAILABLE)return "SOURCE UNAVAILABLE";
    if(state==CONTEXTS_IMPORT_READY)return "READY";
    if(state==CONTEXTS_IMPORT_MISSING)return neural?"NO SAVED MODEL":"NO SAVED EVENTS";
    if(error==RISC_APP_DATA_STALE||error==CONTEXTS_IMPORT_STALE)return "STALE / RELOAD";
    if(state==CONTEXTS_IMPORT_FAILED)return "LOAD FAILED / RETRY";
    return "SIGNATURES ONLY";
}
static void ctx_value(char *out,size_t size) {
    switch(ctx_field) {
      case 0:snprintf(out,size,"%s",ctx_draft.sleep==PORTABLE_SLEEP_LIGHT?"LIGHT":ctx_draft.sleep==PORTABLE_SLEEP_DEEP?"DEEP":"HYBRID");break;
      case 1:snprintf(out,size,"%lu SEC",(unsigned long)(ctx_draft.idle_ms/1000u));break;
      case 2:snprintf(out,size,"%lu MIN",(unsigned long)(ctx_draft.deep_ms/60000u));break;
      case 3:snprintf(out,size,"%u%%",ctx_draft.brightness);break;
      case 4:snprintf(out,size,"%u%%",ctx_draft.volume);break;
      case 5:snprintf(out,size,"%s",ctx_draft.dnd?"ON":"OFF");break;
      case 6:snprintf(out,size,"%s",portable_alert_name(ctx_draft.alert));break;
      default:{const char *name=ctx_draft.face<portable_contexts_face_count()?portable_contexts_face_name(ctx_draft.face):NULL;snprintf(out,size,"%s",name?name:"UNAVAILABLE");break;}
    }
}
static void ctx_draw(void) {
    char text[64];portable_nova_begin();
    const char *title=ctx_page==CT_HOME?"CONTEXTS":ctx_page==CT_MODELS?(ctx_model_source?"RADIO MODELS":"AUDIO MODELS"):ctx_page==CT_PRESETS?"ROOM PRESETS":ctx_page==CT_ROOMS?"SAVED ROOMS":ctx_page==CT_DISCARD?"DISCARD DRAFT?":"ROOM PRESET";
    portable_nova_text(0,12,9,30,"<",NOVA_CYAN);portable_nova_center(0,44,8,186,title,NOVA_CYAN);
    portable_nova_center(2,44,27,186,ctx_message?ctx_message:"",NOVA_CAP);
    if(ctx_page==CT_HOME) {
        portable_nova_button(12,42,216,36,!ctx_enabled_valid?"RETRY SAVED SETTINGS":ctx_enabled?"MONITORING: ON":"MONITORING: OFF",ctx_enabled_valid&&ctx_enabled);
        ctx_draw_source("AUDIO >",&ctx_status.audio,84);portable_nova_rule(12,129,216);ctx_draw_source("RADIO >",&ctx_status.radio,134);
        portable_nova_button(12,188,104,44,"LOAD MODELS",false);portable_nova_button(124,188,104,44,"PRESETS",false);
    } else if(ctx_page==CT_MODELS){
        const contexts_source_status_v1 *s=ctx_model_source?&ctx_status.radio:&ctx_status.audio;
        const contexts_model_details_v1 *d=&ctx_models[ctx_model_source];
        int32_t bank_error=d->bank_error[0]?d->bank_error[0]:d->bank_error[1];
        portable_nova_row(12,43,216,40,"SAVED SIGNATURES",s->signatures_ready?"READY":ctx_source_caption(s),false);
        portable_nova_row(12,87,216,40,"TEMPORAL EVENTS",ctx_model_valid[ctx_model_source]?ctx_import_caption(d->temporal_state,bank_error,false):"SIGNATURES ONLY",false);
        portable_nova_row(12,131,216,40,"NEURAL REFINEMENT",ctx_model_valid[ctx_model_source]?ctx_import_caption(d->neural_state,d->neural_error,true):"SIGNATURES ONLY",false);
        portable_nova_button(12,188,104,44,"LOAD MODELS",false);portable_nova_button(124,188,104,44,"BACK",false);
    } else if(ctx_page==CT_PRESETS||ctx_page==CT_ROOMS) {
        unsigned start=ctx_page==CT_PRESETS?ctx_list:ctx_room_scroll,total=ctx_page==CT_PRESETS?8:ctx_room_count;
        for(unsigned row=0;row<3&&start+row<total;row++) {
            unsigned slot=start+row;const char *caption,*detail;
            if(ctx_page==CT_PRESETS) {
                const portable_context_preset *p=&ctx_presets[slot];
                bool unread=ctx_results[slot]!=RISC_KEY_VALUE_OK&&ctx_results[slot]!=RISC_KEY_VALUE_NOT_FOUND;
                snprintf(text,sizeof(text),"%u  %.16s",slot+1,unread?"UNREAD PRESET":p->source?p->name:"EMPTY");caption=text;
                detail=unread?"RETRY BEFORE EDITING":!p->source?"CHOOSE A SAVED ROOM":p->enabled?"ENABLED":"DISABLED";
            } else {const contexts_label_v1 *room=&ctx_rooms[slot];caption=room->name;detail=room->source==CONTEXTS_AUDIO?"AUDIO ROOM":"RADIO ROOM";}
            portable_nova_row(12,43+(int)row*46,216,42,caption,detail,false);
        }
        if(!total)portable_nova_center(1,12,100,216,"NO SAVED ROOMS LOADED",NOVA_CAP);
        portable_nova_button(12,188,44,44,"<",false);portable_nova_button(64,188,44,44,">",false);
        portable_nova_button(120,188,108,44,ctx_page==CT_PRESETS?"RETRY":"BACK",false);
    } else if(ctx_page==CT_EDIT) {
        snprintf(text,sizeof(text),"%s: %.16s",ctx_draft.source==CONTEXTS_AUDIO?"AUDIO":"RADIO",ctx_draft.name);
        portable_nova_button(12,43,216,38,text,false);portable_nova_button(12,87,216,32,ctx_draft.enabled?"PRESET: ENABLED":"PRESET: DISABLED",ctx_draft.enabled);
        unsigned actions=0;for(unsigned i=0;i<8;i++)actions+=!!(ctx_draft.actions&ctx_bits[i]);
        snprintf(text,sizeof(text),"EDIT ACTIONS (%u / 8)",actions);portable_nova_button(12,125,216,36,text,false);
        portable_nova_button(12,188,104,44,ctx_save_uncertain?"RETRY SAVE":"SAVE",false);portable_nova_button(124,188,104,44,"DISCARD",false);
    } else if(ctx_page==CT_FIELD) {
        portable_nova_center(1,12,46,216,ctx_draft.name,NOVA_WHITE);
        snprintf(text,sizeof(text),"%u/8  %s",ctx_field+1,ctx_fields[ctx_field]);portable_nova_center(2,12,64,216,text,NOVA_CYAN);
        bool selected=(ctx_draft.actions&ctx_bits[ctx_field])!=0;portable_nova_button(12,80,216,36,selected?"APPLY THIS VALUE":"KEEP CURRENT VALUE",selected);
        ctx_value(text,sizeof(text));portable_nova_center(1,60,140,120,text,NOVA_WHITE);
        portable_nova_button(12,128,44,44,"-",false);portable_nova_button(184,128,44,44,"+",false);
        portable_nova_button(12,188,44,44,"<",false);portable_nova_button(64,188,44,44,">",false);portable_nova_button(120,188,108,44,"DONE",false);
    } else {
        portable_nova_wrap(1,16,65,208,20,4,"Discard your local changes? A save that was unconfirmed may already be stored.",NOVA_TEXT);
        portable_nova_button(12,174,104,44,"DISCARD",false);portable_nova_button(124,174,104,44,"KEEP EDITING",false);
    }
    ctx_app->present(false);ctx_dirty=false;
}
static void ctx_retain(void) {
    while(!portable_background_stop())ctx_runtime->yield_ms(50);
    ctx_runtime->diagnostic("CONTEXTS cleanup-unconfirmed; invocation retained");
    for(;;)ctx_runtime->yield_ms(50);
}
void app_main(void) {
    ctx_app=t5_app_get_api(1);ctx_runtime=risc_runtime_get_api(1);
    if(!ctx_app||!ctx_app->poll||!ctx_app->present||!ctx_app->millis||!ctx_runtime||!ctx_runtime->acquire||!ctx_runtime->release||!ctx_runtime->yield_ms||!ctx_runtime->diagnostic)return;
    ctx_service=portable_contexts_service();ctx_page=CT_HOME;ctx_selected=ctx_list=ctx_room_scroll=ctx_room_count=ctx_field=0;
    ctx_enabled=ctx_enabled_valid=ctx_retained=ctx_exit=ctx_status_valid=ctx_draft_dirty=ctx_save_uncertain=false;
    ctx_status=(contexts_status_v1){0};ctx_message="";ctx_refresh_at=0;ctx_dirty=true;ctx_load();
    while(!ctx_exit) {
        if(ctx_retained)ctx_retain();
        uint32_t now=ctx_app->millis();
        if(!ctx_refresh_at||(uint32_t)(now-ctx_refresh_at)>=250){if(ctx_read_status())ctx_dirty=true;ctx_refresh_at=now;}
        if(ctx_dirty)ctx_draw();
        t5_app_input_t input={0};if(!ctx_app->poll(&input,30))break;
        if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK)){ctx_back();continue;}
        if(input.tapped)ctx_tap(input.touch_x,input.touch_y);
        if(ctx_page==CT_FIELD) {
            if(input.buttons&T5_APP_BUTTON_LEFT)ctx_change(-1);
            if(input.buttons&T5_APP_BUTTON_RIGHT)ctx_change(1);
            if(input.buttons&T5_APP_BUTTON_UP){ctx_field=(ctx_field+7)%8;ctx_note("");}
            if(input.buttons&T5_APP_BUTTON_DOWN){ctx_field=(ctx_field+1)%8;ctx_note("");}
        }
    }
    if(ctx_retained)ctx_retain();
}
