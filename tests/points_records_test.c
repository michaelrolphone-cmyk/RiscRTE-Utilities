#include <assert.h>
#include <stdio.h>
#include "PointsSchedule.h"
static points_config example(void){points_config c={.revision=1,.created=800000000};for(unsigned i=0;i<8;i++)c.points[i]=(points_item){.kind=POINTS_BREAK};return c;}
static uint32_t raw(unsigned y,unsigned m,unsigned d,unsigned h,unsigned minute){twatch_rtc_time_v1 t={(uint16_t)y,(uint8_t)m,(uint8_t)d,0,(uint8_t)h,(uint8_t)minute,0};portable_time_candidate v[2];assert(portable_time_inverse(&t,v)==1);uint32_t s;assert(alarm_calendar_seconds(v[0].rtc.year,v[0].rtc.month,v[0].rtc.day,v[0].rtc.hour,v[0].rtc.minute,0,&s));return s;}
int main(void){
 uint8_t bytes[64],copy[64];points_config c=example(),d;c.points[0]=(points_item){POINTS_LUNCH,1,0,127,12,0,30};
 assert(points_config_valid(&c));points_config_encode(&c,bytes);assert(points_config_decode(&d,bytes,64));assert(!memcmp(&c,&d,sizeof(c)));
 for(unsigned i=0;i<64;i++)for(unsigned b=0;b<8;b++){memcpy(copy,bytes,64);copy[i]^=(uint8_t)(1u<<b);assert(!points_config_decode(&d,copy,64));}
 c.points[0].duration_minutes=721;assert(!points_config_valid(&c));c.points[0].duration_minutes=1;c.points[0].kind=POINTS_BEDTIME;assert(!points_config_valid(&c));
 c=example();c.points[0]=(points_item){POINTS_LUNCH,1,0,127,23,55,15};uint32_t now=raw(2026,10,4,23,56);c.created=now-3600;
 points_projection p;assert(points_project(&c,now,&p)&&p.has_previous&&p.count==4);assert(p.next[0].edge==1&&p.next[0].deadline==now+14*60&&p.next[0].parent_day==p.previous.parent_day);
 c.created=now;assert(points_project(&c,now,&p)&&p.next[0].edge==0); /* no orphan old end */
 c=example();c.created=raw(2026,10,1,0,0);c.points[0]=(points_item){POINTS_BREAK,1,1,2,9,0,0};now=raw(2026,10,4,12,0);assert(points_project(&c,now,&p)&&p.next[0].deadline==raw(2026,10,5,9,0));
#ifdef PORTABLE_RTC_UTC8_DENVER
 c.created=raw(2026,3,1,0,0);c.points[0]=(points_item){POINTS_BREAK,1,1,127,2,30,0};now=raw(2026,3,8,0,0);assert(points_project(&c,now,&p)&&(p.flags&POINTS_FLAG_GAP));assert(p.next[0].deadline==raw(2026,3,9,2,30));
 c.created=raw(2026,10,1,0,0);c.points[0].hour=1;c.points[0].minute=30;now=raw(2026,11,1,0,0);assert(points_project(&c,now,&p)&&(p.flags&POINTS_FLAG_FOLD));assert(p.next[0].deadline==raw(2026,11,2,1,30));
#endif
 points_ledger l={.revision=1,.generation=1,.slot=0,.edge=POINTS_EDGE_START,.state=ALARM_OCC_PENDING,.mode=3,.deadline=800000100,.recovery_until=800000160};
 assert(points_ledger_mark(&l,0,100,POINTS_EDGE_START));points_ledger m;points_ledger_encode(&l,bytes);assert(points_ledger_decode(&m,bytes,64));
 assert(m.day[0]==100&&(m.delivered[0]&(1u<<POINTS_EDGE_START)));
 for(unsigned i=0;i<64;i++){memcpy(copy,bytes,64);copy[i]^=1;assert(!points_ledger_decode(&m,copy,64));}
 c=example();c.points[0]=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=1,.weekdays=127,.hour=12,.minute=0,.duration_minutes=30,.notify_end=1,.warn3=1};
 assert(points_config_valid(&c));points_config_encode(&c,bytes);assert(points_config_decode(&d,bytes,64));assert(d.points[0].notify_end&&d.points[0].warn3);
 points_event w={0},e={0};assert(points_event_for_day(&c,0,100,POINTS_EDGE_WARNING,&w,NULL));assert(points_event_for_day(&c,0,100,POINTS_EDGE_END,&e,NULL));assert(w.deadline+180==e.deadline);
 points_meta meta={.revision=1};meta.custom[0].color=6;memcpy(meta.custom[0].name,"MEDICINE",9);assert(points_meta_valid(&meta));points_meta_encode(&meta,bytes);points_meta decoded;assert(points_meta_decode(&decoded,bytes,64));assert(!strcmp(decoded.custom[0].name,"MEDICINE")&&decoded.custom[0].color==6);
 puts("Points records, custom metadata, cue flags, bounded projection, weekdays, midnight, revisions and DST fixtures passed");return 0;
}
