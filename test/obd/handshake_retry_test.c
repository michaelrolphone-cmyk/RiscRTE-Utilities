#define main baseline_lifecycle
#include "lifecycle_test.c"
#undef main
static int32_t empty_rx(void*c,uint64_t t,uint8_t*b,uint32_t n){(void)c;(void)t;(void)b;(void)n;return 0;}
int main(void){
 obd_app a;init_app(&a);risc_bluetooth_serial_v1 transport=ble;transport.read=empty_rx;a.ble=&transport;
 a.token=radio_token=77;radio_state=RISC_SERIAL_READY;a.reconnecting=false;a.elm_started=true;a.elm.state=OBD_FAULT;
 assert(service_link(&a));assert(a.reconnecting&&!a.token&&a.screen==CONNECTING);
 for(unsigned i=1;i<=6;i++){a.now=i*6000;assert(service_link(&a));}
 assert(opens==3&&a.reconnect_attempts==3&&!a.reconnecting&&!a.token&&a.screen==NOTICE);
 puts("Handshake fault retries exactly three times, then stops PASS");return 0;
}
