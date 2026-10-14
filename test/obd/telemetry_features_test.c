/* Real app/model with deterministic BLE; no vehicle or target execution. */
#define main lifecycle_baseline
#include "lifecycle_test.c"
#undef main
static int32_t empty_read(void*c,uint64_t t,uint8_t*b,uint32_t n){(void)c;(void)t;(void)b;(void)n;return 0;}
static void speed_sample(db_model*m,obd_client*c,uint32_t t,int32_t v){
 c->samples[OBD_SPEED]=(obd_sample){.milli=v,.at_ms=t,.supported=true,.valid=true};db_tick(m,c,t);
}
int main(void){
 static db_model m;obd_client c;db_init(&m,0);obd_init(&c,0);c.state=OBD_LIVE;
 assert(m.cfg.imperial==1); /* New sessions default mph; persisted choices still load. */
 speed_sample(&m,&c,1000,0);assert(m.motion[1][1]==DB_NO_VALUE);
 speed_sample(&m,&c,2000,3600);assert(m.motion[0][2]==1000&&m.motion[1][2]==1000&&m.motion[2][2]==DB_NO_VALUE);
 speed_sample(&m,&c,3000,14400);assert(m.motion[1][3]==3000&&m.motion[2][3]==2000);
 /* Uneven interval midpoint derivative: 1s then 0.5s, not a fixed cadence. */
 speed_sample(&m,&c,3500,21600);assert(m.motion[1][3]==4000&&m.motion[2][3]==1333);
 int32_t previous=m.motion[2][3];db_tick(&m,&c,3510);assert(m.motion[2][3]==previous);
 speed_sample(&m,&c,15000,0);assert(m.motion[1][15]==DB_NO_VALUE&&m.motion[2][15]==DB_NO_VALUE);
 c.state=OBD_OFF;db_tick(&m,&c,15100);c.state=OBD_LIVE;speed_sample(&m,&c,16000,3600);assert(m.motion[1][16]==DB_NO_VALUE);
 uint16_t points[151];int32_t lo,hi;db_motion_series(&m,2,30000,30,points,151,&lo,&hi);assert(points[150]==UINT16_MAX);
 db_init(&m,UINT32_MAX-1500);speed_sample(&m,&c,UINT32_MAX-1000,0);speed_sample(&m,&c,UINT32_MAX-500,3600);speed_sample(&m,&c,499,10800);assert(m.motion[1][0]==2000&&m.motion[2][0]==0);
 puts("Motion: mph default, actual sample timing, unequal intervals, duplicate suppression, stale/reconnect gaps and wrap PASS");
 obd_app a;init_app(&a);risc_bluetooth_serial_v1 transport=ble;transport.read=empty_read;a.ble=&transport;
 a.token=radio_token=77;radio_state=RISC_SERIAL_READY;a.reconnecting=false;a.elm_started=true;a.elm.state=OBD_FAULT;a.log_file=123;
 assert(service_link(&a));assert(a.reconnecting&&!a.token&&a.log_file==123&&a.screen==CONNECTING);
 a.log_file=0; /* This fixture doesn't own storage; integration tests cover close. */
 action(&a,ACT_DISCONNECT,0);assert(!a.reconnecting);
 for(unsigned i=0;i<6;i++){a.now=i*6000;assert(service_link(&a));}assert(!opens);
 puts("ELM handshake failure schedules bounded reconnect without ending the recording; explicit cancel prevents retry PASS");
 return 0;
}
