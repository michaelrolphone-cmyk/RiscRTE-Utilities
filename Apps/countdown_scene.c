/* Countdown 0.2.0: domain intent and restorable state only. Presentation is an
 * optional external service. This translation unit has no display, input,
 * font, geometry, product, or form-factor dependencies. */
#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscSceneStateV1.h"
#include "RiscAppDataV1.h"
#include "CountdownControlV1.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { OVERVIEW=1,EDIT_DURATION=2,CONFIRM_CANCEL=3,CONFIRM_RESET=4 };
enum { CHANGE_HOURS=1,CHANGE_MINUTES,CHANGE_SECONDS,START_COUNTDOWN,DISCARD_DURATION,
       CANCEL_COUNTDOWN,DISMISS,REFRESH,RETRY_COMMAND,RESET_DRAFT,BACK_TO_OVERVIEW,LEAVE_APP };
#define PAYLOAD_SIZE 120u
#define CHECKPOINT_SIZE (RISC_SCENE_STATE_HEADER+PAYLOAD_SIZE)
#define CHECKPOINT_NAME "countdown.scene"
#define CHECKPOINT_SCHEMA 1u

typedef struct {
    uint32_t duration_seconds,base_revision,duration_dirty,pending;
    int32_t pending_result;
    countdown_control_command_v1 command;
} model;
static model state;
static countdown_control_snapshot_v1 status;
static risc_scene_document_v1 document,candidate;
static risc_scene_navigation_v1 navigation;
static const risc_runtime_api_v1 *runtime;
static const risc_scene_api_v1 *scene;
static const risc_scene_lifecycle_api_v1 *lifecycle;
static risc_scene_resident_v1 resident;
static bool close_scene(void);
static bool reopen_scene(void);
static bool configure_scene(void);
static bool handoff(uint32_t reason);
static const countdown_control_api_v1 *control;
static const risc_app_data_v1 *storage;
static risc_runtime_capability_v1 grants[3];
static unsigned acquired;
static uint64_t session,last_event;
static bool terminal,recovery,storage_known,storage_present,force_read,command_not_attempted,explicit_refresh;
static uint8_t stored[CHECKPOINT_SIZE];static uint32_t stored_size;
static char notice[72];

static void message(const char *text){snprintf(notice,sizeof(notice),"%s",text);}
static void retain(void){
    terminal=true;
    /* No provider calls, diagnostics, frees, or releases after this fence. */
    if(risc_runtime_get_api(1)==runtime)(void)runtime->retain_invocation();
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
static bool service_terminal(int32_t rc){if(!context_alive())return true;if(rc==COUNTDOWN_CONTROL_RETAINED){retain();return true;}return false;}
static bool scene_terminal(int32_t rc){if(!context_alive())return true;if(rc==RISC_SCENE_RETAINED){retain();return true;}return false;}
static const char *result_message(int32_t rc){
    switch(rc){
    case COUNTDOWN_CONTROL_OK:return "SAVED - CHECKING SCHEDULER";
    case COUNTDOWN_CONTROL_PENDING:return "REQUEST ACCEPTED - CHECKING";
    case COUNTDOWN_CONTROL_STALE:return "SAVED DATA CHANGED - REVIEW DRAFT";
    case COUNTDOWN_CONTROL_STORAGE:return "STORAGE UNAVAILABLE - RETRY";
    case COUNTDOWN_CONTROL_CLOCK:return "CLOCK INVALID";
    case COUNTDOWN_CONTROL_AMBIGUOUS:return "DURATION NOT AVAILABLE";
    case COUNTDOWN_CONTROL_BLOCKED:return "SCHEDULER BLOCKED - RETRY";
    case COUNTDOWN_CONTROL_UNCERTAIN:return "SAVE UNCONFIRMED - RETRY EXACT SAVE";
    case COUNTDOWN_CONTROL_EXPIRED:return "PENDING DEADLINE PASSED - RESET DRAFT";
    default:return "COMMAND INVALID - REVIEW DRAFT";
    }
}
static void root_navigation(void){navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={OVERVIEW}};}
static void fresh_model(void){
    state=(model){.duration_seconds=status.duration_seconds?status.duration_seconds:300,.base_revision=status.revision};
    state.command.struct_size=sizeof(state.command);
}
static void encode_payload(uint8_t out[PAYLOAD_SIZE]){
    const uint32_t words[]={state.duration_seconds,state.base_revision,state.duration_dirty,state.pending,(uint32_t)state.pending_result,COUNTDOWN_CONTROL_PROTOCOL};
    for(unsigned i=0;i<6;i++)risc_scene_put32(out+i*4,words[i]);
    memset(out+24,0,COUNTDOWN_CONTROL_COMMAND_BYTES);
    if(state.pending)memcpy(out+24,state.command.bytes,COUNTDOWN_CONTROL_COMMAND_BYTES);
}
static bool decode_payload(const uint8_t *in,size_t size){
    if(size!=PAYLOAD_SIZE||risc_scene_get32(in+20)!=COUNTDOWN_CONTROL_PROTOCOL)return false;
    model m={0};m.duration_seconds=risc_scene_get32(in);m.base_revision=risc_scene_get32(in+4);
    m.duration_dirty=risc_scene_get32(in+8);m.pending=risc_scene_get32(in+12);
    m.pending_result=(int32_t)risc_scene_get32(in+16);m.command.struct_size=sizeof(m.command);
    if(m.duration_seconds>359999u||m.duration_dirty>1||m.pending>1||m.pending_result>1||m.pending_result<-10)return false;
    if(m.pending)memcpy(m.command.bytes,in+24,COUNTDOWN_CONTROL_COMMAND_BYTES);
    else for(unsigned i=24;i<PAYLOAD_SIZE;i++)if(in[i])return false;
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
static bool reset_allowed(void){return recovery||(state.pending&&command_not_attempted)||(state.pending&&(state.pending_result==COUNTDOWN_CONTROL_STALE||state.pending_result==COUNTDOWN_CONTROL_EXPIRED||state.pending_result==COUNTDOWN_CONTROL_INVALID));}
static void declare(void){
    candidate=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=document.revision?document.revision:1,.root=OVERVIEW,.route_count=4};
    candidate.routes[0]=(risc_scene_route_v1){OVERVIEW,LEAVE_APP,"COUNTDOWN"};
    candidate.routes[1]=(risc_scene_route_v1){EDIT_DURATION,DISCARD_DURATION,"DURATION"};
    candidate.routes[2]=(risc_scene_route_v1){CONFIRM_CANCEL,BACK_TO_OVERVIEW,"CANCEL"};
    candidate.routes[3]=(risc_scene_route_v1){CONFIRM_RESET,BACK_TO_OVERVIEW,"RESET"};
    bool locked=recovery||state.pending||(status.flags&COUNTDOWN_CONTROL_ALERT);
    uint32_t shown=(status.flags&(COUNTDOWN_CONTROL_ENABLED|COUNTDOWN_CONTROL_CLOCK_VALID))==(COUNTDOWN_CONTROL_ENABLED|COUNTDOWN_CONTROL_CLOCK_VALID)&&!state.duration_dirty?status.remaining_seconds:state.duration_seconds;
    risc_scene_node_v1 *n=node(1,OVERVIEW,RISC_SCENE_TEXT_NODE,"COUNTDOWN");
    snprintf(n->text,sizeof(n->text),"%02u:%02u:%02u",(unsigned)(shown/3600u),(unsigned)(shown/60u%60u),(unsigned)(shown%60u));
    n=node(2,OVERVIEW,RISC_SCENE_TEXT_NODE,"STATUS");snprintf(n->text,sizeof(n->text),"%s",notice[0]?notice:status.status);
    n=node(3,OVERVIEW,RISC_SCENE_LINK,"EDIT DURATION");n->target=EDIT_DURATION;if(locked)n->flags=RISC_SCENE_DISABLED;
    n=node(4,OVERVIEW,RISC_SCENE_ACTION,"START COUNTDOWN");n->action=START_COUNTDOWN;n->flags=RISC_SCENE_PRIMARY;
    if(locked||!state.duration_seconds||(status.flags&(COUNTDOWN_CONTROL_CONFIG_VALID|COUNTDOWN_CONTROL_CLOCK_VALID|COUNTDOWN_CONTROL_SERVICE_READY))!=(COUNTDOWN_CONTROL_CONFIG_VALID|COUNTDOWN_CONTROL_CLOCK_VALID|COUNTDOWN_CONTROL_SERVICE_READY))n->flags|=RISC_SCENE_DISABLED;
    n=node(5,OVERVIEW,RISC_SCENE_LINK,"CANCEL COUNTDOWN");n->target=CONFIRM_CANCEL;n->flags=RISC_SCENE_DESTRUCTIVE;if(locked||!(status.flags&COUNTDOWN_CONTROL_ENABLED))n->flags|=RISC_SCENE_DISABLED;
    n=node(6,OVERVIEW,RISC_SCENE_ACTION,"DISMISS ALERT");n->action=DISMISS;if(!(status.flags&COUNTDOWN_CONTROL_ALERT))n->flags=RISC_SCENE_HIDDEN;
    n=node(7,OVERVIEW,RISC_SCENE_ACTION,"RETRY / REFRESH");n->action=REFRESH;
    n=node(8,OVERVIEW,RISC_SCENE_ACTION,"RETRY EXACT SAVE");n->action=RETRY_COMMAND;n->flags=state.pending?RISC_SCENE_PRIMARY:RISC_SCENE_HIDDEN;if(status.flags&COUNTDOWN_CONTROL_ALERT)n->flags|=RISC_SCENE_DISABLED;
    n=node(9,OVERVIEW,RISC_SCENE_LINK,"RESET SAVED DRAFT");n->target=CONFIRM_RESET;n->flags=reset_allowed()?RISC_SCENE_DESTRUCTIVE:RISC_SCENE_HIDDEN;
    const char *labels[]={"HOURS","MINUTES","SECONDS"};
    const uint32_t actions[]={CHANGE_HOURS,CHANGE_MINUTES,CHANGE_SECONDS};
    const uint32_t values[]={state.duration_seconds/3600u,state.duration_seconds/60u%60u,state.duration_seconds%60u};
    for(unsigned i=0;i<3;i++){
        n=node(10+i,EDIT_DURATION,RISC_SCENE_INTEGER,labels[i]);n->action=actions[i];n->value=(int32_t)values[i];n->maximum=i?59:99;n->step=1;if(locked)n->flags=RISC_SCENE_DISABLED;
    }
    n=node(13,EDIT_DURATION,RISC_SCENE_TEXT_NODE,"STATUS");snprintf(n->text,sizeof(n->text),"%s",notice);if(!notice[0])n->flags=RISC_SCENE_HIDDEN;
    n=node(14,EDIT_DURATION,RISC_SCENE_ACTION,"START COUNTDOWN");n->action=START_COUNTDOWN;n->flags=candidate.nodes[3].flags;
    n=node(15,EDIT_DURATION,RISC_SCENE_ACTION,"DISCARD EDIT");n->action=DISCARD_DURATION;if(state.pending)n->flags=RISC_SCENE_DISABLED;
    n=node(16,CONFIRM_CANCEL,RISC_SCENE_TEXT_NODE,"CANCEL COUNTDOWN");snprintf(n->text,sizeof(n->text),"ALARMS AND OTHER EVENTS STAY");
    n=node(17,CONFIRM_CANCEL,RISC_SCENE_ACTION,"CONFIRM CANCEL");n->action=CANCEL_COUNTDOWN;n->flags=RISC_SCENE_DESTRUCTIVE;if(locked)n->flags|=RISC_SCENE_DISABLED;
    n=node(18,CONFIRM_CANCEL,RISC_SCENE_ACTION,"KEEP COUNTDOWN");n->action=BACK_TO_OVERVIEW;
    n=node(19,CONFIRM_RESET,RISC_SCENE_TEXT_NODE,"DISCARD SAVED DRAFT");snprintf(n->text,sizeof(n->text),"THIS DOES NOT CANCEL A SAVED COUNTDOWN");
    n=node(20,CONFIRM_RESET,RISC_SCENE_ACTION,"CONFIRM DRAFT RESET");n->action=RESET_DRAFT;n->flags=RISC_SCENE_DESTRUCTIVE;if(!reset_allowed())n->flags|=RISC_SCENE_DISABLED;
    n=node(21,CONFIRM_RESET,RISC_SCENE_ACTION,"KEEP DRAFT");n->action=BACK_TO_OVERVIEW;
    n=node(22,OVERVIEW,RISC_SCENE_TEXT_NODE,"PRESENTATION CAPABILITIES");
    if(!lifecycle)snprintf(n->text,sizeof(n->text),"AUTOMATIC REFRESH AND SHARED CONTROLS UNAVAILABLE");
    else if(!resident.enabled)snprintf(n->text,sizeof(n->text),"SHARED CONTROLS UNAVAILABLE");
    else n->flags=RISC_SCENE_HIDDEN;
    n=node(23,OVERVIEW,RISC_SCENE_TEXT_NODE,"ALERT CAPABILITIES");
    if(!(status.flags&(COUNTDOWN_CONTROL_HAS_SOUND|COUNTDOWN_CONTROL_HAS_HAPTIC)))snprintf(n->text,sizeof(n->text),"VISUAL ONLY - SOUND AND HAPTIC UNAVAILABLE");
    else if(!(status.flags&COUNTDOWN_CONTROL_HAS_SOUND))snprintf(n->text,sizeof(n->text),"SOUND UNAVAILABLE");
    else if(!(status.flags&COUNTDOWN_CONTROL_HAS_HAPTIC))snprintf(n->text,sizeof(n->text),"HAPTIC UNAVAILABLE");
    else n->flags=RISC_SCENE_HIDDEN;
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
    countdown_control_snapshot_v1 next={.struct_size=sizeof(next)};
    int32_t rc=control->read(control->context,&next);if(service_terminal(rc))return false;
    if(next.api_version!=1||next.struct_size!=sizeof(next)||next.duration_seconds>359999u||next.remaining_seconds>359999u||!memchr(next.status,0,sizeof(next.status))){message("COUNTDOWN CONTROL CONTRACT ERROR");return false;}
    status=next;
    if(!state.pending&&!recovery&&status.confirmed_revision==status.revision&&
       (!strcmp(notice,"SAVED - CHECKING SCHEDULER")||
        (!strcmp(notice,"REQUEST ACCEPTED - CHECKING")&&!(status.flags&COUNTDOWN_CONTROL_ALERT))))notice[0]=0;
    if(!state.duration_dirty&&!state.pending&&!recovery){state.duration_seconds=status.duration_seconds?status.duration_seconds:300;state.base_revision=status.revision;}
    return true;
}
static bool go_home(void){int32_t rc=scene->navigate(scene->context,session,RISC_SCENE_ROOT,0);if(scene_terminal(rc))return false;return rc==0;}
static bool apply_pending(void){
    if(!state.pending||recovery)return false;
    if(status.flags&COUNTDOWN_CONTROL_ALERT){message("DISMISS ACTIVE ALERT FIRST");return false;}
    if(!checkpoint(false)){if(!terminal)(void)go_home();return false;}
    command_not_attempted=false;
    int32_t rc=control->apply(control->context,&state.command);if(service_terminal(rc))return false;
    state.pending_result=rc;message(result_message(rc));
    if(rc==COUNTDOWN_CONTROL_OK){
        state.pending=state.duration_dirty=0;memset(&state.command,0,sizeof(state.command));state.command.struct_size=sizeof(state.command);
        force_read=true;if(!go_home())return false;
        /* If clearing this journal fails, exact replay of the old saved command
         * remains idempotent; it cannot create another schedule revision. */
        (void)checkpoint(false);
    }else {force_read=true;(void)go_home();}
    return !terminal;
}
static void prepare_command(uint32_t op,uint32_t value,uint32_t expected){
    if(recovery||state.pending)return;
    if(status.flags&COUNTDOWN_CONTROL_ALERT){message("DISMISS ACTIVE ALERT FIRST");return;}
    countdown_control_command_v1 cmd={.struct_size=sizeof(cmd)};
    int32_t rc=control->prepare(control->context,op,value,expected,&cmd);if(service_terminal(rc))return;
    if(rc!=COUNTDOWN_CONTROL_OK){message(result_message(rc));force_read=true;return;}
    state.command=cmd;state.pending=1;state.pending_result=COUNTDOWN_CONTROL_PENDING;command_not_attempted=true;
    (void)apply_pending();
}
static bool permitted_event(const risc_scene_event_v1 *e){
    if(e->struct_size!=sizeof(*e)||e->document_revision!=document.revision||!e->sequence||e->sequence<=last_event)return false;
    last_event=e->sequence;
    if(e->kind==RISC_SCENE_SUSPEND_EVENT||e->kind==RISC_SCENE_CONTROLS_EVENT)return e->node==0&&e->action==0;
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
    explicit_refresh=true;
    if(e->kind==RISC_SCENE_SUSPEND_EVENT){
        /* Recovery can leave without changing an unreadable prior checkpoint. */
        if(recovery)return true;
        bool saved=checkpoint(false);
        if(!saved&&!terminal)(void)configure_scene();
        return saved;
    }
    if(e->kind==RISC_SCENE_CONTROLS_EVENT)return resident.enabled?handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS):false;
    switch(e->action){
    case CHANGE_HOURS:state.duration_seconds=(uint32_t)e->value*3600u+state.duration_seconds%3600u;state.duration_dirty=1;notice[0]=0;break;
    case CHANGE_MINUTES:state.duration_seconds=state.duration_seconds/3600u*3600u+(uint32_t)e->value*60u+state.duration_seconds%60u;state.duration_dirty=1;notice[0]=0;break;
    case CHANGE_SECONDS:state.duration_seconds=state.duration_seconds/60u*60u+(uint32_t)e->value;state.duration_dirty=1;notice[0]=0;break;
    case START_COUNTDOWN:prepare_command(COUNTDOWN_CONTROL_ARM,state.duration_seconds,state.base_revision);break;
    case CANCEL_COUNTDOWN:prepare_command(COUNTDOWN_CONTROL_CANCEL,0,state.base_revision);break;
    case DISCARD_DURATION:if(!state.pending){state.duration_dirty=0;state.duration_seconds=status.duration_seconds?status.duration_seconds:300;state.base_revision=status.revision;(void)go_home();}break;
    case LEAVE_APP:
        if(status.flags&COUNTDOWN_CONTROL_ALERT){message("DISMISS ALERT BEFORE BACK");force_read=true;break;}
        if(recovery)return true;
        return checkpoint(false);
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
            if(!terminal&&checkpoint(true)){recovery=false;message("DRAFT RESET - SAVED COUNTDOWN UNCHANGED");}
            else if(!terminal)recovery=true;
        }
        break;
    default:break;
    }
    return false;
}
static bool open_dependencies(void){
    const char *names[]={RISC_SCENE_CAPABILITY,COUNTDOWN_CONTROL_CAPABILITY,RISC_APP_DATA_CAPABILITY};
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
static bool close_scene(void){
    if(!session)return true;
    for(unsigned attempt=0;attempt<250;attempt++){
        int32_t rc=scene->close(scene->context,session);if(scene_terminal(rc))return false;
        if(rc==RISC_SCENE_OK){session=0;return true;}
        if(rc!=RISC_SCENE_AGAIN){retain();return false;}
        runtime->yield_ms(20);if(!context_alive())return false;
    }
    retain();return false;
}
static bool configure_scene(void){
    lifecycle=risc_scene_lifecycle_get_v1(scene);
    explicit_refresh=true;
    if(lifecycle){int32_t rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(scene_terminal(rc))return false;if(rc!=RISC_SCENE_OK){message("SHARED CONTROLS UNAVAILABLE");lifecycle=NULL;}}
    return true;
}
static bool reopen_scene(void){
    declare();document=candidate;
    int32_t rc=scene->open(scene->context,&document,&navigation,&session);if(scene_terminal(rc)||rc!=RISC_SCENE_OK)return false;
    last_event=0;return configure_scene();
}
static bool handoff(uint32_t reason){
    /* Never overwrite an unreadable checkpoint. Pending commands remain an
     * exact, durable journal, not a guessed successful save. */
    if(!recovery&&!checkpoint(false)){
        if(!terminal)(void)configure_scene();
        return terminal;
    }
    if(!close_scene())return true;
    int32_t rc=risc_scene_resident_dispatch_v1(&resident,reason);if(!context_alive())return true;
    if(rc==RISC_RESIDENT_RETAINED){retain();return true;}
    if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return true;
    force_read=true;return !reopen_scene();
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
    runtime=risc_runtime_get_api(1);
    if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release||!runtime->yield_ms)return;
    explicit_refresh=true;lifecycle=NULL;resident=(risc_scene_resident_v1){0};
    acquired=0;terminal=false;command_not_attempted=false;session=0;last_event=0;recovery=false;storage_known=false;storage_present=false;stored_size=0;notice[0]=0;
    scene=NULL;control=NULL;storage=NULL;document=(risc_scene_document_v1){0};root_navigation();status=(countdown_control_snapshot_v1){0};fresh_model();
    if(!open_dependencies()){
        /* A headless deployment may omit this optional presentation grant and
         * all presentation providers. No display/input fallback is attempted. */
        if(terminal)return;
        if(runtime->diagnostic)runtime->diagnostic("Countdown scene dependencies unavailable; no UI fallback");
        if(context_alive())close_dependencies();
        return;
    }
    if(!risc_scene_resident_bind_v1(runtime,&resident)){retain();return;}
    if(!context_alive())return;
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
    if(!configure_scene()){if(!terminal&&close_scene())close_dependencies();return;}
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
        if(resident.enabled){
            uint32_t work=(state.pending||recovery)?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0;
            int32_t rr=risc_scene_resident_poll_work_v1(&resident,flags,work);if(!context_alive())return;
            if(rr==RISC_RESIDENT_RETAINED){retain();return;}if(rr==RISC_RESIDENT_EXIT)break;
            if(resident.policy&&!work&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))){if(handoff(RISC_RESIDENT_CHECKPOINT_POLICY))break;continue;}
        }
        if(!(flags&RISC_SCENE_PRESENTING)){
            int32_t sr=control->step(control->context);if(service_terminal(sr))return;
            if(force_read||++ticks>=25){force_read=false;ticks=0;(void)refresh_status();if(terminal)return;}
        }
        /* Status may advance independently, but never invalidate an active
         * raster or held/queued contact. Without lifecycle tracking, publish
         * only an explicit user refresh/action rather than a timer update. */
        if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(lifecycle||explicit_refresh)){
            if(!present_model()){if(terminal)return;break;}explicit_refresh=false;
        }
        runtime->yield_ms(20);if(!context_alive())return;
    }
    if(close_scene())close_dependencies();
}
