/* Real deployed color provider and shared Watch sleep policy. */
#define PORTABLE_RTC_UTC8_DENVER
#define ALARM_VOLUME_CONTROL
#define ALARM_DND_CONTROL
#define ALARM_CLOCK_CHANGE_CONTROL
#ifdef TEST_BASELINE
#ifndef TEST_WATCH_BASELINE
#define ALARM_STARTUP_BUSY_RETRY
#endif
#else
#define ALARM_RECONCILE_BUSY_RETRY
#endif
#define main prior_points_fixture_main
#include "points_service_test.c"
#undef main
enum { WATCH_SLEEP_REFUSED=0, WATCH_SLEEP_WOKE=1 };
extern int test_watch_sleep(const alarm_service_v1 *);
extern unsigned test_sleep_stage, test_sleep_calls;
extern int32_t test_sleep_detail;

static phase_t inject_phase;
static int32_t inject_result;
static unsigned inject_remaining, injected_reads, busy_advance_ms;
static int32_t injected_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n) {
    if(phase==inject_phase && inject_remaining){
        --inject_remaining;++injected_reads;ms+=busy_advance_ms;*n=0;return inject_result;
    }
    return get_blob(c,k,b,cap,n);
}
static const risc_bound_key_value_v1 injected_store={1,sizeof(injected_store),NULL,injected_get,put_blob};
static void fresh(void){
    boot(true);kv=&injected_store;rtc_base=civil(2026,10,14,2,13);
    inject_remaining=injected_reads=test_sleep_calls=busy_advance_ms=0;inject_result=RISC_BOUND_KEY_VALUE_BUSY;
    points_config empty={.revision=1};save(empty);
    for(unsigned n=0;n<80&&phase!=IDLE;n++)pump(1);
    assert(phase==IDLE&&anchored&&!error);
}
static int sleep_now(void){return test_watch_sleep(client);}
#ifndef TEST_BASELINE
static void test_busy_preserves_pending_and_fences_unsafe(void) {
    const phase_t loads[]={LOAD_ALARM,LOAD_TIMER,LOAD_MODE,LOAD_VOLUME,LOAD_DND,LOAD_ALARM_OCC,LOAD_TIMER_OCC,LOAD_POINTS_CFG,LOAD_POINTS_OCC};
    for(unsigned i=0;i<sizeof(loads)/sizeof(*loads);i++) {
        fresh();
        /* Preserve both committed pending records and choose the earlier timer. */
        for(unsigned kind=1;kind<=2;kind++) {
            uint32_t delay=kind==1?3600u:1800u;
            alarm_config future={.revision=1,.deadline=rtc_base+delay,.created=rtc_base,
                                 .duration=kind==2?delay:0,.kind=(uint8_t)kind,.enabled=1};
            alarm_occurrence pending={.revision=1,.deadline=future.deadline,.generation=kind+6,
                                      .recovery_until=future.deadline+ALARM_RECOVERY_SECONDS,
                                      .kind=(uint8_t)kind,.state=ALARM_OCC_PENDING,.mode=ALARM_MODE_VIBRATE};
            assert(alarm_config_valid(&future)&&alarm_occurrence_valid(&pending));
            alarm_config_encode(&future,blobs[kind-1]);sizes[kind-1]=32;
            alarm_occurrence_encode(&pending,blobs[kind+2]);sizes[kind+2]=32;
        }
        client->refresh(NULL);for(unsigned n=0;n<80&&phase!=IDLE;n++)pump(1);
        assert(phase==IDLE&&!error);
        alarm_sleep_v1 prior=sleep_plan();assert(prior.deadline==rtc_base+1800&&sleep_ticket_valid);
        alarm_config committed[2];memcpy(committed,config,sizeof(config));
        alarm_occurrence committed_occ[2];memcpy(committed_occ,occurrences,sizeof(occurrences));
        uint8_t durable[sizeof(blobs)];memcpy(durable,blobs,sizeof(blobs));
        uint32_t snapshot_id=view.snapshot;
        inject_phase=loads[i];inject_remaining=1;client->refresh(NULL);
        for(unsigned n=0;n<80&&injected_reads==0;n++)pump(1);
        assert(injected_reads==1&&phase==loads[i]&&!error&&!sleep_ticket_valid);
        assert(!memcmp(committed,config,sizeof(config))&&!memcmp(committed_occ,occurrences,sizeof(occurrences)));
        assert(!memcmp(durable,blobs,sizeof(blobs))&&view.snapshot==snapshot_id&&!put_calls&&!opens&&!effects);
        assert(resume_sleep(NULL,&prior)==ALARM_STALE);
        alarm_sleep_v1 next=sleep_plan();assert(next.deadline==rtc_base+1800&&sleep_ticket_valid);
    }
    /* Every unsafe flag independently fences the otherwise retryable read. */
    for(unsigned flag=0;flag<4;flag++) {
        fresh();phase=LOAD_ALARM;inject_phase=LOAD_ALARM;inject_remaining=1;
        active=flag==0;persistence_pending=flag==1;haptic_uncertain=flag==2;audio_uncertain=flag==3;
        assert(client->step(NULL)==ALARM_STORAGE&&phase==BLOCKED&&error==ALARM_STORAGE);
        alarm_sleep_v1 plan={.struct_size=sizeof(plan)};
        assert(client->prepare_sleep(NULL,&plan)!=ALARM_OK&&!sleep_ticket_valid);
        assert(!put_calls&&!opens&&!effects);
    }
    /* Real pending-write verification BUSY is terminal, not a retryable read. */
    fresh();alarm_config due={.revision=1,.deadline=rtc_base+1,.created=rtc_base,.kind=1,.enabled=1};
    alarm_config_encode(&due,blobs[0]);sizes[0]=32;ms+=2000;client->refresh(NULL);
    for(unsigned n=0;n<80&&phase!=WRITE_OCC;n++)pump(1);
    assert(phase==WRITE_OCC&&persistence_pending&&!active);
    pump(1);assert(phase==VERIFY_OCC&&put_calls==1);
    inject_phase=VERIFY_OCC;inject_remaining=1;
    assert(client->step(NULL)==ALARM_STORAGE&&phase==BLOCKED&&persistence_pending&&!active&&!sleep_ticket_valid);
    assert(!effects&&!opens&&desired.deadline==due.deadline);
    puts("Pending alarm/timer preservation, stale sleep ticket, unsafe-state guards and write-verification BUSY PASS");
}
#endif

int main(void){
    const phase_t loads[]={LOAD_ALARM,LOAD_TIMER,LOAD_MODE,LOAD_VOLUME,LOAD_DND,LOAD_ALARM_OCC,LOAD_TIMER_OCC,LOAD_POINTS_CFG,LOAD_POINTS_OCC};
    for(unsigned i=0;i<sizeof(loads)/sizeof(*loads);i++){
        fresh();inject_phase=loads[i];inject_remaining=1;
        int result=sleep_now();
#ifdef TEST_BASELINE
        assert(result==WATCH_SLEEP_REFUSED&&phase==BLOCKED&&error==ALARM_STORAGE);
        assert(test_sleep_stage==4&&test_sleep_detail==-4&&!test_sleep_calls);
        printf("Reproduced Sleep err alarm -4: anchored read phase %u, one typed BUSY\n",i);
#else
        assert(result==WATCH_SLEEP_WOKE&&!error&&test_sleep_calls==1&&injected_reads==1);
        assert(!put_calls&&!opens&&!effects);
#endif
    }
#ifndef TEST_BASELINE
    for(unsigned i=0;i<sizeof(loads)/sizeof(*loads);i++){
        fresh();inject_phase=loads[i];inject_remaining=1000;
        assert(sleep_now()==WATCH_SLEEP_REFUSED&&!error&&phase!=BLOCKED&&!test_sleep_calls);
        assert(test_sleep_stage==4&&test_sleep_detail==ALARM_BUSY&&injected_reads<=64);
        inject_remaining=0;assert(sleep_now()==WATCH_SLEEP_WOKE&&test_sleep_calls==1&&!error);
        fresh();inject_phase=loads[i];inject_remaining=3;
        ms+=1000;for(unsigned n=0;n<80;n++)pump(1);
        assert(phase==IDLE&&!error&&injected_reads==3);
    }
    test_busy_preserves_pending_and_fences_unsafe();
    /* A deadline that becomes due while reads wait must defeat sleep. */
    fresh();alarm_config due={.revision=1,.deadline=rtc_base+2,.created=rtc_base,.kind=1,.enabled=1};
    alarm_config_encode(&due,blobs[0]);sizes[0]=32;
    inject_phase=LOAD_ALARM;inject_remaining=1;busy_advance_ms=3000;
    assert(sleep_now()==WATCH_SLEEP_REFUSED&&!error&&!test_sleep_calls);
    assert(test_sleep_detail==ALARM_BUSY&&desired.deadline==rtc_base+2);
    for(unsigned n=0;n<100&&!effects;n++)pump(1);
    assert(effects==1&&!error&&occurrences[0].deadline==due.deadline&&occurrences[0].state==ALARM_OCC_PENDING);
    /* BUSY must never authorize writing defaults over malformed user data. */
    fresh();sizes[0]=32;memset(blobs[0],0,32);inject_phase=LOAD_ALARM;inject_remaining=1;
    assert(sleep_now()==WATCH_SLEEP_REFUSED&&error==ALARM_STORAGE&&!test_sleep_calls&&!put_calls);
    const int32_t failures[]={RISC_BOUND_KEY_VALUE_IO,RISC_BOUND_KEY_VALUE_CONTEXT,RISC_BOUND_KEY_VALUE_INVALID,RISC_BOUND_KEY_VALUE_BUFFER_SMALL};
    for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++){
        fresh();inject_phase=LOAD_ALARM;inject_result=failures[i];inject_remaining=1;
        assert(sleep_now()==WATCH_SLEEP_REFUSED&&phase==BLOCKED&&error==ALARM_STORAGE);
        assert(test_sleep_stage==4&&test_sleep_detail==-4&&!test_sleep_calls);
    }
#endif
    assert(driver_api->quiesce());puts("Color sleep reconciliation BUSY fixture PASS");return 0;
}
