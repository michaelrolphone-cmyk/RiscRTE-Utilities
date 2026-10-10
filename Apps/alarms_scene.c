/* Alarms 0.3.0: domain intent and restorable state only. Presentation is an
 * optional external service. This translation unit has no display, input,
 * font, geometry, product, or form-factor dependencies. */
#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "RiscSceneStateV1.h"
#include "RiscAppDataV1.h"
#include "AlarmControlV1.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { OVERVIEW=1,EDIT_TIME=2,EDIT_VOLUME=3,CONFIRM_CANCEL=4,CONFIRM_RESET=5 };
enum { CHANGE_TIME=1,ARM_NEXT,SAVE_VOLUME,CHANGE_VOLUME,DISCARD_TIME,DISCARD_VOLUME,
       CANCEL_ALARM,DISMISS,REFRESH,RETRY_COMMAND,RESET_DRAFT,BACK_TO_OVERVIEW };
#define PAYLOAD_SIZE 132u
#define CHECKPOINT_SIZE (RISC_SCENE_STATE_HEADER+PAYLOAD_SIZE)
#define CHECKPOINT_NAME "alarms.scene"
#define CHECKPOINT_SCHEMA 1u

typedef struct {
    uint32_t minutes,volume,base_revision,base_volume,time_dirty,volume_dirty,pending;
    int32_t pending_result;
    alarm_control_command_v1 command;
} model;
static model state;
static alarm_control_snapshot_v1 status;
static risc_scene_document_v1 document,candidate;
static risc_scene_navigation_v1 navigation;
static const risc_runtime_api_v1 *runtime;
static const risc_scene_api_v1 *scene;
static const alarm_control_api_v1 *control;
static const risc_app_data_v1 *storage;
static risc_runtime_capability_v1 grants[3];
static unsigned acquired;
static uint64_t session,last_event;
static bool terminal,recovery,storage_known,storage_present,force_read,command_not_attempted;
static uint8_t stored[CHECKPOINT_SIZE];static uint32_t stored_size;
static char notice[72];

static void message(const char *text){snprintf(notice,sizeof(notice),"%s",text);}
static void retain(void){
    terminal=true;
    /* No provider calls, diagnostics, frees, or releases after this fence. */
    (void)runtime->retain_invocation();
}
static bool context_alive(void){
    if(terminal)return false;
    if(risc_runtime_get_api(1)!=runtime){terminal=true;return false;}
    return true;
}
static bool storage_terminal(int32_t rc){
    if(!context_alive())return true;
    if(rc==RISC_APP_DATA_CONTEXT||rc==RISC_APP_DATA_RETAINED){retain();return true;}return false;
}
static bool service_terminal(int32_t rc){if(!context_alive())return true;if(rc==ALARM_CONTROL_RETAINED){retain();return true;}return false;}
static bool scene_terminal(int32_t rc){if(!context_alive())return true;if(rc==RISC_SCENE_RETAINED){retain();return true;}return false;}
static const char *result_message(int32_t rc){
    switch(rc){
    case ALARM_CONTROL_OK:return "SAVED - CHECKING SCHEDULER";
    case ALARM_CONTROL_PENDING:return "REQUEST ACCEPTED - CHECKING";
    case ALARM_CONTROL_STALE:return "SAVED DATA CHANGED - REVIEW DRAFT";
    case ALARM_CONTROL_STORAGE:return "STORAGE UNAVAILABLE - RETRY";
    case ALARM_CONTROL_CLOCK:return "CLOCK OR TIME ZONE INVALID";
    case ALARM_CONTROL_AMBIGUOUS:return "TIME GAP OR FOLD - CHOOSE ANOTHER";
    case ALARM_CONTROL_BLOCKED:return "SCHEDULER BLOCKED - RETRY";
    case ALARM_CONTROL_UNCERTAIN:return "SAVE UNCONFIRMED - RETRY EXACT SAVE";
    case ALARM_CONTROL_EXPIRED:return "PENDING DEADLINE PASSED - RESET DRAFT";
    default:return "COMMAND INVALID - REVIEW DRAFT";
    }
}
static void root_navigation(void){navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={OVERVIEW}};}
static void fresh_model(void){
    state=(model){.minutes=status.minutes,.volume=status.volume,.base_revision=status.revision,.base_volume=status.volume};
    state.command.struct_size=sizeof(state.command);
}
static void encode_payload(uint8_t out[PAYLOAD_SIZE]){
    const uint32_t words[]={state.minutes,state.volume,state.base_revision,state.base_volume,state.time_dirty,state.volume_dirty,state.pending,(uint32_t)state.pending_result,ALARM_CONTROL_PROTOCOL};
    for(unsigned i=0;i<9;i++)risc_scene_put32(out+i*4,words[i]);
    memset(out+36,0,ALARM_CONTROL_COMMAND_BYTES);
    if(state.pending)memcpy(out+36,state.command.bytes,ALARM_CONTROL_COMMAND_BYTES);
}
static bool decode_payload(const uint8_t *in,size_t size){
    if(size!=PAYLOAD_SIZE||risc_scene_get32(in+32)!=ALARM_CONTROL_PROTOCOL)return false;
    model m={0};m.minutes=risc_scene_get32(in);m.volume=risc_scene_get32(in+4);m.base_revision=risc_scene_get32(in+8);
    m.base_volume=risc_scene_get32(in+12);m.time_dirty=risc_scene_get32(in+16);m.volume_dirty=risc_scene_get32(in+20);m.pending=risc_scene_get32(in+24);
    m.pending_result=(int32_t)risc_scene_get32(in+28);m.command.struct_size=sizeof(m.command);
    if(m.minutes>1439||m.volume>100||m.base_volume>100||m.time_dirty>1||m.volume_dirty>1||m.pending>1||m.pending_result>1||m.pending_result<-10)return false;
    if(m.pending)memcpy(m.command.bytes,in+36,ALARM_CONTROL_COMMAND_BYTES);
    else for(unsigned i=36;i<PAYLOAD_SIZE;i++)if(in[i])return false;
    state=m;return true;
}
static int32_t read_file(uint8_t *buffer,uint32_t *size,uint64_t *revision){
    for(unsigned attempt=0;attempt<2;attempt++){
        *size=0;*revision=0;int32_t rc=storage->stat(storage->context,CHECKPOINT_NAME,size,revision);
        if(storage_terminal(rc))return rc;
        if(rc!=RISC_APP_DATA_OK)return rc;
        if(*size>CHECKPOINT_SIZE)return RISC_APP_DATA_BUFFER_SMALL;
        uint64_t actual=0;rc=storage->read(storage->context,CHECKPOINT_NAME,*revision,buffer,CHECKPOINT_SIZE,size,&actual);
        if(storage_terminal(rc))return rc;
        if(rc==RISC_APP_DATA_STALE)continue;
        if(rc==RISC_APP_DATA_OK)*revision=actual;
        return rc;
    }
    return RISC_APP_DATA_STALE;
}
static bool load_checkpoint(void){
    uint8_t bytes[CHECKPOINT_SIZE],payload[PAYLOAD_SIZE];uint32_t size=0;uint64_t revision=0;
    int32_t rc=read_file(bytes,&size,&revision);if(terminal)return false;
    if(rc==RISC_APP_DATA_NOT_FOUND){storage_known=true;storage_present=false;stored_size=0;root_navigation();fresh_model();recovery=false;return true;}
    if(rc!=RISC_APP_DATA_OK){recovery=true;message("DRAFT UNREADABLE - RETRY OR RESET");return false;}
    storage_known=true;storage_present=true;stored_size=size;memcpy(stored,bytes,size);
    risc_scene_navigation_v1 nav;size_t length=0;
    if(!risc_scene_state_decode(bytes,size,CHECKPOINT_SCHEMA,&nav,payload,sizeof(payload),&length)||!decode_payload(payload,length)){
        recovery=true;message("DRAFT INVALID - RETRY OR RESET");return false;
    }
    /* App schema owns valid logical routes; renderer validates node focus. */
    if(nav.routes[0]!=OVERVIEW){recovery=true;message("UNKNOWN DRAFT ROUTE");return false;}
    for(unsigned i=0;i<nav.depth;i++)if(nav.routes[i]<OVERVIEW||nav.routes[i]>CONFIRM_RESET){recovery=true;message("UNKNOWN DRAFT ROUTE");return false;}
    navigation=nav;recovery=false;
    if(state.pending)message("UNFINISHED SAVE - RETRY TO RECONCILE");
    return true;
}
static bool checkpoint(bool explicit_reset){
    uint8_t payload[PAYLOAD_SIZE],desired[CHECKPOINT_SIZE],actual[CHECKPOINT_SIZE];size_t length=0;
    if(recovery&&!explicit_reset){message("DRAFT RECOVERY REQUIRED");return false;}
    uint32_t flags=0;
    if(session){int32_t rc=scene->snapshot(scene->context,session,&navigation,&flags);if(scene_terminal(rc)||rc!=0)return false;}
    encode_payload(payload);
    if(!risc_scene_state_encode(desired,sizeof(desired),CHECKPOINT_SCHEMA,&navigation,payload,sizeof(payload),&length)){message("DRAFT ENCODING FAILED");return false;}
    if(!explicit_reset&&storage_known&&storage_present&&stored_size==length&&!memcmp(stored,desired,length))return true;
    uint32_t size=0;uint64_t revision=0;int32_t rc=read_file(actual,&size,&revision);if(terminal)return false;
    bool present=rc==RISC_APP_DATA_OK||(explicit_reset&&rc==RISC_APP_DATA_BUFFER_SMALL);
    if(rc!=RISC_APP_DATA_OK&&rc!=RISC_APP_DATA_NOT_FOUND&&!(explicit_reset&&rc==RISC_APP_DATA_BUFFER_SMALL)){message("DRAFT STORAGE UNAVAILABLE - RETRY");return false;}
    if(present&&size==length&&!memcmp(actual,desired,length))goto confirmed;
    if(!explicit_reset&&(!storage_known||present!=storage_present||(present&&(size!=stored_size||memcmp(actual,stored,size))))){message("DRAFT CHANGED ELSEWHERE - RETRY LOAD");return false;}
    rc=storage->replace(storage->context,CHECKPOINT_NAME,revision,desired,(uint32_t)length);
    if(storage_terminal(rc))return false;
    if(rc!=RISC_APP_DATA_OK){
        /* A completed replace may report uncertain. Only exact fresh readback
         * permits a domain commit to follow this journal operation. */
        int32_t readback=read_file(actual,&size,&revision);if(terminal)return false;
        if(readback!=RISC_APP_DATA_OK||size!=length||memcmp(actual,desired,length)){message("DRAFT NOT SAVED - RETRY");return false;}
    }
confirmed:
    memcpy(stored,desired,length);stored_size=(uint32_t)length;storage_known=true;storage_present=true;recovery=false;return true;
}
static risc_scene_node_v1 *node(uint32_t id,uint32_t route,uint32_t kind,const char *label){
    risc_scene_node_v1 *n=&candidate.nodes[candidate.node_count++];*n=(risc_scene_node_v1){.id=id,.route=route,.kind=kind};
    snprintf(n->label,sizeof(n->label),"%s",label);return n;
}
static bool reset_allowed(void){return recovery||(state.pending&&command_not_attempted)||(state.pending&&(state.pending_result==ALARM_CONTROL_STALE||state.pending_result==ALARM_CONTROL_EXPIRED||state.pending_result==ALARM_CONTROL_INVALID));}
static void declare(void){
    candidate=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=document.revision?document.revision:1,.root=OVERVIEW,.route_count=5};
    candidate.routes[0]=(risc_scene_route_v1){OVERVIEW,0,"ALARMS"};
    candidate.routes[1]=(risc_scene_route_v1){EDIT_TIME,DISCARD_TIME,"TIME"};
    candidate.routes[2]=(risc_scene_route_v1){EDIT_VOLUME,DISCARD_VOLUME,"VOLUME"};
    candidate.routes[3]=(risc_scene_route_v1){CONFIRM_CANCEL,BACK_TO_OVERVIEW,"CANCEL"};
    candidate.routes[4]=(risc_scene_route_v1){CONFIRM_RESET,BACK_TO_OVERVIEW,"RESET"};
    bool locked=recovery||state.pending;
    risc_scene_node_v1 *n=node(1,OVERVIEW,RISC_SCENE_TEXT_NODE,"ONE-SHOT ALARM");snprintf(n->text,sizeof(n->text),"%02u:%02u %s",state.minutes/60,state.minutes%60,status.zone);
    n=node(2,OVERVIEW,RISC_SCENE_TEXT_NODE,"STATUS");snprintf(n->text,sizeof(n->text),"%s",notice[0]?notice:status.status);
    n=node(3,OVERVIEW,RISC_SCENE_LINK,"EDIT TIME");n->target=EDIT_TIME;if(locked)n->flags=RISC_SCENE_DISABLED;
    n=node(4,OVERVIEW,RISC_SCENE_LINK,"ALARM VOLUME");n->target=EDIT_VOLUME;if(!(status.flags&ALARM_CONTROL_HAS_VOLUME))n->flags=RISC_SCENE_HIDDEN;else if(locked)n->flags=RISC_SCENE_DISABLED;
    n=node(5,OVERVIEW,RISC_SCENE_LINK,"CANCEL ALARM");n->target=CONFIRM_CANCEL;n->flags=RISC_SCENE_DESTRUCTIVE;if(locked||!(status.flags&ALARM_CONTROL_ENABLED))n->flags|=RISC_SCENE_DISABLED;
    n=node(6,OVERVIEW,RISC_SCENE_ACTION,"DISMISS ALERT");n->action=DISMISS;if(!(status.flags&ALARM_CONTROL_ALERT))n->flags=RISC_SCENE_HIDDEN;
    n=node(7,OVERVIEW,RISC_SCENE_ACTION,"RETRY / REFRESH");n->action=REFRESH;
    n=node(8,OVERVIEW,RISC_SCENE_ACTION,"RETRY EXACT SAVE");n->action=RETRY_COMMAND;n->flags=state.pending?RISC_SCENE_PRIMARY:RISC_SCENE_HIDDEN;
    n=node(9,OVERVIEW,RISC_SCENE_LINK,"RESET SAVED DRAFT");n->target=CONFIRM_RESET;n->flags=reset_allowed()?RISC_SCENE_DESTRUCTIVE:RISC_SCENE_HIDDEN;
    n=node(10,EDIT_TIME,RISC_SCENE_TIME_OF_DAY,"TIME OF DAY");n->action=CHANGE_TIME;n->value=(int32_t)state.minutes;n->maximum=1439;n->step=1;if(locked)n->flags=RISC_SCENE_DISABLED;
    n=node(22,EDIT_TIME,RISC_SCENE_TEXT_NODE,"STATUS");snprintf(n->text,sizeof(n->text),"%s",notice);if(!notice[0])n->flags=RISC_SCENE_HIDDEN;
    n=node(11,EDIT_TIME,RISC_SCENE_ACTION,"ARM NEXT OCCURRENCE");n->action=ARM_NEXT;n->flags=RISC_SCENE_PRIMARY;
    if(locked||(status.flags&(ALARM_CONTROL_CONFIG_VALID|ALARM_CONTROL_CLOCK_VALID|ALARM_CONTROL_SERVICE_READY))!=(ALARM_CONTROL_CONFIG_VALID|ALARM_CONTROL_CLOCK_VALID|ALARM_CONTROL_SERVICE_READY))n->flags|=RISC_SCENE_DISABLED;
    n=node(12,EDIT_TIME,RISC_SCENE_ACTION,"DISCARD EDIT");n->action=DISCARD_TIME;if(state.pending)n->flags=RISC_SCENE_DISABLED;
    n=node(13,EDIT_VOLUME,RISC_SCENE_INTEGER,"VOLUME PERCENT");n->action=CHANGE_VOLUME;n->value=(int32_t)state.volume;n->maximum=100;n->step=5;if(locked)n->flags=RISC_SCENE_DISABLED;
    n=node(23,EDIT_VOLUME,RISC_SCENE_TEXT_NODE,"STATUS");snprintf(n->text,sizeof(n->text),"%s",notice);if(!notice[0])n->flags=RISC_SCENE_HIDDEN;
    n=node(14,EDIT_VOLUME,RISC_SCENE_ACTION,"SAVE VOLUME");n->action=SAVE_VOLUME;n->flags=RISC_SCENE_PRIMARY;
    if(locked||!(status.flags&ALARM_CONTROL_VOLUME_VALID))n->flags|=RISC_SCENE_DISABLED;
    n=node(15,EDIT_VOLUME,RISC_SCENE_ACTION,"DISCARD EDIT");n->action=DISCARD_VOLUME;if(state.pending)n->flags=RISC_SCENE_DISABLED;
    n=node(16,CONFIRM_CANCEL,RISC_SCENE_TEXT_NODE,"CANCEL ONE-SHOT ALARM");snprintf(n->text,sizeof(n->text),"COUNTDOWN AND OTHER EVENTS STAY");
    n=node(17,CONFIRM_CANCEL,RISC_SCENE_ACTION,"CONFIRM CANCEL");n->action=CANCEL_ALARM;n->flags=RISC_SCENE_DESTRUCTIVE;if(locked)n->flags|=RISC_SCENE_DISABLED;
    n=node(18,CONFIRM_CANCEL,RISC_SCENE_ACTION,"KEEP ALARM");n->action=BACK_TO_OVERVIEW;
    n=node(19,CONFIRM_RESET,RISC_SCENE_TEXT_NODE,"DISCARD SAVED DRAFT");snprintf(n->text,sizeof(n->text),"THIS DOES NOT CANCEL A SAVED ALARM");
    n=node(20,CONFIRM_RESET,RISC_SCENE_ACTION,"CONFIRM DRAFT RESET");n->action=RESET_DRAFT;n->flags=RISC_SCENE_DESTRUCTIVE;if(!reset_allowed())n->flags|=RISC_SCENE_DISABLED;
    n=node(21,CONFIRM_RESET,RISC_SCENE_ACTION,"KEEP DRAFT");n->action=BACK_TO_OVERVIEW;
}
static bool present_model(void){
    declare();if(!memcmp(&candidate,&document,sizeof(document)))return true;
    if(document.revision==UINT32_MAX){message("SCENE REVISION EXHAUSTED");return false;}
    candidate.revision=document.revision+1;
    int32_t rc=scene->update(scene->context,session,&candidate);
    if(scene_terminal(rc))return false;
    if(rc!=RISC_SCENE_OK){message("SCENE UPDATE REFUSED");return false;}
    document=candidate;return true;
}
static bool refresh_status(void){
    alarm_control_snapshot_v1 next={.struct_size=sizeof(next)};
    int32_t rc=control->read(control->context,&next);if(service_terminal(rc))return false;
    if(next.api_version!=1||next.struct_size!=sizeof(next)||next.minutes>1439||next.volume>100||!memchr(next.zone,0,sizeof(next.zone))||!memchr(next.status,0,sizeof(next.status))){message("ALARM CONTROL CONTRACT ERROR");return false;}
    status=next;
    if(!state.pending&&!recovery&&status.confirmed_revision==status.revision&&
       (!strcmp(notice,"SAVED - CHECKING SCHEDULER")||
        (!strcmp(notice,"REQUEST ACCEPTED - CHECKING")&&!(status.flags&ALARM_CONTROL_ALERT))))notice[0]=0;
    if(!state.time_dirty&&!state.pending&&!recovery){state.minutes=status.minutes;state.base_revision=status.revision;}
    if(!state.volume_dirty&&!state.pending&&!recovery){state.volume=status.volume;state.base_volume=status.volume;}
    return true;
}
static bool go_home(void){int32_t rc=scene->navigate(scene->context,session,RISC_SCENE_ROOT,0);if(scene_terminal(rc))return false;return rc==0;}
static bool apply_pending(void){
    if(!state.pending||recovery)return false;
    if(!checkpoint(false)){if(!terminal)(void)go_home();return false;}
    command_not_attempted=false;
    int32_t rc=control->apply(control->context,&state.command);if(service_terminal(rc))return false;
    state.pending_result=rc;message(result_message(rc));
    if(rc==ALARM_CONTROL_OK){
        state.pending=state.time_dirty=state.volume_dirty=0;memset(&state.command,0,sizeof(state.command));state.command.struct_size=sizeof(state.command);
        force_read=true;if(!go_home())return false;
        /* If clearing this journal fails, exact replay of the old saved command
         * remains idempotent; it cannot create another schedule revision. */
        (void)checkpoint(false);
    }else {force_read=true;(void)go_home();}
    return !terminal;
}
static void prepare_command(uint32_t op,uint32_t value,uint32_t expected){
    if(recovery||state.pending)return;
    alarm_control_command_v1 cmd={.struct_size=sizeof(cmd)};
    int32_t rc=control->prepare(control->context,op,value,expected,&cmd);if(service_terminal(rc))return;
    if(rc!=ALARM_CONTROL_OK){message(result_message(rc));force_read=true;return;}
    state.command=cmd;state.pending=1;state.pending_result=ALARM_CONTROL_PENDING;command_not_attempted=true;
    (void)apply_pending();
}
static bool permitted_event(const risc_scene_event_v1 *e){
    if(e->struct_size!=sizeof(*e)||e->document_revision!=document.revision||!e->sequence||e->sequence<=last_event)return false;
    last_event=e->sequence;
    if(e->kind==RISC_SCENE_SUSPEND_EVENT)return e->node==0&&e->action==0;
    uint32_t flags=0;int32_t sr=scene->snapshot(scene->context,session,&navigation,&flags);
    if(scene_terminal(sr)||sr!=0)return false;
    uint32_t route=navigation.routes[navigation.depth-1];
    if(!e->node){
        if(e->kind!=RISC_SCENE_ACTION_EVENT)return false;
        for(unsigned i=0;i<document.route_count;i++)if(document.routes[i].id==route)return e->action&&e->action==document.routes[i].back_action;
        return false;
    }
    for(unsigned i=0;i<document.node_count;i++){
        const risc_scene_node_v1 *n=&document.nodes[i];if(n->id!=e->node)continue;
        if(n->route!=route||n->action!=e->action||n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))return false;
        if(e->kind==RISC_SCENE_ACTION_EVENT)return n->kind==RISC_SCENE_ACTION;
        return e->kind==RISC_SCENE_VALUE_EVENT&&n->kind>=RISC_SCENE_TIME_OF_DAY&&n->kind<=RISC_SCENE_BOOLEAN&&e->value>=n->minimum&&e->value<=n->maximum;
    }
    return false;
}
static bool handle_event(const risc_scene_event_v1 *e){
    if(!permitted_event(e))return false;
    if(e->kind==RISC_SCENE_SUSPEND_EVENT){
        /* Recovery can leave without changing an unreadable prior checkpoint. */
        if(recovery)return true;
        return checkpoint(false);
    }
    switch(e->action){
    case CHANGE_TIME:state.minutes=(uint32_t)e->value;state.time_dirty=1;notice[0]=0;break;
    case CHANGE_VOLUME:state.volume=(uint32_t)e->value;state.volume_dirty=1;notice[0]=0;break;
    case ARM_NEXT:prepare_command(ALARM_CONTROL_ARM,state.minutes,state.base_revision);break;
    case SAVE_VOLUME:prepare_command(ALARM_CONTROL_VOLUME,state.volume,state.base_volume);break;
    case CANCEL_ALARM:prepare_command(ALARM_CONTROL_CANCEL,0,state.base_revision);break;
    case DISCARD_TIME:if(!state.pending){state.time_dirty=0;state.minutes=status.minutes;state.base_revision=status.revision;(void)go_home();}break;
    case DISCARD_VOLUME:if(!state.pending){state.volume_dirty=0;state.volume=status.volume;state.base_volume=status.volume;(void)go_home();}break;
    case BACK_TO_OVERVIEW:(void)go_home();break;
    case DISMISS:{int32_t rc=control->dismiss(control->context,&status.occurrence);if(service_terminal(rc))return false;message(result_message(rc));force_read=true;break;}
    case REFRESH:
        notice[0]=0;force_read=true;
        if(recovery){(void)load_checkpoint();if(!terminal)(void)go_home();}
        break;
    case RETRY_COMMAND:(void)apply_pending();break;
    case RESET_DRAFT:
        if(reset_allowed()){
            if(!refresh_status())break;
            fresh_model();(void)go_home();
            if(!terminal&&checkpoint(true)){recovery=false;message("DRAFT RESET - SAVED ALARM UNCHANGED");}
            else if(!terminal)recovery=true;
        }
        break;
    default:break;
    }
    return false;
}
static bool open_dependencies(void){
    const char *names[]={RISC_SCENE_CAPABILITY,ALARM_CONTROL_CAPABILITY,RISC_APP_DATA_CAPABILITY};
    for(unsigned i=0;i<3;i++){
        grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
        /* Zero selects the sole explicitly authorized service/namespace. */
        bool ok=runtime->acquire(names[i],1,0,&grants[i]);
        if(!context_alive()||!ok)return false;
        ++acquired;
    }
    scene=grants[0].api;control=grants[1].api;storage=grants[2].api;
    return scene&&scene->api_version==1&&scene->struct_size>=sizeof(*scene)&&scene->open&&scene->update&&scene->next&&scene->navigate&&scene->snapshot&&scene->close&&
        control&&control->api_version==1&&control->struct_size>=sizeof(*control)&&control->read&&control->step&&control->prepare&&control->apply&&control->dismiss&&
        storage&&storage->api_version==1&&storage->struct_size>=sizeof(*storage)&&storage->stat&&storage->read&&storage->replace;
}
static void close_dependencies(void){
    if(terminal)return;
    while(acquired){bool ok=runtime->release(&grants[--acquired]);if(!context_alive())return;if(!ok){retain();return;}}
}
__attribute__((visibility("default"))) void app_main(void){
    runtime=risc_runtime_get_api(1);
    if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release||!runtime->yield_ms)return;
    acquired=0;terminal=false;command_not_attempted=false;session=0;last_event=0;recovery=false;storage_known=false;storage_present=false;stored_size=0;notice[0]=0;
    scene=NULL;control=NULL;storage=NULL;document=(risc_scene_document_v1){0};root_navigation();status=(alarm_control_snapshot_v1){0};fresh_model();
    if(!open_dependencies()){
        /* A headless deployment may omit this optional presentation grant and
         * all presentation providers. No display/input fallback is attempted. */
        if(terminal)return;
        if(runtime->diagnostic)runtime->diagnostic("Alarms scene dependencies unavailable; no UI fallback");
        if(context_alive())close_dependencies();
        return;
    }
    if(!refresh_status()){close_dependencies();return;}
    fresh_model();(void)load_checkpoint();if(terminal)return;
    declare();document=candidate;
    int32_t rc=scene->open(scene->context,&document,&navigation,&session);
    if(scene_terminal(rc))return;
    if(rc==RISC_SCENE_INVALID){
        recovery=true;message("DRAFT ROUTE INVALID - RESET OR RETRY");root_navigation();declare();document=candidate;
        rc=scene->open(scene->context,&document,&navigation,&session);if(scene_terminal(rc))return;
    }
    if(rc!=RISC_SCENE_OK){close_dependencies();return;}
    unsigned ticks=0;force_read=true;bool leaving=false;
    for(;;){
        risc_scene_event_v1 event={.struct_size=sizeof(event)};
        rc=scene->next(scene->context,session,&event);
        if(scene_terminal(rc))return;
        if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE){message("PRESENTATION SESSION FAILED");leaving=true;}
        if(rc==RISC_SCENE_OK)leaving=handle_event(&event);
        if(terminal)return;
        if(leaving)break;
        uint32_t flags=0;rc=scene->snapshot(scene->context,session,&navigation,&flags);
        if(scene_terminal(rc))return;
        if(rc!=0)break;
        if(!(flags&RISC_SCENE_PRESENTING)){
            int32_t sr=control->step(control->context);if(service_terminal(sr))return;
            if(force_read||++ticks>=25){force_read=false;ticks=0;(void)refresh_status();if(terminal)return;}
        }
        if(!present_model()){if(terminal)return;break;}
        runtime->yield_ms(20);if(!context_alive())return;
    }
    /* Poll only closure, never ordinary input/service work after close starts. */
    for(unsigned attempt=0;attempt<250;attempt++){
        rc=scene->close(scene->context,session);if(scene_terminal(rc))return;
        if(rc==RISC_SCENE_OK){session=0;close_dependencies();return;}
        if(rc!=RISC_SCENE_AGAIN){retain();return;}
        runtime->yield_ms(20);if(!context_alive())return;
    }
    /* An unconfirmed transfer is not a successfully suspended scene. */
    retain();
}
