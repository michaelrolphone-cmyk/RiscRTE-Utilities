/* Bounded ELM327 dashboard client. Read requests are explicitly enumerated;
 * Mode 04 additionally requires expiring user confirmation and a stationary ECU.
 * Commands are serialized until '>'; late/oversized/timeout streams never spill
 * into the next PID. Display freshness is independent of a nonzero last value. */
#include "elm.h"
#include <stdio.h>
#include <string.h>
enum {PERCENT,TEMP,RPM,SPEED,VOLTS,MAF,TRIM,COUNT,MIL,RATE,ADVANCE,O2,WORD};
const obd_spec obd_specs[OBD_METRICS]={
 {0x0c,2,RPM,400,"ENGINE SPEED","rpm"},{0x0d,1,SPEED,400,"VEHICLE SPEED","km/h"},
 {0x05,1,TEMP,1800,"COOLANT","C"},{0x04,1,PERCENT,1500,"ENGINE LOAD","%"},
 {0x11,1,PERCENT,1500,"THROTTLE","%"},{0x42,2,VOLTS,3000,"ECU VOLTAGE","V"},
 {0x2f,1,PERCENT,8000,"FUEL LEVEL","%"},{0x0f,1,TEMP,3000,"INTAKE AIR","C"},
 {0x0b,1,COUNT,1800,"MANIFOLD PRESSURE","kPa"},{0x10,2,MAF,1800,"AIR FLOW","g/s"},
 {0x06,1,TRIM,2500,"SHORT TRIM B1","%"},{0x07,1,TRIM,2500,"LONG TRIM B1","%"},
 {0x5c,1,TEMP,8000,"OIL TEMPERATURE","C"},{0x1f,2,COUNT,8000,"ENGINE RUN TIME","s"},
 {0x01,4,MIL,8000,"CHECK ENGINE","MIL / codes"},{0x5e,2,RATE,3000,"FUEL RATE","L/H"},
 {0x0e,1,ADVANCE,1800,"IGNITION TIMING","deg"},{0x15,2,O2,1800,"O2 SENSOR B1 S2","V"},
 {0x33,1,COUNT,8000,"BAROMETER","KPA"},{0x1c,1,COUNT,60000,"OBD STANDARD",""},
 {0x51,1,COUNT,60000,"FUEL TYPE",""},{0x31,2,WORD,30000,"DISTANCE SINCE CLEAR","KM"},
 {0x21,2,WORD,30000,"DISTANCE WITH LIGHT","KM"}
};
static bool due(uint32_t now,uint32_t at){return (int32_t)(now-at)>=0;}
static void copy(char *d,size_t n,const char *s){if(n)snprintf(d,n,"%s",s?s:"");}
static void fault(obd_client *c,const char *s){c->state=OBD_FAULT;c->waiting=c->prepared=false;for(unsigned i=0;i<OBD_METRICS;i++)c->samples[i].valid=false;copy(c->message,sizeof(c->message),s);c->revision++;}
void obd_init(obd_client *c,uint32_t now){memset(c,0,sizeof(*c));c->job=OBD_JOB_NONE;c->poll_mask=UINT32_MAX;c->state=OBD_INITIALIZING;c->last_sent_ms=now-150;c->metric=-1;copy(c->message,sizeof(c->message),"Identifying ELM adapter");}
void obd_stop(obd_client *c,const char *s){c->clear_armed=false;c->jobs=0;c->readiness_valid=false;c->state=OBD_OFF;c->waiting=c->prepared=false;for(unsigned i=0;i<OBD_METRICS;i++)c->samples[i].valid=false;copy(c->message,sizeof(c->message),s);c->revision++;}
static bool support(const obd_client *c,unsigned pid){return pid&&pid<=96&&(c->mask[(pid-1)/32]&(UINT32_C(1)<<(31-(pid-1)%32)));}
static void log_line(obd_client*c,const char*text,bool tx);
static bool diag_next(obd_client*c,uint32_t now);
static bool clear_conditions(const obd_client*,uint32_t);
static bool diag_line(obd_client*c,const char*text);
static void diag_finish(obd_client*c,bool ok);
static void live(obd_client *c,uint32_t now){
 c->state=OBD_LIVE;unsigned n=0;
 for(unsigned i=0;i<OBD_METRICS;i++){c->samples[i].supported=support(c,obd_specs[i].pid);c->samples[i].due_ms=now;n+=c->samples[i].supported;}
 if(!n){fault(c,"ECU has no supported dashboard readings");return;}
 snprintf(c->message,sizeof(c->message),"ECU %lX: %u supported readings",(unsigned long)c->ecu,n);c->revision++;
}
size_t obd_next(obd_client *c,uint32_t now,const char **out){
 if(!c||!out)return 0;*out=NULL;obd_tick(c,now);
 if(c->state==OBD_OFF||c->state==OBD_FAULT||c->waiting||c->paused)return 0;
 if(c->prepared&&c->job==OBD_JOB_CLEAR&&(due(now,c->clear_deadline)||c->ecu!=c->clear_ecu||!clear_conditions(c,now))){c->prepared=false;c->job=OBD_JOB_NONE;c->clear_status=OBD_DIAG_ERROR;copy(c->message,sizeof(c->message),"Clear cancelled before send: readings or confirmation expired");return 0;}
 if(c->prepared){*out=c->command;return strlen(c->command);}
 if((uint32_t)(now-c->last_sent_ms)<150)return 0;
 static const char *const setup[]={"ATZ\r","ATE0\r","ATL0\r","ATS1\r","ATH1\r","ATSP0\r","0100\r","ATDPN\r","0120\r","0140\r"};
 if(c->stage==8&&!(c->mask[0]&1))c->stage=10;
 if(c->stage==9&&!(c->mask[1]&1))c->stage=10;
 c->metric=-1;c->job=OBD_JOB_NONE;
 if(c->stage<10){
  copy(c->command,sizeof(c->command),setup[c->stage]);
  if(c->stage==5)snprintf(c->command,sizeof(c->command),"ATSP%X\r",(unsigned)c->requested_protocol);
  c->state=c->stage==6?OBD_SEARCHING:(c->stage>=7?OBD_DISCOVERING:OBD_INITIALIZING);
  copy(c->message,sizeof(c->message),c->stage==6?"Searching for vehicle ECU (ignition on)":c->stage>=7?"Reading supported sensors":"Configuring ELM adapter");
 }else {
  if(c->state!=OBD_LIVE)live(c,now);if(c->state==OBD_FAULT)return 0;
  if(diag_next(c,now)){c->prepared=true;*out=c->command;return strlen(c->command);}
  for(unsigned j=0;j<OBD_METRICS;j++){unsigned i=(c->cursor+j)%OBD_METRICS;
   if((c->poll_mask&(UINT32_C(1)<<i))&&c->samples[i].supported&&due(now,c->samples[i].due_ms)){c->metric=(int)i;c->cursor=(i+1)%OBD_METRICS;break;}}
  if(c->metric<0)return 0;snprintf(c->command,sizeof(c->command),"01%02X\r",obd_specs[c->metric].pid);
 }
 c->prepared=true;*out=c->command;return strlen(c->command);
}
void obd_sent(obd_client *c,uint32_t now){
 if(!c||!c->prepared||c->waiting||c->state==OBD_FAULT||c->state==OBD_OFF)return;
 log_line(c,c->command,true);c->assembly_size=c->assembly_needed=c->assembly_seq=c->legacy_vin_seq=0;c->assembly_bad=false;memset(&c->pending_codes,0,sizeof(c->pending_codes));memset(c->pending_vin,0,sizeof(c->pending_vin));
 c->prepared=false;c->waiting=true;c->line_size=c->received=0;c->found=c->at_ok=c->banner=c->negative=c->explicit_error=false;
 c->candidate_ecu=UINT32_MAX;c->candidate_kind=c->candidate_value=0;
 c->sent_ms=c->last_sent_ms=now;c->deadline_ms=now+(c->stage==6?20000:5000);c->requests++;
}
static int hex(char v){return v>='0'&&v<='9'?v-'0':v>='A'&&v<='F'?v-'A'+10:-1;}
static uint32_t number(const char *p,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;i++)v=v*16+(unsigned)hex(p[i]);return v;}
static int32_t decode(unsigned scale,const uint8_t *p){
 uint32_t n=(unsigned)p[0]*256+p[1];switch(scale){
 case PERCENT:return (int32_t)p[0]*100000/255;case TEMP:return ((int32_t)p[0]-40)*1000;
 case RPM:return (int32_t)n*250;case SPEED:case COUNT:return (int32_t)p[0]*1000;
 case VOLTS:return (int32_t)n;case MAF:return (int32_t)n*10;case TRIM:return ((int32_t)p[0]-128)*100000/128;
 case ADVANCE:return ((int32_t)p[0]-128)*500;case O2:return (int32_t)p[0]*5;case WORD:return (int32_t)n*1000;
 case MIL:return (int32_t)p[0]*1000;case RATE:return (int32_t)n*50;default:return 0;}
}
static bool payload(obd_client *c,const char *s,unsigned len,uint32_t *ecu,uint32_t *kind,uint8_t *data,unsigned *count){
 unsigned head=0;bool can=false;
 if(c->protocol==6||c->protocol==8){head=3;*kind=11;can=true;}
 else if(c->protocol==7||c->protocol==9){head=8;*kind=29;can=true;}
 else if(c->protocol>=1&&c->protocol<=5){head=6;*kind=3;}
 else if(len%2){head=3;*kind=11;can=true;}
 else if(len>=14&&(!strncmp(s,"18DA",4)||!strncmp(s,"18DB",4))){head=8;*kind=29;can=true;}
 else {head=6;*kind=3;}
 if(len<head+4||(len-head)%2)return false;
 *ecu=*kind==3?number(s+4,2):number(s,head);
 if((*kind==11&&*ecu>0x7ff)||(*kind==29&&*ecu>0x1fffffff))return false;
 unsigned n=(len-head)/2;if(n>12)return false;uint8_t b[12]={0};for(unsigned i=0;i<n;i++)b[i]=(uint8_t)number(s+head+i*2,2);
 unsigned start=0;
 if(can){if(n<3||b[0]<2||b[0]>7||(unsigned)b[0]+1>n)return false;start=1;n=b[0];}
 else { /* ELM checks the link-layer checksum; keep at most one trailer. */
  if(n<2||n>7)return false;
 }
 memcpy(data,b+start,n);*count=n;return true;
}
static void line(obd_client *c){
 c->line[c->line_size]=0;char *s=c->line;while(*s==' '||*s=='\t')s++;
 size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'))s[--n]=0;
 if(!n)return;log_line(c,s,false);
 if(c->job!=OBD_JOB_NONE&&diag_line(c,s))return;
 if(strstr(s,"ELM327")||strstr(s,"STN")){c->banner=true;copy(c->identity,sizeof(c->identity),s);return;}
 if(!strcmp(s,"OK")){c->at_ok=true;return;}
 if(!strcmp(s,"?")||strstr(s,"NO DATA")||strstr(s,"UNABLE TO CONNECT")||strstr(s,"CAN ERROR")||strstr(s,"BUS ERROR")||strstr(s,"BUS INIT: ...ERROR")||strstr(s,"STOPPED")||strstr(s,"BUFFER FULL")){
  c->explicit_error=true;copy(c->message,sizeof(c->message),s);return;}
 /* Compact hex only after handling banner/status/echo. */
 char compact[OBD_LINE_BYTES];unsigned k=0;bool ok=true;
 for(unsigned i=0;s[i];i++){char v=s[i];if(v==' '||v=='\t')continue;if(v>='a'&&v<='z')v-=32;
  if(hex(v)<0){ok=false;break;}compact[k++]=v;}compact[k]=0;
 char echo[16];copy(echo,sizeof(echo),c->command);echo[strcspn(echo,"\r")]=0;
 if(!strcmp(s,echo)||!strcmp(compact,echo))return;
 if(c->stage==7){unsigned start=k==2&&compact[0]=='A'?1:0;if(ok&&k-start==1&&compact[start]>='1'&&compact[start]<='9'){c->candidate_value=(unsigned)(compact[start]-'0');c->found=true;}return;}
 if(c->stage<6)return;
 if(!ok||!k){if(strstr(s,"SEARCHING")||strstr(s,"BUS INIT")||strstr(s,"AUTO"))return;c->bad_lines++;return;}
 uint32_t ecu,kind;uint8_t b[12]={0};unsigned size;
 if(!payload(c,compact,k,&ecu,&kind,b,&size)){c->bad_lines++;return;}
 if(c->have_ecu&&(ecu!=c->ecu||kind!=c->ecu_kind))return;
 if(size>=3&&b[0]==0x7f&&b[1]==1){if(b[2]!=0x78)c->negative=true;return;}
 unsigned pid=c->metric>=0?obd_specs[c->metric].pid:c->stage==6?0:c->stage==8?32:64;
 unsigned bytes=c->metric>=0?obd_specs[c->metric].bytes:4;
 if(size<bytes+2||b[0]!=0x41||b[1]!=pid){c->bad_lines++;return;}
 if(kind!=3&&size!=bytes+2){c->bad_lines++;return;}
 if(kind==3&&size>bytes+3){c->bad_lines++;return;}
 if(c->found&&ecu>=c->candidate_ecu)return; /* Stable lowest responding ECU for discovery. */
 memcpy(c->candidate_raw,b+2,bytes<4?bytes:4);c->found=true;c->candidate_ecu=ecu;c->candidate_kind=kind;
 if(c->metric<0)c->candidate_value=(uint32_t)b[2]<<24|(uint32_t)b[3]<<16|(uint32_t)b[4]<<8|b[5];
 else if(c->metric==OBD_RUNTIME)c->candidate_value=((uint32_t)b[2]*256+b[3])*1000;
 else c->candidate_value=(uint32_t)decode(obd_specs[c->metric].scale,b+2);
}
static void finish(obd_client *c,uint32_t now){
 c->waiting=false;c->rtt_ms=now-c->sent_ms;c->replies++;bool ok;
 if(c->stage==0)ok=c->banner&&!c->explicit_error;
 else if(c->stage<6)ok=c->at_ok&&!c->explicit_error;
 else ok=c->found&&!c->explicit_error&&!c->negative;
 if(c->job!=OBD_JOB_NONE){diag_finish(c,ok&&!c->assembly_bad);c->revision++;return;}
 if(!ok){
  c->failed_requests++;c->fail_streak++;
  if(c->stage<10){fault(c,c->stage<6?"ELM setup failed: wrong channel or unsupported adapter":c->stage==6?"No ECU response. Check ignition and adapter.":"ECU protocol/support discovery failed");return;}
  if(c->metric>=0){c->samples[c->metric].valid=false;c->samples[c->metric].due_ms=now+obd_specs[c->metric].interval_ms;}
  if(!c->explicit_error)copy(c->message,sizeof(c->message),c->negative?"ECU rejected reading":"Invalid or missing ECU reply");
  c->revision++;if(c->fail_streak>=6)fault(c,"Vehicle data stopped; reconnect with ignition on");return;
 }
 c->fail_streak=0;c->last_good_ms=now;
 if(c->stage==6){c->ecu=c->candidate_ecu;c->ecu_kind=c->candidate_kind;c->have_ecu=true;c->mask[0]=c->candidate_value;}
 else if(c->stage==7){c->protocol=c->candidate_value;
  unsigned expected=c->protocol==6||c->protocol==8?11:c->protocol==7||c->protocol==9?29:3;
  if(expected!=c->ecu_kind){fault(c,"ELM protocol and ECU header disagree");return;}}
 else if(c->stage==8)c->mask[1]=c->candidate_value;
 else if(c->stage==9)c->mask[2]=c->candidate_value;
 else if(c->metric>=0){if(c->metric==OBD_MIL){memcpy(c->readiness,c->candidate_raw,4);c->readiness_valid=true;c->readiness_at=now;}obd_sample *p=&c->samples[c->metric];p->milli=(int32_t)c->candidate_value;p->valid=true;p->at_ms=now;p->due_ms=now+obd_specs[c->metric].interval_ms;copy(c->message,sizeof(c->message),"Live vehicle data (read only)");}
 if(c->stage<10)c->stage++;c->revision++;
}
void obd_receive(obd_client *c,const uint8_t *b,size_t n,uint32_t now){
 if(!c||!b)return;
 /* The app drains RX before its periodic tick. Enforce the request deadline
  * here too, so a delayed prompt cannot revive an already expired command. */
 obd_tick(c,now);
 for(size_t i=0;i<n;i++){
  if(!c->waiting)continue;
  if(++c->received>2048){fault(c,"ELM reply overflow; reconnect");return;}
  unsigned v=b[i];
  if(v=='>'||v=='\r'||v=='\n'){
   if(c->line_size)line(c);c->line_size=0;if(v=='>')finish(c,now);continue;
  }
  if(v==0||v<9||(v>13&&v<32)||v>126){fault(c,"Non-ASCII serial data; wrong channel");return;}
  if(c->line_size+1>=sizeof(c->line)){fault(c,"ELM line too long; reconnect");return;}c->line[c->line_size++]=(char)v;
 }
}
void obd_tick(obd_client *c,uint32_t now){if(c&&c->waiting&&due(now,c->deadline_ms)){if(c->job==OBD_JOB_CLEAR)c->clear_status=OBD_DIAG_ERROR;fault(c,"ELM reply timed out; reconnect (no command overlap)");}}
bool obd_fresh(const obd_client *c,unsigned i,uint32_t now){return c&&i<OBD_METRICS&&c->state==OBD_LIVE&&!c->paused&&c->samples[i].valid&&(uint32_t)(now-c->samples[i].at_ms)<=obd_specs[i].interval_ms*3+1500;}
const char *obd_protocol_name(unsigned p){
 static const char*const n[]={"AUTO","J1850 PWM","J1850 VPW","ISO 9141-2","KWP 5-BAUD","KWP FAST","CAN 11/500","CAN 29/500","CAN 11/250","CAN 29/250"};return p<10?n[p]:"UNKNOWN";
}
const char *obd_unit(unsigned i,bool imperial){
 if(i>=OBD_METRICS)return "";
 if(imperial){if(i==OBD_SPEED)return "MPH";if(obd_specs[i].scale==TEMP)return "F";if(i==OBD_MAP||i==OBD_BARO)return "PSI";if(i==OBD_DISTANCE_CLEAR||i==OBD_DISTANCE_MIL)return "MI";}
 if(i==OBD_SPEED)return "KM/H";if(i==OBD_MAP||i==OBD_BARO)return "KPA";if(i==OBD_RPM)return "RPM";if(i==OBD_MAF)return "G/S";return obd_specs[i].unit;
}
int32_t obd_convert(unsigned i,int32_t n,bool imperial){
 if(i>=OBD_METRICS)return n;
 if(imperial){if(i==OBD_SPEED||i==OBD_DISTANCE_CLEAR||i==OBD_DISTANCE_MIL)n=(int32_t)((int64_t)n*1000000/1609344);
 else if(obd_specs[i].scale==TEMP)n=n*9/5+32000;else if(i==OBD_MAP||i==OBD_BARO)n=(int32_t)((int64_t)n*145038/1000000);}
 return n;
}
void obd_value(unsigned i,int32_t n,bool imperial,char*v,size_t nv){
 n=obd_convert(i,n,imperial);
 if(i==OBD_MIL){snprintf(v,nv,"%s / %ld",(n/1000)&128?"ON":"OFF",(long)((n/1000)&127));return;}
 unsigned digits=i==OBD_O2?2:(i==OBD_VOLTS||i==OBD_MAF||i==OBD_RATE||i==OBD_SHORT_TRIM||i==OBD_LONG_TRIM||i==OBD_ADVANCE)?1:0;
 if(digits){bool neg=n<0;uint32_t a=(uint32_t)(neg?-n:n),factor=digits==2?10:100;snprintf(v,nv, digits==2?"%s%lu.%02lu":"%s%lu.%lu",neg?"-":"",(unsigned long)(a/1000),(unsigned long)(a%1000/factor));}
 else snprintf(v,nv,"%ld",(long)(n/1000));
}
void obd_format(const obd_client*c,unsigned i,uint32_t now,bool imperial,char*v,size_t nv,char*u,size_t nu){
 if(!c||i>=OBD_METRICS){copy(v,nv,"--");copy(u,nu,"");return;}
 copy(u,nu,obd_unit(i,imperial));
 if(!c->samples[i].supported){copy(v,nv,"--");copy(u,nu,"unsupported");return;}
 if(!obd_fresh(c,i,now)){copy(v,nv,"--");copy(u,nu,c->paused?"paused":c->samples[i].valid?"stale":"waiting");return;}
 obd_value(i,c->samples[i].milli,imperial,v,nv);
}
#include "diagnostics.inc"
