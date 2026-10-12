#define main original_protocol_tests
#include "elm_test.c"
#undef main
#include "dashboard.h"
static void diagnostic_ready(obd_client*c,unsigned protocol){init(c,protocol);c->prepared=false;c->poll_mask=0;}
static void healthy_zero(obd_client*c){c->samples[OBD_RPM]=(obd_sample){.supported=1,.valid=1,.milli=0,.at_ms=now};c->samples[OBD_SPEED]=c->samples[OBD_RPM];}
static void diagnostics(void){
 obd_client c;diagnostic_ready(&c,6);obd_request_info(&c);
 command(&c,"0902\r","7E9 10 14 49 02 01 58 58 58\r7E8 10 14 49 02 01 31 4D 38\r7E8 21 47 44 4D 39 41 58 4B\r7E8 22 50 30 34 32 37 38 38\r>");
 assert(c.vin_status==OBD_DIAG_VALID&&!strcmp(c.vin,"1M8GDM9AXKP042788"));
 command(&c,"ATRV\r","14.10V\r>");assert(c.adapter_mv==14100);
 obd_request_faults(&c);command(&c,"03\r","7E8 06 43 02 04 20 01 28\r>");
 assert(c.codes[0].count==2&&c.codes[0].codes[0]==0x420&&c.codes[0].codes[1]==0x128);
 command(&c,"07\r","7E8 04 47 01 01 71\r>");assert(c.codes[1].codes[0]==0x171);
 command(&c,"0A\r","7E8 04 4A 01 04 20\r>");assert(c.codes[2].codes[0]==0x420);
 char code[6];obd_code_text(0x420,code);assert(!strcmp(code,"P0420"));obd_code_text(0xc123,code);assert(!strcmp(code,"U0123"));
 /* No fabricated per-code freeze frame: only use frame zero for its owning DTC. */
 obd_request_freeze(&c,0x128);command(&c,"020200\r","7E8 05 42 02 00 04 20\r>");assert(c.freeze_status==OBD_DIAG_UNAVAILABLE&&!c.jobs);
 obd_request_freeze(&c,0x420);command(&c,"020200\r","7E8 05 42 02 00 04 20\r>");
 command(&c,"020D00\r","7E8 04 42 0D 00 58\r>");
 command(&c,"020C00\r","7E8 05 42 0C 00 26 48\r>");
 command(&c,"020500\r","7E8 04 42 05 00 83\r>");
 command(&c,"020400\r","NO DATA\r>");
 command(&c,"020B00\r","7E8 04 42 0B 00 37\r>");
 command(&c,"020600\r","7E8 04 42 06 00 83\r>");
 command(&c,"020700\r","7E8 04 42 07 00 85\r>");
 command(&c,"020F00\r","7E8 04 42 0F 00 47\r>");
 assert(c.freeze_status==OBD_DIAG_VALID&&c.freeze[OBD_SPEED].milli==88000&&c.freeze[OBD_RPM].milli==2450000&&!c.freeze[OBD_LOAD].valid);
 healthy_zero(&c);assert(obd_confirm_clear(&c,now));assert(!obd_confirm_clear(&c,now));
 command(&c,"04\r","7E8 01 44\r>");assert(c.clear_status==OBD_DIAG_VALID&&!c.readiness_valid&&!c.samples[OBD_MIL].valid);
 assert(c.codes[2].codes[0]==0x420);assert(!c.clear_armed); /* Permanent codes not invented as cleared. */
 command(&c,"03\r","7E8 02 43 00\r>");command(&c,"07\r","7E8 02 47 00\r>");command(&c,"0A\r","7E8 04 4A 01 04 20\r>");
 assert(c.codes[0].count==0&&c.codes[1].count==0&&c.codes[2].count==1);
 healthy_zero(&c);c.samples[OBD_SPEED].milli=1000;assert(!obd_confirm_clear(&c,now));
 healthy_zero(&c);c.samples[OBD_RPM].milli=800000;assert(!obd_confirm_clear(&c,now));
 healthy_zero(&c);assert(!obd_confirm_clear(&c,now+1600));assert(obd_confirm_clear(&c,now));
 const char*q=NULL;assert(!obd_next(&c,now+1600,&q));assert(!c.clear_armed&&c.clear_status==OBD_DIAG_ERROR);
 healthy_zero(&c);assert(obd_confirm_clear(&c,now));c.ecu=0x7e9;assert(!obd_next(&c,now+400,&q));assert(!c.clear_armed);c.ecu=0x7e8;
 healthy_zero(&c);assert(obd_confirm_clear(&c,now));command(&c,"04\r","7E8 03 7F 04 22\r>");assert(c.clear_status!=OBD_DIAG_VALID);
 healthy_zero(&c);c.last_sent_ms=now-1000;assert(obd_confirm_clear(&c,now));
 assert(obd_next(&c,now,&q)==3&&!strcmp(q,"04\r"));assert(c.prepared);
 assert(!obd_next(&c,now+1600,&q)&&!c.prepared&&c.clear_status==OBD_DIAG_ERROR); /* blocked BLE write expires before emission */
 diagnostic_ready(&c,6);obd_request_info(&c);
 command(&c,"0902\r","7E8 10 14 49 02 01 31 4D 38\r7E8 22 47 44 4D 39 41 58 4B\r>");assert(c.vin_status==OBD_DIAG_ERROR&&!c.vin[0]);
 command(&c,"ATRV\r","?\r>");assert(c.voltage_status==OBD_DIAG_UNAVAILABLE);
 obd_request_faults(&c);command(&c,"03\r","7E8 04 43 03 04 20\r>");assert(c.codes[0].status==OBD_DIAG_ERROR);
 command(&c,"07\r","7E9 04 47 01 01 71\r>");assert(c.codes[1].status==OBD_DIAG_ERROR);
 command(&c,"0A\r","NO DATA\r>");assert(c.codes[2].status==OBD_DIAG_UNAVAILABLE);
 diagnostic_ready(&c,7);obd_request_info(&c);
 command(&c,"0902\r","18DAF110 10 14 49 02 01 31 4D 38\r18DAF110 21 47 44 4D 39 41 58 4B\r18DAF110 22 50 30 34 32 37 38 38\r>");assert(c.vin_status==OBD_DIAG_VALID);
 command(&c,"ATRV\r","12.4V\r>");assert(c.adapter_mv==12400);
 diagnostic_ready(&c,3);obd_request_info(&c);
 command(&c,"0902\r","48 6B 10 49 02 01 00 00 00 31 00\r48 6B 10 49 02 02 4D 38 47 44 00\r48 6B 10 49 02 03 4D 39 41 58 00\r48 6B 10 49 02 04 4B 50 30 34 00\r48 6B 10 49 02 05 32 37 38 38 00\r>");assert(c.vin_status==OBD_DIAG_VALID&&!strcmp(c.vin,"1M8GDM9AXKP042788"));
 command(&c,"ATRV\r","12.4V\r>");obd_request_faults(&c);
 command(&c,"03\r","48 6B 10 43 04 20 01 28 00 00 00\r>");assert(c.codes[0].count==2);
 command(&c,"07\r","48 6B 10 47 01 71 00 00 00 00 00\r>");command(&c,"0A\r","NO DATA\r>");
 diagnostic_ready(&c,6);c.poll_mask=(1u<<OBD_METRICS)-1;
 reading(&c,OBD_ADVANCE,"7E8 03 41 0E 7C\r>");assert(c.samples[OBD_ADVANCE].milli==-2000);
 reading(&c,OBD_O2,"7E8 04 41 15 64 FF\r>");assert(c.samples[OBD_O2].milli==500);
 reading(&c,OBD_BARO,"7E8 03 41 33 65\r>");assert(c.samples[OBD_BARO].milli==101000);
 reading(&c,OBD_STANDARD,"7E8 03 41 1C 01\r>");reading(&c,OBD_FUEL_TYPE,"7E8 03 41 51 01\r>");
 reading(&c,OBD_DISTANCE_CLEAR,"7E8 04 41 31 01 1C\r>");assert(c.samples[OBD_DISTANCE_CLEAR].milli==284000);
 reading(&c,OBD_DISTANCE_MIL,"7E8 04 41 21 00 D4\r>");assert(c.samples[OBD_DISTANCE_MIL].milli==212000);
 puts("Diagnostics: VIN CAN11/CAN29/legacy, DTC lists, owned freeze frame, malformed transport, real log, confirmed/expired/moving/ECU-bound clear, extra PIDs PASS");
}
static void model_tests(void){
 static db_model m;obd_client c;db_init(&m,0);obd_init(&c,0);c.state=OBD_LIVE;assert(db_config_valid(&m.cfg));
 c.samples[OBD_RPM]=(obd_sample){.supported=1,.valid=1,.milli=1200000};
 for(unsigned i=1;i<=605;i++){uint32_t t=i*1000;c.samples[OBD_RPM].at_ms=t;c.samples[OBD_RPM].milli=(int32_t)i*1000;db_tick(&m,&c,t);}
 int32_t lo,hi,av;assert(db_stats(&m,OBD_RPM,605000,30,false,&lo,&hi,&av)&&lo==575000&&hi==605000&&av==590000);
 uint16_t points[151];db_series(&m,OBD_RPM,605000,600,false,0,8000000,points,151);assert(points[150]!=UINT16_MAX);
 db_freeze(&m,605000);int32_t last=0;assert(db_last(&m,OBD_RPM,605000,true,&last)&&last==605000);c.samples[OBD_RPM].at_ms=606000;c.samples[OBD_RPM].milli=6500000;db_tick(&m,&c,606000);
 assert(db_last(&m,OBD_RPM,606000,true,&last)&&last==605000);assert(db_stats(&m,OBD_RPM,606000,30,true,&lo,&hi,&av)&&hi==605000);assert(db_stats(&m,OBD_RPM,606000,30,false,&lo,&hi,&av)&&hi==6500000);
 db_series(&m,OBD_RPM,620000,30,false,0,8000000,points,151);assert(points[150]==UINT16_MAX);
 db_init(&m,0);c.samples[OBD_COOLANT]=(obd_sample){.supported=1,.valid=1,.milli=110000,.at_ms=100};c.samples[OBD_RPM].milli=1000000;c.samples[OBD_RPM].at_ms=100;
 for(unsigned i=0;i<300;i++)db_tick(&m,&c,100+i);assert(!m.alert_visible); /* Loop count is not sample count. */
 c.samples[OBD_RPM].at_ms=c.samples[OBD_COOLANT].at_ms=1800;db_tick(&m,&c,1800);assert(m.alert_visible&&m.alert.metric==OBD_COOLANT);
 db_ack(&m,1800,true);assert(!m.alert_visible&&m.muted[OBD_COOLANT]);
 c.samples[OBD_RPM].at_ms=c.samples[OBD_COOLANT].at_ms=1900;c.samples[OBD_COOLANT].milli=90000;db_tick(&m,&c,1900);
 c.samples[OBD_RPM].at_ms=c.samples[OBD_COOLANT].at_ms=3000;c.samples[OBD_COOLANT].milli=110000;db_tick(&m,&c,3000);
 c.samples[OBD_RPM].at_ms=c.samples[OBD_COOLANT].at_ms=5000;db_tick(&m,&c,5000);assert(!m.alert_visible);
 c.samples[OBD_RPM].at_ms=c.samples[OBD_COOLANT].at_ms=302000;db_tick(&m,&c,302000);assert(m.alert_visible);
 db_config corrupt=m.cfg;corrupt.dash[0]=corrupt.dash[1];assert(!db_config_valid(&corrupt));corrupt=m.cfg;corrupt.limits[OBD_VOLTS].low=20000;assert(!db_config_valid(&corrupt));
 assert(db_crc("123456789",9)==0xcbf43926u);assert(db_poll_mask(&m,OBD_O2)&(1u<<OBD_O2));
 puts("Model: 10-minute timestamped ring, gaps, frozen graph, statistics, new-sample threshold debounce, mute, configuration bounds and CRC PASS");
}
int main(void){original_protocol_tests();diagnostics();model_tests();return 0;}
