#include "scanner_alarm.h"
#include <assert.h>
#include <stdio.h>
static risc_runtime_api_v1 rt,other;static scanner_alarm s;static unsigned calls,stops,steps,releases,retains,acks,close_calls,status_calls;static bool live=true,stop_ok=true,dirty,missing,replace,reentry,legacy;static int result,close_result;static alarm_status_v1 state;static alarm_service_descriptor_v2 api;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){(void)v;return replace?&other:&rt;}
static bool alive(void*c){(void)c;return live;}
static void retain(void*c){(void)c;retains++;}
static bool stop(void){stops++;return stop_ok;}
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){(void)n;(void)i;calls++;if(missing||(legacy&&v==2))return false;assert(v==(legacy?1u:2u));*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.api=&api,.slot=1,.generation=1};if(dirty)g->struct_size=0;return true;}
static bool release(risc_runtime_capability_v1*g){calls++;releases++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static int status(void*c,alarm_status_v1*out){(void)c;calls++;status_calls++;if(reentry){reentry=false;(void)scanner_alarm_step(&s,true,stop);}*out=state;return result;}
static int step(void*c){(void)c;calls++;steps++;if(state.state==ALARM_STATE_READY){state.state=ALARM_STATE_ALERT;state.occurrence=(alarm_token_v1){1,2,3,4};}else assert(stops);return result;}
static int refresh(void*c){(void)c;calls++;if(reentry){reentry=false;(void)scanner_alarm_refresh(&s);}return result;}
static int ack(void*c,const alarm_token_v1*t){(void)c;assert(!memcmp(t,&state.occurrence,sizeof(*t)));calls++;acks++;return result;}
static int prepare(void*c,alarm_sleep_v1*q){(void)c;(void)q;return 0;}
static int close_api(void*c){(void)c;calls++;close_calls++;if(close_result==ALARM_OK){state.state=ALARM_STATE_BLOCKED;state.error=ALARM_FOREGROUND;}return close_result;}
static void reset(void){calls=stops=steps=releases=retains=acks=close_calls=status_calls=0;live=stop_ok=true;dirty=missing=replace=reentry=legacy=false;result=close_result=0;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};api=(alarm_service_descriptor_v2){.base={2,sizeof(api),0,status,step,refresh,ack,prepare,close_api},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1};state=(alarm_status_v1){.api_version=1,.struct_size=sizeof(state),.state=ALARM_STATE_READY};scanner_alarm_init(&s,&rt,alive,retain,0);}
int main(int argc,char**argv){assert(argc==2);reset();const char*t=argv[1];
 if(!strcmp(t,"absent")){missing=true;assert(scanner_alarm_open(&s)&&!s.api&&calls==2);}
 else if(!strcmp(t,"dirty")){dirty=true;assert(!scanner_alarm_open(&s)&&retains==1&&!releases);}
 else {if(!strcmp(t,"legacy")){legacy=true;api.base.api_version=1;api.base.struct_size=sizeof(alarm_service_v1);}
 assert(scanner_alarm_open(&s));
 if(!strcmp(t,"legacy")){assert(calls==2&&s.api->api_version==1);assert(scanner_alarm_step(&s,true,stop)&&stops==1);alarm_token_v1 token=state.occurrence;assert(scanner_alarm_ack(&s,&token)&&acks==1);close_result=ALARM_PENDING;assert(!scanner_alarm_close(&s)&&!releases);close_result=ALARM_OK;assert(scanner_alarm_close(&s)&&releases==1);}
 else
 if(!strcmp(t,"refresh-reentry")){reentry=true;assert(!scanner_alarm_refresh(&s)&&s.terminal);}
 else if(!strcmp(t,"refresh")){assert(scanner_alarm_refresh(&s)&&!s.terminal);}
 else if(!strcmp(t,"reserve")){assert(scanner_alarm_step(&s,true,stop)&&steps==1&&stops==1&&s.owned);assert(scanner_alarm_step(&s,true,stop)&&steps==2&&stops==3);}
 else if(!strcmp(t,"defer")){assert(scanner_alarm_step(&s,false,stop)&&steps==0&&stops==0);}
 else if(!strcmp(t,"blocked-stop")){state.state=ALARM_STATE_ALERT;stop_ok=false;assert(scanner_alarm_step(&s,true,stop)&&!steps&&stops==1);}
 else if(!strcmp(t,"cue")){state.state=ALARM_STATE_CUE;assert(scanner_alarm_step(&s,false,stop)&&s.owned&&stops==1&&!steps);}
 else if(!strcmp(t,"generation")){state.occurrence.generation=1;assert(scanner_alarm_step(&s,false,stop)&&s.owned&&stops==1);}
 else if(!strcmp(t,"uncertain")){state.output_uncertain=1;assert(!scanner_alarm_step(&s,true,stop)&&!steps);}
 else if(!strcmp(t,"unknown")){result=-99;assert(!scanner_alarm_step(&s,true,stop)&&!steps);}
 else if(!strcmp(t,"bad-status")){state.struct_size=0;assert(!scanner_alarm_step(&s,true,stop)&&!steps);}
 else if(!strcmp(t,"reentry")){reentry=true;assert(!scanner_alarm_step(&s,true,stop)&&!steps);}
 else if(!strcmp(t,"identity")){replace=true;unsigned n=calls;assert(!scanner_alarm_step(&s,true,stop)&&calls==n);}
 else if(!strcmp(t,"ack")){assert(scanner_alarm_step(&s,true,stop));alarm_token_v1 token=state.occurrence;assert(scanner_alarm_ack(&s,&token)&&acks==1);token.generation++;assert(scanner_alarm_ack(&s,&token)&&acks==1);}
 else if(!strcmp(t,"close-pending")){state.state=ALARM_STATE_ALERT;close_result=ALARM_PENDING;assert(!scanner_alarm_close(&s)&&!releases&&!s.terminal);close_result=0;assert(scanner_alarm_close(&s)&&releases==1);}
 else if(!strcmp(t,"close-output")){state.state=ALARM_STATE_ALERT;close_result=ALARM_OUTPUT;assert(!scanner_alarm_close(&s)&&!releases&&s.terminal);}

 else if(!strcmp(t,"close-ready")){alarm_status_v1 before=state;assert(scanner_alarm_close(&s)&&releases==1&&!close_calls&&status_calls==1&&!memcmp(&before,&state,sizeof(state)));unsigned n=calls;assert(scanner_alarm_close(&s)&&calls==n);}
 else if(!strcmp(t,"close-loading")){state.state=ALARM_STATE_LOADING;assert(scanner_alarm_close(&s)&&releases==1&&!close_calls&&state.state==ALARM_STATE_LOADING);}
 else if(!strcmp(t,"close-shared-ready")){assert(scanner_alarm_close(&s)&&!close_calls&&state.state==ALARM_STATE_READY);assert(api.base.step(0)==0&&state.state==ALARM_STATE_ALERT&&steps==1);}
 else if(!strcmp(t,"close-stale-owned")){s.owned=true;assert(scanner_alarm_close(&s)&&!close_calls&&releases==1);}
 else if(!strcmp(t,"close-generation")){state.occurrence.generation=1;assert(scanner_alarm_close(&s)&&close_calls==1&&releases==1);}
 else if(!strcmp(t,"close-cue")){state.state=ALARM_STATE_CUE;assert(scanner_alarm_close(&s)&&close_calls==1&&releases==1);}
 else if(!strcmp(t,"close-blocked")){state.state=ALARM_STATE_BLOCKED;assert(scanner_alarm_close(&s)&&close_calls==1&&releases==1);}
 else if(!strcmp(t,"close-continuation")){state.state=ALARM_STATE_ALERT;close_result=ALARM_PENDING;assert(!scanner_alarm_close(&s)&&s.closing&&!releases);state.state=ALARM_STATE_READY;close_result=0;assert(scanner_alarm_close(&s)&&close_calls==2&&status_calls==1&&releases==1);}
 else if(!strcmp(t,"close-busy")){state.state=ALARM_STATE_ALERT;close_result=ALARM_BUSY;assert(!scanner_alarm_close(&s)&&s.closing&&!releases);close_result=0;assert(scanner_alarm_close(&s)&&close_calls==2&&releases==1);}
 else if(!strcmp(t,"close-bad-status")){state.struct_size=0;assert(!scanner_alarm_close(&s)&&s.terminal&&!close_calls&&!releases);}
 else if(!strcmp(t,"close-uncertain")){state.output_uncertain=1;assert(!scanner_alarm_close(&s)&&s.terminal&&!close_calls&&!releases);}
 else if(!strcmp(t,"close-unknown-status")){result=-99;assert(!scanner_alarm_close(&s)&&s.terminal&&!close_calls&&!releases);}
 else if(!strcmp(t,"close-reentry")){reentry=true;assert(!scanner_alarm_close(&s)&&s.terminal&&!close_calls&&!releases);}

 else assert(0);
 if(s.terminal){unsigned n=calls;assert(!scanner_alarm_step(&s,true,stop)&&calls==n);}
 }
 puts("PASS");return 0;}
