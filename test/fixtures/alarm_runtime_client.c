#include <assert.h>
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "AlarmRecords.h"
extern unsigned alarm_test_enter(void);
extern void alarm_test_advance(uint32_t);
static void pump(const alarm_service_v1*s,unsigned n){while(n--){int32_t r=s->step(s->context);assert(r==0);alarm_test_advance(1);}}
static alarm_status_v1 read_status(const alarm_service_v1*s){alarm_status_v1 v={.struct_size=sizeof(v)};assert(s->status(s->context,&v)==0);return v;}
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);unsigned invocation=alarm_test_enter();
 risc_runtime_capability_v1 service={.struct_size=sizeof(service)},store={.struct_size=sizeof(store)};
 assert(r->acquire("alarm.service",1,0,&service));const alarm_service_v1*s=service.api;
 assert(r->acquire("storage.key-value",1,3,&store));const risc_key_value_v1*kv=store.api;
 if(invocation==0){
  uint8_t bytes[32];alarm_config cfg={1,105,100,5,2,1};alarm_config_encode(&cfg,bytes);
  assert(kv->put(kv->context,"timer_cfg",bytes,32)==0);
  /* App namespace 3 never observes service-private namespace 4. */
  uint32_t n=0;assert(kv->get(kv->context,"timer_occ",bytes,32,&n)==RISC_KEY_VALUE_NOT_FOUND);
  s->refresh(s->context);pump(s,8);assert(read_status(s).schedules[1].state==ALARM_SCHEDULE_ARMED);
  assert(r->release(&store)&&r->release(&service));assert(r->request_launch("second.elf"));return;
 }
 if(invocation==2){assert(read_status(s).schedules[1].state==ALARM_SCHEDULE_DISMISSED);assert(r->release(&store)&&r->release(&service));return;}
 assert(invocation==1);alarm_test_advance(5000);pump(s,15);alarm_status_v1 v=read_status(s);
 assert(v.state==ALARM_STATE_ALERT&&v.occurrence.kind==2);
 assert(s->acknowledge(s->context,&v.occurrence)==ALARM_PENDING);pump(s,20);v=read_status(s);
 assert(v.state==ALARM_STATE_READY&&v.schedules[1].state==ALARM_SCHEDULE_DISMISSED);
 uint8_t bytes[32];uint32_t n=0;assert(kv->get(kv->context,"timer_occ",bytes,32,&n)==RISC_KEY_VALUE_NOT_FOUND);
 assert(r->release(&store)&&r->release(&service));
}
