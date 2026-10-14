#define PORTABLE_BLE_BROADCAST
#ifdef PORTABLE_UTILITIES_TARGET_X4
#define PORTABLE_NATIVE_TIME_TOOLBAR
#else
#define PORTABLE_NATIVE_CUSTODY_FENCE
#endif
#include "../Apps/battery_power.inc"
#include "utility_atomic_transport.h"
#include <assert.h>
#include <stdio.h>
static bool retained,stop_bad,stop_retained;static unsigned grants,stops;
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){assert(!retained&&!strcmp(name,RISC_KEY_VALUE_ATOMIC_CAPABILITY)&&version==1&&instance==1);g->api=&uat_api;g->slot=1;g->generation=1;grants++;return true;}
static bool release(risc_runtime_capability_v1 *g){assert(!retained&&g->api==&uat_api&&grants);if(uat.release_bad)return false;grants--;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void yield(uint32_t ms){assert(!retained&&ms==5);uat.yields++;}
static bool exit_requested(void){return uat.exit_pending;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.yield_ms=yield,.exit_requested=exit_requested};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&rt:NULL;}
bool portable_adapter_retained(void){return retained;}
void portable_adapter_retain(void){assert(!retained);retained=true;}
bool portable_broadcast_preference_result(bool enabled,bool saved){(void)enabled;assert(!retained);return saved;}
bool portable_broadcast_stop(void){assert(!retained);stops++;if(stop_retained)retained=true;return !stop_bad&&!stop_retained;}
int main(int argc,char **argv){
 assert(argc==2);const char *test=argv[1];uat_reset();
 if(!strcmp(test,"toggle")){assert(power_broadcast_save(true));uint8_t b[]={0x62,1,1,0xa4};uat_assert_user("ble_broadcast",b,4,1);assert(power_broadcast_save(false)&&uat.commits==2&&stops==2);}
 else if(!strcmp(test,"same")){assert(power_broadcast_save(true)&&power_broadcast_save(true)&&uat.commits==2);}
 else if(!strcmp(test,"pause-failed")){stop_bad=true;assert(!power_broadcast_save(true)&&!uat.submits&&!grants);}
 else if(!strcmp(test,"busy")){uat.submit_error=RISC_ATOMIC_BUSY;assert(!power_broadcast_save(true)&&uat.submits==1&&!uat.commits);}
 else if(!strcmp(test,"delayed")){uat.pending_polls=3;assert(power_broadcast_save(true)&&uat.yields==3&&uat.commits==1);}
 else if(!strcmp(test,"exit")){uat.exit_pending=true;assert(!power_broadcast_save(true)&&!uat.submits&&!grants);}
 else if(!strcmp(test,"cancel-pending")){uat.exit_on_submit=true;assert(!power_broadcast_save(true)&&uat.cancels==1&&!uat.commits&&!grants);}
 else if(!strcmp(test,"cancel-race")){uat.exit_on_submit=uat.cancel_race=true;assert(power_broadcast_save(true)&&uat.cancels==1&&uat.commits==1&&!grants);}
 else if(!strcmp(test,"collect-error")){uat.collect_error=RISC_ATOMIC_IO;assert(!power_broadcast_save(true)&&retained&&grants==1&&power_preference.edit.token);}
 else if(!strcmp(test,"release-error")){uat.release_bad=true;assert(!power_broadcast_save(true)&&retained&&grants==1&&!power_preference.edit.token&&uat.commits==1);}
 else assert(0);
 if(!retained)assert(!grants&&!power_preference.edit.token);
 printf("Actual Battery broadcast controller %s PASS\n",test);return 0;
}
