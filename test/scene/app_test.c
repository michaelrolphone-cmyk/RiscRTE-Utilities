#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "RiscSceneStateV1.h"
#include "RiscAppDataV1.h"
#include "AlarmControlV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void app_main(void);
static const char *mode;static unsigned invocation,ticks,phase,acquires,releases,retains,domain_writes,apply_calls,file_writes,opens,closes;
static bool fenced,present_file,fail_write_once=true;
static uint8_t file_bytes[512];static uint32_t file_size;static uint64_t file_revision=1,event_seq;
static risc_scene_document_v1 view;static risc_scene_navigation_v1 path;
static alarm_control_snapshot_v1 alarm;
static unsigned committed_id,command_id;
static void io(void){assert(!fenced);}
static bool is(const char *s){return !strcmp(mode,s);}
static bool retain(void){assert(!fenced);++retains;fenced=true;return true;}
static bool log_line(const char*s){io();assert(s);return true;}
static void yield_ms(uint32_t ms){io();assert(ms>=1&&ms<=50);if(is("yield-context"))fenced=true;}
static bool release(risc_runtime_capability_v1*g){io();assert(g->slot);g->slot=0;++releases;return true;}
static void saved_state(uint8_t payload[132],risc_scene_navigation_v1*n){size_t size=0;assert(present_file);assert(risc_scene_state_decode(file_bytes,file_size,1,n,payload,132,&size)&&size==132);}
static int32_t stat_file(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;io();assert(!strcmp(n,"alarms.scene"));
    if(is("read-retained"))return RISC_APP_DATA_RETAINED;
    *s=present_file?file_size:0;*r=present_file?file_revision:0;return present_file?0:RISC_APP_DATA_NOT_FOUND;
}
static int32_t read_file(void*c,const char*n,uint64_t rev,void*b,uint32_t cap,uint32_t*s,uint64_t*r){(void)c;(void)n;io();assert(rev==file_revision&&cap>=file_size);memcpy(b,file_bytes,file_size);*s=file_size;*r=file_revision;return 0;}
static int32_t replace_file(void*c,const char*n,uint64_t rev,const void*b,uint32_t size){(void)c;(void)n;io();assert(rev==(present_file?file_revision:0));
    if(is("write-retained"))return RISC_APP_DATA_RETAINED;
    if(is("checkpoint-failure")&&fail_write_once){fail_write_once=false;return RISC_APP_DATA_NO_SPACE;}
    assert(size<=sizeof(file_bytes));memcpy(file_bytes,b,size);file_size=size;present_file=true;++file_revision;++file_writes;
    return is("checkpoint-unknown")?RISC_APP_DATA_COMMIT_UNKNOWN:0;
}
static int32_t read_control(void*c,alarm_control_snapshot_v1*out){(void)c;io();*out=alarm;return 0;}
static int32_t step(void*c){(void)c;io();if(is("step-context"))fenced=true;return 0;}
static int32_t prepare(void*c,uint32_t op,uint32_t value,uint32_t expected,alarm_control_command_v1*out){
    (void)c;io();assert(op==1&&value==465&&expected==alarm.revision);assert(!apply_calls);out->struct_size=sizeof(*out);memset(out->bytes,0,sizeof(out->bytes));risc_scene_put32(out->bytes,++command_id);return 0;
}
static int32_t apply(void*c,const alarm_control_command_v1*cmd){
    (void)c;io();++apply_calls;uint8_t payload[132];risc_scene_navigation_v1 n;saved_state(payload,&n);
    assert(risc_scene_get32(payload+24)==1&&!memcmp(payload+36,cmd->bytes,96)); /* journal before domain mutation */
    if(is("apply-retained"))return ALARM_CONTROL_RETAINED;
    unsigned id=risc_scene_get32(cmd->bytes);if(id!=committed_id){++domain_writes;committed_id=id;alarm.revision++;alarm.confirmed_revision=alarm.revision;alarm.minutes=465;alarm.flags|=ALARM_CONTROL_ENABLED;}
    return is("save-unknown")&&apply_calls==1?ALARM_CONTROL_UNCERTAIN:ALARM_CONTROL_OK;
}
static int32_t dismiss(void*c,const alarm_control_occurrence_v1*t){(void)c;(void)t;io();return 0;}
static const alarm_control_api_v1 domain={1,sizeof(domain),NULL,step,read_control,prepare,apply,dismiss};
static const risc_app_data_v1 storage={1,sizeof(storage),NULL,stat_file,read_file,replace_file};
static const risc_scene_node_v1 *node(uint32_t id){for(unsigned i=0;i<view.node_count;i++)if(view.nodes[i].id==id)return &view.nodes[i];assert(0);return NULL;}
static int32_t scene_open(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*p,uint64_t*s){
    (void)c;io();++opens;view=*d;path=*p;*s=100+invocation;
    if(invocation==2&&is("edit-restore")){assert(path.depth==2&&path.routes[1]==2&&path.focus[1]==10&&node(10)->value==465);}
    if(invocation==2&&is("save-unknown")){assert(path.depth==1&&!(node(8)->flags&RISC_SCENE_HIDDEN));assert(apply_calls==1);}
    return 0;
}
static int32_t update(void*c,uint64_t s,const risc_scene_document_v1*d){(void)c;(void)s;io();assert(d->revision>view.revision);view=*d;return 0;}
static int32_t navigate(void*c,uint64_t s,uint32_t op,uint32_t route){(void)c;(void)s;(void)route;io();assert(op==RISC_SCENE_ROOT);path=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(path),.depth=1,.routes={1}};return 0;}
static int32_t snapshot(void*c,uint64_t s,risc_scene_navigation_v1*p,uint32_t*f){(void)c;(void)s;io();if(is("snapshot-retained")&&phase==3)return RISC_SCENE_RETAINED;*p=path;*f=0;return 0;}
static int32_t close_scene(void*c,uint64_t s){(void)c;(void)s;io();++closes;return is("close-retained")?RISC_SCENE_RETAINED:RISC_SCENE_OK;}
static int32_t event(risc_scene_event_v1 *e,uint32_t kind,uint32_t id,uint32_t action,int32_t value){
    *e=(risc_scene_event_v1){sizeof(*e),kind,view.revision,id,action,value,++event_seq};return 0;
}
static int32_t next_scene(void*c,uint64_t s,risc_scene_event_v1*e){
    (void)c;(void)s;io();assert(++ticks<200);
    if(is("corrupt-checkpoint")){assert(node(3)->flags&RISC_SCENE_DISABLED);return event(e,RISC_SCENE_SUSPEND_EVENT,0,0,0);}
    if(is("close-retained"))return event(e,RISC_SCENE_SUSPEND_EVENT,0,0,0);
    if(invocation==2){
        if(!phase++){
            if(is("edit-restore"))return event(e,RISC_SCENE_ACTION_EVENT,0,5,0);
            assert(is("save-unknown"));return event(e,RISC_SCENE_ACTION_EVENT,8,10,0);
        }
        return event(e,RISC_SCENE_SUSPEND_EVENT,0,0,0);
    }
    if(phase==0){ /* Renderer follows the declared link, not application geometry. */
        const risc_scene_node_v1 *link=node(3);assert(link->kind==RISC_SCENE_LINK&&link->target==2);
        path.depth=2;path.routes[1]=link->target;path.focus[1]=10;phase++;return RISC_SCENE_IDLE;
    }
    if(phase==1){phase++;return event(e,RISC_SCENE_VALUE_EVENT,10,1,465);}
    if(is("edit-restore"))return event(e,RISC_SCENE_SUSPEND_EVENT,0,0,0);
    if(is("stale-event")){
        if(phase++==2){event(e,RISC_SCENE_ACTION_EVENT,11,2,0);--e->document_revision;return 0;}
        return event(e,RISC_SCENE_SUSPEND_EVENT,0,0,0);
    }
    if(phase==2){phase++;return event(e,RISC_SCENE_ACTION_EVENT,11,2,0);}
    if(is("checkpoint-failure")&&phase==3){assert(!apply_calls&&!domain_writes&&path.depth==1);phase++;return event(e,RISC_SCENE_ACTION_EVENT,8,10,0);}
    return event(e,RISC_SCENE_SUSPEND_EVENT,0,0,0);
}
static const risc_scene_api_v1 ui={1,sizeof(ui),NULL,scene_open,update,next_scene,navigate,snapshot,close_scene};
static bool acquire(const char*name,uint32_t api,uint64_t instance,risc_runtime_capability_v1*g){
    io();++acquires;assert(api==1&&instance==0);
    if(!strcmp(name,"ui.scene")){if(is("acquire-context")){fenced=true;return false;}if(is("headless"))return false;g->api=&ui;g->slot=1;}
    else if(!strcmp(name,"alarm.control")){g->api=&domain;g->slot=2;}
    else if(!strcmp(name,"storage.app-data")){g->api=&storage;g->slot=3;}
    else assert(0&&"application requested presentation hardware");
    return true;
}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.yield_ms=yield_ms,.diagnostic=log_line,.acquire=acquire,.release=release,.retain_invocation=retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t api){assert(api==1);return fenced?NULL:&rt;}
int main(int argc,char**argv){
    assert(argc==2);mode=argv[1];alarm=(alarm_control_snapshot_v1){.api_version=1,.struct_size=sizeof(alarm),.flags=ALARM_CONTROL_CONFIG_VALID|ALARM_CONTROL_CLOCK_VALID|ALARM_CONTROL_SERVICE_READY|ALARM_CONTROL_VOLUME_VALID|ALARM_CONTROL_HAS_VOLUME,.minutes=390,.volume=50,.zone="UTC",.status="NO ALARM ARMED"};
    if(is("corrupt-checkpoint")){present_file=true;memset(file_bytes,0xa5,228);file_size=228;}
    invocation=1;app_main();
    if(is("edit-restore")||is("save-unknown")){assert(!fenced);invocation=2;phase=ticks=0;event_seq=0;app_main();}
    if(is("headless")){assert(acquires==1&&!opens&&!file_writes&&!domain_writes);}
    else if(strstr(mode,"context")){assert(fenced&&!retains&&!releases);}
    else if(strstr(mode,"retained")){assert(fenced&&retains==1&&!releases);}
    else {
        assert(!retains&&releases==3*invocation&&closes==invocation);
        if(is("edit-restore")||is("stale-event")||is("corrupt-checkpoint"))assert(!domain_writes);
        else assert(domain_writes==1);
        if(!is("corrupt-checkpoint")){uint8_t p[132];risc_scene_navigation_v1 n;saved_state(p,&n);assert(!risc_scene_get32(p+24));}
    }
    printf("alarms app %s PASS invocations=%u journal-writes=%u domain-writes=%u retained=%u\n",mode,invocation,file_writes,domain_writes,retains);return 0;
}
