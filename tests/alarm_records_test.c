#include <assert.h>
#include <stdio.h>
#include "AlarmRecords.h"
int main(void){
 for(unsigned kind=1;kind<=2;kind++)for(unsigned enabled=0;enabled<2;enabled++) {
  alarm_config c={UINT32_MAX,enabled?200u:0u,100,enabled&&kind==2?100u:0u,(uint8_t)kind,(uint8_t)enabled},out;
  assert(alarm_config_valid(&c));uint8_t b[32];alarm_config_encode(&c,b);assert(alarm_config_decode(&out,b,32,kind));assert(out.revision==UINT32_MAX&&out.deadline==c.deadline);
  for(unsigned byte=0;byte<32;byte++)for(unsigned bit=0;bit<8;bit++){b[byte]^=(uint8_t)(1u<<bit);assert(!alarm_config_decode(&out,b,32,kind));b[byte]^=(uint8_t)(1u<<bit);}
  for(unsigned n=0;n<64;n++)if(n!=32)assert(!alarm_config_decode(&out,b,n,kind));
 }
 for(unsigned state=1;state<=3;state++)for(unsigned mode=1;mode<=3;mode++){
  alarm_occurrence o={1,ALARM_RTC_MAX-60,UINT32_MAX,ALARM_RTC_MAX,1,(uint8_t)state,(uint8_t)mode,0},out;
  assert(alarm_occurrence_valid(&o));uint8_t b[32];alarm_occurrence_encode(&o,b);assert(alarm_occurrence_decode(&out,b,32,1));
  for(unsigned byte=0;byte<32;byte++)for(unsigned bit=0;bit<8;bit++){b[byte]^=(uint8_t)(1u<<bit);assert(!alarm_occurrence_decode(&out,b,32,1));b[byte]^=(uint8_t)(1u<<bit);}
 }
 uint32_t seconds;assert(alarm_calendar_seconds(2000,1,1,0,0,0,&seconds)&&!seconds);
 assert(alarm_calendar_seconds(2099,12,31,23,59,59,&seconds)&&seconds==ALARM_RTC_MAX);
 assert(!alarm_calendar_seconds(2100,1,1,0,0,0,&seconds));assert(!alarm_calendar_seconds(2026,2,29,0,0,0,&seconds));
 assert(alarm_calendar_seconds(2024,2,29,0,0,0,&seconds));
 puts("Alarm record bounds, integrity, dates and reserved-byte fixtures passed");return 0;
}
