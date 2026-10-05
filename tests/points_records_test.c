#include <assert.h>
#include <stdio.h>
#include "PointsSchedule.h"
static points_config example(void){points_config c={.revision=1,.created=800000000};for(unsigned i=0;i<8;i++)c.points[i]=(points_item){.kind=POINTS_BREAK};return c;}
static uint32_t raw(unsigned y,unsigned m,unsigned d,unsigned h,unsigned minute){twatch_rtc_time_v1 t={(uint16_t)y,(uint8_t)m,(uint8_t)d,0,(uint8_t)h,(uint8_t)minute,0};portable_time_candidate v[2];assert(portable_time_inverse(&t,v)==1);uint32_t s;assert(alarm_calendar_seconds(v[0].rtc.year,v[0].rtc.month,v[0].rtc.day,v[0].rtc.hour,v[0].rtc.minute,0,&s));return s;}
static void assert_item(const points_item *actual,const points_item *expected){
 assert(actual->kind==expected->kind&&actual->enabled==expected->enabled&&actual->mode==expected->mode);
 assert(actual->weekdays==expected->weekdays&&actual->hour==expected->hour&&actual->minute==expected->minute);
 assert(actual->duration_minutes==expected->duration_minutes&&actual->notify_end==expected->notify_end&&actual->warn3==expected->warn3);
}
static void test_default_schedule(void){
 const points_item expected[POINTS_MAX]={
  {POINTS_CUSTOM_2,1,3,30,4,30,0,0,0},
  {POINTS_CUSTOM_1,1,3,30,5,30,15,0,0},
  {POINTS_WORK_START,1,3,30,6,0,0,0,0},
  {POINTS_BREAK,1,3,30,9,0,15,0,1},
  {POINTS_LUNCH,1,3,30,12,0,30,0,1},
  {POINTS_BREAK,1,3,30,14,15,15,0,1},
  {POINTS_WORK_END,1,3,30,16,30,0,0,0},
  {0}
 };
 points_config c=points_default_config(),decoded;uint8_t bytes[POINTS_RECORD_SIZE];
 assert(c.revision==1&&c.created==0&&points_config_valid(&c));
 points_config_encode(&c,bytes);assert(points_config_decode(&decoded,bytes,sizeof(bytes)));
 assert(decoded.revision==1&&decoded.created==0);
 for(unsigned slot=0;slot<POINTS_MAX;slot++){
  assert_item(&c.points[slot],&expected[slot]);assert_item(&decoded.points[slot],&expected[slot]);
  assert(points_notice_enabled(&c.points[slot],POINTS_EDGE_START)==(slot<7));
  assert(!points_notice_enabled(&c.points[slot],POINTS_EDGE_END));
  assert(points_notice_enabled(&c.points[slot],POINTS_EDGE_WARNING)==(slot>=3&&slot<=5));
 }
 /* October 4 is Sunday: check every slot and every edge on all seven days. */
 for(unsigned date=4;date<=10;date++){
  uint32_t day;assert(points_local_day(raw(2026,10,date,12,0),&day));
  bool working=date>=5&&date<=8;
  for(unsigned slot=0;slot<POINTS_MAX;slot++)for(unsigned edge=0;edge<POINTS_EDGE_COUNT;edge++){
   const points_item *p=&expected[slot];points_event event={0};
   bool present=working&&slot<7&&(edge==POINTS_EDGE_START||(edge==POINTS_EDGE_END&&p->duration_minutes)||(edge==POINTS_EDGE_WARNING&&p->warn3));
   assert(points_event_for_day(&c,slot,day,edge,&event,NULL)==present);
   if(present){
    uint32_t deadline=raw(2026,10,date,p->hour,p->minute);
    if(edge!=POINTS_EDGE_START)deadline+=(uint32_t)p->duration_minutes*60-(edge==POINTS_EDGE_WARNING?180u:0u);
    assert(event.deadline==deadline&&event.parent_day==day&&event.slot==slot&&event.edge==edge);
    assert(event.kind==p->kind&&event.mode==3);
    if(edge==POINTS_EDGE_WARNING){
     const unsigned hours[]={9,12,14},minutes[]={12,27,27};
     assert(event.deadline==raw(2026,10,date,hours[slot-3],minutes[slot-3]));
    }
   }
  }
 }
 points_projection projection;
 assert(points_project(&c,raw(2026,10,4,12,0),&projection)&&projection.count==4);
 assert(projection.next[0].slot==0&&projection.next[0].deadline==raw(2026,10,5,4,30));
 assert(points_project(&c,raw(2026,10,9,0,0),&projection)&&projection.count==4);
 assert(projection.next[0].slot==0&&projection.next[0].deadline==raw(2026,10,12,4,30));
}
static void assert_meta(const points_meta *actual,const points_meta *expected){
 assert(actual->revision==expected->revision);
 for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)assert(actual->custom[i].color==expected->custom[i].color&&!strcmp(actual->custom[i].name,expected->custom[i].name));
}
static void test_meta_versions(void){
 points_meta factory=points_default_meta(),decoded;
 assert(factory.revision==1&&points_meta_valid(&factory));
 assert(factory.custom[0].color==6&&!strcmp(factory.custom[0].name,"Drive to Work"));
 assert(factory.custom[1].color==0&&!strcmp(factory.custom[1].name,"Wakeup"));
 assert(strlen(factory.custom[0].name)==13);
 for(unsigned version=1;version<=2;version++){
  points_meta original={.revision=37,.custom={{.color=5,.name="ABCDEFGHIJKL"},{.color=2,.name="BREAK"}}};
  if(version==2)original=factory;
  uint8_t bytes[POINTS_RECORD_SIZE],copy[POINTS_RECORD_SIZE],roundtrip[POINTS_RECORD_SIZE];
  points_meta_encode(&original,bytes);assert(!memcmp(bytes,version==1?"PTM1":"PTM2",4));
  assert(points_meta_decode(&decoded,bytes,sizeof(bytes)));assert_meta(&decoded,&original);
  points_meta_encode(&decoded,roundtrip);assert(!memcmp(bytes,roundtrip,sizeof(bytes)));
  const unsigned stride=version==1?14:15,max= stride-2;
  /* Invalid declared lengths and padding are rejected even with a valid hash. */
  for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++){
   unsigned offset=8+i*stride;
   memcpy(copy,bytes,sizeof(copy));copy[offset+1]=(uint8_t)(max+1);alarm_write32(copy+60,points_checksum(copy));
   assert(!points_meta_decode(&decoded,copy,sizeof(copy)));
   memcpy(copy,bytes,sizeof(copy));copy[offset+1]--;alarm_write32(copy+60,points_checksum(copy));
   assert(!points_meta_decode(&decoded,copy,sizeof(copy)));
   for(unsigned j=bytes[offset+1];j<max;j++){
    memcpy(copy,bytes,sizeof(copy));copy[offset+2+j]=1;alarm_write32(copy+60,points_checksum(copy));
    assert(!points_meta_decode(&decoded,copy,sizeof(copy)));
   }
  }
  for(unsigned i=8+2*stride;i<60;i++){
   memcpy(copy,bytes,sizeof(copy));copy[i]=1;alarm_write32(copy+60,points_checksum(copy));
   assert(!points_meta_decode(&decoded,copy,sizeof(copy)));
  }
  assert(!points_meta_decode(&decoded,bytes,sizeof(bytes)-1));
  for(unsigned i=0;i<sizeof(bytes);i++){
   memcpy(copy,bytes,sizeof(copy));copy[i]^=1;assert(!points_meta_decode(&decoded,copy,sizeof(copy)));
  }
 }
 /* A long name in the second slot must also select and survive PTM2. */
 points_meta second={.revision=2,.custom={{.color=1,.name="Short"},{.color=6,.name="Drive to Work"}}};
 uint8_t bytes[POINTS_RECORD_SIZE];points_meta_encode(&second,bytes);assert(!memcmp(bytes,"PTM2",4));
 assert(points_meta_decode(&decoded,bytes,sizeof(bytes)));assert_meta(&decoded,&second);
}
int main(void){
 test_default_schedule();test_meta_versions();
 uint8_t bytes[64],copy[64];points_config c=example(),d;c.points[0]=(points_item){.kind=POINTS_LUNCH,.enabled=1,.mode=0,.weekdays=127,.hour=12,.minute=0,.duration_minutes=30};
 assert(points_config_valid(&c));points_config_encode(&c,bytes);assert(points_config_decode(&d,bytes,64));assert(!memcmp(&c,&d,sizeof(c)));
 for(unsigned i=0;i<64;i++)for(unsigned b=0;b<8;b++){memcpy(copy,bytes,64);copy[i]^=(uint8_t)(1u<<b);assert(!points_config_decode(&d,copy,64));}
 c.points[0].duration_minutes=721;assert(!points_config_valid(&c));c.points[0].duration_minutes=1;c.points[0].kind=POINTS_BEDTIME;assert(!points_config_valid(&c));
 c=example();c.points[0]=(points_item){.kind=POINTS_LUNCH,.enabled=1,.mode=0,.weekdays=127,.hour=23,.minute=55,.duration_minutes=15};uint32_t now=raw(2026,10,4,23,56);c.created=now-3600;
 points_projection p;assert(points_project(&c,now,&p)&&p.has_previous&&p.count==4);assert(p.next[0].edge==1&&p.next[0].deadline==now+14*60&&p.next[0].parent_day==p.previous.parent_day);
 c.created=now;assert(points_project(&c,now,&p)&&p.next[0].edge==0); /* no orphan old end */
 c=example();c.created=raw(2026,10,1,0,0);c.points[0]=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=1,.weekdays=2,.hour=9,.minute=0};now=raw(2026,10,4,12,0);assert(points_project(&c,now,&p)&&p.next[0].deadline==raw(2026,10,5,9,0));
#ifdef PORTABLE_RTC_UTC8_DENVER
 c.created=raw(2026,3,1,0,0);c.points[0]=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=1,.weekdays=127,.hour=2,.minute=30};now=raw(2026,3,8,0,0);assert(points_project(&c,now,&p)&&(p.flags&POINTS_FLAG_GAP));assert(p.next[0].deadline==raw(2026,3,9,2,30));
 c.created=raw(2026,10,1,0,0);c.points[0].hour=1;c.points[0].minute=30;now=raw(2026,11,1,0,0);assert(points_project(&c,now,&p)&&(p.flags&POINTS_FLAG_FOLD));assert(p.next[0].deadline==raw(2026,11,2,1,30));
#endif
 points_ledger l={.revision=1,.generation=1,.slot=0,.edge=POINTS_EDGE_START,.state=ALARM_OCC_PENDING,.mode=3,.deadline=800000100,.recovery_until=800000160};
 assert(points_ledger_mark(&l,0,100,POINTS_EDGE_START));points_ledger m;points_ledger_encode(&l,bytes);assert(points_ledger_decode(&m,bytes,64));
 assert(m.day[0]==100&&(m.delivered[0]&(1u<<POINTS_EDGE_START)));
 for(unsigned i=0;i<64;i++){memcpy(copy,bytes,64);copy[i]^=1;assert(!points_ledger_decode(&m,copy,64));}
 c=example();c.created=0;c.points[0]=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=1,.weekdays=127,.hour=12,.minute=0,.duration_minutes=30,.notify_end=1,.warn3=1};
 assert(points_config_valid(&c));points_config_encode(&c,bytes);assert(points_config_decode(&d,bytes,64));assert(d.points[0].notify_end&&d.points[0].warn3);
 points_event w={0},e={0};assert(points_event_for_day(&c,0,100,POINTS_EDGE_WARNING,&w,NULL));assert(points_event_for_day(&c,0,100,POINTS_EDGE_END,&e,NULL));assert(w.deadline+180==e.deadline);
 points_meta meta={.revision=1};meta.custom[0].color=6;memcpy(meta.custom[0].name,"MEDICINE",9);assert(points_meta_valid(&meta));points_meta_encode(&meta,bytes);points_meta decoded;assert(points_meta_decode(&decoded,bytes,64));assert(!strcmp(decoded.custom[0].name,"MEDICINE")&&decoded.custom[0].color==6);
 puts("Points default schedule, PTM1/PTM2 metadata, cue flags, bounded projection, weekdays, midnight, revisions and DST fixtures passed");return 0;
}
