/* Run the actual selected service with caller time spent after each scan.
 * Hardware speed is represented by elapsed time, never by sleeping the test. */
#define main original_service_fixture_main
#include "points_catalog_service_test.c"
#undef main
#ifdef ALARM_NATIVE_UTC
static int32_t context_read(void *c,risc_realtime_snapshot_v1 *out) {
    (void)c;(void)out;++io;allocation_terminal=true;return RISC_REALTIME_CONTEXT;
}
static const risc_platform_realtime_api_v1 failed_time={1,sizeof(failed_time),NULL,context_read};
#else
static bool failed_read(void *c,twatch_rtc_time_v1 *out) {(void)c;(void)out;++io;return false;}
static const twatch_rtc_api_v1 failed_time={2,sizeof(failed_time),NULL,failed_read,NULL,NULL,NULL};
#endif
int main(int argc,char **argv) {
    unsigned scene=argc>1?(unsigned)strtoul(argv[1],NULL,10):0;
    boot(true);points_catalog c=make_catalog(100);
    for(unsigned i=0;i<c.event_count;i++)c.events[i].hour=13;
    save_catalog(&c);points_catalog_dispose(&c);
    alarm_sleep_v1 plan={.struct_size=sizeof(plan)};unsigned scans=0;bool ready=false;
    for(unsigned i=0;i<2000;i++) {
        int32_t r=client->prepare_sleep(NULL,&plan);
        if(!r){ready=true;break;}assert(r==ALARM_PENDING);
        phase_t before=phase;tick(true);if(before==EVALUATE){ms+=250;++scans;}
    }
    assert(ready&&scans&&scans<10&&plan.rtc_seconds==base+ms/1000&&plan.deadline==noon()+3600);
    assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING);idle();
    if(scene==0) {
        ms+=1500;assert(!client->prepare_sleep(NULL,&plan));
        assert(plan.rtc_seconds==base+ms/1000&&plan.deadline==noon()+3600&&!active);
        assert(!resume_sleep(NULL,&plan));idle();
    } else if(scene==1) {
        ms=3600000;assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING&&!sleep_ticket_valid);
        for(unsigned i=0;i<1000&&!(active&&phase==PLAYING);i++)tick(false);
        assert(active&&phase==PLAYING&&selected==2);
        assert(client->acknowledge(NULL,&(alarm_token_v1){desired.kind,desired.revision,desired.deadline,desired.generation})==ALARM_PENDING);
        idle();
    } else if(scene==2||scene==3) {
        if(scene==2)base+=10;else --base;
        assert(client->prepare_sleep(NULL,&plan)==ALARM_RTC&&!sleep_ticket_valid);
    } else if(scene==4) {
        ms=0;assert(client->prepare_sleep(NULL,&plan)==ALARM_RTC&&!sleep_ticket_valid);
    } else {
#ifdef ALARM_NATIVE_UTC
        realtime=&failed_time;assert(client->prepare_sleep(NULL,&plan)==ALARM_RETAINED);
        unsigned calls=io;assert(client->step(NULL)==ALARM_RETAINED);
        assert(client->prepare_sleep(NULL,&plan)==ALARM_RETAINED&&!provider->quiesce()&&io==calls);
        return 0;
#else
        rtc=&failed_time;assert(client->prepare_sleep(NULL,&plan)==ALARM_RTC&&!sleep_ticket_valid);
#endif
    }
    assert(provider->quiesce());printf("Slow-scan sleep boundary %u PASS\n",scene);return 0;
}
