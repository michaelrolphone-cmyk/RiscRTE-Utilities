/* Actual Battery UI, common adapter and singleton broadcast service. */
#define main unused_capture_main
#define risc_runtime_get_api unused_runtime_api
#include "nova_peripherals.h"
#undef risc_runtime_get_api
#undef main
#include "TelemetryBroadcastV1.h"
#include "RiscBluetoothTelemetryV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscProviderV2.h"
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static const telemetry_broadcast_v1 *broadcast;
static uint64_t live,serial;
static unsigned publications,radio_polls,radio_closes,enable_writes;
static bool fail_save;
static uint64_t ms(void *c){(void)c;return ticks;}
static int32_t bound_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){assert(!frames);return fake_get(c,k,b,cap,n);}
static int32_t bound_put(void*c,const char*k,const void*b,uint32_t n){assert(!frames&&!strcmp(k,TELEMETRY_BROADCAST_KEY));enable_writes++;return fail_save?RISC_KEY_VALUE_IO:fake_put(c,k,b,n);}
static const risc_key_value_v1 bound={1,sizeof(bound),NULL,bound_get,bound_put};
static const risc_platform_clock_api_v1 clk={.api_version=1,.struct_size=sizeof(clk),.monotonic_ms=ms};
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;assert(!frames);static const risc_telemetry_field_v1 f[]={{3,RISC_TELEMETRY_BATTERY_PERCENT,"Battery"},{8,RISC_TELEMETRY_VOLTAGE_MV,"Voltage"},{4,RISC_TELEMETRY_CHARGING,"Charging"}};if(i>=3)return 0;*out=f[i];return 1;}
static int32_t reading(void*c,uint32_t id,int32_t*out){(void)c;assert(!frames);*out=id==3?73:id==8?3970:1;return 1;}
static bool publish(void*c,const uint32_t*ids,uint32_t n,bool open,uint64_t*out){(void)c;assert(!frames&&!live&&n==3&&open&&ids[0]==3&&ids[1]==8&&ids[2]==4);live=++serial;*out=live;publications++;return true;}
static bool poll_radio(void*c,uint64_t t,uint32_t n){(void)c;assert(!frames&&t==live&&n==4);radio_polls++;return true;}
static bool radio_status(void*c,risc_ble_telemetry_status_v1*out){(void)c;*out=(risc_ble_telemetry_status_v1){.struct_size=sizeof(*out),.state=live?RISC_BLE_TELEMETRY_PUBLISHING:RISC_BLE_TELEMETRY_OFF,.updates=1};return true;}
static bool radio_close(void*c,uint64_t t){(void)c;assert(!frames&&t==live&&live);live=0;radio_closes++;return true;}
static const risc_bluetooth_telemetry_v1 radio={1,sizeof(radio),NULL,enumerate,reading,publish,poll_radio,radio_status,radio_close};
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(v==1&&id==1);g->api=&bound;grants++;return true;}
 if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(v==1&&id==0);g->api=broadcast;grants++;return true;}
 return fake_acquire(n,v,id,g);
}
static const risc_runtime_api_v1 test_runtime={1,sizeof(test_runtime),fake_health,fake_yield,fake_diag,fake_launch,test_acquire,fake_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&test_runtime:NULL;}
#include NOVA_APP_SOURCE
static void tap(unsigned at,int x,int y){actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
int main(int argc,char **argv){
 assert(argc==3);directory=argv[1];unsigned scenario=(unsigned)atoi(argv[2]);memset(pixels,0xa5,sizeof(pixels));
 const risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clk},{RISC_BLUETOOTH_TELEMETRY_CAPABILITY,1,&radio}};
 const risc_driver_v2 *driver=t5_driver_get(2);broadcast=driver->capability;assert(driver->start(deps,2));
 uint8_t policy[]={0x51,1,3,0xa6};fake_put(NULL,"quick_radio",policy,4);
 stop_poll=300;
 tap(10,180,212); /* overview BLE */
 if(scenario==0){tap(30,120,212);tap(50,120,212);tap(70,30,26);tap(90,30,26);} /* Off, On, Back, Back */
 else if(scenario==1){tap(30,30,26);tap(50,30,26);} /* healthy switch while live */
 else if(scenario==2){fail_save=true;tap(30,120,212);tap(60,30,26);tap(80,30,26);}
 else {tap(30,30,26);tap(50,30,26);}
 assert(app_module_init()==0);app_main();app_module_fini();assert(!grants&&!frames&&!subs);
 telemetry_broadcast_status_v1 view={.struct_size=sizeof(view)};assert(broadcast->status(NULL,&view));
 if(scenario==0)assert(enable_writes==2&&view.enabled&&view.settings_valid&&publications==2&&live);
 if(scenario==1){
  assert(!enable_writes&&live&&publications==1);uint64_t before=live;
  stop_poll=polls+100;action_count=0;tap(polls+20,30,26);
  assert(app_module_init()==0);app_main();app_module_fini();assert(!grants&&!frames&&!subs&&live==before&&publications==1);
 }
 if(scenario==2)assert(enable_writes==1&&!view.enabled&&!view.settings_valid&&!live&&power_broadcast_error);
#ifdef PORTABLE_BLE_FOREGROUND
 assert(!live&&!publications&&!radio_polls);
#else
 if(scenario!=2)assert(live&&radio_polls>1);
#endif
 assert(driver->quiesce()&&!live);driver->stop();
 printf("Battery BLE production UI/adapter/service case%u: %u publishes, %u polls, %u closes, %u writes; state, nested Back, preservation and cleanup PASS\n",scenario,publications,radio_polls,radio_closes,enable_writes);
}
