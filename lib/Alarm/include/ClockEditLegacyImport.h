#ifndef UTILITIES_CLOCK_EDIT_LEGACY_IMPORT_H
#define UTILITIES_CLOCK_EDIT_LEGACY_IMPORT_H
#include "ClockEditScheduleParticipants.h"
#define CLOCK_EDIT_LEGACY_BYTES 316u
/* Read-only compatibility view. No Points authority is synthesized and no
 * rewrite occurs. An interrupted intent remains an intent, never verified.
 * Original ACE2 validation is preserved independently of the ACM2 registry. */
static inline bool clock_participant_import_ace2(clock_participant_record*out,const uint8_t*b,size_t size){
 if(!out||!b||size!=CLOCK_EDIT_LEGACY_BYTES||memcmp(b,"ACE2",4)||alarm_read32(b+4)!=1||
 alarm_read32(b+8)>2||!alarm_read32(b+12)||alarm_read32(b+16)>ALARM_RTC_MAX||alarm_read32(b+20)>ALARM_RTC_MAX||
 alarm_read32(b+24)>2||alarm_read32(b+28)>ALARM_RTC_MAX||alarm_read32(b+32)>ALARM_RTC_MAX||alarm_read32(b+36)||
 alarm_read32(b+312)!=clock_participant_checksum(b,312))return false;
 clock_participant_record r={.phase=alarm_read32(b+8),.generation=alarm_read32(b+12),.requested=alarm_read32(b+16),
 .previous=alarm_read32(b+20),.prior_phase=alarm_read32(b+24),.prior_requested=alarm_read32(b+28),.prior_previous=alarm_read32(b+32)};
 if(r.phase!=CLOCK_PARTICIPANT_INTENT&&(!clock_schedule_zero(b+24,16)||!clock_schedule_zero(b+176,136)))return false;
 if(!r.phase&&(!clock_schedule_zero(b+16,8)||!clock_schedule_zero(b+40,136)))return false;
 for(unsigned group=0;group<2;group++)for(unsigned i=0;i<2;i++){
  const uint8_t*row=b+40+(group*2+i)*68;bool absent=clock_schedule_zero(row,68);
  if(!absent){
   alarm_config c;alarm_occurrence o;
   if(!alarm_config_decode(&c,row,32,(uint8_t)(i+1))||alarm_read32(row+64)>c.created||
      (!clock_schedule_zero(row+32,32)&&!alarm_occurrence_decode(&o,row+32,32,(uint8_t)(i+1))))return false;
   if(r.phase&&group==0&&alarm_read32(row+64)!=(c.created<r.requested?c.created:r.requested))return false;
  }
  if(r.phase&&(group==0||r.phase==CLOCK_PARTICIPANT_INTENT))r.rows[r.count++]=(clock_participant_row){group,i+1,1,alarm_read32(row+64),0,absent?CLOCK_CONFIG_ABSENT:CLOCK_CONFIG_SAVED,row,row+32};
 }
 *out=r;return true;
}
#endif
