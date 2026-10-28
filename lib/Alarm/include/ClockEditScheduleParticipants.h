#ifndef UTILITIES_CLOCK_EDIT_SCHEDULE_PARTICIPANTS_H
#define UTILITIES_CLOCK_EDIT_SCHEDULE_PARTICIPANTS_H
#ifndef ALARM_NATIVE_UTC
#error "Mixed schedule authorization requires native UTC record identities"
#endif
#include "ClockEditParticipants.h"
#include "PointsRecords.h"
#define CLOCK_CONFIG_ABSENT 0u
#define CLOCK_CONFIG_SAVED 1u
#define CLOCK_CONFIG_VIRTUAL 2u
static inline bool clock_schedule_zero(const uint8_t *p,size_t size){for(size_t i=0;i<size;i++)if(p[i])return false;return true;}
static inline bool clock_alarm_participant(const uint8_t*c,const uint8_t*h,uint32_t floor,uint32_t zone,uint32_t provenance,uint32_t requested,bool exact_floor,uint8_t kind){
 if(zone)return false;
 if(provenance==CLOCK_CONFIG_ABSENT)return !floor&&clock_schedule_zero(c,32)&&clock_schedule_zero(h,32);
 if(provenance!=CLOCK_CONFIG_SAVED)return false;
 alarm_config config;alarm_occurrence history;
 if(!alarm_config_decode(&config,c,32,kind)||floor>config.created||(exact_floor&&floor!=(config.created<requested?config.created:requested)))return false;
 return clock_schedule_zero(h,32)||(alarm_occurrence_decode(&history,h,32,kind)&&history.revision<=config.revision);
}
static inline bool clock_absolute_participant(const uint8_t*c,const uint8_t*h,uint32_t f,uint32_t z,uint32_t p,uint32_t r,bool e){return clock_alarm_participant(c,h,f,z,p,r,e,1);}
static inline bool clock_countdown_participant(const uint8_t*c,const uint8_t*h,uint32_t f,uint32_t z,uint32_t p,uint32_t r,bool e){return clock_alarm_participant(c,h,f,z,p,r,e,2);}
static inline bool clock_points_participant(const uint8_t*c,const uint8_t*h,uint32_t floor,uint32_t zone,uint32_t provenance,uint32_t requested,bool exact_floor){
 if(zone>=419)return false;
 if(provenance==CLOCK_CONFIG_ABSENT)return !floor&&clock_schedule_zero(c,64)&&clock_schedule_zero(h,64);
 if(provenance!=CLOCK_CONFIG_SAVED&&provenance!=CLOCK_CONFIG_VIRTUAL)return false;
 points_config config;points_ledger history;
 if(!points_config_decode(&config,c,64)||floor>config.created||(exact_floor&&floor!=(config.created<requested?config.created:requested)))return false;
 if(provenance==CLOCK_CONFIG_VIRTUAL){uint8_t bytes[64];points_config defaults=points_default_config();points_config_encode(&defaults,bytes);if(memcmp(bytes,c,64))return false;}
 return clock_schedule_zero(h,64)||(points_ledger_decode(&history,h,64)&&history.revision<=config.revision);
}
/* Occurrence identity excludes delivery generation: pending replay can advance
 * generation, but ACK/expiry cannot become a newly unhandled occurrence. */
static inline bool clock_points_same_event(const points_ledger*a,const points_ledger*b){
 return a->revision==b->revision&&a->slot==b->slot&&a->edge==b->edge&&a->deadline==b->deadline&&
 a->recovery_until==b->recovery_until&&a->mode==b->mode&&a->timezone_index==b->timezone_index&&
 a->day[a->slot]==b->day[b->slot];
}
static inline bool clock_points_history_successor(const points_ledger*old,const points_ledger*next){
 if(!old||!next||!points_ledger_valid(old)||!points_ledger_valid(next)||next->generation<old->generation||next->revision<old->revision)return false;
 if(next->revision==old->revision){
  for(unsigned i=0;i<POINTS_MAX;i++)if(next->day[i]<old->day[i]||(next->day[i]==old->day[i]&&(next->delivered[i]&old->delivered[i])!=old->delivered[i]))return false;
  bool same=clock_points_same_event(old,next);
  if(same&&old->state){
   if(next->silenced<old->silenced)return false;
   if(old->state!=ALARM_OCC_PENDING&&next->state!=old->state)return false;
   if(!next->state)return false;
  }
  if(next->state==ALARM_OCC_PENDING&&points_ledger_handled(old,next->slot,next->day[next->slot],next->edge)&&!(same&&old->state==ALARM_OCC_PENDING))return false;
 }
 if(next->generation==old->generation){
  uint8_t a[64],b[64];points_ledger_encode(old,a);points_ledger_encode(next,b);
  if(!memcmp(a,b,64))return true;
  if(old->state!=ALARM_OCC_PENDING||next->silenced<old->silenced||
     (next->state!=ALARM_OCC_PENDING&&next->state!=ALARM_OCC_ACKED&&next->state!=ALARM_OCC_EXPIRED))return false;
  points_ledger permitted=*old;permitted.state=next->state;permitted.silenced=next->silenced;points_ledger_encode(&permitted,a);
  return !memcmp(a,b,64);
 }
 return true;
}
#endif
