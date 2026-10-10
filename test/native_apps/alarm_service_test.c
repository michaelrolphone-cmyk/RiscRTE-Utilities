/* Production ordinary-provider entry point with fault-injected dependencies.
 * Target ELF/load integration is a separate required build check. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Services/alarm_service/service.c"
static uint8_t blobs[6][64];static uint32_t sizes[6];
static uint64_t ms;static uint32_t rtc_base;static int get_count,put_count,reads,opens,writes,effects,stops,silences,closes;
static bool get_fail,put_fail,put_persists,rtc_fail,open_fail,write_fail,effect_fail,stop_fail,close_fail;
static unsigned gains,last_gain,last_peak;static bool gain_fail,volume_read_fail;
static bool revoked,reentrant;static const alarm_service_v1 *client;
static int index_key(const char *key) {const char *keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,"alarm_volume"};for(int i=0;i<6;i++)if(!strcmp(keys[i],key))return i;assert(0);return 0;}
static int32_t get_blob(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(!revoked);get_count++;*n=0;if(volume_read_fail&&!strcmp(k,"alarm_volume"))return RISC_BOUND_KEY_VALUE_IO;if(get_fail)return -5;int i=index_key(k);if(!sizes[i])return -1;if(cap<sizes[i])return -2;memcpy(b,blobs[i],sizes[i]);*n=sizes[i];if(reentrant){alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==ALARM_BUSY);assert(client->step(NULL)==ALARM_BUSY);}return 0;}
static int32_t put_blob(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!revoked);put_count++;int i=index_key(k);assert(i==3||i==4);if(!put_fail||put_persists){memcpy(blobs[i],b,n);sizes[i]=n;}return put_fail?-5:0;}
static uint64_t mono(void*c){(void)c;return ms;}
static bool read_rtc(void*c,twatch_rtc_time_v1*out){(void)c;reads++;if(rtc_fail)return false;uint32_t t=rtc_base+(uint32_t)(ms/1000);*out=(twatch_rtc_time_v1){2000,1,(uint8_t)(1+t/86400),6,(uint8_t)(t/3600%24),(uint8_t)(t/60%60),(uint8_t)(t%60)};return true;}
static bool h_effect(void*c,uint8_t e){(void)c;assert(e);effects++;return !effect_fail;}
static bool h_stop(void*c){(void)c;stops++;return !stop_fail;}
static bool a_open(void*c,uint32_t r,uint8_t n){(void)c;assert(r==8000&&n==1);opens++;return !open_fail;}
static bool a_write(void*c,const int16_t*p,size_t n){(void)c;assert(n==256);bool audible=false;for(size_t i=0;i<n;i++){audible|=p[i]!=0;unsigned peak=p[i]<0?(unsigned)-p[i]:(unsigned)p[i];if(peak>last_peak)last_peak=peak;}assert(audible);writes++;return !write_fail;}
static bool a_gain(void*c,uint16_t g,uint16_t m){(void)c;assert(m==100&&g<=m);gains++;last_gain=g;return !gain_fail;}
static bool a_silence(void*c){(void)c;silences++;return true;}
static bool a_close(void*c){(void)c;closes++;return !close_fail;}
static const risc_bound_key_value_v1 bound={1,sizeof(bound),NULL,get_blob,put_blob};
static const risc_platform_clock_api_v1 clk={1,sizeof(clk),NULL,mono,NULL};
static const twatch_rtc_api_v1 rtc_api={2,sizeof(rtc_api),NULL,read_rtc,NULL,NULL,NULL};
static const twatch_haptic_api_v1 hapi={1,sizeof(hapi),NULL,h_effect,h_stop};
static const twatch_audio_out_api_v1 aapi={1,sizeof(aapi),NULL,a_open,a_write,a_gain,a_silence,a_close};
static const risc_provider_dependency_v1 deps[]={
 {"storage.key-value.bound",1,&bound},{"platform.clock",1,&clk},{"rtc.clock",2,&rtc_api},{"haptic.effect",1,&hapi},{"audio.output",1,&aapi}};
static const risc_driver_v2 *driver_api;
static alarm_status_v1 snapshot(void){alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==0);return s;}
static void pump(unsigned count){while(count--){int before=get_count+put_count+reads+opens+writes+effects+stops+silences+closes+(int)gains;(void)client->step(NULL);int after=get_count+put_count+reads+opens+writes+effects+stops+silences+closes+(int)gains;assert(after-before<=2);ms+=1;}}
static void until_at(unsigned state,int line){for(int i=0;i<100;i++){if(snapshot().state==state)return;pump(1);}fprintf(stderr,"line=%d state=%u actual=%u error=%d phase=%d ms=%llu\n",line,state,snapshot().state,snapshot().error,phase,(unsigned long long)ms);assert(!"state not reached");}
#define until(state) until_at(state,__LINE__)
static void boot(bool clear){if(driver_api){stop_fail=close_fail=false;assert(driver_api->quiesce());}if(clear){memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;rtc_base=100;}get_fail=put_fail=put_persists=rtc_fail=open_fail=write_fail=effect_fail=stop_fail=close_fail=revoked=reentrant=false;get_count=put_count=reads=opens=writes=effects=stops=silences=closes=0;gains=last_gain=last_peak=0;gain_fail=volume_read_fail=false;driver_api=t5_driver_get(2);assert(driver_api&&driver_api->struct_size==sizeof(*driver_api));client=driver_api->capability;assert(driver_api->start(deps,5));}
static void schedule(unsigned kind,unsigned revision,unsigned deadline){alarm_config c={revision,deadline,100,kind==2?deadline-100:0,(uint8_t)kind,1};assert(alarm_config_valid(&c));alarm_config_encode(&c,blobs[kind-1]);sizes[kind-1]=32;}
static void mode(unsigned v){blobs[2][0]=(uint8_t)v;sizes[2]=1;}
static alarm_token_v1 ring(unsigned v){boot(true);schedule(1,1,101);mode(v);rtc_base=101;until(ALARM_STATE_ALERT);pump(4);return snapshot().occurrence;}
int main(void){
 boot(true);until(ALARM_STATE_READY);assert(!put_count&&!opens&&!effects);alarm_sleep_v1 plan={.struct_size=sizeof(plan)};assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING);pump(8);assert(client->prepare_sleep(NULL,&plan)==0&&!plan.deadline);
 for(unsigned v=1;v<=3;v++){alarm_token_v1 t=ring(v);assert((opens>0)==!!(v&2));assert((effects>0)==!!(v&1));assert((writes>0)==!!(v&2));alarm_token_v1 bad=t;bad.generation++;assert(client->acknowledge(NULL,&bad)==ALARM_STALE);assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(snapshot().schedules[0].state==ALARM_SCHEDULE_DISMISSED);assert(client->acknowledge(NULL,&t)==0);assert(stops&&closes&&silences);}
 alarm_token_v1 old=ring(3);boot(false);until(ALARM_STATE_ALERT);assert(snapshot().occurrence.generation==old.generation+1);assert(client->acknowledge(NULL,&old)==ALARM_STALE);
 alarm_token_v1 current=snapshot().occurrence;stop_fail=close_fail=true;assert(client->acknowledge(NULL,&current)==1);until(ALARM_STATE_BLOCKED);assert(stops&&closes&&snapshot().output_uncertain);int writes_before=put_count;pump(10);assert(put_count==writes_before);stop_fail=false;assert(client->acknowledge(NULL,&current)==1);until(ALARM_STATE_BLOCKED);assert(!haptic_uncertain&&audio_uncertain);close_fail=false;assert(client->acknowledge(NULL,&current)==1);until(ALARM_STATE_READY);
 ring(2);write_fail=true;ms+=500;pump(5);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_OUTPUT);current=snapshot().occurrence;write_fail=false;assert(client->acknowledge(NULL,&current)==1);until(ALARM_STATE_READY);
 boot(true);schedule(1,1,101);rtc_base=101;put_fail=true;put_persists=true;until(ALARM_STATE_ALERT);assert(effects==0);pump(3);assert(effects>0);current=snapshot().occurrence;put_persists=false;assert(client->acknowledge(NULL,&current)==1);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&!snapshot().output_uncertain);put_fail=false;client->acknowledge(NULL,&current);until(ALARM_STATE_READY);
 boot(true);schedule(1,1,101);rtc_base=101;put_fail=true;until(ALARM_STATE_BLOCKED);assert(!opens&&!effects);put_fail=false;client->refresh(NULL);until(ALARM_STATE_ALERT);
 ring(1);ms+=ALARM_INVOCATION_MS;pump(15);until(ALARM_STATE_READY);assert(snapshot().schedules[0].state==ALARM_SCHEDULE_EXPIRED);boot(false);until(ALARM_STATE_READY);assert(!effects&&!opens);
 boot(true);schedule(1,1,101);rtc_base=162;until(ALARM_STATE_READY);assert(snapshot().schedules[0].state==ALARM_SCHEDULE_EXPIRED&&!effects&&!opens);
 boot(true);schedule(1,1,105);schedule(2,1,104);rtc_base=105;until(ALARM_STATE_ALERT);assert(snapshot().occurrence.kind==2);current=snapshot().occurrence;client->acknowledge(NULL,&current);pump(15);until(ALARM_STATE_ALERT);assert(snapshot().occurrence.kind==1);
 boot(true);schedule(1,1,200);until(ALARM_STATE_READY);plan=(alarm_sleep_v1){.struct_size=sizeof(plan)};assert(client->prepare_sleep(NULL,&plan)==1);pump(8);assert(client->prepare_sleep(NULL,&plan)==0&&plan.deadline==200);assert(client->step(NULL)==0); /* refusal never leaves suppression */
 ms+=1000;rtc_base=96;pump(10);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_RTC);rtc_base=100;client->refresh(NULL);until(ALARM_STATE_READY);
 boot(true);rtc_fail=true;until(ALARM_STATE_BLOCKED);rtc_fail=false;client->refresh(NULL);until(ALARM_STATE_READY);
 boot(true);sizes[0]=32;memset(blobs[0],0,32);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&!put_count);
 boot(true);mode(0);until(ALARM_STATE_BLOCKED);assert(!put_count);
 boot(true);get_fail=true;until(ALARM_STATE_BLOCKED);get_fail=false;client->refresh(NULL);until(ALARM_STATE_READY);
 boot(true);reentrant=true;until(ALARM_STATE_READY);
 ring(3);revoked=true;stop_fail=true;int old_puts=put_count;assert(!driver_api->quiesce());assert(put_count==old_puts&&closes);stop_fail=false;assert(driver_api->quiesce());assert(put_count==old_puts);revoked=false;assert(driver_api->start(deps,5));
 assert(driver_api->quiesce());risc_provider_dependency_v1 bad[5];memcpy(bad,deps,sizeof(bad));bad[2].api_version=1;assert(!driver_api->start(bad,5));assert(driver_api->quiesce());
 puts("Alarm production service lifecycle, persistence, outputs, RTC, sleep and retry fixtures passed");return 0;
}
