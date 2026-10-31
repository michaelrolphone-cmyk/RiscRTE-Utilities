#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/calculator_scene.c"
static unsigned opens,updates,nexts,closes,release_calls,retains,yields,diagnostics;
static bool live=true,refuse_acquire,refuse_keypad,refuse_release,resident_enabled,policy_request;
static bool replace_context;static risc_runtime_api_v1 replacement;
static int checkpoint_result;static unsigned checkpoint_calls,dispatch_calls,configures;
static int fail_next,fail_update,close_again,fail_close,lose_at;
static const char *script="";static unsigned at;static risc_components_document_v1 shown;
static int32_t op(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;(void)n;assert(live&&!retains);opens++;if(refuse_keypad&&d->node_count>1)return RISC_SCENE_INVALID;shown=*d;*s=42;return 0;}
static int32_t up(void*c,uint64_t s,const risc_components_document_v1*d){(void)c;assert(s==42&&live&&!retains);updates++;if(fail_update)return fail_update;assert(d->revision>shown.revision);shown=*d;return 0;}
static int32_t nx(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;assert(s==42&&live&&!retains);nexts++;if(lose_at==1){live=false;return 0;}if(fail_next)return fail_next;
 *e=(risc_scene_event_v1){.struct_size=sizeof(*e),.sequence=nexts,.document_revision=shown.revision};
 char ch=script[at];if(ch=='@'){at++;e->kind=RISC_SCENE_CONTROLS_EVENT;return 0;}if(!ch){e->kind=RISC_SCENE_SUSPEND_EVENT;return 0;}at++;
 for(unsigned i=0;i<sizeof(keys);i++)if(keys[i]==ch){e->kind=RISC_SCENE_ACTION_EVENT;e->node=i+2;e->action=i+1;return 0;}
 assert(0);return 1;}
static int32_t cl(void*c,uint64_t s){(void)c;assert(s==42&&live&&!retains);closes++;if(lose_at==2){live=false;return 0;}if(fail_close)return fail_close;if(close_again-->0)return RISC_SCENE_AGAIN;return 0;}
static int32_t snap(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){(void)c;assert(s==42&&n&&n->api_version==1&&n->struct_size==sizeof(*n));*n=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(*n),.depth=1,.routes={1}};*f=0;return 0;}
static int32_t conf(void*c,uint64_t s,uint32_t f){(void)c;assert(s==42&&f==(resident_enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));configures++;return 0;}
static const risc_scene_components_api_v1 presenter={.lifecycle={.base={.api_version=1,.struct_size=sizeof(presenter),.next=nx,.close=cl,.snapshot=snap},.configure=conf},.tag=RISC_COMPONENTS_TAG,.version=1,.open=op,.update=up};
static bool acq(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!strcmp(n,RISC_SCENE_CAPABILITY)&&v==1&&!id);if(refuse_acquire)return false;g->api=&presenter;return true;}
static bool rel(risc_runtime_capability_v1*g){(void)g;assert(live&&!retains);release_calls++;return !refuse_release;}
static bool ret(void){assert(live&&!retains);retains++;return true;}
static void yield(uint32_t m){assert(m==20&&live&&!retains);yields++;assert(yields<500);}
static bool diag(const char*s){assert(s&&*s);diagnostics++;return true;}
static int32_t checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 assert(invocation==42&&live&&!retains);checkpoint_calls++;
 if(q->reason!=RISC_RESIDENT_CHECKPOINT_POLL){assert(closes>=opens);dispatch_calls++;return checkpoint_result;}
 if(policy_request){r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;policy_request=false;}
 return 0;
}
static bool shell(risc_resident_client_v1*c){if(lose_at==3)replace_context=true;if(!resident_enabled)return false;*c=(risc_resident_client_v1){.struct_size=sizeof(*c),.api_version=1,.invocation=42,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint};return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acq,.release=rel,.retain_invocation=ret,.yield_ms=yield,.diagnostic=diag,.resident_shell=shell};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&live?(replace_context?&replacement:&rt):NULL;}
static void run(const char *keys_in,const char *expected){script=keys_in;app_main();assert(!strcmp(state.text,expected));assert(opens==1&&closes==1&&release_calls==1&&!retains);}
int main(int argc,char**argv){assert(argc==2);int mode=atoi(argv[1]);switch(mode){
 case 0:run("12+3==","18");break;
 case 1:run("1/3=","0.333333");break;
 case 2:run("1.2345678","1.234567");break;
 case 3:run("12BSC9*9=","81");break;
 case 4:script="9/0=";app_main();assert(state.error==CALC_DIV_ZERO);assert(!strcmp(shown.nodes[0].label,"DIV BY ZERO"));break;
 case 5:run("9/0=7","7");break;
 case 6:script="999999999+1=";app_main();assert(state.error==CALC_OVERFLOW);break;
 case 7:refuse_acquire=true;app_main();assert(!opens&&!release_calls&&diagnostics==1);break;
 case 8:refuse_keypad=true;app_main();assert(opens==2&&closes==1&&release_calls==1&&!strcmp(shown.nodes[0].label,"KEYPAD UNAVAILABLE"));break;
 case 9:close_again=3;app_main();assert(closes==4&&yields==3&&release_calls==1);break;
 case 10:close_again=300;app_main();assert(closes==250&&retains==1&&!release_calls);break;
 case 11:fail_next=RISC_SCENE_RETAINED;app_main();assert(retains==1&&!closes&&!release_calls);break;
 case 12:script="1";fail_update=RISC_SCENE_RETAINED;app_main();assert(retains==1&&!closes&&!release_calls);break;
 case 13:fail_close=RISC_SCENE_RETAINED;app_main();assert(retains==1&&!release_calls);break;
 case 14:refuse_release=true;app_main();assert(retains==1&&release_calls==1);break;
 case 15:lose_at=1;app_main();assert(!closes&&!release_calls&&!retains);break;
 case 16:lose_at=2;app_main();assert(closes==1&&!release_calls&&!retains);break;
 case 17:fail_next=RISC_SCENE_STALE;app_main();assert(closes==1&&release_calls==1);break;
 case 18:{calc_reset(&state);document.revision=3;last_event=0;risc_scene_event_v1 e={sizeof(e),RISC_SCENE_ACTION_EVENT,2,6,5,0,1};assert(!event(&e)&&state.value==0);e.document_revision=3;assert(!event(&e)&&state.value==0);e.sequence=2;e.node=5;assert(!event(&e)&&state.value==0);e.sequence=3;e.node=6;assert(!event(&e)&&state.value==7000000);break;}
 case 19:run("0S1.5*2=","-3");break;
 case 20:resident_enabled=true;script="12@+3=";app_main();assert(!strcmp(state.text,"15")&&dispatch_calls==1&&opens==2&&closes==2&&release_calls==1&&configures==2);break;
 case 21:resident_enabled=true;policy_request=true;script="12+3=";app_main();assert(!strcmp(state.text,"15")&&dispatch_calls==1&&opens==2&&closes==2&&release_calls==1);break;
 case 22:resident_enabled=true;script="12@+3=";checkpoint_result=RISC_RESIDENT_RETAINED;app_main();assert(!strcmp(state.text,"12")&&retains==1&&dispatch_calls==1&&opens==1&&!release_calls);break;
 case 23:resident_enabled=true;script="12@+3=";checkpoint_result=RISC_RESIDENT_EXIT;app_main();assert(!strcmp(state.text,"12")&&dispatch_calls==1&&opens==1&&release_calls==1);break;
 case 24:resident_enabled=true;script="12@+3=";checkpoint_result=RISC_RESIDENT_BUSY;app_main();assert(!strcmp(state.text,"15")&&opens==2&&release_calls==1);break;
 case 26:lose_at=3;app_main();assert(!opens&&!closes&&!release_calls&&!retains);break;
 case 25:script="12@+3=";app_main();assert(!strcmp(state.text,"15")&&!dispatch_calls&&opens==1&&release_calls==1);break;
 default:assert(0);
 }printf("calculator scene case %d passed\n",mode);return 0;}
