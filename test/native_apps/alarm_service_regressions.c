/* Independent-review reproductions promoted into the production-source suite. */
#define main baseline_fixture_main
#include "alarm_service_test.c"
#undef main
static bool probe;static int probe_result;
static uint64_t probe_mono(void*c){(void)c;if(probe){probe=false;probe_result=client->step(NULL);}return ms;}
static const risc_platform_clock_api_v1 probe_clock={1,sizeof(probe_clock),NULL,probe_mono,NULL};
int main(void) {
 alarm_token_v1 t=ring(1);assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);boot(false);until(ALARM_STATE_READY);assert(client->acknowledge(NULL,&t)==ALARM_OK);
 alarm_occurrence old=occurrences[0];old.state=ALARM_OCC_PENDING;alarm_occurrence_encode(&old,blobs[3]);client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE);
 phase_t boundaries[]={WRITE_OCC,VERIFY_OCC,ACTIVATE_RTC,START_AUDIO};
 for(unsigned mode_value=1;mode_value<=3;mode_value++)for(unsigned hint=0;hint<=1;hint++)for(unsigned boundary=0;boundary<4;boundary++){
  boot(true);schedule(1,1,101);mode(mode_value);rtc_base=101;while(phase!=boundaries[boundary])pump(1);
  alarm_config cancelled={.revision=2,.created=100,.kind=1};alarm_config_encode(&cancelled,blobs[0]);if(hint)client->refresh(NULL);
  until(ALARM_STATE_READY);assert(!opens&&!effects&&snapshot().schedules[0].state==ALARM_SCHEDULE_OFF);
 }
 boot(true);schedule(1,1,101);rtc_base=101;while(phase!=ACTIVATE_RTC)pump(1);rtc_fail=true;pump(1);assert(phase==BLOCKED);rtc_fail=false;client->refresh(NULL);until(ALARM_STATE_ALERT);pump(4);assert(effects);
 boot(true);until(ALARM_STATE_READY);schedule(1,1,200);client->refresh(NULL);pump(1);schedule(1,2,201);
 alarm_sleep_v1 plan={.struct_size=sizeof(plan)};assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING);pump(8);assert(client->prepare_sleep(NULL,&plan)==ALARM_OK&&plan.deadline==201);
 boot(true);until(ALARM_STATE_READY);clock_api=&probe_clock;plan=(alarm_sleep_v1){.struct_size=sizeof(plan)};client->prepare_sleep(NULL,&plan);pump(8);probe=true;assert(client->prepare_sleep(NULL,&plan)==ALARM_OK);assert(probe_result==ALARM_BUSY);
 boot(true);schedule(1,1,101);rtc_base=101;while(phase!=ACTIVATE_RTC)pump(1);t=snapshot().occurrence;client->acknowledge(NULL,&t);client->refresh(NULL);until(ALARM_STATE_READY);assert(!effects&&!opens&&snapshot().schedules[0].state==ALARM_SCHEDULE_DISMISSED);
 /* Stop-only failure cleanup performs no normal storage/RTC/start/write and
    keeps the durable pending token intact until healthy explicit dismissal. */
 t=ring(3);int before=gets+put_count+reads+opens+writes+effects;
 assert(client->stop_only(NULL)==ALARM_PENDING);assert(client->stop_only(NULL)==ALARM_PENDING);assert(client->stop_only(NULL)==ALARM_OK);
 assert(before==gets+put_count+reads+opens+writes+effects);alarm_status_v1 stopped=snapshot();assert(alarm_token_equal(&t,&stopped.occurrence));
 assert(snapshot().state==ALARM_STATE_BLOCKED&&snapshot().error==ALARM_FOREGROUND&&!snapshot().output_uncertain);
 assert(client->step(NULL)==ALARM_FOREGROUND&&before==gets+put_count+reads+opens+writes+effects);
 assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(snapshot().schedules[0].state==ALARM_SCHEDULE_DISMISSED);
 t=ring(3);stop_fail=true;before=gets+put_count+reads+opens+writes+effects;
 assert(client->stop_only(NULL)==1);assert(client->stop_only(NULL)==1);assert(client->stop_only(NULL)==ALARM_OUTPUT);assert(closes&&stops);
 int cleanup_count=stops+silences+closes;assert(client->stop_only(NULL)==ALARM_OUTPUT&&cleanup_count==stops+silences+closes);assert(before==gets+put_count+reads+opens+writes+effects);
 stop_fail=false;assert(client->acknowledge(NULL,&t)==1);until(ALARM_STATE_READY);
 /* A pending record without its configuration is contradictory on startup. */
 ring(1);sizes[0]=0;boot(false);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&!effects&&!opens);
 for(unsigned failure=0;failure<3;failure++) {
  t=ring(3);assert(client->acknowledge(NULL,&t)==ALARM_PENDING);
  before=gets+put_count+reads+opens+writes+effects;
  if(failure==1)stop_fail=true;
  if(failure==2)close_fail=true;
  assert(client->stop_only(NULL)==1);assert(client->stop_only(NULL)==1);
  assert(client->stop_only(NULL)==(failure?ALARM_OUTPUT:ALARM_OK));assert(before==gets+put_count+reads+opens+writes+effects);
  assert(dismiss);stop_fail=close_fail=false;assert(client->refresh(NULL)==1);until(ALARM_STATE_READY);
  assert(snapshot().schedules[0].state==ALARM_SCHEDULE_DISMISSED&&client->acknowledge(NULL,&t)==0);
 }
 boot(true);schedule(1,1,101);rtc_base=160;until(ALARM_STATE_ALERT);pump(4);assert(alert_limit_ms<1100&&effects);ms+=1000;until(ALARM_STATE_READY);assert(snapshot().schedules[0].state==ALARM_SCHEDULE_EXPIRED);
 puts("Reviewed cancellation, generation, RTC retry, sleep, reentrancy and stop-only regressions passed");
}
