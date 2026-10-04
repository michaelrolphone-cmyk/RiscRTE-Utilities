#define POINTS_IN_TIME_SERVICE
/* Production ordinary-provider entry point with fault-injected dependencies.
 * Target ELF/load integration is a separate required build check. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Services/alarm_service/service.c"
static uint8_t blobs[7][64];static uint32_t sizes[7];
static uint64_t ms;static uint32_t rtc_base;static int gets,put_count,reads,opens,writes,effects,stops,silences,closes;
static bool get_fail,put_fail,put_persists,rtc_fail,open_fail,write_fail,effect_fail,stop_fail,close_fail;
static bool revoked,reentrant;static const alarm_service_v1 *client;
static int index_key(const char *key) {const char *keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,POINTS_CONFIG_KEY,POINTS_OCCURRENCE_KEY};for(int i=0;i<7;i++)if(!strcmp(keys[i],key))return i;assert(0);return 0;}
static int32_t get_blob(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(!revoked);gets++;*n=0;if(get_fail)return -5;int i=index_key(k);if(!sizes[i])return -1;if(cap<sizes[i])return -2;memcpy(b,blobs[i],sizes[i]);*n=sizes[i];if(reentrant){alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==ALARM_BUSY);assert(client->step(NULL)==ALARM_BUSY);}return 0;}
static int32_t put_blob(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!revoked);put_count++;int i=index_key(k);assert(i==3||i==4||i==6);if(!put_fail||put_persists){memcpy(blobs[i],b,n);sizes[i]=n;}return put_fail?-5:0;}
static uint64_t mono(void*c){(void)c;return ms;}
static bool read_rtc(void*c,twatch_rtc_time_v1*out){(void)c;reads++;if(rtc_fail)return false;uint32_t t=rtc_base+(uint32_t)(ms/1000);return points_calendar(t,out);}
static bool h_effect(void*c,uint8_t e){(void)c;assert(e);effects++;return !effect_fail;}
static bool h_stop(void*c){(void)c;stops++;return !stop_fail;}
static bool a_open(void*c,uint32_t r,uint8_t n){(void)c;assert(r==8000&&n==1);opens++;return !open_fail;}
static bool a_write(void*c,const int16_t*p,size_t n){(void)c;assert(n==256);bool audible=false;for(size_t i=0;i<n;i++)audible|=p[i]!=0;assert(audible);writes++;return !write_fail;}
static bool a_gain(void*c,uint16_t g,uint16_t m){(void)c;(void)g;(void)m;return true;}
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
static void pump(unsigned count){while(count--){int before=gets+put_count+reads+opens+writes+effects+stops+silences+closes;(void)client->step(NULL);int after=gets+put_count+reads+opens+writes+effects+stops+silences+closes;assert(after-before<=2);ms+=1;}}
static void until_at(unsigned state,int line){for(int i=0;i<100;i++){if(snapshot().state==state)return;pump(1);}fprintf(stderr,"line=%d state=%u actual=%u error=%d phase=%d ms=%llu\n",line,state,snapshot().state,snapshot().error,phase,(unsigned long long)ms);assert(!"state not reached");}
#define until(state) until_at(state,__LINE__)
static void boot(bool clear){if(driver_api){stop_fail=close_fail=false;assert(driver_api->quiesce());}if(clear){memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;rtc_base=100;}get_fail=put_fail=put_persists=rtc_fail=open_fail=write_fail=effect_fail=stop_fail=close_fail=revoked=reentrant=false;gets=put_count=reads=opens=writes=effects=stops=silences=closes=0;driver_api=t5_driver_get(2);assert(driver_api&&driver_api->struct_size==sizeof(*driver_api));client=driver_api->capability;assert(driver_api->start(deps,5));}
static uint32_t civil(unsigned y,unsigned m,unsigned d,unsigned h,unsigned minute) {
    twatch_rtc_time_v1 t={(uint16_t)y,(uint8_t)m,(uint8_t)d,0,(uint8_t)h,(uint8_t)minute,0};
    portable_time_candidate c[2];assert(portable_time_inverse(&t,c)==1);uint32_t out;
    assert(alarm_calendar_seconds(c[0].rtc.year,c[0].rtc.month,c[0].rtc.day,c[0].rtc.hour,c[0].rtc.minute,0,&out));return out;
}
static points_config catalog(uint32_t now,unsigned kind,unsigned duration,unsigned output) {
    points_config c={.revision=1,.created=now-3600};
    for(unsigned i=0;i<POINTS_MAX;i++)c.points[i]=(points_item){.kind=POINTS_BREAK};
    c.points[0]=(points_item){.kind=(uint8_t)kind,.enabled=1,.mode=(uint8_t)output,.weekdays=127,.hour=12,.minute=0,.duration_minutes=(uint16_t)duration};
    return c;
}
static void save(points_config c){assert(points_config_valid(&c));points_config_encode(&c,blobs[5]);sizes[5]=64;}
static void at(uint32_t now){boot(true);rtc_base=now;}
static void ack(void){alarm_token_v1 t=snapshot().occurrence;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(client->acknowledge(NULL,&t)==ALARM_OK);}
static alarm_sleep_v1 sleep_plan(void){alarm_sleep_v1 p={.struct_size=sizeof(p)};for(unsigned i=0;i<100;i++){int r=client->prepare_sleep(NULL,&p);if(!r)return p;assert(r==ALARM_PENDING);pump(1);}assert(0);return p;}
int main(void) {
    uint32_t noon=civil(2026,10,4,12,0);
    at(noon);until(ALARM_STATE_READY);assert(!put_count&&!effects&&!opens);
    for(unsigned mode=0;mode<4;mode++) {
        at(noon);blobs[2][0]=3;sizes[2]=1;save(catalog(noon,POINTS_LUNCH,30,mode));until(ALARM_STATE_ALERT);pump(5);
        unsigned expected=mode?mode:3;assert((opens>0)==!!(expected&2));assert((effects>0)==!!(expected&1));
        assert(!strcmp(snapshot().label,"LUNCH"));assert(snapshot().occurrence.kind==3);ack();
        assert(sleep_plan().deadline==noon+1800);
        ms+=1800000;until(ALARM_STATE_ALERT);pump(4);assert(!strcmp(snapshot().label,"LUNCH ENDED"));assert(snapshot().occurrence.kind==4);ack();
        assert(sleep_plan().deadline==noon+86400);boot(false);until(ALARM_STATE_READY);assert(!opens&&!effects);
    }
    at(noon);save(catalog(noon,POINTS_BREAK,15,1));until(ALARM_STATE_ALERT);alarm_token_v1 old=snapshot().occurrence;
    boot(false);until(ALARM_STATE_ALERT);assert(snapshot().occurrence.generation==old.generation+1);assert(client->acknowledge(NULL,&old)==ALARM_STALE);ack();
    at(noon);save(catalog(noon,POINTS_LUNCH,30,2));put_fail=true;put_persists=true;until(ALARM_STATE_ALERT);pump(5);assert(opens);ack();
    at(noon);save(catalog(noon,POINTS_BREAK,15,3));put_fail=true;until(ALARM_STATE_BLOCKED);assert(!opens&&!effects);put_fail=false;client->refresh(NULL);until(ALARM_STATE_ALERT);pump(5);
    old=snapshot().occurrence;stop_fail=true;assert(client->acknowledge(NULL,&old)==1);until(ALARM_STATE_BLOCKED);assert(snapshot().output_uncertain);stop_fail=false;client->acknowledge(NULL,&old);until(ALARM_STATE_READY);
    /* An edit committed before first physical output cancels the old token. */
    at(noon);points_config c=catalog(noon,POINTS_LUNCH,30,3);save(c);
    while(phase!=START_AUDIO)pump(1);
    c.revision++;c.created=noon;save(c);until(ALARM_STATE_READY);assert(!opens&&!effects);assert(sleep_plan().deadline==noon+86400);
    /* A catalog edit during a duration never leaves its old derived end. */
    at(noon);c=catalog(noon,POINTS_LUNCH,30,1);save(c);until(ALARM_STATE_ALERT);ack();
    ms+=60000;c.revision++;c.created=noon+60;save(c);client->refresh(NULL);until(ALARM_STATE_READY);assert(sleep_plan().deadline==noon+86400);
    /* All sixteen expired boundaries compact in one write and settle under64 phases. */
    at(noon+13*3600);c=catalog(noon,POINTS_BREAK,1,1);c.created=noon-86400;for(unsigned i=0;i<8;i++){c.points[i]=c.points[0];c.points[i].hour=10;}save(c);
    unsigned phases=0;while(snapshot().state!=ALARM_STATE_READY&&phases<64){pump(1);phases++;}assert(phases<64);assert(put_count==1&&!opens&&!effects);
    /* Live output failure leaves exact durable pending; stop_only does not ACK. */
    at(noon);save(catalog(noon,POINTS_LUNCH,30,3));until(ALARM_STATE_ALERT);pump(5);old=snapshot().occurrence;
    int before=put_count;for(int i=0;i<3;i++)client->stop_only(NULL);assert(put_count==before&&snapshot().state==ALARM_STATE_BLOCKED);
    boot(false);until(ALARM_STATE_ALERT);assert(snapshot().occurrence.generation>old.generation);ack();
    /* Past-recovery restart skips an old pending output, still arms the end. */
    at(noon);save(catalog(noon,POINTS_LUNCH,30,1));until(ALARM_STATE_ALERT);ms+=61000;boot(false);until(ALARM_STATE_READY);assert(!opens&&!effects);assert(sleep_plan().deadline==noon+1800);
    /* Corrupt/reused schedule bytes, rollback, RTC discontinuity fail closed. */
    at(noon-1);c=catalog(noon,POINTS_LUNCH,30,1);save(c);until(ALARM_STATE_READY);c.points[0].minute=1;save(c);client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE);
    at(noon-1);c=catalog(noon,POINTS_LUNCH,30,1);c.revision=2;save(c);until(ALARM_STATE_READY);c.revision=1;save(c);client->refresh(NULL);until(ALARM_STATE_BLOCKED);
    at(noon-1);save(catalog(noon,POINTS_LUNCH,30,1));until(ALARM_STATE_READY);rtc_base-=3;ms+=1000;pump(20);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_RTC);
    at(noon);save(catalog(noon,POINTS_LUNCH,30,1));blobs[5][60]^=1;until(ALARM_STATE_BLOCKED);assert(!put_count);
    /* Exact-day highwaters survive an explicit backward-RTC retry. */
    at(noon);c=catalog(noon,POINTS_BREAK,0,1);c.created=noon-7*86400;save(c);until(ALARM_STATE_ALERT);ack();
    rtc_base=noon-600;ms=0;client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_RTC);
    client->refresh(NULL);until(ALARM_STATE_READY);assert(sleep_plan().deadline==noon+86400);
    /* A fresh restart before a PENDING deadline must wait, never ring early. */
    at(noon);c=catalog(noon,POINTS_BREAK,0,1);save(c);until(ALARM_STATE_ALERT);old=snapshot().occurrence;
    rtc_base=noon-30;ms=0;boot(false);until(ALARM_STATE_READY);assert(!opens&&!effects);assert(sleep_plan().deadline==noon);
    ms+=30000;until(ALARM_STATE_ALERT);assert(snapshot().occurrence.generation>old.generation);ack();
    /* Higher generations cannot roll back same-catalog per-edge highwaters. */
    points_ledger rollback=points_occ;rollback.generation++;rollback.highwater[0]--;rollback.state=rollback.mode=0;rollback.deadline=rollback.recovery_until=0;
    points_ledger_encode(&rollback,blobs[6]);client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE);
    /* Generation exhaustion never starts output or wraps tokens. */
    at(noon);c=catalog(noon,POINTS_BREAK,0,1);save(c);until(ALARM_STATE_ALERT);points_ledger exhausted=points_occ;exhausted.generation=UINT32_MAX;
    points_ledger_encode(&exhausted,blobs[6]);boot(false);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_EXHAUSTED&&!opens&&!effects);
    /* Checksums do not excuse structurally impossible persisted cursors. */
    at(noon);c=catalog(noon,POINTS_BEDTIME,0,1);save(c);points_ledger impossible={.revision=1};impossible.highwater[1]=36525;
    points_ledger_encode(&impossible,blobs[6]);sizes[6]=64;until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&!opens&&!effects);
    impossible.generation=1;points_ledger_encode(&impossible,blobs[6]);client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE);
    /* Explicit output mode is part of the same-catalog occurrence identity. */
    at(noon);save(catalog(noon,POINTS_BREAK,0,1));until(ALARM_STATE_ALERT);points_ledger wrong_mode=points_occ;wrong_mode.mode=2;
    points_ledger_encode(&wrong_mode,blobs[6]);boot(false);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&!opens&&!effects);
    puts("Points ordinary service recurrence, ends, durable replay, cancellation, mode, sleep and failure fixtures passed");return 0;
}
