/* Capability-selected NOVA paper controller. Shared adapter owns fonts, icons,
 * reader flip, frame custody, Quick Controls and Light; this app owns drafts. */
#include "PortableAppLaunchGuard.h"
#include "PaperFrame.h"
static portable_touch_scroll ctxp_scroll;
typedef struct {unsigned page,field,source,generation;int q8;} ctxp_image;
static ctxp_image ctxp_submitted,ctxp_completed,ctxp_down;
static unsigned ctxp_generation,ctxp_page=UINT32_MAX,ctxp_field,ctxp_source;
static bool ctxp_hit_ready,ctxp_was_available;
static const char *ctxp_destination;
static void ctx_paper_cancel_exit(void){ctxp_destination=NULL;}
extern uint16_t portable_contexts_action_mask(void);
static bool ctx_paper_field_available(unsigned field) {
    return field<8&&(portable_contexts_action_mask()&ctx_bits[field]);
}
static void ctx_paper_reset(void) {
    ctxp_scroll=(portable_touch_scroll){0};ctxp_submitted=ctxp_completed=ctxp_down=(ctxp_image){0};
    ctxp_page=UINT32_MAX;ctxp_field=ctxp_source=ctxp_generation=0;
    ctxp_hit_ready=false;ctxp_was_available=true;ctxp_destination=NULL;
}
static bool ctx_paper_leave(const char *destination) {
    if(!portable_paper_scroll_settled()){ctx_note("WAIT FOR DISPLAY / RETRY");return false;}
    if(!portable_background_stop()){ctx_retained=true;return false;}
    if(!ctx_runtime->request_launch||!ctx_runtime->request_launch(destination)){ctx_note("APP UNAVAILABLE / RETRY");return false;}
    ctx_exit=true;return true;
}
bool portable_app_before_launch(const char *destination) {
    if(ctx_paper&&(ctx_draft_dirty||ctx_save_uncertain)){
        ctxp_destination=destination;ctx_page=CT_DISCARD;ctx_note("SAVE OR DISCARD BEFORE LEAVING");return false;
    }
    return true;
}
static void ctx_paper_discarded(void) {
    const char *destination=ctxp_destination;ctxp_destination=NULL;
    if(destination&&!ctx_retained)(void)ctx_paper_leave(destination);
}
static unsigned ctxp_count(void) {
    if(ctx_page==CT_PRESETS)return 8;
    if(ctx_page==CT_ROOMS)return ctx_room_count;
    if(ctx_page==CT_EDIT)return 10;
    return 0;
}
static void ctxp_prepare(void) {
    if(ctxp_page!=ctx_page||ctxp_field!=ctx_field||ctxp_source!=ctx_model_source){
        ctxp_page=ctx_page;ctxp_field=ctx_field;ctxp_source=ctx_model_source;ctxp_generation++;
        ctxp_scroll=(portable_touch_scroll){0};ctxp_hit_ready=false;
    }
    portable_scroll_configure(&ctxp_scroll,(portable_scroll_viewport){24,152,ctx_app->screen_width()-48,ctx_app->screen_height()-302},84,(int)ctxp_count());
    if(portable_paper_scroll_settled())ctxp_completed=ctxp_submitted;
}
static void ctxp_text(int x,int y,int w,const char *s,bool heading,bool black) {
    ctx_paper->text(x,y,w,s,1,heading,black);
}
static void ctxp_rule(int y){ctx_app->fill_rect(32,y,ctx_app->screen_width()-64,2,true);}
static void ctxp_icon(int x,int y,const char *icon,bool black) {
    if(ctx_app->draw_icon)ctx_app->draw_icon(x,y,icon,24,black);
}
static void ctxp_button(int x,int y,int w,int h,const char *s,bool selected) {
    ctx_app->fill_rect(x,y,w,h,true);
    if(!selected)ctx_app->fill_rect(x+3,y+3,w-6,h-6,false);
    int measured=ctx_paper->measure(s,false);
    ctxp_text(x+(measured<w-16?(w-measured)/2:8),y+(h-24)/2,w-16,s,false,!selected);
}
static void ctxp_wrap(int y,const char *s,unsigned lines) {
    unsigned at=0;int width=ctx_app->screen_width()-64;
    for(unsigned row=0;row<lines&&s&&s[at];row++){
        char line[97];unsigned n=0,last_space=0;
        while(n<96&&s[at+n]){line[n]=s[at+n];line[n+1]=0;if(ctx_paper->measure(line,false)>width)break;if(line[n]==' ')last_space=n;n++;}
        if(!n)break;
        if(s[at+n]&&last_space)n=last_space;
        line[n]=0;ctxp_text(32,y+(int)row*28,width,line,false,true);at+=n;while(s[at]==' ')at++;
    }
}
static void ctxp_row(int y,const char *title,const char *detail,const char *icon,bool selected) {
    int w=ctx_app->screen_width();
    if(selected)ctx_app->fill_rect(24,y,w-48,80,true);
    ctxp_text(40,y+10,w-120,title,true,!selected);
    ctxp_text(40,y+46,w-120,detail,false,!selected);
    if(icon)ctxp_icon(w-66,y+29,icon,!selected);
    ctx_app->fill_rect(32,y+81,w-64,2,true);
}
static void ctxp_draw_source(int y,const char *title,unsigned source) {
    const contexts_source_status_v1 *s=source?&ctx_status.radio:&ctx_status.audio;
    contexts_source_status_v1 shown=*s;
    if(!ctx_enabled_valid||!ctx_enabled){shown.current=shown.room_valid=shown.event_valid=false;s=&shown;}
    ctxp_text(40,y,360,title,true,true);ctxp_icon(ctx_app->screen_width()-66,y+24,"solid:f054",true);
    ctxp_text(40,y+31,ctx_app->screen_width()-112,ctx_status_valid?ctx_source_caption(s):"MONITOR UNAVAILABLE",false,true);
    char text[64];bool current=ctx_status_valid&&ctx_enabled_valid&&ctx_enabled&&s->current;
    snprintf(text,sizeof(text),"ROOM: %.16s",current&&s->room_valid?s->room_name:s->room_name[0]?s->room_name:"--");
    ctxp_text(40,y+59,ctx_app->screen_width()-80,text,false,true);
    const char *engine=ctx_model_valid[source]&&ctx_models[source].event_engine==CONTEXTS_EVENT_NEURAL?"NEURAL":ctx_model_valid[source]&&ctx_models[source].event_engine==CONTEXTS_EVENT_TEMPORAL?"TEMPORAL":"EVENT";
    snprintf(text,sizeof(text),"%s: %.16s",engine,current&&s->event_valid?s->event_name:"--");
    ctxp_text(40,y+85,ctx_app->screen_width()-80,text,false,true);ctxp_rule(y+118);
}
static void ctx_paper_draw(void) {
    ctxp_prepare();
    if(!portable_paper_scroll_available()||!paper_frame_ready())return;
    char text[96];int w=ctx_app->screen_width(),h=ctx_app->screen_height();ctx_paper->begin();
    const char *title=ctx_page==CT_HOME?"CONTEXTS":ctx_page==CT_MODELS?(ctx_model_source?"RADIO MODELS":"AUDIO MODELS"):ctx_page==CT_PRESETS?"ROOM PRESETS":ctx_page==CT_ROOMS?"SAVED ROOMS":ctx_page==CT_DISCARD?"DISCARD DRAFT?":ctx_page==CT_FIELD?"PRESET ACTION":"ROOM PRESET";
    ctxp_icon(36,67,"solid:f053",true);ctxp_text(88,62,w-120,title,true,true);ctxp_rule(108);
    ctxp_text(32,118,w-64,ctx_page==CT_FIELD?ctx_draft.name:ctx_page==CT_EDIT?"CHANGES STAY LOCAL UNTIL SAVE":"AWAKE ROOM MONITOR",false,true);
    if(ctx_page==CT_HOME){
        ctxp_button(32,160,w-64,64,!ctx_enabled_valid?"RETRY SAVED SETTINGS":ctx_enabled?"MONITORING ON":"MONITORING OFF",ctx_enabled_valid&&ctx_enabled);
        ctxp_draw_source(246,"RADIO",1);ctxp_draw_source(382,"AUDIO",0);
        ctxp_row(522,"ROOM PRESETS","SAVED ROOM ACTIONS","solid:f054",false);
    }else if(ctx_page==CT_MODELS){
        const contexts_source_status_v1 *s=ctx_model_source?&ctx_status.radio:&ctx_status.audio;
        const contexts_model_details_v1 *d=&ctx_models[ctx_model_source];
        bool available=ctx_status_valid&&s->model_state!=CONTEXTS_MODEL_UNAVAILABLE;
        ctxp_row(164,"SAVED SIGNATURES",!available?"SOURCE UNAVAILABLE":s->signatures_ready?"READY":ctx_source_caption(s),NULL,false);
        ctxp_row(264,"TEMPORAL EVENTS",!available?"SOURCE UNAVAILABLE":ctx_model_valid[ctx_model_source]?ctx_import_caption(d->temporal_state,d->bank_error[0]?d->bank_error[0]:d->bank_error[1],false):"SIGNATURES ONLY",NULL,false);
        ctxp_row(364,"NEURAL REFINEMENT",!available?"SOURCE UNAVAILABLE":ctx_model_valid[ctx_model_source]?ctx_import_caption(d->neural_state,d->neural_error,true):"SIGNATURES ONLY",NULL,false);
        ctxp_wrap(476,!available?"This source is not supplied by the monitoring service.":"Reload reads the saved models from their original app. Training remains in that app.",4);
    }else if(ctx_page==CT_PRESETS||ctx_page==CT_ROOMS||ctx_page==CT_EDIT){
        portable_paper_scroll_clip(&ctxp_scroll.view);
        for(unsigned i=0;i<ctxp_count();i++){
            int y=portable_scroll_row_y(&ctxp_scroll,(int)i);if(y+84<=ctxp_scroll.view.y||y>=ctxp_scroll.view.y+ctxp_scroll.view.height)continue;
            const char *title=NULL,*detail=NULL;bool selected=false;
            if(ctx_page==CT_PRESETS){
                const portable_context_preset *p=&ctx_presets[i];bool unread=ctx_results[i]!=RISC_KEY_VALUE_OK&&ctx_results[i]!=RISC_KEY_VALUE_NOT_FOUND;
                snprintf(text,sizeof(text),"%u  %.16s",i+1,unread?"UNREAD PRESET":p->source?p->name:"EMPTY");title=text;
                detail=unread?"RETRY BEFORE EDITING":!p->source?"CHOOSE A SAVED ROOM":p->enabled?"ENABLED":"DISABLED";
            }else if(ctx_page==CT_ROOMS){title=ctx_rooms[i].name;detail=ctx_rooms[i].source==CONTEXTS_RADIO?"RADIO ROOM":"AUDIO ROOM";
            }else if(i==0){title=ctx_draft.name;detail=ctx_draft.source==CONTEXTS_RADIO?"RADIO / CHANGE ROOM":"AUDIO / CHANGE ROOM";
            }else if(i==1){title="PRESET";detail=ctx_draft.enabled?"ENABLED":"DISABLED";selected=ctx_draft.enabled;
            }else{unsigned field=i-2;title=ctx_fields[field];bool apply=(ctx_draft.actions&ctx_bits[field])!=0;
                detail=!ctx_paper_field_available(field)?apply?"UNAVAILABLE / SAVED ACTION":"UNAVAILABLE ON THIS DEVICE":apply?"APPLY SAVED VALUE":"KEEP CURRENT VALUE";
            }
            ctxp_row(y,title,detail,"solid:f054",selected);
        }
        portable_paper_scroll_clip(NULL);
        if(!ctxp_count())ctxp_wrap(220,"No saved room models are loaded. Return and load models first.",4);
        if(ctxp_scroll.limit){int rail=ctxp_scroll.view.height,thumb=rail*rail/(rail+ctxp_scroll.limit);ctx_app->fill_rect(w-18,ctxp_scroll.view.y+portable_scroll_offset(&ctxp_scroll)*(rail-thumb)/ctxp_scroll.limit,4,thumb,true);}
    }else if(ctx_page==CT_FIELD){
        snprintf(text,sizeof(text),"%u / 8  %s",ctx_field+1,ctx_fields[ctx_field]);ctxp_text(32,176,w-64,text,true,true);
        bool available=ctx_paper_field_available(ctx_field),selected=(ctx_draft.actions&ctx_bits[ctx_field])!=0;
        ctxp_button(32,240,w-64,76,selected?"APPLY THIS VALUE":available?"KEEP CURRENT VALUE":"UNAVAILABLE",selected);
        ctx_value(text,sizeof(text));ctxp_text(112,366,w-224,text,true,true);
        ctxp_button(32,348,64,70,"-",false);ctxp_button(w-96,348,64,70,"+",false);
        if(!available)ctxp_wrap(458,selected?"This saved action cannot run here. Tap above to remove it, or keep the stored value unchanged.":"This action is unavailable with this device's capabilities.",4);
        else ctxp_wrap(458,"Choose whether this room changes the value. Save the preset to commit your changes.",4);
    }else ctxp_wrap(184,"Discard your local changes? A save that was unconfirmed may already be stored. Discard reloads the saved record.",8);
    ctxp_wrap(h-144,ctx_message?ctx_message:"",2);
    if(ctx_page==CT_FIELD){
        ctxp_button(32,h-80,70,56,"<",false);ctxp_button(110,h-80,70,56,">",false);ctxp_button(188,h-80,w-220,56,"DONE",false);
    }else{
        const char *left=ctx_page==CT_HOME||ctx_page==CT_MODELS?"LOAD MODELS":ctx_page==CT_EDIT?(ctx_save_uncertain?"RETRY SAVE":"SAVE"):ctx_page==CT_DISCARD?"DISCARD":ctx_page==CT_PRESETS?"RETRY":"BACK";
        const char *right=ctx_page==CT_HOME?"PRESETS":ctx_page==CT_EDIT?"DISCARD":ctx_page==CT_DISCARD?"KEEP EDITING":"BACK";
        ctxp_button(32,h-80,(w-76)/2,56,left,false);ctxp_button((w+12)/2,h-80,(w-76)/2,56,right,false);
    }
    ctxp_submitted=(ctxp_image){ctx_page,ctx_field,ctx_model_source,ctxp_generation,ctxp_scroll.position_q8};
    ctx_app->present(false);ctx_dirty=false;
    if(portable_paper_scroll_settled())ctxp_completed=ctxp_submitted;
}
static void ctxp_toggle_monitor(void) {
    if(!ctx_enabled_valid){ctx_load();return;}
    bool requested=!ctx_enabled;
    if(!portable_background_stop()){ctx_retained=true;return;}
    if(portable_contexts_enable(requested)){ctx_enabled=requested;ctx_note(requested?"ON / LOAD MODELS TO BEGIN":"MONITORING OFF");}
    else{ctx_enabled_valid=false;ctx_note("SAVE UNCONFIRMED / RETRY");}
}
static void ctx_paper_tap(int x,int y) {
    int w=ctx_app->screen_width(),h=ctx_app->screen_height();
    if(x<0||y<0||x>=w||y>=h)return;
    if(!portable_paper_scroll_settled()){ctx_note("WAIT FOR DISPLAY / RETRY");return;}
    if(ctx_hit(x,y,24,44,56,64)){ctxp_destination=NULL;ctx_back();return;}
    if(y>=h-80&&y<h-24&&x>=32&&x<w-32){
        if(ctx_page==CT_FIELD){if(x<102){ctx_field=(ctx_field+7)%8;ctx_note("");}else if(x>=110&&x<180){ctx_field=(ctx_field+1)%8;ctx_note("");}else if(x>=188){ctx_page=CT_EDIT;ctx_note("");}return;}
        bool left=x<(w-12)/2,right=x>=(w+12)/2;if(!left&&!right)return;
        if(left){
            if(ctx_page==CT_HOME||ctx_page==CT_MODELS)ctx_load_models();
            else if(ctx_page==CT_EDIT)ctx_save();
            else if(ctx_page==CT_DISCARD)ctx_discard();
            else if(ctx_page==CT_PRESETS)ctx_load();else ctx_back();
        }else if(ctx_page==CT_HOME){ctx_page=CT_PRESETS;ctx_note("");}
        else if(ctx_page==CT_EDIT){ctx_page=CT_DISCARD;ctxp_destination=NULL;ctx_note("DISCARD LOCAL CHANGES?");}
        else if(ctx_page==CT_DISCARD){ctx_page=CT_EDIT;ctxp_destination=NULL;ctx_note("");}else ctx_back();
        return;
    }
    if(ctx_page==CT_HOME){
        if(ctx_hit(x,y,32,160,w-64,64))ctxp_toggle_monitor();
        else if(ctx_hit(x,y,32,240,w-64,130)){ctx_model_source=1;ctx_page=CT_MODELS;ctx_note("");}
        else if(ctx_hit(x,y,32,376,w-64,130)){ctx_model_source=0;ctx_page=CT_MODELS;ctx_note("");}
        else if(ctx_hit(x,y,24,522,w-48,84)){ctx_page=CT_PRESETS;ctx_note("");}
    }else if(ctx_page==CT_FIELD){
        if(ctx_hit(x,y,32,240,w-64,76)){
            if(ctx_save_uncertain)ctx_note("RETRY SAVE OR DISCARD");
            else if(ctx_paper_field_available(ctx_field)||(ctx_draft.actions&ctx_bits[ctx_field])){ctx_draft.actions^=ctx_bits[ctx_field];ctx_draft_dirty=true;ctx_note("");}
        }else if(ctx_paper_field_available(ctx_field)&&ctx_hit(x,y,32,348,64,70))ctx_change(-1);
        else if(ctx_paper_field_available(ctx_field)&&ctx_hit(x,y,w-96,348,64,70))ctx_change(1);
    }else if(ctx_page==CT_PRESETS||ctx_page==CT_ROOMS||ctx_page==CT_EDIT){
        portable_touch_scroll visible=ctxp_scroll;visible.position_q8=ctxp_down.q8;
        int row=portable_scroll_row(&visible,x,y);if(row<0)return;
        if(ctx_page==CT_PRESETS)ctx_open_preset((unsigned)row);
        else if(ctx_page==CT_ROOMS){const contexts_label_v1 *room=&ctx_rooms[row];ctx_draft.source=(uint8_t)room->source;ctx_draft.slot=(uint8_t)room->slot;memcpy(ctx_draft.name,room->name,17);ctx_draft_dirty=true;ctx_page=CT_EDIT;ctx_note("CHOOSE ACTIONS, THEN SAVE");}
        else if(row==0){if(ctx_save_uncertain)ctx_note("RETRY SAVE OR DISCARD");else ctx_collect_rooms();}
        else if(row==1){if(ctx_save_uncertain)ctx_note("RETRY SAVE OR DISCARD");else{ctx_draft.enabled=!ctx_draft.enabled;ctx_draft_dirty=true;ctx_note("");}}
        else{ctx_field=(unsigned)row-2;ctx_page=CT_FIELD;ctx_note("");}
    }
}
static void ctx_paper_input(t5_app_input_t *input) {
    ctxp_prepare();input->tapped=false;
    bool available=portable_paper_scroll_available();
    if(available&&ctxp_count()&&(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_UP))){
        int row=portable_scroll_offset(&ctxp_scroll)/84+(input->buttons&T5_APP_BUTTON_DOWN?5:-1);
        if(row<0)row=0;
        if(row>=(int)ctxp_count())row=(int)ctxp_count()-1;
        portable_scroll_reveal(&ctxp_scroll,row);ctx_dirty=true;
    }
    if(!available||input->buttons||input->exit_requested){portable_scroll_cancel(&ctxp_scroll);ctxp_hit_ready=false;ctxp_was_available=available;return;}
    if(!ctxp_was_available){ctx_dirty=true;ctxp_was_available=true;}
    portable_touch_sample sample;uint32_t now;portable_paper_scroll_sample(&sample,&now);
    if(sample.began){ctxp_down=ctxp_completed;ctxp_hit_ready=ctxp_down.generation==ctxp_generation&&ctxp_down.page==ctx_page;}
    unsigned change=portable_scroll_touch(&ctxp_scroll,&sample,now);
    if(change&PORTABLE_SCROLL_BACK){input->buttons=T5_APP_BUTTON_BACK;ctxp_hit_ready=false;ctxp_destination=NULL;}
    else if((change&PORTABLE_SCROLL_TAP)&&ctxp_hit_ready&&ctxp_down.generation==ctxp_generation){input->tapped=true;input->touch_x=(int16_t)sample.x;input->touch_y=(int16_t)sample.y;}
    if(change&PORTABLE_SCROLL_CHANGED)ctx_dirty=true;
}
