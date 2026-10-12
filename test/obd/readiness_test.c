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
int main(void){late_reply();return 0;}
