/* Exact shared color 0.4.12 profile from build_alarm_sleep_retry.py.
 * Each case starts the production provider once. Recovery below uses step()
 * only: no refresh/Retry, restart, or pre-initialized alarm API is substituted.
 */
#define PORTABLE_RTC_UTC8_DENVER
#define ALARM_VOLUME_CONTROL
#define ALARM_DND_CONTROL
#define ALARM_CLOCK_CHANGE_CONTROL
#define ALARM_RECONCILE_BUSY_RETRY
#define main prior_points_fixture_main
#include "points_service_test.c"
#undef main

static const phase_t cold_reads[]={LOAD_ALARM,LOAD_TIMER,LOAD_MODE,LOAD_VOLUME,
    LOAD_DND,LOAD_ALARM_OCC,LOAD_TIMER_OCC,LOAD_POINTS_CFG,LOAD_POINTS_OCC};
static phase_t refused_phase;
static int32_t refused_result;
static unsigned refused_reads, read_calls;

static int32_t cold_get(void *c,const char *k,void *b,uint32_t cap,uint32_t *n) {
    ++read_calls;
    if(phase==refused_phase && refused_result) {
        ++refused_reads;*n=0;return refused_result;
    }
    return get_blob(c,k,b,cap,n);
}
static const risc_bound_key_value_v1 cold_store={1,sizeof(cold_store),NULL,cold_get,put_blob};

static void cold_boot(bool saved) {
    boot(true);kv=&cold_store;rtc_base=civil(2026,10,14,2,13);
    refused_result=0;refused_reads=read_calls=0;
    if(saved) {
        /* Preserve an ordinary future alarm and timer, user preferences and
         * an explicitly empty Points selection through first acquisition. */
        for(unsigned i=0;i<2;i++) {
            uint32_t delay=3600u*(i+1u);
            alarm_config future={.revision=7+i,.deadline=rtc_base+delay,
                .created=rtc_base,.duration=i?delay:0,.kind=(uint8_t)(i+1),.enabled=1};
            assert(alarm_config_valid(&future));
            alarm_config_encode(&future,blobs[i]);sizes[i]=ALARM_RECORD_SIZE;
        }
        blobs[2][0]=ALARM_MODE_BOTH;sizes[2]=1;
        blobs[7][0]=80;sizes[7]=1;
        blobs[8][0]=1;sizes[8]=1;
        points_config empty={.revision=9};save(empty);
    }
    assert(phase==LOAD_ALARM&&!anchored&&!error&&!sleep_ticket_valid);
    assert(snapshot().state==ALARM_STATE_LOADING);
}

static void reach_cold_read(phase_t wanted) {
    for(unsigned n=0;n<40&&phase!=wanted;n++) {
        assert(client->step(NULL)==ALARM_OK);++ms;
    }
    assert(phase==wanted&&!anchored&&!error);
    assert(!put_calls&&!opens&&!writes&&!effects&&!sleep_ticket_valid);
}

static void ready_without_retry(bool saved) {
    for(unsigned n=0;n<200&&phase!=IDLE;n++) {
        assert(client->step(NULL)==ALARM_OK);++ms;
    }
    assert(phase==IDLE&&anchored&&!error&&!active);
    alarm_status_v1 value=snapshot();assert(value.state==ALARM_STATE_READY&&!value.error);
    assert(!opens&&!writes&&!effects);
    if(saved) {
        assert(!put_calls&&points_configured.revision==9&&staged_volume==80&&staged_dnd);
        for(unsigned i=0;i<2;i++) {
            assert(value.schedules[i].state==ALARM_SCHEDULE_ARMED);
            assert(value.schedules[i].revision==7+i);
            assert(value.schedules[i].deadline==rtc_base+3600u*(i+1u));
        }
    } else {
        /* Virtual factory defaults may create a handled-occurrence ledger;
         * they must never fabricate user configuration or emit a stale cue. */
        assert(!sizes[0]&&!sizes[1]&&!sizes[2]&&!sizes[5]&&!sizes[7]&&!sizes[8]);
        assert(staged_volume==ALARM_VOLUME_DEFAULT&&!staged_dnd);
    }
}

static void test_cold_busy(bool saved,phase_t wanted,unsigned waits) {
    cold_boot(saved);reach_cold_read(wanted);
    uint8_t durable[sizeof(blobs)];uint32_t lengths[9];
    memcpy(durable,blobs,sizeof(blobs));memcpy(lengths,sizes,sizeof(sizes));
    uint32_t initial_snapshot=view.snapshot;
    refused_phase=wanted;refused_result=RISC_BOUND_KEY_VALUE_BUSY;
    unsigned calls=read_calls;
    for(unsigned n=0;n<waits;n++) {
        assert(client->step(NULL)==ALARM_PENDING);++ms;
        assert(phase==wanted&&!anchored&&!error&&!active&&!persistence_pending);
        assert(!sleep_ticket_valid&&!put_calls&&!opens&&!writes&&!effects);
        alarm_status_v1 value=snapshot();
        assert(value.state==ALARM_STATE_LOADING&&!value.error&&value.snapshot==initial_snapshot);
        assert(!memcmp(durable,blobs,sizeof(blobs))&&!memcmp(lengths,sizes,sizeof(sizes)));
    }
    assert(refused_reads==waits&&read_calls==calls+waits);
    refused_result=0;ready_without_retry(saved);
    if(saved)assert(!memcmp(durable,blobs,sizeof(blobs))&&!memcmp(lengths,sizes,sizeof(sizes)));
}

static void test_cold_hard_failures(void) {
    for(unsigned i=0;i<sizeof(cold_reads)/sizeof(*cold_reads);i++) {
        cold_boot(false);reach_cold_read(cold_reads[i]);
        refused_phase=cold_reads[i];refused_result=RISC_BOUND_KEY_VALUE_BUSY;
        assert(client->step(NULL)==ALARM_PENDING);
        refused_result=RISC_BOUND_KEY_VALUE_IO;
        assert(client->step(NULL)==ALARM_STORAGE&&phase==BLOCKED&&!anchored);
        refused_result=0;
        for(unsigned n=0;n<128;n++)assert(client->step(NULL)==ALARM_STORAGE);
        assert(snapshot().state==ALARM_STATE_BLOCKED&&!put_calls&&!opens&&!writes&&!effects);
        assert(!sleep_ticket_valid);
    }
    cold_boot(false);sizes[0]=ALARM_RECORD_SIZE;memset(blobs[0],0,ALARM_RECORD_SIZE);
    refused_phase=LOAD_ALARM;refused_result=RISC_BOUND_KEY_VALUE_BUSY;
    assert(client->step(NULL)==ALARM_PENDING);refused_result=0;
    assert(client->step(NULL)==ALARM_STORAGE&&phase==BLOCKED&&!put_calls);
    assert(sizes[0]==ALARM_RECORD_SIZE&&!opens&&!writes&&!effects);
}

int main(void) {
    for(unsigned saved=0;saved<2;saved++) {
        cold_boot(saved!=0);ready_without_retry(saved!=0);
        for(unsigned i=0;i<sizeof(cold_reads)/sizeof(*cold_reads);i++) {
            test_cold_busy(saved!=0,cold_reads[i],1);
            test_cold_busy(saved!=0,cold_reads[i],129);
        }
    }
    test_cold_hard_failures();assert(driver_api->quiesce());
    puts("Exact color 0.4.12 cold start: empty/saved healthy boots; 36 nine-phase transient/persistent BUSY cases recover by step only, without Retry/restart; durable settings/schedules preserved; nine IO fences and corruption PASS");
    return 0;
}
