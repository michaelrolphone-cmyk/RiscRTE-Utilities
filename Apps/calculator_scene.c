/* One semantic Calculator application. The optional scene provider owns all
 * display, input and physical lifecycle. Decimal behavior is the original core.
 * Historical Calculator has no persisted records; this app does not touch
 * storage or invent a migration/reset of user data. */
#include "RiscRuntimeV1.h"
#include "RiscSceneKeypadV1.h"
#include "RiscSceneResidentV1.h"
#include "calculator_core.h"
#include <limits.h>
#include <stdio.h>

static const risc_runtime_api_v1 *runtime;
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static risc_runtime_capability_v1 grant;
static calculator state;
static risc_components_document_v1 document,candidate;
static uint64_t session,last_event;
static bool acquired,terminal;
static risc_scene_resident_v1 resident;
static const risc_scene_lifecycle_api_v1 *lifecycle;
static risc_scene_navigation_v1 navigation;
static const char keys[]={'C','S','B','/','7','8','9','*','4','5','6','-','1','2','3','+','0','.','='};
static const char *const labels[]={"C","+/-","DEL","/","7","8","9","x","4","5","6","-","1","2","3","+","0",".","="};
static void retain(void){if(!terminal){terminal=true;(void)runtime->retain_invocation();}}
static bool alive(void){if(terminal)return false;if(risc_runtime_get_api(1)!=runtime){terminal=true;return false;}return true;}
static bool checked(int32_t rc){if(!alive())return false;if(rc==RISC_SCENE_RETAINED){retain();return false;}return true;}
static void release(void){if(!alive()||!acquired)return;acquired=false;bool ok=runtime->release(&grant);if(alive()&&!ok)retain();}
static void declare(void){
    candidate=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=document.revision?document.revision:1,.root=1,.route_count=1,.node_count=21,.screen_key=1};
    candidate.routes[0]=(risc_scene_route_v1){.id=1,.title="CALCULATOR"};
    candidate.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE};
    snprintf(candidate.nodes[0].label,sizeof(candidate.nodes[0].label),"%s",state.error==CALC_DIV_ZERO?"DIV BY ZERO":state.error==CALC_OVERFLOW?"OVERFLOW":state.text);
    snprintf(candidate.nodes[0].text,sizeof(candidate.nodes[0].text),"%s",state.error?"C OR DIGIT TO RESET":"6 DP");
    if(!state.error&&state.pending){size_t n=strlen(candidate.nodes[0].text);candidate.nodes[0].text[n++]=' ';candidate.nodes[0].text[n++]=state.pending;candidate.nodes[0].text[n]=0;}
    for(unsigned i=0;i<sizeof(keys);i++){
        risc_scene_node_v1 *n=&candidate.nodes[i+1];
        *n=(risc_scene_node_v1){.id=i+2,.route=1,.kind=RISC_COMPONENT_KEY,.action=i+1};
        snprintf(n->label,sizeof(n->label),"%s",labels[i]);
    }
    candidate.nodes[20]=(risc_scene_node_v1){.id=21,.route=1,.kind=RISC_COMPONENT_HEADER_ACTION,.action=100,.label="BACK"};
    candidate.details[20].symbol=RISC_SYMBOL_BACK;
}
static bool update(void){
    declare();if(!memcmp(&candidate,&document,sizeof(document)))return true;
    if(document.revision==UINT32_MAX)return false;
    candidate.revision=document.revision+1;
    int32_t rc=components->update(scene->context,session,&candidate);
    if(!checked(rc)||rc!=RISC_SCENE_OK)return false;
    document=candidate;return true;
}
static bool event(const risc_scene_event_v1 *e){
    if(e->struct_size<sizeof(*e)||!e->sequence||e->sequence<=last_event)return false;
    last_event=e->sequence;
    if(e->document_revision!=document.revision)return false;
    if(e->kind==RISC_SCENE_SUSPEND_EVENT)return !e->node&&!e->action;
    if(e->kind!=RISC_SCENE_ACTION_EVENT)return false;
    if(e->node==21&&e->action==100)return true;
    if(e->action<1||e->action>sizeof(keys)||e->node!=e->action+1)return false;
    calc_key(&state,keys[e->action-1]);return false;
}
static bool close_scene(void){
    if(!session)return true;
    for(unsigned i=0;i<250;i++){
        int32_t rc=scene->close(scene->context,session);if(!checked(rc))return false;
        if(rc==RISC_SCENE_OK){session=0;return true;}
        if(rc!=RISC_SCENE_AGAIN){retain();return false;}
        runtime->yield_ms(20);if(!alive())return false;
    }retain();return false;
}
static bool configure(void){
    if(!lifecycle)return !resident.enabled;
    int32_t rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);
    return checked(rc)&&rc==RISC_SCENE_OK;
}
static bool handoff(uint32_t reason){
    uint32_t flags=0;int32_t rc=scene->snapshot(scene->context,session,&navigation,&flags);
    if(!checked(rc)||rc!=RISC_SCENE_OK)return false;
    if(!close_scene())return false;
    rc=risc_scene_resident_dispatch_v1(&resident,reason);
    if(!alive())return false;
    if(rc==RISC_RESIDENT_RETAINED){retain();return false;}
    if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
    rc=components->open(scene->context,&document,&navigation,&session);
    if(!checked(rc)||rc!=RISC_SCENE_OK)return false;
    /* New provider session starts a fresh event sequence. */
    last_event=0;return configure();
}
static bool unavailable(void){
    /* Old component providers can still display a truthful capability gap. */
    candidate=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=1,.root=1,.route_count=1,.node_count=1,.screen_key=1};
    candidate.routes[0]=(risc_scene_route_v1){.id=1,.title="CALCULATOR"};
    candidate.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE,.label="KEYPAD UNAVAILABLE",.text="Presentation provider needs keypad support"};
    int32_t rc=components->open(scene->context,&candidate,NULL,&session);
    if(!checked(rc)||rc!=RISC_SCENE_OK)return false;
    document=candidate;return true;
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
    runtime=risc_runtime_get_api(1);
    if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release||!runtime->yield_ms)return;
    terminal=false;acquired=false;session=last_event=0;scene=NULL;components=NULL;document=(risc_components_document_v1){0};calc_reset(&state);
    navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation)};
    grant=(risc_runtime_capability_v1){.struct_size=sizeof(grant)};
    bool ok=runtime->acquire(RISC_SCENE_CAPABILITY,1,0,&grant);
    if(!alive())return;
    if(!ok){if(runtime->diagnostic)runtime->diagnostic("Calculator presentation unavailable");return;}
    acquired=true;scene=grant.api;components=risc_scene_components_get_v1(scene);
    if(!components||!scene->next||!scene->close||!scene->snapshot){release();return;}
    lifecycle=risc_scene_lifecycle_get_v1(scene);
    if(!risc_scene_resident_bind_v1(runtime,&resident)){if(alive())retain();return;}
    if(!alive())return;
    declare();document=candidate;
    int32_t rc=components->open(scene->context,&document,NULL,&session);
    if(!checked(rc))return;
    bool gap=rc==RISC_SCENE_INVALID;
    if(gap){if(!unavailable()){release();return;}}
    else if(rc!=RISC_SCENE_OK){release();return;}
    if(!configure()){if(!terminal&&close_scene())release();return;}
    for(;;){
        risc_scene_event_v1 e={.struct_size=sizeof(e)};rc=scene->next(scene->context,session,&e);
        if(!checked(rc))return;
        if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)break;
        if(rc==RISC_SCENE_OK&&e.kind==RISC_SCENE_CONTROLS_EVENT){
            if(e.struct_size>=sizeof(e)&&e.sequence>last_event&&e.document_revision==document.revision&&!e.node&&!e.action){
                last_event=e.sequence;if(resident.enabled&&!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS))break;
            }
            continue;
        }
        if(rc==RISC_SCENE_OK&&event(&e))break;
        if(!gap&&!update()){if(terminal)return;break;}
        uint32_t flags=0;rc=scene->snapshot(scene->context,session,&navigation,&flags);
        if(!checked(rc))return;
        if(rc!=RISC_SCENE_OK)break;
        int32_t rr=risc_scene_resident_poll_v1(&resident,flags);
        if(!alive())return;
        if(rr==RISC_RESIDENT_RETAINED){retain();return;}
        if(rr==RISC_RESIDENT_EXIT)break;
        if(resident.policy&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))){if(!handoff(RISC_RESIDENT_CHECKPOINT_POLICY))break;continue;}
        runtime->yield_ms(20);if(!alive())return;
    }
    if(!terminal&&close_scene())release();
}
