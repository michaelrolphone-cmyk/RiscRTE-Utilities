#define ALARM_NATIVE_MIXED_POINTS
#define ALARM_SERVICE_CLOCK_EDIT_V2
#define POINTS_SERVICE_CONTEXT
#define main historical_fixture_main
#include "../native_apps/native_utc_alarm_service_test.c"
#undef main
#include "AlarmControlV1.h"
extern const risc_driver_v2 *control_driver_get(uint32_t);
static unsigned control_writes;
static int32_t control_put(void*c,const char*k,const void*b,uint32_t n){assert(c==blobs);int i=index_key(k);assert(i==0&&n==32);++control_writes;memcpy(blobs[i],b,n);sizes[i]=n;return 0;}
static const risc_bound_key_value_v1 control_store={1,sizeof(control_store),blobs,get_blob,control_put};
int main(void){
 epoch=INT64_C(946684800)+date(2026,10,5,10,0,0);boot(true);until(ALARM_STATE_READY);
 const risc_driver_v2*d=control_driver_get(2);const alarm_control_api_v1*a=d->capability;
 risc_provider_dependency_v1 dependencies[]={{"storage.key-value.bound",1,&control_store},{"alarm.service",2,client},{"platform.realtime",1,&real_table}};assert(d->start(dependencies,3));
 alarm_control_command_v1 cmd={.struct_size=sizeof(cmd)};assert(a->prepare(NULL,ALARM_CONTROL_ARM,720,0,&cmd)==0);
 const alarm_clock_edit_descriptor_v2*e=alarm_clock_edit_descriptor(client);assert(e);
 alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};assert(e->begin(NULL,seconds,&p)==0);
 assert(a->step(NULL)==ALARM_CONTROL_PENDING);
 alarm_control_snapshot_v1 state={.struct_size=sizeof(state)};assert(a->read(NULL,&state)==ALARM_CONTROL_PENDING&&!(state.flags&ALARM_CONTROL_SERVICE_READY));
 unsigned writes=put_count,gets=get_count;
 assert(a->apply(NULL,&cmd)==ALARM_CONTROL_PENDING);assert(!control_writes&&put_count==writes&&get_count==gets);
 assert(e->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 assert(a->apply(NULL,&cmd)==0&&control_writes==1);until(ALARM_STATE_READY);
 assert(a->apply(NULL,&cmd)==0&&control_writes==1);until(ALARM_STATE_READY);
 p=(alarm_clock_edit_permit_v2){.struct_size=sizeof(p)};assert(e->begin(NULL,seconds,&p)==0);
 assert(a->prepare(NULL,ALARM_CONTROL_CANCEL,0,1,&cmd)==ALARM_CONTROL_PENDING);
 assert(e->finish(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 assert(a->prepare(NULL,ALARM_CONTROL_CANCEL,0,1,&cmd)==0);assert(a->apply(NULL,&cmd)==0&&control_writes==2);until(ALARM_STATE_READY);
 p=(alarm_clock_edit_permit_v2){.struct_size=sizeof(p)};assert(e->begin(NULL,seconds,&p)==0);
 boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
 unsigned before_recovery=control_writes;assert(a->apply(NULL,&cmd)==ALARM_CONTROL_PENDING&&control_writes==before_recovery);
 p=(alarm_clock_edit_permit_v2){.struct_size=sizeof(p)};assert(e->begin(NULL,seconds,&p)==0);assert(e->finish(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 assert(a->apply(NULL,&cmd)==0&&control_writes==before_recovery);until(ALARM_STATE_READY);
 assert(d->quiesce());d->stop();puts("Current mixed scheduler + control: INTENT no-write, cancel, finish, exact retry PASS");
}
