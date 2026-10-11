/* Exact production core, deterministic boundary/property coverage. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../Apps/stopwatch_core.h"

static uint32_t rng_state=0x951947efu;
static uint32_t random32(void){rng_state^=rng_state<<13;rng_state^=rng_state>>17;rng_state^=rng_state<<5;return rng_state;}
static void record_tests(void){
 uint8_t b[SW_RECORD_SIZE]; sw_record out={1,2,true,true}, zero={0};
 sw_encode(&zero,b);assert(sw_decode(&out,b,sizeof(b)));assert(out.elapsed_ms==0&&!out.running&&!out.approximate);
 assert(!sw_decode(NULL,b,sizeof(b)));assert(!sw_decode(&out,NULL,sizeof(b)));
 for(uint32_t n=0;n<SW_RECORD_SIZE+5;n++)if(n!=SW_RECORD_SIZE)assert(!sw_decode(&out,b,n));
 for(unsigned i=0;i<SW_RECORD_SIZE;i++)for(unsigned bit=0;bit<8;bit++){
  b[i]^=(uint8_t)(1u<<bit);assert(!sw_decode(&out,b,sizeof(b)));b[i]^=(uint8_t)(1u<<bit);
 }
 for(unsigned i=0;i<100000;i++){
  sw_record in={random32()%(SW_MAX_MS+1u),random32()%(SW_RTC_MAX+1u),(random32()&1u)!=0,(random32()&1u)!=0};
  if(in.running&&in.elapsed_ms==SW_MAX_MS)in.elapsed_ms--;
  sw_encode(&in,b);assert(sw_decode(&out,b,sizeof(b)));
  assert(out.elapsed_ms==in.elapsed_ms&&out.running==in.running&&out.approximate==in.approximate);
  assert(out.anchor_seconds==(in.running?in.anchor_seconds:0));
 }
 sw_record invalid[]={ {SW_MAX_MS+1u,0,false,false},{0,SW_RTC_MAX+1u,true,false},{SW_MAX_MS,1,true,false} };
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){sw_encode(&invalid[i],b);assert(!sw_decode(&out,b,sizeof(b)));}
 sw_encode(&zero,b);b[4]=2;sw_write32(b+16,sw_checksum(b));assert(!sw_decode(&out,b,sizeof(b)));
 sw_encode(&zero,b);b[5]=2;sw_write32(b+16,sw_checksum(b));assert(!sw_decode(&out,b,sizeof(b)));
 sw_encode(&zero,b);b[6]=1;sw_write32(b+16,sw_checksum(b));assert(!sw_decode(&out,b,sizeof(b)));
 sw_encode(&zero,b);b[7]=1;sw_write32(b+16,sw_checksum(b));assert(!sw_decode(&out,b,sizeof(b)));
 sw_encode(&zero,b);b[3]='2';sw_write32(b+16,sw_checksum(b));assert(!sw_decode(&out,b,sizeof(b)));
 sw_encode(&zero,b);sw_write32(b+12,1);sw_write32(b+16,sw_checksum(b));assert(!sw_decode(&out,b,sizeof(b)));
}
static void calendar_tests(void){
 uint32_t s;assert(sw_calendar_seconds(2000,1,1,0,0,0,&s)&&s==0);
 assert(sw_calendar_seconds(2000,2,29,0,0,0,&s)&&s==59u*86400u);
 assert(sw_calendar_seconds(2000,3,1,0,0,0,&s)&&s==60u*86400u);
 assert(sw_calendar_seconds(2001,1,1,0,0,0,&s)&&s==366u*86400u);
 assert(sw_calendar_seconds(2099,12,31,23,59,59,&s)&&s==SW_RTC_MAX);
 assert(!sw_calendar_seconds(1999,12,31,23,59,59,&s));assert(!sw_calendar_seconds(2100,1,1,0,0,0,&s));
 assert(!sw_calendar_seconds(2001,2,29,0,0,0,&s));assert(!sw_calendar_seconds(2000,4,31,0,0,0,&s));
 assert(!sw_calendar_seconds(2000,0,1,0,0,0,&s));assert(!sw_calendar_seconds(2000,13,1,0,0,0,&s));
 assert(!sw_calendar_seconds(2000,1,0,0,0,0,&s));assert(!sw_calendar_seconds(2000,1,1,24,0,0,&s));
 assert(!sw_calendar_seconds(2000,1,1,0,60,0,&s));assert(!sw_calendar_seconds(2000,1,1,0,0,60,&s));
 assert(!sw_calendar_seconds(2000,1,1,0,0,0,NULL));
 /* Exhaust every valid calendar day and ensure the transform is consecutive. */
 uint32_t day_index=0;
 for(uint16_t year=2000;year<2100;year++)for(uint8_t month=1;month<=12;month++)for(uint8_t day=1;day<=31;day++)
  if(sw_calendar_seconds(year,month,day,0,0,0,&s)){assert(s==day_index*86400u);day_index++;}
 assert(day_index==36525u);
}
static void restore_tests(void){
 sw_record r={1234,100,true,false};sw_clock c;
 sw_restore(&c,&r,true,103,10);assert(c.running&&c.approximate&&c.elapsed_ms==4234);
 sw_tick(&c,65);assert(c.elapsed_ms==4289&&c.running);
 sw_restore(&c,&r,false,0,15);assert(!c.running&&c.clock_changed&&c.elapsed_ms==1234);
 sw_restore(&c,&r,true,99,15);assert(!c.running&&c.clock_changed&&c.elapsed_ms==1234);
 r.running=false;r.anchor_seconds=0;r.approximate=true;
 sw_restore(&c,&r,false,0,15);assert(!c.running&&!c.clock_changed&&c.approximate&&c.elapsed_ms==1234);
 r=(sw_record){SW_MAX_MS-500u,100,true,false};sw_restore(&c,&r,true,101,0);
 assert(!c.running&&c.limit&&c.elapsed_ms==SW_MAX_MS);
 r=(sw_record){0,0,true,false};sw_restore(&c,&r,true,SW_RTC_MAX,0);assert(!c.running&&c.limit&&c.elapsed_ms==SW_MAX_MS);
 r=(sw_record){SW_MAX_MS-1000u,100,true,false};sw_restore(&c,&r,true,101,0);
 assert(c.elapsed_ms==SW_MAX_MS&&!c.running&&c.limit);
}
static void restore_property_tests(void){
 for(unsigned i=0;i<100000;i++){
  uint32_t initial=random32()%SW_MAX_MS,anchor=random32()%SW_RTC_MAX,delta=random32()%(SW_RTC_MAX-anchor+1u);
  sw_record r={initial,anchor,true,false};sw_clock c;sw_restore(&c,&r,true,anchor+delta,random32());
  uint64_t expected=(uint64_t)initial+(uint64_t)delta*1000u;
  assert(c.elapsed_ms==(expected>=SW_MAX_MS?SW_MAX_MS:expected));
  assert(c.running==(expected<SW_MAX_MS)&&c.limit==(expected>=SW_MAX_MS)&&c.approximate&&!c.clock_changed);
  assert(c.saved.elapsed_ms==initial&&c.saved.anchor_seconds==anchor&&c.saved.running);
 }
}
static void tick_tests(void){
 sw_clock c={.running=true,.last_ms=UINT32_MAX-100,.elapsed_ms=555};sw_tick(&c,99);assert(c.elapsed_ms==755&&c.running);
 c.running=false;sw_tick(&c,12345);assert(c.elapsed_ms==755&&c.last_ms==12345);
 c=(sw_clock){.running=true,.elapsed_ms=SW_MAX_MS-50};sw_tick(&c,50);assert(c.elapsed_ms==SW_MAX_MS&&!c.running&&c.limit);
 c=(sw_clock){.running=true};sw_tick(&c,UINT32_MAX);assert(c.elapsed_ms==SW_MAX_MS&&!c.running&&c.limit);
 for(unsigned i=0;i<100000;i++){
  uint32_t initial=random32()%(SW_MAX_MS+1u), last=random32(),delta=random32();
  c=(sw_clock){.running=true,.elapsed_ms=initial,.last_ms=last};sw_tick(&c,last+delta);
  uint64_t expected=(uint64_t)initial+delta;
  assert(c.elapsed_ms==(expected>=SW_MAX_MS?SW_MAX_MS:expected));
  assert(c.running==(expected<SW_MAX_MS));assert(c.limit==(expected>=SW_MAX_MS));
 }
}
static void consistency_tests(void){
 sw_clock c={.rtc_anchor=100,.ms_anchor=UINT32_MAX-1000};
 assert(sw_clock_consistent(&c,true,103,1999));assert(!sw_clock_consistent(&c,false,103,1999));
 assert(!sw_clock_consistent(&c,true,99,1999));assert(!sw_clock_consistent(&c,true,110,1999));
 assert(!sw_clock_consistent(&c,true,100,2999));
}
static void format_tests(void){
 char b[12];
#ifdef PORTABLE_UNPADDED_HOURS
 sw_format(0,b);assert(!strcmp(b,"0:00:00.00"));
 sw_format(3723456,b);assert(!strcmp(b,"1:02:03.45"));
 sw_format(9u*3600000u+62340u,b);assert(!strcmp(b,"9:01:02.34"));
 sw_format(10u*3600000u+62340u,b);assert(!strcmp(b,"10:01:02.34"));
#else
 sw_format(0,b);assert(!strcmp(b,"00:00:00.00"));
 sw_format(3723456,b);assert(!strcmp(b,"01:02:03.45"));
#endif
 sw_format(SW_MAX_MS,b);assert(!strcmp(b,"99:59:59.99"));
 for(unsigned i=0;i<100000;i++){
  uint32_t ms=random32()%(SW_MAX_MS+1u);unsigned h,m,s,cs;sw_format(ms,b);
  #ifdef PORTABLE_UNPADDED_HOURS
  assert(strlen(b)==(ms<10u*3600000u?10u:11u));
#else
  assert(strlen(b)==11);
#endif
  assert(sscanf(b,"%u:%u:%u.%u",&h,&m,&s,&cs)==4);
  assert(h<100&&m<60&&s<60&&cs<100&&h*3600000u+m*60000u+s*1000u+cs*10u==ms-ms%10u);
 }
}
int main(int argc,char**argv){
 const char *name=argc>1?argv[1]:"all";
 #define RUN(n) if(!strcmp(name,"all")||!strcmp(name,#n)){n##_tests();puts("stopwatch core " #n ": passed");}
 RUN(record);RUN(calendar);RUN(restore);RUN(restore_property);RUN(tick);RUN(consistency);RUN(format);
 return 0;
}
