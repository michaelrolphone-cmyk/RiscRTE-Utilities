/* Reuse the complete accepted native UTC lifecycle suite, then exercise the
 * exact read-phase retry boundary. No scheduler model replaces production. */
#define main original_native_utc_main
#include "native_utc_alarm_service_test.c"
#undef main
static void retry_case(unsigned scenario){
 epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);clean_schedule();
 if(scenario==11){
  alarm_sleep_v1 ticket=plan();assert(sleep_ticket_valid);
  phase=READ_RTC;read_result=RISC_REALTIME_IO;
  assert(client->step(NULL)==ALARM_PENDING&&!sleep_ticket_valid);
  unsigned before=reads;assert(alarm_service_resume(client,&ticket)==ALARM_STALE&&reads==before);
  alarm_sleep_v1 next={.struct_size=sizeof(next)};
  assert(client->prepare_sleep(NULL,&next)==ALARM_PENDING&&!sleep_ticket_valid);
  for(unsigned i=0;i<80&&phase!=READ_RTC;i++){tick();}
  assert(phase==READ_RTC);
  assert(client->step(NULL)==ALARM_PENDING&&!sleep_ticket_valid);
  assert(client->prepare_sleep(NULL,&next)==ALARM_PENDING&&!sleep_ticket_valid);
  read_result=0;until(ALARM_STATE_READY);assert(provider->quiesce());return;
 }
 if(scenario==0||scenario==1){
  until(ALARM_STATE_READY);assert(anchored);const uint32_t prior=previous_seconds;
  const uint64_t prior_ms=previous_ms;phase=READ_RTC;read_result=RISC_REALTIME_IO;
  unsigned before_reads=reads,before_get=get_count,before_put=put_count;
  for(unsigned i=0;i<3;i++){
   assert(client->step(NULL)==ALARM_PENDING&&phase==READ_RTC);
   assert(reads==before_reads+i+1&&get_count==before_get&&put_count==before_put);
   assert(anchored&&previous_seconds==prior&&previous_ms==prior_ms);
  }
  read_result=0;
  if(scenario==1)epoch-=100;
  if(scenario==0){assert(client->step(NULL)==ALARM_OK);until(ALARM_STATE_READY);}
  else {
   assert(client->step(NULL)==ALARM_RTC&&phase==BLOCKED&&anchored&&previous_seconds==prior);
   before_reads=reads;assert(client->step(NULL)==ALARM_RTC&&reads==before_reads);
   assert(client->refresh(NULL)==ALARM_PENDING);until(ALARM_STATE_READY);
  }
  assert(provider->quiesce());return;
 }
 phase=READ_RTC;read_result=RISC_REALTIME_IO;
 if(scenario==2)active=true;
 if(scenario==3)persistence_pending=true;
 if(scenario==4)haptic_uncertain=true;
 if(scenario==5)audio_uncertain=true;
 if(scenario==6){phase=ACTIVATE_RTC;active=true;alert_started=ms;alert_limit_ms=60000;}
 if(scenario==7)read_result=RISC_REALTIME_INVALID;
 if(scenario==8)read_result=RISC_REALTIME_CONTEXT;
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
 if(scenario==9)edit_recovery=true;
 if(scenario==10)edit_permit_valid=true;
#endif
 const unsigned before=reads,puts=put_count;
 int32_t rc=client->step(NULL);
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
 if(scenario==10){assert(rc==ALARM_BUSY&&reads==before&&put_count==puts);return;}
#endif
 if(!(reads==before+1&&put_count==puts&&phase==BLOCKED))fprintf(stderr,"scenario=%u rc=%d reads=%u before=%u puts=%u previous=%u phase=%d\n",scenario,rc,reads,before,put_count,puts,phase);
 assert(reads==before+1&&put_count==puts&&phase==BLOCKED);
 assert(rc==(scenario==8?ALARM_RETAINED:ALARM_RTC));
 if(scenario==8){unsigned calls=reads+get_count+put_count+clocks;forbid=true;
  assert(client->step(NULL)==ALARM_RETAINED&&!provider->quiesce());
  assert(calls==reads+get_count+put_count+clocks);
 }
}
int main(void){
 assert(original_native_utc_main()==0);
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
 const unsigned cases[]={0,1,2,3,4,5,6,7,8,9,10,11};
#else
 const unsigned cases[]={0,1,2,3,4,5,6,7,8,11};
#endif
 const unsigned scenarios=sizeof(cases)/sizeof(cases[0]);
 for(unsigned i=0;i<scenarios;i++){
  pid_t child=fork();assert(child>=0);if(child==0){retry_case(cases[i]);_exit(0);}
  int state;assert(waitpid(child,&state,0)==child&&WIFEXITED(state)&&WEXITSTATUS(state)==0);
 }
 printf("%u production native read-phase retry/unsafe-custody controls passed; original lifecycle suite retained\n",scenarios);
 return 0;
}
