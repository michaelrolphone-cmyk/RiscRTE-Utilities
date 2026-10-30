#include "ClockEditScheduleParticipants.h"
#include <assert.h>
#include <stdio.h>
static points_ledger fixture(void){points_ledger l={.revision=3,.generation=8,.deadline=1000,.recovery_until=1060,.slot=2,.edge=0,.state=ALARM_OCC_PENDING,.mode=1,.timezone_index=4};l.day[2]=2;l.delivered[2]=1;return l;}
int main(void){
 points_ledger old=fixture(),next=old;assert(clock_points_history_successor(&old,&next));
 next.generation++;assert(clock_points_history_successor(&old,&next));
 for(unsigned state=ALARM_OCC_PENDING;state<=ALARM_OCC_EXPIRED;state++)for(unsigned extra=0;extra<2;extra++){
  next=old;next.state=state;next.generation+=extra;assert(clock_points_history_successor(&old,&next));
  next.silenced=1;assert(clock_points_history_successor(&old,&next));
 }
 for(unsigned state=ALARM_OCC_ACKED;state<=ALARM_OCC_EXPIRED;state++)for(unsigned target=0;target<=ALARM_OCC_EXPIRED;target++){
  old=fixture();old.state=state;next=old;next.state=target;next.generation++;
  assert(clock_points_history_successor(&old,&next)==(state==target));
 }
 old=fixture();next=old;next.generation--;assert(!clock_points_history_successor(&old,&next));
 next=old;next.revision--;assert(!clock_points_history_successor(&old,&next));
 next=old;next.generation++;next.day[2]--;assert(!clock_points_history_successor(&old,&next));
 next=old;next.generation++;next.delivered[2]=0;assert(!clock_points_history_successor(&old,&next));
 old.silenced=1;next=old;next.generation++;next.silenced=0;assert(!clock_points_history_successor(&old,&next));
 old=fixture();next=old;next.day[3]=3;next.delivered[3]=1;assert(!clock_points_history_successor(&old,&next));
 next=old;next.generation++;next.slot=3;next.day[3]=3;next.delivered[3]=1;next.deadline=1100;next.recovery_until=1160;assert(clock_points_history_successor(&old,&next));
 /* A handled edge cannot be resurrected by selecting it with a new generation. */
 old.day[3]=3;old.delivered[3]=1;assert(!clock_points_history_successor(&old,&next));
 uint8_t c[64]={0},h[64]={0};assert(clock_points_participant(c,h,0,0,CLOCK_CONFIG_ABSENT,70,false));
 assert(!clock_points_participant(c,h,1,0,CLOCK_CONFIG_ABSENT,70,false));
 points_config config=points_default_config();points_config_encode(&config,c);
 assert(clock_points_participant(c,h,0,4,CLOCK_CONFIG_VIRTUAL,70,false));assert(clock_points_participant(c,h,0,4,CLOCK_CONFIG_SAVED,70,false));
 config.created=100;config.revision=3;points_config_encode(&config,c);
 assert(!clock_points_participant(c,h,0,4,CLOCK_CONFIG_VIRTUAL,70,false));assert(clock_points_participant(c,h,70,4,CLOCK_CONFIG_SAVED,70,false));
 assert(!clock_points_participant(c,h,101,4,CLOCK_CONFIG_SAVED,70,false));assert(!clock_points_participant(c,h,70,419,CLOCK_CONFIG_SAVED,70,false));
 assert(clock_points_participant(c,h,70,4,CLOCK_CONFIG_SAVED,70,true));assert(!clock_points_participant(c,h,69,4,CLOCK_CONFIG_SAVED,70,true));
 old=fixture();points_ledger_encode(&old,h);assert(clock_points_participant(c,h,70,4,CLOCK_CONFIG_SAVED,70,false));
 old.revision=4;points_ledger_encode(&old,h);assert(!clock_points_participant(c,h,70,4,CLOCK_CONFIG_SAVED,70,false));
 puts("Concrete schedule identity and monotonic history passed");return 0;
}
