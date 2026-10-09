#include "TelemetryBroadcastV1.h"
#include "RiscBluetoothTelemetryV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscProviderV2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static const risc_driver_v2 *driver;
static const telemetry_broadcast_v1 *api;
static telemetry_broadcast_policy_v1 policy;
static uint64_t now,live,next_token;
static unsigned reads,enumerations,publishes,polls,closes;
static bool close_fail,publish_fail,poll_fail,unavailable,malformed,many;
static uint32_t publish_ids[16],publish_count;
static uint64_t ms(void*c){(void)c;return now;}
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*out) {
 (void)c;enumerations++;
 static const risc_telemetry_field_v1 fields[]={{19,RISC_TELEMETRY_TEMPERATURE_CENTIC,"Temperature"},{41,RISC_TELEMETRY_CHARGING,"Charging"},{7,RISC_TELEMETRY_BATTERY_PERCENT,"Battery"},{99,RISC_TELEMETRY_VOLTAGE_MV,"Voltage"}};
 if(many){if(i>=16)return 0;*out=(risc_telemetry_field_v1){.id=i+1,.metric=i<7?i+1:2,.label="Field"};return 1;}
 if(i>=4)return malformed?1:0;
 *out=fields[i];return 1;
}
static int32_t read_value(void*c,uint32_t id,int32_t*out) {
 (void)c;reads++;if(unavailable&&id==7)return 0;
 *out=0;return 1;
}
static bool publish(void*c,const uint32_t *ids,uint32_t count,bool public_broadcast,uint64_t*out) {
 (void)c;assert(!live&&public_broadcast&&count>=3&&count<=16);publishes++;
 memcpy(publish_ids,ids,count*sizeof(*ids));publish_count=count;live=++next_token;*out=live;return !publish_fail;
}
static bool poll(void*c,uint64_t token,uint32_t limit){(void)c;assert(token==live&&live&&limit==4);polls++;return !poll_fail;}
static bool status(void*c,risc_ble_telemetry_status_v1*out){(void)c;assert(out->struct_size==sizeof(*out));*out=(risc_ble_telemetry_status_v1){.struct_size=sizeof(*out),.state=live?RISC_BLE_TELEMETRY_PUBLISHING:RISC_BLE_TELEMETRY_OFF,.cleanup_pending=live!=0,.updates=polls};return true;}
static bool close_radio(void*c,uint64_t token){(void)c;assert(live&&token==live);closes++;if(close_fail)return false;live=0;return true;}
static const risc_platform_clock_api_v1 clock_api={.api_version=1,.struct_size=sizeof(clock_api),.monotonic_ms=ms};
static const risc_bluetooth_telemetry_v1 radio={1,sizeof(radio),NULL,enumerate,read_value,publish,poll,status,close_radio};
static const risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock_api},{RISC_BLUETOOTH_TELEMETRY_CAPABILITY,1,&radio}};
static telemetry_broadcast_status_v1 view(void){telemetry_broadcast_status_v1 v={.struct_size=sizeof(v)};assert(api->status(NULL,&v));return v;}
static bool step(bool allow){return api->step(NULL,allow,&policy);}
static void reset(void) {
 if(driver){close_fail=false;assert(driver->quiesce());driver->stop();}
 reads=enumerations=publishes=polls=closes=0;now=live=next_token=0;close_fail=publish_fail=poll_fail=unavailable=malformed=many=false;
 driver=t5_driver_get(2);assert(driver&&!t5_driver_get(1));api=driver->capability;
 policy=(telemetry_broadcast_policy_v1){.struct_size=sizeof(policy),.enabled=true,.settings_valid=true};
 assert(!driver->start(deps,1));assert(driver->start(deps,2));assert(!driver->start(deps,2));assert(!live&&!reads&&!publishes);
}
int main(void) {
 reset();assert(step(true));assert(!live&&!publishes&&view().enabled&&view().state==TELEMETRY_BROADCAST_BLE_OFF);
 policy.radios_allowed=true;assert(step(true)&&live&&publish_count==4&&publish_ids[0]==19&&publish_ids[1]==41&&publish_ids[2]==7&&publish_ids[3]==99&&view().state==TELEMETRY_BROADCAST_LIVE);
 uint64_t first=live;for(unsigned i=0;i<50;i++){now+=50;assert(step(true)&&live==first);}assert(publishes==1);
 assert(step(false)&&!live&&view().state==TELEMETRY_BROADCAST_PAUSED);assert(step(false)&&!live);assert(step(true)&&live);
 policy.radios_allowed=false;assert(step(true)&&!live&&view().state==TELEMETRY_BROADCAST_BLE_OFF);policy.radios_allowed=true;assert(step(true)&&live);
 policy.settings_valid=false;assert(step(true)&&!live&&view().state==TELEMETRY_BROADCAST_SETTINGS_ERROR);policy.settings_valid=true;assert(step(true)&&live);
 policy.enabled=false;assert(step(true)&&!live&&!view().enabled);for(unsigned i=0;i<20;i++){now+=1000;assert(step(true)&&!live);}
 policy.enabled=true;assert(step(true)&&live);assert(api->pause(NULL)&&!live&&view().enabled);assert(step(true)&&live);
 close_fail=true;assert(!api->pause(NULL)&&live&&view().cleanup_pending);
 unsigned old_reads=reads,old_polls=polls,old_enumerations=enumerations;int32_t value;risc_telemetry_field_v1 field;
 assert(!step(true)&&api->enumerate(NULL,0,&field)==-1&&api->read(NULL,7,&value)==-1);
 assert(reads==old_reads&&polls==old_polls&&enumerations==old_enumerations);
 close_fail=false;assert(step(true)&&!live&&!view().cleanup_pending);assert(step(true)&&live);
 assert(driver->quiesce()&&!live);driver->stop();assert(!step(true));driver=NULL;
 reset();policy.radios_allowed=true;unavailable=true;assert(step(true)&&live&&publish_count==3&&publish_ids[0]==19&&publish_ids[1]==41&&publish_ids[2]==99);
 unavailable=false;now+=5000;assert(step(true)&&live&&publish_count==4&&publishes==2);
 reset();policy.radios_allowed=true;malformed=true;assert(step(true)&&!live&&!publishes&&view().state==TELEMETRY_BROADCAST_RETRY);malformed=false;now+=5000;assert(step(true)&&live);
 reset();policy.radios_allowed=true;publish_fail=true;assert(step(true)&&!live&&publishes==1&&closes==1);publish_fail=false;now+=4999;assert(step(true)&&!live&&publishes==1);now++;assert(step(true)&&live&&publishes==2);
 poll_fail=true;assert(step(true)&&!live&&view().state==TELEMETRY_BROADCAST_RETRY);poll_fail=false;now+=5000;assert(step(true)&&live);
 now=UINT64_MAX;assert(step(true)&&!live);
 reset();policy.radios_allowed=true;many=true;assert(step(true)&&live&&publish_count==7&&view().omitted_fields==9);for(unsigned i=0;i<7;i++)assert(publish_ids[i]==i+1);
 assert(driver->quiesce());driver->stop();
 puts("Broadcast service: capability-only seven metrics, no invented values, packet-budget omissions, unknown/zero, copied app policy, boot-owned polling, pause/resume, bounded retry and retained cleanup PASS");
}
