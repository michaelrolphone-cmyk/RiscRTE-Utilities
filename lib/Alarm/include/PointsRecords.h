#ifndef UTILITIES_POINTS_RECORDS_H
#define UTILITIES_POINTS_RECORDS_H
#include "AlarmRecords.h"
#define POINTS_MAX 8u
#define POINTS_RECORD_SIZE 64u
#define POINTS_CONFIG_KEY "points_cfg"
#define POINTS_OCCURRENCE_KEY "points_occ"
#define POINTS_DURATION_MAX 720u
#define POINTS_EMPTY 0u
#define POINTS_WORK_START 1u
#define POINTS_WORK_END 2u
#define POINTS_LUNCH 3u
#define POINTS_BREAK 4u
#define POINTS_BEDTIME 5u
#define POINTS_KIND_BASE 3u
typedef struct { uint8_t kind,enabled,mode,weekdays,hour,minute; uint16_t duration_minutes; } points_item;
typedef struct { uint32_t revision,created; points_item points[POINTS_MAX]; } points_config;
typedef struct {
    uint32_t revision,generation,deadline,recovery_until;
    uint8_t slot,edge,state,mode;
    uint16_t highwater[POINTS_MAX*2];
} points_ledger;
static inline uint32_t points_checksum(const uint8_t *p) {
    uint32_t h=2166136261u;for(unsigned i=0;i<60;i++)h=(h^p[i])*16777619u;return h;
}
static inline bool points_config_valid(const points_config *c) {
    if(!c||!c->revision||c->created>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS)return false;
    for(unsigned i=0;i<POINTS_MAX;i++) {
        const points_item *p=&c->points[i];
        if(p->kind==POINTS_EMPTY) {
            if(p->enabled||p->mode||p->weekdays||p->hour||p->minute||p->duration_minutes)return false;
            continue;
        }
        if(p->kind<POINTS_WORK_START||p->kind>POINTS_BEDTIME||p->enabled>1||p->mode>3||
           p->weekdays>127||(p->enabled&&!p->weekdays)||p->hour>23||p->minute>59||
           p->duration_minutes>POINTS_DURATION_MAX||
           (p->duration_minutes&&p->kind!=POINTS_LUNCH&&p->kind!=POINTS_BREAK))return false;
    }return true;
}
static inline void points_config_encode(const points_config *c,uint8_t b[POINTS_RECORD_SIZE]) {
    memset(b,0,POINTS_RECORD_SIZE);memcpy(b,"PTC1",4);alarm_write32(b+4,c->revision);alarm_write32(b+8,c->created);
    for(unsigned i=0;i<POINTS_MAX;i++) {
        const points_item *p=&c->points[i];uint8_t *q=b+12+i*6;
        q[0]=(uint8_t)(p->kind|(p->mode<<3)|(p->enabled<<5));q[1]=p->weekdays;q[2]=p->hour;q[3]=p->minute;
        q[4]=(uint8_t)p->duration_minutes;q[5]=(uint8_t)(p->duration_minutes>>8);
    }alarm_write32(b+60,points_checksum(b));
}
static inline bool points_config_decode(points_config *c,const uint8_t *b,uint32_t n) {
    if(!c||!b||n!=POINTS_RECORD_SIZE||memcmp(b,"PTC1",4)||alarm_read32(b+60)!=points_checksum(b))return false;
    points_config v={.revision=alarm_read32(b+4),.created=alarm_read32(b+8)};
    for(unsigned i=0;i<POINTS_MAX;i++) {
        const uint8_t *q=b+12+i*6;if(q[0]&0xc0)return false;
        v.points[i]=(points_item){(uint8_t)(q[0]&7),(uint8_t)((q[0]>>5)&1),(uint8_t)((q[0]>>3)&3),q[1],q[2],q[3],(uint16_t)(q[4]|((uint16_t)q[5]<<8))};
    }if(!points_config_valid(&v))return false;*c=v;return true;
}
static inline bool points_ledger_valid(const points_ledger *l) {
    if(!l||!l->revision||!l->generation||l->slot>=POINTS_MAX||l->edge>1||l->state>ALARM_OCC_EXPIRED)return false;
    for(unsigned i=0;i<POINTS_MAX*2;i++)if(l->highwater[i]>36525)return false;
    if(!l->state)return !l->slot&&!l->edge&&!l->deadline&&!l->recovery_until&&!l->mode;
    return l->generation&&l->mode>=1&&l->mode<=3&&l->deadline&&
        l->deadline<=ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS&&l->recovery_until==l->deadline+ALARM_RECOVERY_SECONDS;
}
static inline void points_ledger_encode(const points_ledger *l,uint8_t b[POINTS_RECORD_SIZE]) {
    memset(b,0,POINTS_RECORD_SIZE);memcpy(b,"PTO1",4);alarm_write32(b+4,l->revision);alarm_write32(b+8,l->generation);
    alarm_write32(b+12,l->deadline);alarm_write32(b+16,l->recovery_until);
    b[20]=l->slot;b[21]=l->edge;b[22]=l->state;b[23]=l->mode;
    for(unsigned i=0;i<POINTS_MAX*2;i++){b[24+i*2]=(uint8_t)l->highwater[i];b[25+i*2]=(uint8_t)(l->highwater[i]>>8);}
    alarm_write32(b+60,points_checksum(b));
}
static inline bool points_ledger_decode(points_ledger *l,const uint8_t *b,uint32_t n) {
    if(!l||!b||n!=POINTS_RECORD_SIZE||memcmp(b,"PTO1",4)||alarm_read32(b+56)||alarm_read32(b+60)!=points_checksum(b))return false;
    points_ledger v={.revision=alarm_read32(b+4),.generation=alarm_read32(b+8),.deadline=alarm_read32(b+12),
        .recovery_until=alarm_read32(b+16),.slot=b[20],.edge=b[21],.state=b[22],.mode=b[23]};
    for(unsigned i=0;i<POINTS_MAX*2;i++)v.highwater[i]=(uint16_t)(b[24+i*2]|((uint16_t)b[25+i*2]<<8));
    if(!points_ledger_valid(&v))return false;
    *l=v;return true;
}
static inline const char *points_label(unsigned kind,unsigned edge) {
    switch(kind){case POINTS_WORK_START:return "WORK START";case POINTS_WORK_END:return "WORK END";
    case POINTS_LUNCH:return edge?"LUNCH ENDED":"LUNCH";case POINTS_BREAK:return edge?"BREAK ENDED":"BREAK";
    case POINTS_BEDTIME:return "BEDTIME";default:return "POINTS IN TIME";}
}
static inline uint32_t points_token_kind(unsigned slot,unsigned edge){return POINTS_KIND_BASE+slot*2+edge;}
#endif
