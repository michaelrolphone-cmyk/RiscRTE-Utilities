/* Reproduce a successful Settings RTC write against a live alarm service. */
#define ALARM_CLOCK_CHANGE_CONTROL
#define main original_service_fixture_main
#include "alarm_service_test.c"
#undef main
int main(void) {
    boot(true);until(ALARM_STATE_READY);
    uint32_t before=snapshot().rtc_seconds;
    rtc_base+=3600; /* Settings wrote and read back the new RTC hour. */
    ms+=1000;pump(20);
    assert(snapshot().state==ALARM_STATE_BLOCKED);
    assert(snapshot().error==ALARM_RTC);
    assert(client->refresh(NULL)==ALARM_PENDING);until(ALARM_STATE_READY);
    assert(snapshot().rtc_seconds>=before+3600);
    assert(!put_count&&!opens&&!effects);
    puts("BEFORE: successful RTC write produces ALARM_RTC; Retry alone reanchors and succeeds");
    const alarm_clock_change_v1 *control=alarm_clock_change_control(client);assert(control);
    for(unsigned direction=0;direction<2;direction++) {
        boot(true);until(ALARM_STATE_READY);rtc_base=direction?50:3700;
        assert(control->clock_changed(NULL,rtc_base)==ALARM_PENDING);
        until(ALARM_STATE_READY);assert(snapshot().rtc_seconds==rtc_base);
        assert(!put_count&&!opens&&!effects);
    }
    boot(true);until(ALARM_STATE_READY);rtc_base=500;
    assert(control->clock_changed(NULL,501)==ALARM_RTC);assert(previous_seconds==100);
    assert(control->clock_changed(NULL,498)==ALARM_RTC);assert(previous_seconds==100);
    assert(control->clock_changed(NULL,499)==ALARM_PENDING);until(ALARM_STATE_READY);
    alarm_sleep_v1 ticket={.struct_size=sizeof(ticket)};
    assert(client->prepare_sleep(NULL,&ticket)==ALARM_PENDING);pump(8);
    assert(client->prepare_sleep(NULL,&ticket)==ALARM_OK);
    rtc_base=600;assert(control->clock_changed(NULL,600)==ALARM_PENDING);
    assert(control->sleep.resume_sleep(NULL,&ticket)==ALARM_STALE);until(ALARM_STATE_READY);
    for(unsigned failure=0;failure<2;failure++) {
        ticket=(alarm_sleep_v1){.struct_size=sizeof(ticket)};
        assert(client->prepare_sleep(NULL,&ticket)==ALARM_PENDING);pump(8);
        assert(client->prepare_sleep(NULL,&ticket)==ALARM_OK);
        rtc_fail=failure!=0;
        assert(control->clock_changed(NULL,999)==ALARM_RTC);rtc_fail=false;
        assert(control->sleep.resume_sleep(NULL,&ticket)==ALARM_STALE);
    }
    rtc_base+=10;ms+=1000;pump(20);assert(snapshot().error==ALARM_RTC);
    assert(client->refresh(NULL)==ALARM_PENDING);until(ALARM_STATE_READY);
    boot(true);schedule(1,1,200);until(ALARM_STATE_READY);rtc_base=200;
    assert(control->clock_changed(NULL,200)==ALARM_PENDING);until(ALARM_STATE_ALERT);
    alarm_token_v1 occurrence=snapshot().occurrence;assert(occurrence.generation);
    int stored_puts=put_count;assert(control->clock_changed(NULL,200)==ALARM_BUSY);
    assert(alarm_token_equal(&occurrence,&view.occurrence)&&stored_puts==put_count);
    assert(client->acknowledge(NULL,&occurrence)==ALARM_PENDING);until(ALARM_STATE_READY);
    assert(snapshot().schedules[0].state==ALARM_SCHEDULE_DISMISSED);
    boot(true);get_fail=true;until(ALARM_STATE_BLOCKED);
    int old_reads=reads;assert(control->clock_changed(NULL,100)==ALARM_BUSY&&reads==old_reads);
    alarm_clock_change_v1 malformed=*control;malformed.tag^=1;
    assert(!alarm_clock_change_control(&malformed.sleep.base));
    malformed=*control;malformed.sleep.base.struct_size=sizeof(alarm_service_sleep_v1);
    assert(!alarm_clock_change_control(&malformed.sleep.base));
    malformed=*control;malformed.sleep.base.api_version=2;
    assert(!alarm_clock_change_control(&malformed.sleep.base));
    assert(driver_api->quiesce());
    puts("AFTER: explicit forward/backward clock changes, tick, due alarm, ACK, old-ticket invalidation, busy/storage rejection, descriptor guards and unexplained jump detection PASS");
    return 0;
}
