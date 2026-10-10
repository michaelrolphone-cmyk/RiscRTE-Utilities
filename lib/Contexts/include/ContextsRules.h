#ifndef CONTEXTS_RULES_H
#define CONTEXTS_RULES_H
/* Shared, bounded model and deterministic rule composition. No hardware or UI. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define CR_ITEMS 24u
#define CR_RULES 16u
#define CR_STEPS 3u
#define CR_SETTINGS 10u
#define CR_LOGS 30u
#define CR_FILE_BYTES (16u+CR_ITEMS*64u+CR_RULES*256u+4u)
#define CR_KEEP (-1)
enum { CR_PLACE=1,CR_EVENT,CR_SIGNAL,CR_BATTERY,CR_TIME,CR_CHARGER };
enum { CR_FACE,CR_POWER,CR_SLEEP,CR_DEEP,CR_WIFI,CR_BLUETOOTH,CR_BRIGHTNESS,CR_VOLUME,CR_NOTIFY,CR_DND };
enum { CR_LAUNCH=1,CR_TIMER,CR_MESSAGE };
enum { CR_ENTER=1,CR_LEAVE,CR_APPLIED,CR_PARTIAL,CR_RAN,CR_FAILED };
typedef struct {uint8_t kind,arg0,arg1;char name[17];} cr_item;
typedef struct {uint8_t kind;uint32_t value;char text[33];} cr_step;
typedef struct {char name[17];uint8_t item,priority,enabled,steps;uint32_t hold_seconds;int32_t settings[CR_SETTINGS];cr_step workflow[CR_STEPS];} cr_rule;
typedef struct {uint32_t generation;cr_item items[CR_ITEMS];cr_rule rules[CR_RULES];} cr_store;
typedef struct {uint32_t sequence,at_ms;uint8_t rule,kind;char name[17];} cr_log;
typedef struct {uint32_t generation,sequence;uint64_t hold_until[CR_RULES];bool active[CR_RULES];int8_t winners[CR_SETTINGS];int32_t values[CR_SETTINGS];uint16_t pending;cr_log logs[CR_LOGS];uint8_t log_next,log_count;} cr_engine;
typedef struct {bool enabled,battery_valid,charging,time_valid,classification_pending;uint8_t battery,hour;const char *room,*event,*signal;uint64_t now_ms;} cr_observation;
typedef struct {uint16_t entered,left,active;uint16_t changed;int32_t settings[CR_SETTINGS];int8_t winners[CR_SETTINGS];} cr_result;
static inline void cr_rule_init(cr_rule*r){memset(r,0,sizeof(*r));r->priority=5;for(unsigned i=0;i<CR_SETTINGS;i++)r->settings[i]=CR_KEEP;}
static inline void cr_init(cr_store*s){memset(s,0,sizeof(*s));for(unsigned i=0;i<CR_RULES;i++)cr_rule_init(&s->rules[i]);}
static inline bool cr_name(const char*s,unsigned n){if(!s||!s[0])return false;for(unsigned i=0;i<n;i++){if(!s[i])return true;if((unsigned char)s[i]<32||(unsigned char)s[i]>126)return false;}return false;}
static inline bool cr_item_valid(const cr_item*i){if(!i->kind)return !i->name[0];if(i->kind>CR_CHARGER||!cr_name(i->name,17))return false;if(i->kind==CR_BATTERY)return i->arg0>=5&&i->arg0<=50;if(i->kind==CR_TIME)return i->arg0<24&&i->arg1<24&&i->arg0!=i->arg1;return true;}
static inline bool cr_setting_valid(unsigned i,int32_t v){if(v==CR_KEEP)return true;switch(i){case CR_FACE:return v>=0&&v<256;case CR_POWER:return v>=0&&v<=2;case CR_SLEEP:return v==0||v==15||v==30||v==60||v==300;case CR_DEEP:return v==0||v==600||v==1800||v==3600||v==14400;case CR_BRIGHTNESS:case CR_VOLUME:return v>=0&&v<=10;case CR_NOTIFY:return v>=0&&v<=3;default:return v==0||v==1;}}
static inline bool cr_valid(const cr_store*s){
 if(!s)return false;
 for(unsigned i=0;i<CR_ITEMS;i++){if(!cr_item_valid(&s->items[i]))return false;for(unsigned j=0;j<i;j++)if(s->items[i].kind&&s->items[j].kind==s->items[i].kind&&!strcmp(s->items[j].name,s->items[i].name))return false;}
 for(unsigned i=0;i<CR_RULES;i++){const cr_rule*r=&s->rules[i];if(!r->name[0]){if(r->enabled)return false;continue;}if(!cr_name(r->name,17)||r->item>=CR_ITEMS||!s->items[r->item].kind||r->priority<1||r->priority>9||r->enabled>1||r->steps>CR_STEPS||r->hold_seconds>900)return false;for(unsigned f=0;f<CR_SETTINGS;f++)if(!cr_setting_valid(f,r->settings[f]))return false;for(unsigned j=0;j<r->steps;j++){const cr_step*w=&r->workflow[j];if(w->kind<CR_LAUNCH||w->kind>CR_MESSAGE)return false;if(w->kind==CR_TIMER){if(!w->value||w->value>86400)return false;}else if(!cr_name(w->text,33))return false;}}
 return true;
}
static inline uint32_t cr_crc(const uint8_t*b,unsigned n){uint32_t c=~0u;while(n--){c^=*b++;for(unsigned k=0;k<8;k++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
static inline void cr_put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline uint32_t cr_get(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline bool cr_encode(const cr_store*s,uint8_t*b,unsigned n){
 if(n<CR_FILE_BYTES||!cr_valid(s))return false;
 memset(b,0,CR_FILE_BYTES);memcpy(b,"CTX2",4);cr_put(b+4,s->generation);cr_put(b+8,CR_FILE_BYTES);
 for(unsigned i=0;i<CR_ITEMS;i++){uint8_t*p=b+16+i*64;const cr_item*q=&s->items[i];p[0]=q->kind;p[1]=q->arg0;p[2]=q->arg1;memcpy(p+4,q->name,17);}
 for(unsigned i=0;i<CR_RULES;i++){uint8_t*p=b+16+CR_ITEMS*64+i*256;const cr_rule*r=&s->rules[i];memcpy(p,r->name,17);p[17]=r->item;p[18]=r->priority;p[19]=r->enabled;p[20]=r->steps;cr_put(p+24,r->hold_seconds);for(unsigned f=0;f<CR_SETTINGS;f++)cr_put(p+28+f*4,(uint32_t)r->settings[f]);for(unsigned j=0;j<CR_STEPS;j++){uint8_t*w=p+72+j*48;w[0]=r->workflow[j].kind;cr_put(w+4,r->workflow[j].value);memcpy(w+8,r->workflow[j].text,33);}}
 cr_put(b+CR_FILE_BYTES-4,cr_crc(b,CR_FILE_BYTES-4));return true;
}
static inline bool cr_decode(cr_store*s,const uint8_t*b,unsigned n){
 if(!s||!b||n!=CR_FILE_BYTES||memcmp(b,"CTX2",4)||cr_get(b+8)!=n||cr_get(b+n-4)!=cr_crc(b,n-4))return false;
 cr_init(s);s->generation=cr_get(b+4);
 for(unsigned i=0;i<CR_ITEMS;i++){const uint8_t*p=b+16+i*64;cr_item*q=&s->items[i];q->kind=p[0];q->arg0=p[1];q->arg1=p[2];memcpy(q->name,p+4,17);}
 for(unsigned i=0;i<CR_RULES;i++){const uint8_t*p=b+16+CR_ITEMS*64+i*256;cr_rule*r=&s->rules[i];memcpy(r->name,p,17);r->item=p[17];r->priority=p[18];r->enabled=p[19];r->steps=p[20];r->hold_seconds=cr_get(p+24);for(unsigned f=0;f<CR_SETTINGS;f++)r->settings[f]=(int32_t)cr_get(p+28+f*4);for(unsigned j=0;j<CR_STEPS;j++){const uint8_t*w=p+72+j*48;r->workflow[j].kind=w[0];r->workflow[j].value=cr_get(w+4);memcpy(r->workflow[j].text,w+8,33);}}
 return cr_valid(s);
}
static inline void cr_engine_init(cr_engine*e){memset(e,0,sizeof(*e));for(unsigned i=0;i<CR_SETTINGS;i++){e->winners[i]=-1;e->values[i]=CR_KEEP;}}
static inline void cr_record(cr_engine*e,const cr_store*s,unsigned rule,unsigned kind,uint64_t now){cr_log*l=&e->logs[e->log_next];*l=(cr_log){.sequence=++e->sequence,.at_ms=(uint32_t)now,.rule=(uint8_t)rule,.kind=(uint8_t)kind};if(rule<CR_RULES)memcpy(l->name,s->rules[rule].name,17);e->log_next=(e->log_next+1)%CR_LOGS;if(e->log_count<CR_LOGS)e->log_count++;}
static inline bool cr_condition(const cr_item*i,const cr_observation*o){switch(i->kind){case CR_PLACE:return o->room&&o->room[0]&&!strcmp(i->name,o->room);case CR_EVENT:return o->event&&o->event[0]&&!strcmp(i->name,o->event);case CR_SIGNAL:return o->signal&&o->signal[0]&&!strcmp(i->name,o->signal);case CR_BATTERY:return o->battery_valid&&o->battery<=i->arg0;case CR_CHARGER:return o->battery_valid&&o->charging;case CR_TIME:return o->time_valid&&(i->arg0<i->arg1?(o->hour>=i->arg0&&o->hour<i->arg1):(o->hour>=i->arg0||o->hour<i->arg1));default:return false;}}
static inline cr_result cr_evaluate(cr_engine*e,const cr_store*s,const cr_observation*o){
 cr_result result={0};for(unsigned f=0;f<CR_SETTINGS;f++){result.settings[f]=CR_KEEP;result.winners[f]=-1;}
 /* Configuration changes preserve existing edges. Deleting/disabling a rule
  * deactivates it; changing an action never silently replays its workflow. */
 for(unsigned i=0;i<CR_RULES;i++){const cr_rule*r=&s->rules[i];bool enabled=o->enabled&&r->name[0]&&r->enabled&&r->item<CR_ITEMS;bool pending=enabled&&o->classification_pending&&s->items[r->item].kind<=CR_SIGNAL;bool condition=enabled&&cr_condition(&s->items[r->item],o);if(condition)e->hold_until[i]=o->now_ms+(uint64_t)r->hold_seconds*1000u;bool active=enabled&&(condition||(e->active[i]&&(pending||o->now_ms<e->hold_until[i])));if(active&&!e->active[i]){result.entered|=1u<<i;e->pending|=1u<<i;cr_record(e,s,i,CR_ENTER,o->now_ms);}if(!active&&e->active[i]){result.left|=1u<<i;e->pending&=~(1u<<i);cr_record(e,s,i,CR_LEAVE,o->now_ms);}e->active[i]=active;if(!active)continue;result.active|=1u<<i;for(unsigned f=0;f<CR_SETTINGS;f++)if(r->settings[f]!=CR_KEEP&&(result.winners[f]<0||r->priority<s->rules[(unsigned)result.winners[f]].priority)){result.winners[f]=(int8_t)i;result.settings[f]=r->settings[f];}}
 for(unsigned f=0;f<CR_SETTINGS;f++){if(result.settings[f]!=CR_KEEP&&(e->winners[f]!=result.winners[f]||e->values[f]!=result.settings[f]))result.changed|=1u<<f;e->winners[f]=result.winners[f];e->values[f]=result.settings[f];}e->generation=s->generation;return result;
}
#endif
