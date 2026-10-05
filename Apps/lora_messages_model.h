#pragma once
/* Bounded app-owned raw LoRa messaging. No implicit RF parameters, automatic
 * transmission, acknowledgement protocol, encryption or hardware discovery. */
#include "PortableLoRaV2.h"
#include <string.h>
#define LM_TEXT_MAX 240u
#define LM_HISTORY_MAX 16u
#define LM_PROFILE_BYTES 32u
#define LM_STORE_INSTANCE 9u
#define LM_PROFILE_KEY "lora_profile"
enum { LM_IDLE, LM_LISTENING, LM_SENDING, LM_TRANSMITTED, LM_RECEIVED,
       LM_SEND_UNKNOWN, LM_RADIO_ERROR, LM_BAD_PACKET, LM_STOPPED, LM_CLEANUP_ERROR };
enum { LM_OUT_PENDING, LM_OUT_SENT, LM_OUT_UNKNOWN, LM_IN };
typedef struct {
 char text[256];
 uint16_t year;
 uint8_t month,day,hour,minute,status;
 uint16_t length;
 int16_t rssi;
 int8_t snr;
 bool clock_valid,binary;
} lm_message;
typedef struct {
 twatch_lora_config_v2 profile;
 const twatch_radio_api_v2 *radio;
 lm_message history[LM_HISTORY_MAX];
 unsigned count,next,active;
 unsigned state;
 bool profile_valid,owned,listening,uncertain;
 uint32_t began;
} lm_model;
static inline bool lm_profile_valid(const twatch_lora_config_v2 *p) {
 if(!p || p->reserved[0] || p->reserved[1] || p->reserved[2] || p->sf<5 || p->sf>12 ||
    p->coding_rate<5 || p->coding_rate>8 || p->preamble<8 || p->preamble>4096)return false;
 if(p->frequency_hz>=2400000000u && p->frequency_hz<=2500000000u)
  return p->power_dbm>=-18 && p->power_dbm<=13 && (p->bandwidth_hz==203125 || p->bandwidth_hz==406250 || p->bandwidth_hz==812500 || p->bandwidth_hz==1625000);
 if((p->frequency_hz>=430000000u && p->frequency_hz<=440000000u) ||
    (p->frequency_hz>=863000000u && p->frequency_hz<=870000000u) ||
    (p->frequency_hz>=902000000u && p->frequency_hz<=928000000u))
  return p->power_dbm>=-9 && p->power_dbm<=22 && (p->bandwidth_hz==125000 || p->bandwidth_hz==250000 || p->bandwidth_hz==500000);
 return false;
}
static inline bool lm_api_valid(const twatch_radio_api_v2 *a) {
 return a && a->api_version==2 && a->struct_size>=sizeof(*a) && a->configure && a->send && a->receive && a->poll && a->read && a->cancel;
}
static inline uint32_t lm_u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void lm_put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static inline uint32_t lm_checksum(const uint8_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<28;i++)h=(h^p[i])*16777619u;return h;}
static inline void lm_profile_encode(const twatch_lora_config_v2 *p,uint8_t out[32]) {
 memset(out,0,32);out[0]='L';out[1]='M';out[2]=1;lm_put32(out+4,p->frequency_hz);lm_put32(out+8,p->bandwidth_hz);
 out[12]=(uint8_t)p->preamble;out[13]=(uint8_t)(p->preamble>>8);out[14]=p->sf;out[15]=p->coding_rate;out[16]=(uint8_t)(p->power_dbm+18);lm_put32(out+28,lm_checksum(out));
}
static inline bool lm_profile_decode(twatch_lora_config_v2 *out,const uint8_t *p,size_t n) {
 if(!out || !p || n!=32 || p[0]!='L' || p[1]!='M' || p[2]!=1 || p[3] || p[16]>40 || lm_u32(p+28)!=lm_checksum(p))return false;
 for(unsigned i=17;i<28;i++)if(p[i])return false;
 twatch_lora_config_v2 v={.frequency_hz=lm_u32(p+4),.bandwidth_hz=lm_u32(p+8),.preamble=(uint16_t)(p[12]|(uint16_t)p[13]<<8),.sf=p[14],.coding_rate=p[15],.power_dbm=(int8_t)((int)p[16]-18)};
 if(!lm_profile_valid(&v))return false;
 *out=v;return true;
}
static inline bool lm_number(const char *s,bool signed_value,int64_t *out) {
 if(!s || !*s || !out)return false;
 bool negative=*s=='-';if(negative){if(!signed_value)return false;s++;}
 if(!*s)return false;
 uint64_t n=0;unsigned count=0;
 for(;*s;s++){if(*s<'0' || *s>'9' || ++count>10)return false;n=n*10+(unsigned)(*s-'0');if(n>UINT32_MAX)return false;}
 *out=negative?-(int64_t)n:(int64_t)n;return true;
}
static inline lm_message *lm_add(lm_model *m,const uint8_t *text,size_t n,unsigned status) {
 if(!m || !text || !n || n>255)return NULL;
 unsigned index=m->next;m->next=(index+1)%LM_HISTORY_MAX;if(m->count<LM_HISTORY_MAX)m->count++;
 lm_message *v=&m->history[index];memset(v,0,sizeof(*v));v->length=(uint16_t)n;v->status=(uint8_t)status;
 for(size_t i=0;i<n;i++){bool printable=text[i]>=32 && text[i]<=126;v->text[i]=printable?(char)text[i]:'.';v->binary|=!printable;}v->text[n]=0;
 m->active=index;return v;
}
static inline lm_message *lm_at(lm_model *m,unsigned newest_index){return m && newest_index<m->count?&m->history[(m->next+LM_HISTORY_MAX-1-newest_index)%LM_HISTORY_MAX]:NULL;}
static inline bool lm_stop(lm_model *m) {
 if(!m)return false;
 m->listening=false;
 if(m->state==LM_SENDING && m->count)m->history[m->active].status=LM_OUT_UNKNOWN;
 if(m->uncertain)return false;
 if(m->owned && (!m->radio || !m->radio->cancel(m->radio->context))){m->uncertain=true;m->state=LM_CLEANUP_ERROR;return false;}
 m->owned=false;m->state=LM_STOPPED;return true;
}
static inline bool lm_fail(lm_model *m,unsigned state) {
 if(m->state==LM_SENDING && m->count)m->history[m->active].status=LM_OUT_UNKNOWN;
 bool clean=lm_stop(m);if(clean)m->state=state;return false;
}
static inline bool lm_configure(lm_model *m) {
 if(!m || m->uncertain || !m->profile_valid || !lm_profile_valid(&m->profile) || !lm_api_valid(m->radio))return false;
 if(!lm_stop(m))return false;
 m->owned=true; /* A failed configure may already own chip state. */
 if(!m->radio->configure(m->radio->context,&m->profile))return lm_fail(m,LM_RADIO_ERROR);
 m->state=LM_IDLE;return true;
}
static inline bool lm_listen(lm_model *m,uint32_t now) {
 if(!lm_configure(m))return false;
 if(!m->radio->receive(m->radio->context,5000))return lm_fail(m,LM_RADIO_ERROR);
 m->listening=true;m->began=now;m->state=LM_LISTENING;return true;
}
static inline bool lm_send(lm_model *m,const char *text,uint32_t now) {
 if(!m || !text || m->state==LM_SENDING)return false;
 size_t n=0;bool content=false;
 while(n<=LM_TEXT_MAX && text[n]){if((unsigned char)text[n]<32 || (unsigned char)text[n]>126)return false;content|=text[n]!=' ';n++;}
 if(!n || n>LM_TEXT_MAX || !content || !lm_configure(m))return false;
 if(!lm_add(m,(const uint8_t*)text,n,LM_OUT_PENDING))return lm_fail(m,LM_RADIO_ERROR);
 m->state=LM_SENDING;m->began=now;
 /* Failure can occur after the TX opcode reached the chip. Never call it sent,
  * never retry automatically, and keep the draft for an explicit decision. */
 if(!m->radio->send(m->radio->context,(const uint8_t*)text,n,60000))return lm_fail(m,LM_SEND_UNKNOWN);
 return true;
}
static inline bool lm_step(lm_model *m,uint32_t now) {
 if(!m || m->uncertain || (m->state!=LM_SENDING && m->state!=LM_LISTENING))return false;
 twatch_lora_status_v2 status={0};bool tx=m->state==LM_SENDING;
 if(!m->radio->poll(m->radio->context,&status))return lm_fail(m,tx?LM_SEND_UNKNOWN:LM_RADIO_ERROR);
 if(status.state==(tx?TW_LORA_TX:TW_LORA_RX)) {
  if((uint32_t)(now-m->began)>(tx?61000u:6000u))return lm_fail(m,tx?LM_SEND_UNKNOWN:LM_RADIO_ERROR);
  return false;
 }
 if(tx) {
  if(status.state!=TW_LORA_SENT)return lm_fail(m,LM_SEND_UNKNOWN);
  m->history[m->active].status=LM_OUT_SENT;
  if(!lm_stop(m))return false;
  /* lm_stop marks a pending TX unknown; restore only after confirmed cleanup. */
  m->history[m->active].status=LM_OUT_SENT;m->state=LM_TRANSMITTED;return true;
 }
 bool changed=false;
 if(status.state==TW_LORA_RECEIVED) {
  uint8_t bytes[255];size_t n=0;
  if(!status.length || !m->radio->read(m->radio->context,bytes,sizeof(bytes),&n) || n!=status.length)return lm_fail(m,LM_RADIO_ERROR);
  lm_message *v=lm_add(m,bytes,n,LM_IN);if(!v)return lm_fail(m,LM_RADIO_ERROR);
  v->rssi=status.rssi_dbm;v->snr=status.snr_quarter_db;changed=true;
 } else if(status.state==TW_LORA_CRC_ERROR)changed=true;
 else if(status.state!=TW_LORA_TIMEOUT)return lm_fail(m,LM_RADIO_ERROR);
 if(!m->radio->receive(m->radio->context,5000))return lm_fail(m,LM_RADIO_ERROR);
 m->began=now;return changed;
}
