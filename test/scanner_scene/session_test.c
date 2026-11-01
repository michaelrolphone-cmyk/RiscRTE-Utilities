#include "scanner_session.h"
#include <assert.h>
#include <stdio.h>
static risc_runtime_api_v1 rt,other;static const risc_runtime_api_v1*current;
static scanner_session s;static unsigned calls,retains,closes,releases,mode;static bool present=true;static unsigned polls,statuses,devices,opens;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){(void)v;return current;}
static bool live(void*p){(void)p;return current==&rt;}
static void retained(void*p){(void)p;retains++;}
static bool open_sensor(void*p,uint64_t*t){(void)p;calls++;opens++;*t=7;if(mode==15){*t=0;return false;}if(mode==34)current=&other;if(mode==2)return false;if(mode==3)*t=0;return true;}
static bool poll_sensor(void*p,uint64_t t,uint32_t n){(void)p;assert(t==7&&n==6);calls++;polls++;if(mode==4)current=&other;if(mode==5)scanner_session_stop(&s);return mode!=6;}
static bool status_sensor(void*p,risc_ble_sensor_status_v1*v){(void)p;calls++;statuses++;if(mode==25)current=&other;if(mode==38)scanner_session_stop(&s);if(mode==26)return false;if(mode==22)v->struct_size=0;if(mode==24)memset(v->error,1,sizeof(v->error));v->state=mode==7?RISC_BLE_SENSOR_COMPLETE:RISC_BLE_SENSOR_SCANNING;if(mode==23)v->state=99;v->count=1;if(mode==8)v->count=33;if(mode==9)v->cleanup_pending=true;if(mode==10)v->restore_failed=true;return true;}
static bool device_sensor(void*p,uint32_t i,risc_ble_sensor_device_v1*d){(void)p;assert(i==0);calls++;devices++;if(mode==33)current=&other;if(mode==32)return false;d->address_type=1;d->reading_count=12;d->payload_size=31;if(mode==11)d->reading_count=13;if(mode==28)d->service_count=9;if(mode==29)d->payload_size=32;if(mode==30)d->address_type=4;if(mode==31)memset(d->name,1,sizeof(d->name));return true;}
static bool close_sensor(void*p,uint64_t t){(void)p;assert(t==7);calls++;closes++;if(mode==27)current=&other;if(mode==39)scanner_session_stop(&s);return mode!=12;}
static risc_bluetooth_sensors_v1 api={1,sizeof(api),NULL,open_sensor,poll_sensor,status_sensor,device_sensor,close_sensor};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){assert(!strcmp(n,RISC_BLUETOOTH_SENSORS_CAPABILITY)&&v==1&&!i);calls++;if(mode==35)current=&other;if(!present){if(mode==1)g->slot=1;return false;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.api=&api,.slot=1,.generation=1};if(mode==16)g->struct_size=0;if(mode==17)g->api=NULL;if(mode==18)g->slot=0;if(mode==19)g->generation=0;return true;}
static bool release(risc_runtime_capability_v1*g){calls++;releases++;if(mode==36)current=&other;if(mode==40)scanner_session_stop(&s);*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};if(mode==13)g->struct_size=0;if(mode==37)g->slot=1;return mode!=14;}
static void reset(void){calls=retains=closes=releases=mode=polls=statuses=devices=opens=0;api.api_version=1;api.struct_size=sizeof(api);api.poll=poll_sensor;present=true;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};current=&rt;scanner_session_init(&s,&rt,live,retained,NULL);}
static void frozen(void){unsigned n=calls;assert(!scanner_session_step(&s,100));assert(!scanner_session_stop(&s));assert(!scanner_session_start(&s));assert(calls==n&&retains==1);}
int main(void){unsigned cases=0;
 reset();present=false;assert(!scanner_session_start(&s)&&!s.terminal&&!releases);cases++;
 reset();present=false;mode=1;assert(!scanner_session_start(&s));frozen();cases++;
 reset();mode=2;assert(!scanner_session_start(&s)&&closes==1&&releases==1&&!s.terminal);cases++;
 reset();mode=3;assert(!scanner_session_start(&s));frozen();cases++;
 reset();assert(scanner_session_start(&s));for(unsigned i=0;i<100;i++)assert(scanner_session_step(&s,i*10));assert(!closes&&!releases&&s.status.count==1);assert(scanner_session_stop(&s)&&closes==1&&releases==1);cases++;
 for(unsigned m=4;m<=5;m++){reset();assert(scanner_session_start(&s));mode=m;assert(!scanner_session_step(&s,10));frozen();cases++;}
 reset();assert(scanner_session_start(&s));mode=6;assert(scanner_session_step(&s,10));assert(s.status.state==RISC_BLE_SENSOR_ERROR&&closes==1&&releases==1);cases++;
 reset();assert(scanner_session_start(&s));mode=7;assert(scanner_session_step(&s,10)&&closes==1&&releases==1);cases++;
 for(unsigned m=8;m<=9;m++){reset();assert(scanner_session_start(&s));mode=m;assert(!scanner_session_step(&s,10));frozen();cases++;}
 reset();assert(scanner_session_start(&s));mode=10;assert(scanner_session_stop(&s)&&s.restore_failed);cases++;
 reset();assert(scanner_session_start(&s));mode=11;assert(!scanner_session_step(&s,10));frozen();cases++;
 reset();assert(scanner_session_start(&s));mode=12;assert(!scanner_session_stop(&s)&&s.cleanup_pending&&!s.terminal);unsigned n=calls;assert(!scanner_session_step(&s,20)&&calls==n+1);mode=0;assert(scanner_session_stop(&s)&&!s.cleanup_pending&&releases==1);cases++;
 for(unsigned m=13;m<=14;m++){reset();assert(scanner_session_start(&s));mode=m;assert(!scanner_session_stop(&s));frozen();cases++;}

 /* Clean failed open has no close token; dirty acquired grants never invoke provider. */
 reset();mode=15;assert(!scanner_session_start(&s)&&!s.terminal&&opens==1&&!closes&&releases==1);cases++;
 for(unsigned m=16;m<=19;m++){reset();mode=m;assert(!scanner_session_start(&s)&&!opens&&!releases);frozen();cases++;}
 /* Unsupported table is cleanly released without open or close. */
 for(unsigned m=0;m<3;m++){reset();if(m==0)api.api_version=2;if(m==1)api.struct_size=0;if(m==2)api.poll=NULL;assert(!scanner_session_start(&s)&&!s.terminal&&!opens&&!closes&&releases==1);cases++;}
 /* Malformed status and callback identity/reentry fence all later I/O. */
 for(unsigned m=22;m<=26;m++){reset();assert(scanner_session_start(&s));mode=m;assert(!scanner_session_step(&s,1)&&!devices&&!closes&&!releases);frozen();cases++;}
 for(unsigned m=28;m<=33;m++){reset();assert(scanner_session_start(&s));mode=m;assert(!scanner_session_step(&s,1)&&!closes&&!releases);frozen();cases++;}
 for(unsigned m=34;m<=35;m++){reset();mode=m;assert(!scanner_session_start(&s)&&!closes&&!releases);frozen();cases++;}
 reset();assert(scanner_session_start(&s));mode=27;assert(!scanner_session_stop(&s)&&closes==1&&!statuses&&!releases);frozen();cases++;
 for(unsigned m=36;m<=37;m++){reset();assert(scanner_session_start(&s));mode=m;assert(!scanner_session_stop(&s)&&closes==1&&statuses==1&&releases==1);frozen();cases++;}
 for(unsigned m=38;m<=40;m++){reset();assert(scanner_session_start(&s));mode=m;if(m==38)assert(!scanner_session_step(&s,1));else assert(!scanner_session_stop(&s));frozen();cases++;}
 /* Pending cleanup exclusively retries the same close; no poll/status/release. */
 reset();assert(scanner_session_start(&s));mode=12;assert(!scanner_session_stop(&s));
 for(unsigned i=0;i<8;i++){assert(!scanner_session_step(&s,i));assert(s.token==7&&s.grant.api==&api&&!polls&&!statuses&&!releases&&!s.terminal);}
 assert(!scanner_session_start(&s)&&opens==1&&!polls&&!statuses&&!releases);
 mode=0;assert(scanner_session_step(&s,100)&&!s.cleanup_pending&&!s.token&&statuses==1&&releases==1);cases++;
 printf("%u scanner session cases PASS\n",cases);return 0;
}
