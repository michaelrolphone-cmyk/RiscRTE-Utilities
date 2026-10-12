/* Readiness regressions for the selected OBD 0.2.x protocol source. */
#define main original_protocol_tests
#include "elm_test.c"
#undef main
static void late_reply(void){
 obd_client c;const char*q;
 for(unsigned wrap=0;wrap<2;wrap++){
  now=wrap?UINT32_MAX-1000:100;obd_init(&c,now);
  assert(obd_next(&c,now,&q));obd_sent(&c,now);
  const uint8_t r[]="ELM327 v1.5\r>";
  obd_receive(&c,r,sizeof(r)-1,now+5000);
  assert(c.state==OBD_FAULT&&!c.waiting&&c.replies==0);
 }
 now=100;init(&c,6);c.prepared=false;c.last_sent_ms=now-200;
 assert(obd_next(&c,now,&q));obd_sent(&c,now);
 const uint8_t r[]="7E8 04 41 0C 1A F8\r>";
 obd_receive(&c,r,sizeof(r)-1,now+5001);
 assert(c.state==OBD_FAULT&&!c.samples[OBD_RPM].valid);
 puts("Receive-before-tick late setup/PID replies and clock wrap rejected PASS");
}
static void discovery_and_frames(void){
 obd_client c;const char*q;now=100;obd_init(&c,now);
 command(&c,"ATZ\r","ELM327 v1.5\r>");
 command(&c,"ATE0\r","OK\r>");command(&c,"ATL0\r","OK\r>");
 command(&c,"ATS1\r","OK\r>");command(&c,"ATH1\r","OK\r>");command(&c,"ATSP0\r","OK\r>");
 /* Broadcast discovery must select a positive ECU, not reject it because a
  * different module reports that it cannot answer Mode 01. */
 command(&c,"0100\r","7E9 03 7F 01 11\r7E8 06 41 00 00 10 00 00\r>");
 assert(c.state!=OBD_FAULT&&c.have_ecu&&c.ecu==0x7e8);
 command(&c,"ATDPN\r","A6\r>");now+=200;assert(obd_next(&c,now,&q));
 assert(c.state==OBD_LIVE&&!strcmp(q,"010C\r"));
 assert(c.samples[OBD_RPM].supported&&!c.samples[OBD_SPEED].supported);
 reading(&c,OBD_RPM,"7E8 04 41 0C 1A F8 00 00 00 00\r>");
 assert(!c.samples[OBD_RPM].valid); /* Nine CAN bytes are never a frame. */
 reading(&c,OBD_RPM,"7E8 04 41 0C 1A F8 00 00 00\r>");
 assert(c.samples[OBD_RPM].valid&&c.samples[OBD_RPM].milli==1726000);
 puts("Mixed ECU discovery, no-continuation mask and CAN frame bounds PASS");
}
int main(void){late_reply();discovery_and_frames();return 0;}
