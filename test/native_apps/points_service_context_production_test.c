#define POINTS_SERVICE_CONTEXT
#define main historical_main
#include "native_utc_alarm_service_test.c"
#undef main
static void context_result(points_config *config,const char zone_id[40],unsigned zone_index,int expected) {
    points_service_context out;memset(&out,0x5a,sizeof(out));out.struct_size=sizeof(out);
    points_service_context before=out;unsigned calls=get_count+put_count+reads+clocks;
    assert(points_service_read_context(client,config,zone_id,zone_index,&out)==expected);
    assert(calls==get_count+put_count+reads+clocks);
    if(expected)assert(!memcmp(&before,&out,sizeof(out)));
    else {assert(out.sampled_utc==seconds&&out.snapshot==view.snapshot);assert(out.effective_created==config->created);}
}
int main(void) {
    assert(historical_main()==0);
    epoch=INT64_C(946684800)+date(2026,10,5,10,0,0);boot(true);
    points_config c=schedule(12,0,3);save(c);const char utc[40]="UTC",denver[40]="America/Denver";
    context_result(&c,utc,0,ALARM_PENDING);until(ALARM_STATE_READY);
    context_result(&c,utc,0,ALARM_OK);
    points_config stale=c;stale.points[0].minute++;context_result(&stale,utc,0,ALARM_STALE);
    context_result(&c,denver,0,ALARM_STALE);
    context_result(&c,utc,1,ALARM_STALE);
    in_call=true;context_result(&c,utc,0,ALARM_BUSY);in_call=false;
    client->refresh(NULL);context_result(&c,utc,0,ALARM_PENDING);until(ALARM_STATE_READY);
    zone(denver);client->refresh(NULL);until(ALARM_STATE_READY);
    context_result(&c,utc,0,ALARM_STALE);
    context_result(&c,denver,(unsigned)portable_timezone_find(denver,sizeof(denver)),ALARM_OK);
    read_result=RISC_REALTIME_CONTEXT;client->refresh(NULL);until(ALARM_STATE_BLOCKED);
    forbid=true;context_result(&c,denver,timezone_index,ALARM_RETAINED);
    puts("Actual Points service context: no I/O, exact identities, refresh and retained custody passed");
    return 0;
}
