#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ALARM_SERVICE_TAGGED_V2
#define PORTABLE_NATIVE_CUSTODY_FENCE
void portable_adapter_retain_silent(void);
#include "PortableAlarmTerminalClient.h"
static bool fenced,lost;
static unsigned acquire_mode,retain_calls,release_calls;
static int32_t result=ALARM_OK;
static int32_t status(void *c,alarm_status_v1 *s){assert(c==(void*)42);*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return result;}
static int32_t step(void *c){assert(c==(void*)42);return result;}
static int32_t ack(void*c,const alarm_token_v1*t){assert(c==(void*)42&&t->generation==9);return result;}
static int32_t sleep_call(void*c,alarm_sleep_v1*t){assert(c==(void*)42);t->deadline=7;return result;}
static int32_t resume(void*c,const alarm_sleep_v1*t){assert(c==(void*)42&&t->deadline==7);return result;}
static alarm_service_descriptor_v2 descriptor;
static bool acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g){
 assert(!strcmp(name,ALARM_SERVICE_FACADE_CAPABILITY)&&version==2&&!id);
 if(acquire_mode==0)return false;
 if(acquire_mode==1){g->slot=1;return false;}
 if(acquire_mode==2){lost=true;return false;}
 *g=(risc_runtime_capability_v1){sizeof(*g),1,2,&descriptor};
 if(acquire_mode==3)lost=true;
 return true;
}
static bool release(risc_runtime_capability_v1*g){assert(g->api);release_calls++;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!lost?&runtime:NULL;}
bool portable_adapter_retained(void){return fenced;}
void portable_adapter_retain_silent(void){fenced=true;retain_calls++;}
void portable_adapter_retain(void){portable_adapter_retain_silent();}
static void reset(void){
 fenced=lost=false;retain_calls=release_calls=0;result=ALARM_OK;
 descriptor=(alarm_service_descriptor_v2){{2,sizeof(descriptor),(void*)42,status,step,step,ack,sleep_call,step},
 ALARM_SERVICE_DESCRIPTOR_TAG,1,ALARM_MODE_BOTH,ALARM_DESCRIPTOR_RESUME_SLEEP,resume};
}
int main(void){
 portable_alarm_client c;reset();acquire_mode=0;
 assert(portable_alarm_open(&c,&runtime)&&c.unavailable&&!c.api&&!c.grant.api);
 assert(!portable_alarm_callable(&c)&&!portable_alarm_refresh(&c));
 assert(portable_alarm_close(&c,&runtime)&&!release_calls);
 for(acquire_mode=1;acquire_mode<=3;acquire_mode++){reset();assert(!portable_alarm_open(&c,&runtime)&&fenced&&!c.unavailable);}
 reset();acquire_mode=4;descriptor.tag=0;
 assert(!portable_alarm_open(&c,&runtime)&&!c.unavailable);
 reset();assert(portable_alarm_open(&c,&runtime)&&!c.unavailable&&c.api==&descriptor.base);
 assert(portable_alarm_pump(&c));alarm_token_v1 token={.generation=9};assert(portable_alarm_acknowledge(&c,&token));
 alarm_sleep_v1 ticket={.struct_size=sizeof(ticket)};assert(c.api->prepare_sleep(c.api->context,&ticket)==ALARM_OK);
 assert(alarm_service_resume(c.api,&ticket)==ALARM_OK&&ticket.deadline==7);
 assert(portable_alarm_output_modes(&c)==ALARM_MODE_BOTH);
 result=ALARM_STORAGE;assert(portable_alarm_refresh(&c)&&!fenced&&!c.unavailable);
 result=ALARM_OUTPUT;assert(!portable_alarm_refresh(&c)&&fenced&&!c.unavailable);
 assert(!portable_alarm_close(&c,&runtime)&&!release_calls);
 puts("SDR alarm facade: refusal visibility, original callback/ticket forwarding and terminal custody PASS");return 0;
}
