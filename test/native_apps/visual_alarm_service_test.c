/* Reuse dependency/storage fixtures; run only the real visual profile. */
#define ALARM_VISUAL_ONLY
#define ALARM_DND_CONTROL
#define main watch_points_fixture_main
#include "points_service_test.c"
#undef main
static void visual_boot(bool clear) {
    if(driver_api)assert(driver_api->quiesce());
    if(clear){memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;}
    put_fail=put_persists=write_fail=effect_fail=revoked=false;
    opens=writes=effects=stops=silences=closes=put_calls=0;
    driver_api=t5_driver_get(2);client=driver_api->capability;
    assert(!driver_api->start(deps,5)); /* No hidden/unused output dependencies. */
    assert(driver_api->start(deps,3));
    assert(alarm_service_output_modes(client)==ALARM_MODE_VISUAL);
    assert(!audio&&!haptic);
}
static void visual_step(void) {
    (void)client->step(NULL);ms++;
    alarm_status_v1 s=snapshot();assert(s.mode==ALARM_MODE_VISUAL&&!s.output_uncertain);
    assert(!opens&&!writes&&!effects&&!stops&&!silences&&!closes);
}
static void visual_until(unsigned state) {
    for(unsigned i=0;i<100;i++){if(snapshot().state==state)return;visual_step();}
    assert(!"visual state not reached");
}
static alarm_token_v1 visual_point(unsigned mode) {
    rtc_base=civil(2026,10,5,12,0);visual_boot(true);
    save(catalog(rtc_base,POINTS_BREAK,15,mode,true,true));
    visual_until(ALARM_STATE_ALERT);alarm_status_v1 s=snapshot();
    assert(s.label[0]&&s.occurrence.generation&&s.remaining_ms>19000);
    assert(points_occ.state==ALARM_OCC_PENDING&&points_occ.mode==mode);
    while(phase!=PLAYING)visual_step();
    return s.occurrence;
}
int main(void) {
    alarm_service_v1 legacy={.api_version=1,.struct_size=sizeof(legacy)};
    assert(alarm_service_output_modes(&legacy)==ALARM_MODE_BOTH);
    assert(sizeof(alarm_status_v1)==104);
    for(unsigned mode=1;mode<=3;mode++) {
        alarm_token_v1 t=visual_point(mode),bad=t;bad.generation++;
        assert(client->acknowledge(NULL,&bad)==ALARM_STALE);
        assert(client->acknowledge(NULL,&t)==ALARM_PENDING);visual_until(ALARM_STATE_READY);
        assert(points_occ.state==ALARM_OCC_ACKED&&points_occ.mode==mode);
        assert(client->acknowledge(NULL,&t)==ALARM_OK);
        unsigned generation=points_occ.generation;visual_boot(false);visual_until(ALARM_STATE_READY);
        assert(points_occ.generation==generation&&!put_calls);
    }
    /* Reset replay preserves the portable preference but replaces the token. */
    alarm_token_v1 old=visual_point(3);visual_boot(false);visual_until(ALARM_STATE_ALERT);
    alarm_token_v1 current=snapshot().occurrence;assert(current.generation==old.generation+1);
    assert(client->acknowledge(NULL,&old)==ALARM_STALE);
    /* Foreground failure is storage-free and retains the durable occurrence. */
    unsigned puts_before=put_calls;uint8_t before[64];memcpy(before,blobs[6],64);
    revoked=true;
    assert(client->stop_only(NULL)==ALARM_PENDING);
    assert(client->stop_only(NULL)==ALARM_PENDING);
    assert(client->stop_only(NULL)==ALARM_OK);
    assert(put_calls== (int)puts_before&&!memcmp(before,blobs[6],64));
    assert(snapshot().state==ALARM_STATE_BLOCKED&&snapshot().error==ALARM_FOREGROUND);
    assert(client->step(NULL)==ALARM_FOREGROUND);revoked=false;
    assert(client->acknowledge(NULL,&current)==ALARM_PENDING);visual_until(ALARM_STATE_READY);
    /* Failed durable ACK is still a visible error and blocks sleep. */
    current=visual_point(2);put_fail=true;
    assert(client->acknowledge(NULL,&current)==ALARM_PENDING);visual_until(ALARM_STATE_BLOCKED);
    assert(snapshot().error==ALARM_STORAGE);
    alarm_sleep_v1 plan={.struct_size=sizeof(plan)};
    assert(client->prepare_sleep(NULL,&plan)==ALARM_STORAGE);
    put_fail=false;assert(client->acknowledge(NULL,&current)==ALARM_PENDING);visual_until(ALARM_STATE_READY);
    /* DND marks mute durably but visual delivery stays available. */
    current=visual_point(1);blobs[8][0]=1;sizes[8]=1;
    for(unsigned i=0;i<12;i++)visual_step();
    assert(snapshot().state==ALARM_STATE_ALERT&&points_occ.silenced);
    assert(client->acknowledge(NULL,&current)==ALARM_PENDING);visual_until(ALARM_STATE_READY);
    /* Visual timeout settles the same ledger; it does not become a 350 ms cue. */
    visual_point(3);ms+=1000;visual_step();assert(snapshot().state==ALARM_STATE_ALERT);
    ms+=ALARM_INVOCATION_MS;visual_until(ALARM_STATE_READY);assert(points_occ.state==ALARM_OCC_ACKED);
    plan=(alarm_sleep_v1){.struct_size=sizeof(plan)};
    assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING);
    visual_until(ALARM_STATE_READY);assert(client->prepare_sleep(NULL,&plan)==ALARM_OK);
    assert(plan.deadline==civil(2026,10,5,12,12));
    /* Due activation writes must verify before the caller sees an alert. */
    rtc_base=civil(2026,10,5,12,0);visual_boot(true);save(catalog(rtc_base,POINTS_BREAK,15,3,true,true));
    put_fail=true;visual_until(ALARM_STATE_BLOCKED);assert(!snapshot().occurrence.generation);
    put_fail=false;client->refresh(NULL);visual_until(ALARM_STATE_ALERT);
    /* A committed cancellation before first physical/visual activation wins. */
    rtc_base=civil(2026,10,5,12,0);visual_boot(true);points_config c=catalog(rtc_base,POINTS_BREAK,15,3,true,true);save(c);
    while(phase!=START_AUDIO)visual_step();
    c.revision++;c.points[0].enabled=0;save(c);
    visual_step();visual_until(ALARM_STATE_READY);assert(!active);
    /* Ordinary alarm records share the same foreground/durability path. */
    visual_boot(true);save((points_config){.revision=1});
    alarm_config cfg={.revision=1,.deadline=rtc_base,.created=rtc_base-1,.kind=1,.enabled=1};
    alarm_config_encode(&cfg,blobs[0]);sizes[0]=32;blobs[2][0]=3;sizes[2]=1;
    visual_until(ALARM_STATE_ALERT);current=snapshot().occurrence;
    assert(current.kind==ALARM_KIND_ALARM&&!strcmp(snapshot().label,"ALARM"));
    assert(client->acknowledge(NULL,&current)==ALARM_PENDING);visual_until(ALARM_STATE_READY);
    assert(occurrences[0].state==ALARM_OCC_ACKED&&occurrences[0].mode==3);
    revoked=true;puts_before=put_calls;assert(driver_api->quiesce());assert(put_calls==(int)puts_before);
    assert(!opens&&!writes&&!effects&&!stops&&!silences&&!closes);
    puts("Visual-only real service: descriptor, delivery, persistence, replay, foreground failure, DND and sleep passed");
    return 0;
}
