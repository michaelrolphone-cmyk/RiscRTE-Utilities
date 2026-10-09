#define PORTABLE_CONTEXTS_CLIENT
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "PortableContextsClient.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t pixels[240*240+2];
static risc_display_surface_v1 surface;
static bool list_mode;
static int width(void){return 240;}
static int height(void){return 240;}
static void clear_color(uint32_t color){(void)color;memset(pixels+1,0,240*240*2);}
#include CONTEXTS_NOVA_UI
static unsigned writes,reads,acquires,releases,stops,requests;
static bool stop_fail,write_fail,commit_unknown,read_after_write_fail;
static int unread=-1;
static uint8_t cells[9][64];static uint32_t sizes[9];
static contexts_status_v1 live;
static contexts_model_details_v1 model_status[2];
static unsigned model_reads;
static bool get_models(void*c,uint32_t source,contexts_model_details_v1*out){(void)c;model_reads++;*out=model_status[source==CONTEXTS_AUDIO?0:1];return true;}
static int key_index(const char *key){if(!strcmp(key,"contexts_on"))return 8;assert(!strncmp(key,"ctx_p",5)&&key[5]>='0'&&key[5]<='7'&&!key[6]);return key[5]-'0';}
static int32_t kv_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size){(void)c;reads++;int i=key_index(key);*size=0;if(i==unread||(read_after_write_fail&&writes>=2))return RISC_KEY_VALUE_IO;if(!sizes[i])return RISC_KEY_VALUE_NOT_FOUND;*size=sizes[i];if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(data,cells[i],*size);return RISC_KEY_VALUE_OK;}
static int32_t kv_put(void *c,const char *key,const void *data,uint32_t size){(void)c;int i=key_index(key);assert(size<=64);writes++;if(!write_fail||commit_unknown){memcpy(cells[i],data,size);sizes[i]=size;}return write_fail?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static bool acquire(const char *cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *out){assert(!strcmp(cap,RISC_KEY_VALUE_CAPABILITY)&&api==1&&instance==1&&stops);acquires++;out->api=&kv;return true;}
static bool release(risc_runtime_capability_v1 *grant){assert(grant->api==&kv);releases++;grant->api=NULL;return true;}
static bool get_status(void *c,contexts_status_v1 *out){(void)c;*out=live;return true;}
static int32_t label(void *c,uint32_t source,uint32_t slot,contexts_label_v1 *out){(void)c;if(slot>=8)return 0;*out=(contexts_label_v1){.source=source,.slot=slot};if(!slot){out->kind=1;strcpy(out->name,source==CONTEXTS_AUDIO?"Study":"Studio");}return 1;}
static bool request(void *c,uint32_t sources){(void)c;assert(sources==CONTEXTS_ALL);requests++;live.export_pending=sources;return true;}
static const contexts_service_v1 service={.api_version=1,.struct_size=sizeof(service),.status=get_status,.label=label,.request_export=request};
const contexts_service_v1 *portable_contexts_service(void){return &service;}
bool portable_contexts_stop(void){stops++;return !stop_fail;}
bool portable_contexts_enable(bool enabled){return portable_context_enabled_save(&kv,enabled);}
unsigned portable_contexts_face_count(void){return 3;}
const char *portable_contexts_face_name(unsigned id){static const char *names[]={"NOVA","ANALOG","CONTEXTS"};return id<3?names[id]:NULL;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
static void present(bool partial){(void)partial;assert(pixels[0]==0xa55a&&pixels[240*240+1]==0xa55a);}
static const t5_app_api_v1 app={.abi_version=1,.struct_size=sizeof(app),.present=present};
const t5_app_api_v1 *t5_app_get_api(uint32_t version){return version==1?&app:NULL;}
#include "../../Apps/contexts.c"
static void screenshot(const char *directory,const char *name) {
    ctx_draw();char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n240 240\n255\n");
    for(unsigned i=1;i<=240*240;i++){uint16_t c=pixels[i];uint8_t rgb[]={(uint8_t)((c>>11)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};assert(fwrite(rgb,1,3,out)==3);}assert(!fclose(out));
}
int main(int argc,char **argv) {
    assert(argc==2);pixels[0]=pixels[240*240+1]=0xa55a;surface=(risc_display_surface_v1){.frame=1,.pixels=pixels+1,.stride_bytes=480,.width=240,.height=240};
    ctx_app=&app;ctx_runtime=&runtime;ctx_service=&service;ctx_page=CT_HOME;ctx_message="";
    live=(contexts_status_v1){.struct_size=sizeof(live),.state=CONTEXTS_LIVE,
        .audio={.source=CONTEXTS_AUDIO,.model_state=CONTEXTS_MODEL_READY,.signatures_ready=true,.current=true,.room_valid=true,.room_name="Study",.event_valid=true,.event_name="Clink"},
        .radio={.source=CONTEXTS_RADIO,.model_state=CONTEXTS_MODEL_READY,.signatures_ready=true,.current=true,.room_valid=true,.room_name="Studio",.event_name="Burst"}};
    ctx_load();assert(ctx_enabled_valid&&!ctx_enabled&&!writes&&acquires==releases);ctx_read_status();screenshot(argv[1],"contexts-home");
    ctx_tap(174,208);assert(ctx_page==CT_PRESETS);ctx_tap(90,65);assert(ctx_page==CT_ROOMS&&ctx_room_count==2&&!writes);screenshot(argv[1],"contexts-rooms");
    ctx_tap(90,65);assert(ctx_page==CT_EDIT&&ctx_draft.source==CONTEXTS_AUDIO&&!ctx_draft.enabled&&!ctx_draft.actions&&!writes);
    ctx_tap(120,143);assert(ctx_page==CT_FIELD);ctx_field=3;ctx_tap(120,96);ctx_tap(208,148);assert(ctx_draft.actions==PORTABLE_CONTEXT_BRIGHTNESS&&ctx_draft.brightness==50&&!writes);screenshot(argv[1],"contexts-brightness");
    ctx_tap(170,208);ctx_tap(120,103);assert(ctx_draft.enabled);screenshot(argv[1],"contexts-preset");ctx_tap(60,208);assert(ctx_page==CT_PRESETS&&writes==1&&ctx_presets[0].enabled);
    ctx_open_preset(0);ctx_field=4;ctx_change(1);ctx_back();assert(ctx_page==CT_DISCARD);screenshot(argv[1],"contexts-discard");ctx_tap(170,194);assert(ctx_page==CT_EDIT&&ctx_draft.volume==60);ctx_back();ctx_tap(60,194);assert(ctx_page==CT_PRESETS&&writes==1&&ctx_presets[0].volume==50);
    ctx_open_preset(0);ctx_tap(120,60);assert(ctx_page==CT_ROOMS);ctx_back();assert(ctx_page==CT_EDIT);
    ctx_field=3;ctx_change(1);write_fail=commit_unknown=read_after_write_fail=true;ctx_save();assert(ctx_save_uncertain&&ctx_page==CT_EDIT&&writes==2);unsigned before=ctx_draft.brightness;ctx_change(1);assert(ctx_draft.brightness==before);screenshot(argv[1],"contexts-save-unconfirmed");
    write_fail=commit_unknown=read_after_write_fail=false;ctx_save();assert(!ctx_save_uncertain&&ctx_page==CT_PRESETS&&writes==2&&ctx_presets[0].brightness==60);
    unread=1;ctx_load();ctx_open_preset(1);assert(ctx_page==CT_PRESETS&&!strcmp(ctx_message,"UNREAD PRESET / TAP RETRY"));screenshot(argv[1],"contexts-unread");unread=-1;ctx_load();ctx_open_preset(1);assert(ctx_page==CT_ROOMS);
    ctx_page=CT_HOME;ctx_load_models();assert(!requests&&!ctx_exit);ctx_tap(120,60);assert(ctx_enabled&&writes==3);ctx_load_models();assert(requests==1&&ctx_exit&&live.export_pending==CONTEXTS_ALL);
    ctx_exit=false;ctx_page=CT_HOME;live.audio.current=false;live.audio.room_valid=false;live.radio.room_valid=false;live.radio.room_ambiguous=true;ctx_read_status();screenshot(argv[1],"contexts-paused-ambiguous");
    ctx_page=CT_FIELD;ctx_field=7;ctx_draft.face=2;screenshot(argv[1],"contexts-watch-face");
    ctx_field=1;ctx_draft.idle_ms=3599000;ctx_change(1);assert(ctx_draft.idle_ms==3600000);ctx_draft.idle_ms=6000;ctx_change(-1);assert(ctx_draft.idle_ms==5000);
    ctx_field=3;ctx_draft.brightness=99;ctx_change(1);assert(ctx_draft.brightness==100);ctx_draft.brightness=15;ctx_change(-1);assert(ctx_draft.brightness==10);
    contexts_service_v1 extended=service;extended.model_details=get_models;ctx_service=&extended;ctx_page=CT_HOME;
    model_status[0]=(contexts_model_details_v1){.struct_size=sizeof(contexts_model_details_v1),.source=CONTEXTS_AUDIO,.temporal_state=CONTEXTS_IMPORT_MISSING,.neural_state=CONTEXTS_IMPORT_MISSING};
    model_status[1]=model_status[0];model_status[1].source=CONTEXTS_RADIO;
    ctx_read_status();ctx_tap(90,99);assert(ctx_page==CT_MODELS&&!ctx_model_source&&model_reads==2);screenshot(argv[1],"contexts-models-missing");
    assert(!strcmp(ctx_import_caption(CONTEXTS_IMPORT_MISSING,0,false),"NO SAVED EVENTS"));
    model_status[0].temporal_state=CONTEXTS_IMPORT_READY;model_status[0].neural_state=CONTEXTS_IMPORT_FAILED;model_status[0].neural_error=CONTEXTS_IMPORT_STALE;ctx_read_status();screenshot(argv[1],"contexts-models-stale");
    assert(!strcmp(ctx_import_caption(CONTEXTS_IMPORT_FAILED,CONTEXTS_IMPORT_STALE,true),"STALE / RELOAD"));
    model_status[0].temporal_state=CONTEXTS_IMPORT_FAILED;model_status[0].bank_error[0]=RISC_APP_DATA_IO;ctx_read_status();screenshot(argv[1],"contexts-models-failed");
    model_status[0].temporal_state=model_status[0].neural_state=CONTEXTS_IMPORT_READY;model_status[0].neural_error=model_status[0].bank_error[0]=0;model_status[0].event_engine=CONTEXTS_EVENT_NEURAL;
    live.audio.current=live.audio.room_valid=live.audio.event_valid=true;ctx_read_status();screenshot(argv[1],"contexts-models-ready");ctx_back();screenshot(argv[1],"contexts-neural-event");
    unsigned char *old=malloc(CONTEXTS_SERVICE_V1_SIZE);assert(old);memcpy(old,&service,CONTEXTS_SERVICE_V1_SIZE);uint32_t prefix_size=CONTEXTS_SERVICE_V1_SIZE;memcpy(old+offsetof(contexts_service_v1,struct_size),&prefix_size,sizeof(prefix_size));ctx_service=(const void*)old;
    before=model_reads;ctx_read_status();assert(model_reads==before&&!ctx_model_valid[0]&&!ctx_model_valid[1]);ctx_tap(90,149);assert(ctx_page==CT_MODELS&&ctx_model_source==1);screenshot(argv[1],"contexts-models-old-provider");free(old);ctx_service=&service;
    stop_fail=true;before=acquires;ctx_load();assert(ctx_retained&&acquires==before&&acquires==releases);
    printf("Contexts Nova7 production UI: no default writes, identity selection, action masks, verified Save, draft discard, uncertain retry, unread protection, model retry and retained storage fence PASS\n");
}
