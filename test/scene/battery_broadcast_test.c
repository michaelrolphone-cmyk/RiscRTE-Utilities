#include "../../Apps/battery_broadcast.h"
#include <assert.h>
#include <stdio.h>
static risc_runtime_api_v1 runtime,other;
static bool owner,missing_service,missing_preferences,missing_kv,release_fail,pause_fail,step_fail,swap_submit,bad_token;
static bool atomic_available,bad_atomic_flags,malformed_preference_grant;
static unsigned atomic_acquires,service_acquires,service_releases,live_polls;
static bool stored,malformed,radio_on,clean_pending,default_enabled,default_fail,old_provider;static uint8_t value[4],submitted[4];
static unsigned retains,releases,submits,collects,cancels,pauses,steps,puts_count,pending,uptime;
static int submit_result,receipt_result,state_result;static telemetry_broadcast_policy_v1 applied;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t n){return n==1?(owner?&runtime:&other):NULL;}
static bool alive(void*c){(void)c;return owner;}
static void hold(void*c){(void)c;retains++;}
static bool svc_pause(void*c){(void)c;pauses++;return !pause_fail;}
static bool svc_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;steps++;if(allow&&p->enabled&&p->settings_valid&&p->radios_allowed)live_polls++;applied=*p;if(!allow)assert(!p->enabled||!p->radios_allowed||p->settings_valid);return !step_fail;}
static bool svc_status(void*c,telemetry_broadcast_status_v1*p){(void)c;*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p),.state=TELEMETRY_BROADCAST_PAUSED,.cleanup_pending=clean_pending,.enabled=applied.enabled,.settings_valid=applied.settings_valid};return true;}
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;(void)i;(void)out;return -1;}
static int32_t read_field(void*c,uint32_t i,int32_t*out){(void)c;(void)i;(void)out;return -1;}
static telemetry_broadcast_v1 service={1,sizeof(service),NULL,svc_step,svc_pause,svc_status,enumerate,read_field};
static bool default_policy(void*c,telemetry_broadcast_policy_v1*p){(void)c;*p=(telemetry_broadcast_policy_v1){.struct_size=sizeof(*p),.enabled=default_enabled,.settings_valid=true};return !default_fail;}
static telemetry_broadcast_policy_api_v1 extended;
static int32_t kv_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(cap>=4);*n=0;if(!strcmp(k,TELEMETRY_BROADCAST_KEY)){if(!stored)return RISC_KEY_VALUE_NOT_FOUND;memcpy(b,value,4);if(malformed)((uint8_t*)b)[0]=0;*n=4;return 0;}assert(!strcmp(k,"quick_radio"));uint8_t r[]={0x51,1,radio_on?2:1,(uint8_t)((radio_on?2:1)^0xa5)};memcpy(b,r,4);*n=4;return 0;}
static int32_t kv_put(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;puts_count++;assert(!"legacy write forbidden");return -1;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static int32_t ps(void*c,risc_key_value_atomic_state_v1*s){(void)c;*s=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*s),.namespace_id=1,.revision=7,.flags=RISC_PREFERENCE_STORE_COPIED_INPUT,.max_mutations=1,.max_value_bytes=64,.max_group_bytes=512,.max_intent_bytes=176};return state_result;}
static int32_t pg(void*c,uint64_t r,const char*k,void*b,uint32_t n,uint32_t*z){assert(r==7);return kv_get(c,k,b,n,z);}
static int32_t submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*i,uint32_t z,uint64_t*t){(void)c;assert(r==7&&n==1&&m->size==4&&!strcmp(m->key,"ble_broadcast")&&z==176&&!memcmp(i,"PFI0001",7));memcpy(submitted,m->value,4);submits++;*t=submit_result==0||bad_token?55:0;if(swap_submit)owner=false;return submit_result;}
static int32_t collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){(void)c;assert(t==55);collects++;if(pending){pending--;return RISC_ATOMIC_PENDING;}*r=(risc_key_value_atomic_receipt_v1){.struct_size=sizeof(*r),.status=receipt_result,.operation_id=receipt_result==0?88:0,.revision=receipt_result==0?8:7};if(receipt_result==0){stored=true;memcpy(value,submitted,4);}return 0;}
static int32_t cancel(void*c,uint64_t t){(void)c;assert(t==55);cancels++;return 0;}
static int32_t atomic_state(void*c,risc_key_value_atomic_state_v1*out){int32_t rc=ps(c,out);out->flags=bad_atomic_flags?RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE:(RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC);return rc;}
static risc_key_value_atomic_v1 atomic={1,sizeof(atomic),NULL,atomic_state,pg,submit,collect,cancel};
static risc_preference_store_v1 preferences={1,sizeof(preferences),NULL,ps,pg,submit,collect,cancel};
static bool acquire(const char*k,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1);const void*p=NULL;if(!strcmp(k,TELEMETRY_BROADCAST_CAPABILITY)){assert(id==0);if(missing_service)return false;service_acquires++;p=old_provider?(void*)&service:(void*)&extended;}else if(!strcmp(k,RISC_PREFERENCE_STORE_CAPABILITY)){assert(id==1);if(missing_preferences){if(malformed_preference_grant)g->slot=99;return false;}p=&preferences;}else if(!strcmp(k,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){assert(id==1);atomic_acquires++;if(!atomic_available)return false;p=&atomic;}else{assert(!strcmp(k,RISC_KEY_VALUE_CAPABILITY)&&id==1);if(missing_kv)return false;p=&kv;}g->api=p;g->slot=1;g->generation=2;return true;}
static bool release(risc_runtime_capability_v1*g){releases++;if(release_fail)return false;if(g->api==&service||g->api==&extended){service_releases++;applied=(telemetry_broadcast_policy_v1){0};}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool health(risc_runtime_health_v1*h){h->uptime_ms=uptime;return true;}
static battery_broadcast s;
static void reset(void){atomic_available=bad_atomic_flags=malformed_preference_grant=false;atomic_acquires=service_acquires=service_releases=live_polls=0;applied=(telemetry_broadcast_policy_v1){0};owner=true;default_enabled=true;default_fail=old_provider=false;extended=(telemetry_broadcast_policy_api_v1){.base=service,.tag=TELEMETRY_BROADCAST_POLICY_TAG,.version=1,.defaults=default_policy};extended.base.struct_size=sizeof(extended);missing_service=missing_preferences=missing_kv=release_fail=pause_fail=step_fail=swap_submit=bad_token=false;stored=true;malformed=clean_pending=false;radio_on=true;retains=releases=submits=collects=cancels=pauses=steps=puts_count=pending=0;uptime=100;submit_result=receipt_result=state_result=0;memcpy(value,(uint8_t[]){0x62,1,1,0xa4},4);runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.health=health};other=runtime;battery_broadcast_init(&s,&runtime,alive,hold,NULL);}
int main(void){telemetry_broadcast_status_v1 status;
 reset();assert(battery_broadcast_step(&s,true)==0&&applied.enabled&&applied.radios_allowed&&applied.settings_valid&&!puts_count);
 reset();radio_on=false;assert(!battery_broadcast_step(&s,true)&&!applied.radios_allowed);
 reset();malformed=true;assert(!battery_broadcast_step(&s,true)&&!applied.settings_valid);
 reset();missing_kv=true;assert(!battery_broadcast_step(&s,true)&&!applied.settings_valid);
 reset();assert(!battery_broadcast_pause(&s));missing_service=true;assert(battery_broadcast_read(&s,&status)==BATTERY_BROADCAST_UNAVAILABLE&&battery_broadcast_cleanup(&s)==0);
 reset();missing_preferences=true;assert(battery_broadcast_set_enabled(&s,false)==BATTERY_BROADCAST_UNAVAILABLE&&!submits&&!puts_count);assert(!battery_broadcast_read(&s,&status));
 reset();assert(battery_broadcast_set_enabled(&s,false)==BATTERY_BROADCAST_PENDING&&submits==1);assert(!battery_broadcast_step(&s,false)&&!value[2]&&!s.edit.token&&!s.save_failed&&!puts_count);
 reset();pending=2;assert(battery_broadcast_set_enabled(&s,false)==3);unsigned before=releases;assert(battery_broadcast_read(&s,&status)==3&&battery_broadcast_pause(&s)==3&&releases==before);assert(battery_broadcast_step(&s,false)==3&&battery_broadcast_step(&s,false)==3);assert(!battery_broadcast_step(&s,false)&&collects==3&&submits==1);
 reset();receipt_result=RISC_ATOMIC_UNCERTAIN;assert(battery_broadcast_set_enabled(&s,false)==3&&battery_broadcast_step(&s,false)==5&&s.save_failed);for(int i=0;i<3;i++)assert(!battery_broadcast_step(&s,true));assert(submits==1&&!puts_count);receipt_result=0;assert(battery_broadcast_set_enabled(&s,false)==3&&!battery_broadcast_step(&s,false)&&submits==2);
 reset();receipt_result=RISC_ATOMIC_CONFLICT;assert(battery_broadcast_set_enabled(&s,false)==3&&battery_broadcast_step(&s,false)==2&&!puts_count);
 reset();submit_result=RISC_ATOMIC_BUSY;assert(battery_broadcast_set_enabled(&s,false)==2&&!s.edit.token&&submits==1&&!collects&&!puts_count);
 reset();submit_result=RISC_ATOMIC_IO;assert(battery_broadcast_set_enabled(&s,false)==2&&!puts_count);
 reset();submit_result=RISC_ATOMIC_IO;bad_token=true;assert(battery_broadcast_set_enabled(&s,false)==4&&s.edit.token==55&&retains==1);before=releases;assert(battery_broadcast_cleanup(&s)==4&&releases==before);
 reset();swap_submit=true;assert(battery_broadcast_set_enabled(&s,false)==4&&retains==1&&s.edit.token==55);
 reset();pause_fail=true;assert(battery_broadcast_set_enabled(&s,false)==4&&!submits&&retains==1);
 reset();release_fail=true;assert(battery_broadcast_pause(&s)==4&&retains==1);
 reset();clean_pending=true;assert(battery_broadcast_read(&s,&status)==4&&retains==1);
 reset();step_fail=true;assert(battery_broadcast_step(&s,true)==4&&retains==1);
 reset();state_result=-99;assert(battery_broadcast_set_enabled(&s,false)==4&&!submits&&retains==1);
 reset();pending=1;assert(battery_broadcast_set_enabled(&s,false)==3);assert(battery_broadcast_cleanup(&s)==3&&cancels==1);assert(!battery_broadcast_cleanup(&s)&&cancels==1&&!s.edit.token&&!s.preference.api&&!s.service.api);
 reset();stored=false;default_enabled=true;assert(!battery_broadcast_step(&s,true)&&applied.settings_valid&&applied.enabled&&!puts_count);
 reset();stored=false;default_enabled=false;assert(!battery_broadcast_step(&s,true)&&applied.settings_valid&&!applied.enabled&&!puts_count);
 reset();stored=true;default_enabled=false;assert(!battery_broadcast_step(&s,true)&&applied.enabled);
 reset();stored=false;old_provider=true;assert(!battery_broadcast_pause(&s));assert(!battery_broadcast_step(&s,true)&&!applied.settings_valid&&!applied.enabled);
 reset();stored=false;extended.tag=0;assert(battery_broadcast_step(&s,true)==4&&retains==1);
 reset();stored=false;default_fail=true;assert(battery_broadcast_step(&s,true)==4&&retains==1);
 reset();stored=true;old_provider=true;assert(!battery_broadcast_pause(&s));assert(!battery_broadcast_step(&s,true)&&applied.enabled);
 reset();unsigned acquired=service_acquires;for(int i=0;i<8;i++){uptime+=20;assert(!battery_broadcast_step(&s,true));assert(!battery_broadcast_read(&s,&status)&&status.enabled&&status.settings_valid);}assert(service_acquires==acquired&&!service_releases&&live_polls==8);assert(!battery_broadcast_cleanup(&s)&&service_releases==1&&!s.service.api);
 reset();assert(!battery_broadcast_step(&s,true));assert(!battery_broadcast_pause(&s));assert(!battery_broadcast_step(&s,true)&&!battery_broadcast_read(&s,&status)&&status.enabled);assert(service_acquires==2&&service_releases==1);
 reset();missing_preferences=true;atomic_available=true;assert(battery_broadcast_set_enabled(&s,false)==3&&atomic_acquires==1&&submits==1);assert(!battery_broadcast_step(&s,false)&&!value[2]&&!puts_count&&!battery_broadcast_read(&s,&status)&&!status.enabled);
 reset();atomic_available=true;assert(battery_broadcast_set_enabled(&s,false)==3&&!atomic_acquires);assert(!battery_broadcast_step(&s,false)&&!atomic_acquires);
 reset();missing_preferences=true;atomic_available=true;receipt_result=RISC_ATOMIC_UNCERTAIN;assert(battery_broadcast_set_enabled(&s,false)==3&&battery_broadcast_step(&s,false)==5&&atomic_acquires==1&&submits==1&&!puts_count);for(int i=0;i<5;i++)assert(!battery_broadcast_step(&s,true));assert(submits==1&&atomic_acquires==1);
 reset();missing_preferences=true;atomic_available=true;pending=2;assert(battery_broadcast_set_enabled(&s,false)==3);assert(battery_broadcast_cleanup(&s)==3&&s.preference.api==&atomic);assert(battery_broadcast_cleanup(&s)==3&&cancels==1);assert(!battery_broadcast_cleanup(&s)&&cancels==1&&!s.preference.api);
 reset();missing_preferences=true;atomic_available=true;bad_atomic_flags=true;assert(battery_broadcast_set_enabled(&s,false)==2&&!submits&&!puts_count);
 reset();missing_preferences=true;atomic_available=true;malformed_preference_grant=true;assert(battery_broadcast_set_enabled(&s,false)==4&&!atomic_acquires&&!submits);
 reset();atomic_available=true;state_result=RISC_ATOMIC_IO;assert(battery_broadcast_set_enabled(&s,false)==2&&!atomic_acquires&&!submits);
 reset();missing_preferences=true;atomic_available=true;swap_submit=true;assert(battery_broadcast_set_enabled(&s,false)==4&&s.edit.token==55&&retains==1);before=releases;assert(battery_broadcast_cleanup(&s)==4&&releases==before);
 puts("Battery broadcast: 37 capability/policy/transaction/custody cases passed");
}
