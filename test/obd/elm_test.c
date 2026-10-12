#include "elm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned now=100;
static void reply(obd_client*c,const char *s){for(unsigned i=0;s[i];i++){uint8_t b=(uint8_t)s[i];obd_receive(c,&b,1,++now);}}
static void command(obd_client*c,const char *q,const char *r){const char *out;now+=200;assert(obd_next(c,now,&out)==strlen(q));assert(!strcmp(q,out));const char *again;assert(obd_next(c,now,&again)==strlen(q)&&again==out);obd_sent(c,now);assert(!obd_next(c,now,&out));reply(c,r);assert(!c->waiting);}
static void init(obd_client*c,unsigned protocol){
 obd_init(c,now);command(c,"ATZ\r","ATZ\r\rELM327 v1.5\r>");command(c,"ATE0\r","ATE0\rOK\r>");command(c,"ATL0\r","OK\r>");command(c,"ATS1\r","OK\r>");command(c,"ATH1\r","OK\r>");command(c,"ATSP0\r","OK\r>");
 const char *r=protocol==6?"SEARCHING...\r7E9 06 41 00 00 00 00 00\r7E8 06 41 00 FF FF FF FF\r>":protocol==7?"18 DA F1 10 06 41 00 FF FF FF FF\r>":"48 6B 10 41 00 FF FF FF FF 00\r>";
 command(c,"0100\r",r);assert(c->have_ecu);assert(c->ecu==(protocol==6?0x7e8:protocol==7?0x18daf110:0x10));
 command(c,"ATDPN\r",protocol==6?"A6\r>":protocol==7?"A7\r>":"A3\r>");
 command(c,"0120\r",protocol==6?"7E8 06 41 20 FF FF FF FF\r>":protocol==7?"18DAF110 06 41 20 FF FF FF FF\r>":"48 6B 10 41 20 FF FF FF FF 00\r>");
 command(c,"0140\r",protocol==6?"7E8 06 41 40 FF FF FF FF\r>":protocol==7?"18DAF110 06 41 40 FF FF FF FF\r>":"48 6B 10 41 40 FF FF FF FF 00\r>");
 const char*out;now+=200;assert(obd_next(c,now,&out));assert(c->state==OBD_LIVE);assert(!strcmp(out,"010C\r"));
}
static void reading(obd_client*c,unsigned metric,const char *r){for(unsigned i=0;i<OBD_METRICS;i++)c->samples[i].due_ms=now+100000;c->prepared=false;c->samples[metric].due_ms=now;c->cursor=metric;char q[16];snprintf(q,sizeof(q),"01%02X\r",obd_specs[metric].pid);command(c,q,r);}
int main(void){
 obd_client c;init(&c,6);reading(&c,OBD_RPM,"7E9 04 41 0C FF FF\r7E8 04 41 0C 1A F8\r>");assert(c.samples[OBD_RPM].milli==1726000&&obd_fresh(&c,OBD_RPM,now));
 reading(&c,OBD_SPEED,"7E8 03 41 0D 64\r>");assert(c.samples[OBD_SPEED].milli==100000);
 reading(&c,OBD_COOLANT,"7E8 03 41 05 82\r>");assert(c.samples[OBD_COOLANT].milli==90000);
 reading(&c,OBD_LOAD,"7E8 03 41 04 FF\r>");assert(c.samples[OBD_LOAD].milli==100000);
 reading(&c,OBD_VOLTS,"7E8 04 41 42 36 B0\r>");assert(c.samples[OBD_VOLTS].milli==14000);
 reading(&c,OBD_MAF,"7E8 04 41 10 01 F4\r>");assert(c.samples[OBD_MAF].milli==5000);
 reading(&c,OBD_SHORT_TRIM,"7E8 03 41 06 40\r>");assert(c.samples[OBD_SHORT_TRIM].milli==-50000);
 reading(&c,OBD_RUNTIME,"7E8 04 41 1F 0E 10\r>");assert(c.samples[OBD_RUNTIME].milli==3600000);
 reading(&c,OBD_MIL,"7E8 06 41 01 83 00 00 00\r>");assert(c.samples[OBD_MIL].milli==131000);
 reading(&c,OBD_RATE,"7E8 04 41 5E 00 64\r>");assert(c.samples[OBD_RATE].milli==5000);
 reading(&c,OBD_THROTTLE,"7E8 03 41 11 FF\r>");assert(c.samples[OBD_THROTTLE].milli==100000);
 reading(&c,OBD_FUEL,"7E8 03 41 2F 00\r>");assert(c.samples[OBD_FUEL].milli==0);
 reading(&c,OBD_INTAKE,"7E8 03 41 0F 00\r>");assert(c.samples[OBD_INTAKE].milli==-40000);
 reading(&c,OBD_MAP,"7E8 03 41 0B 64\r>");assert(c.samples[OBD_MAP].milli==100000);
 reading(&c,OBD_LONG_TRIM,"7E8 03 41 07 80\r>");assert(c.samples[OBD_LONG_TRIM].milli==0);
 reading(&c,OBD_OIL,"7E8 03 41 5C 8C\r>");assert(c.samples[OBD_OIL].milli==100000);
 char v[32],u[32];reading(&c,OBD_SPEED,"7E8 03 41 0D 64\r>");obd_format(&c,OBD_SPEED,now,1,v,sizeof(v),u,sizeof(u));assert(!strcmp(v,"62")&&!strcmp(u,"MPH"));
 reading(&c,OBD_COOLANT,"7E8 03 41 05 82\r>");obd_format(&c,OBD_COOLANT,now,1,v,sizeof(v),u,sizeof(u));assert(!strcmp(v,"194"));
 reading(&c,OBD_RPM,"010C\r7E8 04 41 0D 1A F8\r>");assert(!c.samples[OBD_RPM].valid&&c.bad_lines);
 reading(&c,OBD_RPM,"7E8 07 41 0C 1A F8\r>");assert(!c.samples[OBD_RPM].valid);
 reading(&c,OBD_RPM,"7E8 03 7F 01 12\r>");assert(c.negative&&!c.samples[OBD_RPM].valid);
 reading(&c,OBD_RPM,"NO DATA\r>");assert(!c.samples[OBD_RPM].valid);
 reading(&c,OBD_RPM,"7E8 04 41 0C 00 00\r>");assert(c.samples[OBD_RPM].valid&&c.samples[OBD_RPM].milli==0);
 assert(!obd_fresh(&c,OBD_RPM,now+10000));c.paused=1;assert(!obd_fresh(&c,OBD_RPM,now));c.paused=0;
 init(&c,7);reading(&c,OBD_RPM,"18DAF110 04 41 0C 1A F8\r>");assert(c.samples[OBD_RPM].milli==1726000);
 init(&c,3);reading(&c,OBD_RPM,"48 6B 10 41 0C 1A F8 00\r>");assert(c.samples[OBD_RPM].milli==1726000);
 obd_init(&c,now);command(&c,"ATZ\r","?\r>");assert(c.state==OBD_FAULT);
 now=UINT32_MAX-10;obd_init(&c,now);const char*out;assert(obd_next(&c,now,&out));obd_sent(&c,now);obd_tick(&c,now+4999);assert(c.waiting);obd_tick(&c,now+5000);assert(c.state==OBD_FAULT&&!obd_next(&c,now+6000,&out));reply(&c,"ELM327\r>");assert(c.state==OBD_FAULT);
 now=100;obd_init(&c,now);assert(obd_next(&c,now,&out));obd_sent(&c,now);for(unsigned i=0;i<200;i++)reply(&c,"x");assert(c.state==OBD_FAULT);
 init(&c,6);c.mask[0]=0x00100000; c.mask[1]=c.mask[2]=0;c.stage=10;c.state=OBD_DISCOVERING;c.prepared=0;for(unsigned k=0;k<200;k++){now+=1000;assert(obd_next(&c,now,&out));assert(!strcmp(out,"010C\r"));obd_sent(&c,now);reply(&c,"7E8 04 41 0C 1A F8\r>");}assert(c.samples[OBD_RPM].supported&&!c.samples[OBD_SPEED].supported);
 obd_stop(&c,"disconnected");assert(!obd_fresh(&c,OBD_RPM,now));
 puts("ELM init/fragmentation/multi-ECU/header/protocol/16-PID math/units/malformed/timeout-wrap/stale/read-only schedule PASS");
 return 0;
}
