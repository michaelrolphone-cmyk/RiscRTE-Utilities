#include "AlarmControlV1.h"
#include "AlarmRecords.h"
#include "AlarmServiceV2.h"
#include "RiscProviderV2.h"
#include "RiscBoundKeyValueV1.h"
#include "PortableRtcClock.h"
#include "PortableTimeZonePreference.h"
#include "RiscPlatformRealtimeV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t cfg[32],volume[1],zone[44];static unsigned cfg_size,volume_size,zone_size;
static uint32_t now_sec;static int reads,writes,calls,acks,refreshes;
static bool get_context,put_context,get_io,put_io,commit_even_on_io,readback_io,service_retained,copy_retained,clock_context;
static bool clock_valid=true,blocked;static alarm_token_v1 token;
static int32_t kv_get(void*c,const char*key,void*out,uint32_t cap,uint32_t*size){
    (void)c;++calls;++reads;*size=0;if(get_context)return RISC_BOUND_KEY_VALUE_CONTEXT;
    if(get_io||(readback_io&&writes))return RISC_BOUND_KEY_VALUE_IO;
    const uint8_t *data=NULL;unsigned n=0;
    if(!strcmp(key,ALARM_CONFIG_KEY)){data=cfg;n=cfg_size;}
    else if(!strcmp(key,"alarm_volume")){data=volume;n=volume_size;}
    else if(!strcmp(key,"time_zone")){data=zone;n=zone_size;}
    else assert(0&&"unexpected key");
    if(!n)return RISC_BOUND_KEY_VALUE_NOT_FOUND;
    *size=n;if(cap<n)return RISC_BOUND_KEY_VALUE_BUFFER_SMALL;
    memcpy(out,data,n);return 0;
}
static int32_t kv_put(void*c,const char*key,const void*data,uint32_t n){
    (void)c;++calls;++writes;if(put_context)return RISC_BOUND_KEY_VALUE_CONTEXT;
    if(!put_io||commit_even_on_io){
        if(!strcmp(key,ALARM_CONFIG_KEY)){assert(n==32);memcpy(cfg,data,n);cfg_size=n;}
        else if(!strcmp(key,"alarm_volume")){assert(n==1);memcpy(volume,data,n);volume_size=n;}
        else assert(0);
    }
    return put_io?RISC_BOUND_KEY_VALUE_IO:0;
}
static int32_t status(void*c,alarm_status_v1*out){
    (void)c;++calls;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=blocked?ALARM_STATE_BLOCKED:ALARM_STATE_READY,.error=copy_retained?ALARM_RETAINED:0,.rtc_seconds=now_sec,.occurrence=token};
    return service_retained?ALARM_RETAINED:ALARM_OK;
}
static int32_t step(void*c){(void)c;++calls;return service_retained?ALARM_RETAINED:0;}
static int32_t refresh(void*c){(void)c;++calls;++refreshes;return service_retained?ALARM_RETAINED:0;}
static int32_t ack(void*c,const alarm_token_v1*t){(void)c;++calls;++acks;assert(alarm_token_equal(t,&token));return 1;}
static int32_t sleep_prepare(void*c,alarm_sleep_v1*t){(void)c;(void)t;return 0;}
static int32_t stop_outputs(void*c){(void)c;return 0;}
static int32_t realtime_read(void*c,risc_realtime_snapshot_v1*out){(void)c;++calls;if(clock_context)return RISC_REALTIME_CONTEXT;
    *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=clock_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,.epoch_seconds=INT64_C(946684800)+now_sec};return 0;
}
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;++calls;if(!clock_valid)return false;
    portable_timezone_civil p;assert(portable_timezone_epoch_to_civil(INT64_C(946684800)+now_sec,&p)==0);
    *out=(twatch_rtc_time_v1){(uint16_t)p.year,p.month,p.day,p.weekday,p.hour,p.minute,p.second};return true;
}
static const risc_bound_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static const risc_realtime_api_v1 rt={1,sizeof(rt),NULL,realtime_read};
static const twatch_rtc_api_v1 rtc={2,sizeof(rtc),NULL,rtc_read,NULL,NULL,NULL};
#ifdef ALARM_NATIVE_UTC
static const alarm_service_descriptor_v2 svc={{2,sizeof(svc),NULL,status,step,refresh,ack,sleep_prepare,stop_outputs},ALARM_SERVICE_DESCRIPTOR_TAG,1,0,0,NULL};
#else
static const alarm_service_v1 svc={1,sizeof(svc),NULL,status,step,refresh,ack,sleep_prepare,stop_outputs};
#endif
static const alarm_control_api_v1 *api;static const risc_driver_v2 *driver;
static void set_time(unsigned year,unsigned month,unsigned day,unsigned utc_hour,unsigned minute){
    portable_timezone_civil p={(int32_t)year,(uint8_t)month,(uint8_t)day,(uint8_t)utc_hour,(uint8_t)minute,0,0};int64_t epoch;
    assert(portable_timezone_civil_to_epoch(&p,&epoch)==0);
#ifdef ALARM_NATIVE_UTC
    now_sec=(uint32_t)(epoch-INT64_C(946684800));
#else
    now_sec=(uint32_t)(epoch-INT64_C(946684800)+8*3600);
#endif
}
static void set_zone(void){
    memset(zone,0,sizeof(zone));zone[0]='T';zone[1]='Z';zone[2]=1;memcpy(zone+4,"America/Denver",14);
    uint8_t sum=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)sum^=zone[i];zone[3]=sum;zone_size=44;
}
static void startup(void){
    set_zone();set_time(2026,10,9,12,0); /* Denver 06:00 */
    driver=t5_driver_get(2);api=driver->capability;
    risc_provider_dependency_v1 deps[]={{"storage.key-value.bound",1,&kv},
#ifdef ALARM_NATIVE_UTC
        {"alarm.service",2,&svc},{"platform.realtime",1,&rt}
#else
        {"alarm.service",1,&svc},{"rtc.clock",2,&rtc}
#endif
    };
    (void)rt;(void)rtc;assert(driver->start(deps,3));assert(calls==0);assert(driver->quiesce());
}
static alarm_control_command_v1 command(uint32_t op,uint32_t value,uint32_t expected){alarm_control_command_v1 c={.struct_size=sizeof(c)};assert(api->prepare(NULL,op,value,expected,&c)==0);return c;}
static void no_more_calls(void){unsigned before=(unsigned)calls;alarm_control_snapshot_v1 s={.struct_size=sizeof(s)};alarm_control_command_v1 cmd={.struct_size=sizeof(cmd)};
    assert(api->read(NULL,&s)==ALARM_CONTROL_RETAINED);assert(api->step(NULL)==ALARM_CONTROL_RETAINED);assert(api->prepare(NULL,1,10,0,&cmd)==ALARM_CONTROL_RETAINED);assert(api->apply(NULL,&cmd)==ALARM_CONTROL_RETAINED);
    assert(api->dismiss(NULL,NULL)==ALARM_CONTROL_RETAINED);assert(!driver->quiesce());driver->stop();assert((unsigned)calls==before);
}
int main(int argc,char**argv){
    assert(argc==2);startup();const char*mode=argv[1];
    alarm_control_snapshot_v1 s={.struct_size=sizeof(s)};assert(api->read(NULL,&s)==0&&s.revision==0&&s.minutes==361);
    if(!strcmp(mode,"arm-cancel")){
        alarm_control_command_v1 c=command(ALARM_CONTROL_ARM,390,0);assert(!writes);assert(api->apply(NULL,&c)==0&&writes==1);
        alarm_config a;assert(alarm_config_decode(&a,cfg,32,1)&&a.deadline==now_sec+1800&&a.revision==1);
        assert(api->apply(NULL,&c)==0&&writes==1); /* same intent cannot create another revision */
        c=command(ALARM_CONTROL_CANCEL,0,1);assert(api->apply(NULL,&c)==0&&writes==2);assert(alarm_config_decode(&a,cfg,32,1)&&!a.enabled&&a.revision==2);
    }else if(!strcmp(mode,"tomorrow")){
        alarm_control_command_v1 c=command(1,300,0);assert(api->apply(NULL,&c)==0);alarm_config a;assert(alarm_config_decode(&a,cfg,32,1)&&a.deadline==now_sec+23*3600);
    }else if(!strcmp(mode,"gap")||!strcmp(mode,"fold")){
        if(!strcmp(mode,"gap"))set_time(2026,3,8,8,0);else set_time(2026,11,1,6,0);
        alarm_control_command_v1 c={.struct_size=sizeof(c)};assert(api->prepare(NULL,1,!strcmp(mode,"gap")?150:90,0,&c)==ALARM_CONTROL_AMBIGUOUS);assert(!writes);
    }else if(!strcmp(mode,"stale")){
        alarm_control_command_v1 c=command(1,390,0),other=command(1,400,0);assert(api->apply(NULL,&other)==0);assert(api->apply(NULL,&c)==ALARM_CONTROL_STALE&&writes==1);
        assert(api->prepare(NULL,1,410,0,&c)==ALARM_CONTROL_STALE);
    }else if(!strcmp(mode,"corrupt")){
        cfg_size=32;memset(cfg,0xaa,32);alarm_control_command_v1 c={.struct_size=sizeof(c)};assert(api->prepare(NULL,1,390,0,&c)==ALARM_CONTROL_STORAGE&&writes==0);
        assert(api->read(NULL,&s)==ALARM_CONTROL_STORAGE&&!(s.flags&ALARM_CONTROL_CONFIG_VALID));
    }else if(!strcmp(mode,"unknown-commit")||!strcmp(mode,"unwritten-retry")){
        alarm_control_command_v1 c=command(1,390,0);put_io=true;commit_even_on_io=!strcmp(mode,"unknown-commit");readback_io=true;
        assert(api->apply(NULL,&c)==ALARM_CONTROL_UNCERTAIN);assert(writes==1);now_sec+=60;
        readback_io=false;put_io=false;assert(api->apply(NULL,&c)==0);assert(writes==(commit_even_on_io?1:2));
        alarm_config a;assert(alarm_config_decode(&a,cfg,32,1)&&a.created==now_sec-60);
    }else if(!strcmp(mode,"io-confirmed")){
        alarm_control_command_v1 c=command(1,390,0);put_io=commit_even_on_io=true;assert(api->apply(NULL,&c)==0&&writes==1);
    }else if(!strcmp(mode,"expired")||!strcmp(mode,"backward")){
        alarm_control_command_v1 c=command(1,390,0);if(!strcmp(mode,"expired"))now_sec+=3600;else --now_sec;
        assert(api->apply(NULL,&c)==(!strcmp(mode,"expired")?ALARM_CONTROL_EXPIRED:ALARM_CONTROL_CLOCK));assert(!writes);
    }else if(!strcmp(mode,"bad-command")){
        alarm_control_command_v1 c=command(1,390,0);int before=calls;
        for(unsigned i=0;i<sizeof(c.bytes);i++){c.bytes[i]^=1;assert(api->apply(NULL,&c)==ALARM_CONTROL_INVALID&&calls==before);c.bytes[i]^=1;}
    }else if(!strcmp(mode,"volume")){
        alarm_control_command_v1 c={.struct_size=sizeof(c)};
#ifdef ALARM_NATIVE_UTC
        assert(!(s.flags&ALARM_CONTROL_HAS_VOLUME));assert(api->prepare(NULL,3,75,50,&c)==ALARM_CONTROL_INVALID&&!writes);
#else
        assert(s.flags&ALARM_CONTROL_HAS_VOLUME);c=command(3,75,50);assert(api->apply(NULL,&c)==0&&volume[0]==75);assert(api->apply(NULL,&c)==0&&writes==1);
        assert(api->prepare(NULL,3,80,50,&c)==ALARM_CONTROL_STALE);
#endif
    }else if(!strcmp(mode,"dismiss")){
        token=(alarm_token_v1){1,5,now_sec,12};alarm_control_occurrence_v1 copy={1,5,now_sec,11};assert(api->dismiss(NULL,&copy)==ALARM_CONTROL_STALE&&!acks);
        copy.generation=12;assert(api->dismiss(NULL,&copy)==ALARM_CONTROL_PENDING&&acks==1);
    }else if(!strcmp(mode,"blocked")){
        blocked=true;alarm_control_command_v1 c={.struct_size=sizeof(c)};assert(api->prepare(NULL,1,390,0,&c)==ALARM_CONTROL_BLOCKED&&!writes);
    }else if(!strcmp(mode,"get-context")){
        get_context=true;assert(api->read(NULL,&s)==ALARM_CONTROL_RETAINED);no_more_calls();
    }else if(!strcmp(mode,"put-context")){
        alarm_control_command_v1 c=command(1,390,0);put_context=true;int before=reads;assert(api->apply(NULL,&c)==ALARM_CONTROL_RETAINED&&reads>=before);no_more_calls();
    }else if(!strcmp(mode,"service-retained")||!strcmp(mode,"copied-retained")){
        service_retained=!strcmp(mode,"service-retained");copy_retained=!service_retained;assert(api->read(NULL,&s)==ALARM_CONTROL_RETAINED);no_more_calls();
    }else if(!strcmp(mode,"clock-context")){
#ifdef ALARM_NATIVE_UTC
        clock_context=true;assert(api->read(NULL,&s)==ALARM_CONTROL_RETAINED);no_more_calls();
#else
        clock_valid=false;assert(api->read(NULL,&s)==ALARM_CONTROL_CLOCK);
#endif
    }else if(!strcmp(mode,"bad-zone")){
#ifdef ALARM_NATIVE_UTC
        zone[3]^=1;assert(api->read(NULL,&s)==ALARM_CONTROL_CLOCK&&!writes);
#endif
    }else assert(0);
    printf("alarm control %s policy=%s PASS writes=%d\n",mode,
#ifdef ALARM_NATIVE_UTC
           "native-utc",
#else
           "raw-utc8-denver",
#endif
           writes);return 0;
}
