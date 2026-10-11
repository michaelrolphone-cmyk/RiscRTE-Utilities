/* The shipping app_main + the production shared renderer, with only hardware
 * and persistence transports replaced. Screens are captured from that path. */
#define main scene_transport_tests
#define risc_runtime_get_api host_fixture_runtime
#include "host_test.c"
#undef risc_runtime_get_api
#undef main
#include "RiscSceneComponentsV1.h"
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t);
#include "../../Apps/contexts.c"
static cr_store bank;
static uint8_t stored[CR_FILE_BYTES];static uint64_t revision=1;
static bool fail_write,learning,ready,fail_storage,terminal_mode;
static unsigned phase,app_acquires,app_releases,kv_writes,learn_saves;
static uint8_t preferences[4];static unsigned preferences_size;
static const char *folder,*device;
const t5_app_manifest_t portable_catalog[]={{.file_name="calculator.elf",.display_name="CALCULATOR",.compatible=true}};
const unsigned portable_catalog_count=1;
static int32_t file_stat(void*c,const char*n,uint32_t*size,uint64_t*rev){(void)c;*size=0;*rev=0;if(fail_storage)return RISC_APP_DATA_IO;if(strcmp(n,"context-rules.ctx"))return RISC_APP_DATA_NOT_FOUND;*size=sizeof(stored);*rev=revision;return 0;}
static int32_t file_read(void*c,const char*n,uint64_t rev,void*p,uint32_t cap,uint32_t*sz,uint64_t*actual){(void)c;assert(!strcmp(n,"context-rules.ctx")&&rev==revision&&cap>=sizeof(stored));memcpy(p,stored,sizeof(stored));*sz=sizeof(stored);*actual=revision;return 0;}
static int32_t file_replace(void*c,const char*n,uint64_t rev,const void*p,uint32_t size){(void)c;assert(!strcmp(n,"context-rules.ctx")&&rev==revision&&size==sizeof(stored));if(fail_write)return RISC_APP_DATA_IO;memcpy(stored,p,size);revision++;return 0;}
static const risc_app_data_v1 files={1,sizeof(files),NULL,file_stat,file_read,file_replace};
static int32_t pref_get(void*c,const char*n,void*p,uint32_t cap,uint32_t*size){(void)c;*size=0;if(strcmp(n,"contexts_on")||!preferences_size)return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=preferences_size);memcpy(p,preferences,preferences_size);*size=preferences_size;return 0;}
static int32_t pref_put(void*c,const char*n,const void*p,uint32_t size){(void)c;assert(!strcmp(n,"contexts_on")&&size<=4);memcpy(preferences,p,size);preferences_size=size;kv_writes++;return 0;}
static const risc_key_value_v1 prefs={1,sizeof(prefs),NULL,pref_get,pref_put};
static bool svc_pause(void*c){(void)c;return true;}
static bool svc_step(void*c,const contexts_policy_v1*p){(void)c;assert(p->enabled==((preferences_size&&preferences[2])||learning));return true;}
static bool svc_status(void*c,contexts_status_v1*q){(void)c;*q=(contexts_status_v1){.struct_size=sizeof(*q),.audio={.model_state=format==1?CONTEXTS_MODEL_UNAVAILABLE:CONTEXTS_MODEL_READY},.radio={.model_state=CONTEXTS_MODEL_READY}};return true;}
static bool svc_mask(void*c,uint32_t s){(void)c;(void)s;return true;}
static bool svc_record(void*c,uint32_t s,uint32_t k,uint32_t i,const void*b,uint32_t n){(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static bool svc_finish(void*c,uint32_t s,uint32_t r){(void)c;(void)s;(void)r;return true;}
static bool svc_result(void*c,uint32_t s,uint32_t g,uint32_t r){(void)g;return svc_finish(c,s,r);}
static int32_t svc_label(void*c,uint32_t s,uint32_t i,contexts_label_v1*q){(void)c;(void)s;(void)i;(void)q;return 0;}
static bool svc_claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){(void)c;(void)s;(void)i;(void)n;(void)g;return true;}
static bool fp(void*c,uint32_t op,void*p){(void)c;
 switch(op){
 case CONTEXTS_FP_STATUS:*(contexts_fingerprint_status_v1*)p=(contexts_fingerprint_status_v1){.struct_size=sizeof(contexts_fingerprint_status_v1),.sources=3,.room_slot=-1,.event_slot=-1};return true;
 case CONTEXTS_FP_CONFIG:return true;
 case CONTEXTS_FP_RULES:{contexts_rules_v1*q=p;if(q->set)bank=q->store;else {q->store=bank;q->available=true;}return true;}
 case CONTEXTS_FP_PROFILE:{contexts_profile_v1*q=p;if(q->index<2){q->kind=q->index+1;q->samples[1]=3;q->sources=2;strcpy(q->name,q->index?"DOORBELL":"HOME");}return true;}
 case CONTEXTS_FP_LOG:((contexts_log_v1*)p)->count=0;return false;
 case CONTEXTS_FP_LEARN:{contexts_learning_v1*q=p;if(q->operation==CONTEXTS_LEARN_BEGIN)learning=true;if(q->operation==CONTEXTS_LEARN_CANCEL)learning=false;if(q->operation==CONTEXTS_LEARN_SAVE){learning=false;learn_saves++;}q->state=ready?CONTEXTS_LEARN_READY:CONTEXTS_LEARN_RECORDING;q->progress=ready?100:40;q->samples=ready?3:1;q->sources=2;return true;}
 case CONTEXTS_FP_EVALUATE:((contexts_evaluate_v1*)p)->result=(cr_result){0};return true;
 case CONTEXTS_FP_WORKFLOW:((contexts_workflow_v1*)p)->available=false;return true;
 default:assert(0);return false;
 }
}
static const contexts_fingerprint_service_v1 service={.base={.api_version=1,.struct_size=sizeof(service),.step=svc_step,.pause=svc_pause,.status=svc_status,.request_export=svc_mask,.begin_export=svc_mask,.export_record=svc_record,.finish_export=svc_finish,.label=svc_label,.claim_preset=svc_claim,.preset_result=svc_result,.capture_audio=svc_pause},.fingerprint_abi=0x31504643u,.fingerprint=fp};
static bool app_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)v;(void)id;assert(native_alive);
 const void*p=!strcmp(n,RISC_SCENE_CAPABILITY)?api:!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)?(const void*)&service:!strcmp(n,RISC_SHARED_DATA_CAPABILITY)?(const void*)&files:!strcmp(n,RISC_KEY_VALUE_CAPABILITY)?(const void*)&prefs:NULL;
 if(!p)return false;
 g->api=p;g->slot=++app_acquires;g->generation=1;return true;
}
static bool app_release(risc_runtime_capability_v1*g){assert(native_alive&&g->api);app_releases++;return true;}
static bool health(risc_runtime_health_v1*q){q->uptime_ms=(uint32_t)ms;return true;}
static bool app_retain(void){native_alive=false;return true;}
static bool diagnostic(const char*s){fprintf(stderr,"app: %s\n",s);return true;}
extern bool contexts_test_target(uint32_t,int,int*,int*);
static void frame(const char*name){
 session=cs_session;settle();unsigned lw=w==800?h:w,lh=w==800?w:h;char path[1024];snprintf(path,sizeof(path),"%s/%s-%s.ppm",folder,device,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",lw,lh);
 for(unsigned y=0;y<lh;y++)for(unsigned x=0;x<lw;x++){unsigned px=x,py=y;if(prof.display_rotation==270){px=y;py=h-1-x;}uint8_t rgb[3];if(format==5){unsigned at=16+(py*w+px)*2;uint16_t c=pixels[at]|(uint16_t)pixels[at+1]<<8;rgb[0]=(c>>11)*255/31;rgb[1]=((c>>5)&63)*255/63;rgb[2]=(c&31)*255/31;}else{unsigned bit=py*w+px;rgb[0]=rgb[1]=rgb[2]=pixels[16+bit/8]&(128>>(bit%8))?0:255;}assert(fwrite(rgb,1,3,f)==3);}
 assert(!fclose(f));
}
static void tap(unsigned action,int value){
 session=cs_session;int x,y;
 for(unsigned i=0;i<100;i++){if(contexts_test_target(action,value,&x,&y)){touch_tap(x,y);return;}nav_pressed=RISC_NAV_DOWN;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);}
 fprintf(stderr,"No visible target for action %u value %d page %u\n",action,value,cu_page);assert(0);
}
static void back_input(void){nav_pressed=RISC_NAV_BACK;}
static void app_yield(uint32_t delay){
 ms+=delay;assert(phase<100);session=cs_session;
 if(!cs_session)return;
 /* Finish only completed raster transfers, never discard pending user input. */
 if(event_at!=event_count||nav_pressed)return;
 if(terminal_mode){loss_callback="status";allow_complete=false;begin_frame();return;}
 if(fail_storage){frame("unavailable");nav_pressed=RISC_NAV_HOME;phase=99;return;}
 switch(phase++){
 case 0:assert(cu_page==CU_HOME);frame("home");tap(CU_ROW_LIBRARY,-999);break;
 case 1:assert(cu_page==CU_ITEMS);frame("places");allow_complete=false;tap(CS_TAB,1);break;
 case 2:assert(cu_kind==CR_EVENT);begin_frame();assert(presenting);tap(CS_TAB,2);allow_complete=true;break;
 case 3:assert(cu_kind==CR_BATTERY);frame("triggers");tap(CU_ROW_ITEMS+2,-999);break;
 case 4:assert(cu_page==CU_TRIGGER);frame("battery");tap(CU_ROW_ARG0,5);break;
 case 5:assert(cu_store.items[2].arg0==25);tap(CU_ROW_ARG0,4);break;
 case 6:assert(cu_store.items[2].arg0==20);tap(CU_ROW_SAVE,-999);break;
 case 7:assert(!cu_edit);back_input();break;
 case 8:assert(cu_page==CU_ITEMS);tap(CS_TAB,0);break;
 case 9:tap(CU_ROW_ITEMS,-999);break;
 case 10:assert(cu_page==CU_ITEM);frame("place-details");tap(CU_ROW_SAMPLE,-999);break;
 case 11:assert(cu_page==CU_INTRO);frame("intro");tap(CU_ROW_START,-999);break;
 case 12:assert(cu_page==CU_LEARN&&learning&&!ctx_enabled);frame("learning");ready=true;break;
 case 13:assert(cu_learning.state==CONTEXTS_LEARN_READY);tap(CU_ROW_SAVE,-999);break;
 case 14:assert(cu_page==CU_DONE&&learn_saves==1&&!kv_writes);frame("learned");tap(CU_ROW_NEW,-999);break;
 case 15:assert(cu_page==CU_RULE);frame("rule");tap(CU_ROW_SETTINGS,-999);break;
 case 16:assert(cu_page==CU_SETTINGS);frame("settings");tap(CU_ROW_FIELD+CR_WIFI,1);break;
 case 17:assert(cu_store.rules[0].settings[CR_WIFI]==0);tap(CU_ROW_FIELD+CR_WIFI,0);break;
 case 18:assert(cu_store.rules[0].settings[CR_WIFI]==CR_KEEP);back_input();break;
 case 19:tap(CU_ROW_WORKFLOW,-999);break;
 case 20:assert(cu_page==CU_WORKFLOW);frame("workflow");tap(CU_ROW_ADD_STEP,-999);break;
 case 21:frame("add-step");tap(CU_ROW_TIMER,-999);break;
 case 22:frame("timer");tap(CU_ROW_DURATION+2,-999);break;
 case 23:assert(cu_store.rules[0].steps==1&&cu_store.rules[0].workflow[0].value==900);back_input();break;
 case 24:tap(CU_ROW_RENAME,-999);break;
 case 25:assert(cu_page==CU_NAME);frame("keyboard");tap(CS_KEY,'A');break;
 case 26:assert(strstr(cu_name,"A"));tap(CS_KEY,RISC_SCENE_KEY_DONE);break;
 case 27:fail_write=true;tap(CU_ROW_SAVE,-999);break;
 case 28:assert(cu_pending&&cu_page==CU_RULE);frame("save-failed");fail_write=false;tap(CU_ROW_SAVE,-999);break;
 case 29:assert(!cu_pending&&cu_page==CU_RULES);frame("contexts");tap(CU_ROW_RULE,-999);break;
 case 30:tap(CU_ROW_DELETE,-999);break;
 case 31:assert(cu_page==CU_DELETE);frame("delete");tap(CU_ROW_CANCEL,-999);break;
 case 32:assert(cu_page==CU_RULE);tap(CU_ROW_ENABLE,1);break;
 case 33:assert(cu_edit);back_input();break;
 case 34:assert(cu_page==CU_CANCEL);frame("discard");tap(CU_ROW_CANCEL,-999);break;
 case 35:assert(cu_page==CU_RULE);back_input();break;
 case 36:tap(CU_ROW_CONFIRM,-999);break;
 case 37:assert(cu_page==CU_HOME&&!cu_edit);tap(CU_ROW_AUTO,1);break;
 case 38:assert(ctx_enabled&&kv_writes==1);frame("auto-on");tap(CU_ROW_AUTO,0);break;
 case 39:assert(!ctx_enabled&&kv_writes==2);tap(CU_ROW_LIBRARY,-999);break;
 case 40:tap(CS_TAB,1);break;
 case 41:frame("events");tap(CU_ROW_NEW,-999);break;
 case 42:assert(cu_page==CU_NEW);frame("new-event");tap(CU_ROW_ITEMS+3,-999);break;
 case 43:assert(cu_page==CU_INTRO&&!strcmp(cu_name,"KNOCK"));frame("event-intro");ready=false;tap(CU_ROW_START,-999);break;
 case 44:assert(cu_page==CU_LEARN&&cu_learning.kind==2&&!ctx_enabled);frame("event-learning");tap(CU_ROW_CANCEL,-999);break;
 case 45:assert(cu_page==CU_CANCEL);tap(CU_ROW_CONFIRM,-999);break;
 case 46:assert(cu_page==CU_HOME&&!learning&&kv_writes==2);nav_pressed=RISC_NAV_HOME;break;
 default:assert(0);
 }
}
static const risc_runtime_api_v1 app_runtime={.api_version=1,.struct_size=sizeof(app_runtime),.yield_ms=app_yield,.diagnostic=diagnostic,.acquire=app_acquire,.release=app_release,.health=health,.retain_invocation=app_retain};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return native_alive&&v==1?&app_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==4);device=argv[1];folder=argv[2];fail_storage=!strcmp(argv[3],"storage");terminal_mode=!strcmp(argv[3],"retained");
 if(!strcmp(device,"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
 startup();assert(api->close(NULL,session)==0);expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 cr_init(&bank);bank.generation=1;bank.items[0]=(cr_item){.kind=CR_PLACE,.name="HOME"};bank.items[1]=(cr_item){.kind=CR_EVENT,.name="DOORBELL"};bank.items[2]=(cr_item){.kind=CR_BATTERY,.name="LOW BATTERY",.arg0=20};strcpy(bank.rules[0].name,"HOME");bank.rules[0].item=0;assert(cr_encode(&bank,stored,sizeof(stored)));
 app_main();
 if(terminal_mode){assert(!native_alive&&app_releases<app_acquires);puts("Contexts real launch: terminal presentation loss fences cleanup PASS");return 0;}
 assert(app_acquires==app_releases&&!held&&!presenting&&!subscribed);assert(phase==(fail_storage?99:47));assert(driver->quiesce());driver->stop();printf("Contexts actual app_main, renderer, touch flows, pending refresh, training, save retry %s PASS\n",device);
}
