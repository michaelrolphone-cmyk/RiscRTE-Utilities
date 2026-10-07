/* Production provider, independent clocks, and the optional append-only API.
 * Native sleep is a caller contract here, not simulated hardware qualification. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Services/alarm_service/service.c"
static uint8_t blobs[9][64];static uint32_t sizes[9];
static uint64_t mono_ms;static uint32_t wall_seconds;
static unsigned rtc_reads,output_calls,put_calls;
static bool bad_read,bad_calendar,bad_weekday,reenter;
static const alarm_service_v1 *client;
#ifdef ALARM_SERVICE_TAGGED_V2
static const alarm_service_descriptor_v2 *sleep_api;
#else
static const alarm_service_sleep_v1 *sleep_api;
#endif
static const risc_driver_v2 *provider;
static alarm_sleep_v1 reentry_ticket;
static int key_index(const char *key) {
    const char *keys[]={"alarm_cfg","timer_cfg","alert_mode","alarm_occ","timer_occ","points_cfg","points_occ","alarm_volume","alert_dnd"};
    for(unsigned i=0;i<9;i++)if(!strcmp(key,keys[i]))return (int)i;
    assert(0);return 0;
}
static int32_t get_blob(void *c,const char *key,void *out,uint32_t cap,uint32_t *n) {
    (void)c;int i=key_index(key);*n=0;if(!sizes[i])return RISC_BOUND_KEY_VALUE_NOT_FOUND;
    assert(cap>=sizes[i]);memcpy(out,blobs[i],sizes[i]);*n=sizes[i];return ALARM_OK;
}
static int32_t put_blob(void *c,const char *key,const void *in,uint32_t n) {
    (void)c;int i=key_index(key);assert(n<=64);memcpy(blobs[i],in,n);sizes[i]=n;put_calls++;return ALARM_OK;
}
static uint64_t mono(void *c){(void)c;return mono_ms;}
static bool read_calendar(void *c,twatch_rtc_time_v1 *out) {
    (void)c;rtc_reads++;
    if(reenter) {
        reenter=false;
        assert(sleep_api->resume_sleep(NULL,&reentry_ticket)==ALARM_BUSY);
        assert(client->step(NULL)==ALARM_BUSY);
    }
    if(bad_read)return false;
    uint32_t t=wall_seconds;
    *out=(twatch_rtc_time_v1){2000,1,(uint8_t)(1+t/86400),0,(uint8_t)(t/3600%24),(uint8_t)(t/60%60),(uint8_t)(t%60)};
    if(bad_calendar)out->month=13;
    if(bad_weekday)out->weekday=7;
    return true;
}
static bool effect(void *c,uint8_t e){(void)c;(void)e;output_calls++;return true;}
static bool success(void *c){(void)c;return true;}
static bool audio_open(void *c,uint32_t r,uint8_t n){(void)c;(void)r;(void)n;output_calls++;return true;}
static bool audio_write(void *c,const int16_t *p,size_t n){(void)c;(void)p;(void)n;output_calls++;return true;}
static bool audio_gain(void *c,uint16_t g,uint16_t m){(void)c;(void)g;(void)m;return true;}
static const risc_bound_key_value_v1 storage={1,sizeof(storage),NULL,get_blob,put_blob};
static const risc_platform_clock_api_v1 clock_fake={1,sizeof(clock_fake),NULL,mono,NULL};
static const twatch_rtc_api_v1 rtc_fake={2,sizeof(rtc_fake),NULL,read_calendar,NULL,NULL,NULL};
static const twatch_haptic_api_v1 haptic_fake={1,sizeof(haptic_fake),NULL,effect,success};
static const twatch_audio_out_api_v1 audio_fake={1,sizeof(audio_fake),NULL,audio_open,audio_write,audio_gain,success,success};
static const risc_provider_dependency_v1 deps[]={
    {"storage.key-value.bound",1,&storage},{"platform.clock",1,&clock_fake},{"rtc.clock",2,&rtc_fake},
    {"haptic.effect",1,&haptic_fake},{"audio.output",1,&audio_fake}};
static void restart(void) {
    if(provider)assert(provider->quiesce());
    provider=t5_driver_get(2);client=provider->capability;
    #ifdef ALARM_SERVICE_TAGGED_V2
    assert(client->api_version==2&&client->struct_size==sizeof(alarm_service_descriptor_v2));
    sleep_api=alarm_service_descriptor(client);
#else
    assert(client->api_version==1&&client->struct_size==ALARM_SERVICE_SLEEP_V1_SIZE);
    sleep_api=(const alarm_service_sleep_v1 *)client;
#endif
    assert(sleep_api->resume_sleep);
    assert(provider->start(deps,5));
}
static void initial(uint32_t deadline) {
    if(provider){assert(provider->quiesce());provider=NULL;}
    memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));
    mono_ms=0;wall_seconds=100100;rtc_reads=output_calls=put_calls=0;
    bad_read=bad_calendar=bad_weekday=reenter=false;
#ifdef POINTS_IN_TIME_SERVICE
    points_config empty={.revision=1};points_config_encode(&empty,blobs[5]);sizes[5]=64;
#endif
    if(deadline) {
        alarm_config a={.revision=1,.deadline=100000+deadline,.created=100100,.kind=1,.enabled=1};
        alarm_config_encode(&a,blobs[0]);sizes[0]=32;
    }
    restart();
}
static void pump(void){(void)client->step(NULL);mono_ms++;}
static void reach(phase_t target) {
    for(unsigned i=0;i<100&&phase!=target;i++)pump();
    if(phase!=target)fprintf(stderr,"target=%d actual=%d error=%d\n",target,phase,error);
    assert(phase==target);
}
static alarm_sleep_v1 plan(void) {
    alarm_sleep_v1 p={.struct_size=sizeof(p)};
    for(unsigned i=0;i<100;i++) {
        int32_t result=client->prepare_sleep(NULL,&p);
        if(result==ALARM_OK)return p;
        assert(result==ALARM_PENDING);pump();
    }
    assert(0);return p;
}
static alarm_status_v1 view_copy(void) {
    alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==ALARM_OK);return s;
}
static void test_old_client_and_refusal(void) {
    /* Unchanged legacy table remains valid for ordinary clients. */
    alarm_service_v1 old={1,sizeof(old),NULL,status,step,refresh,acknowledge,prepare_sleep,stop_only};
    assert(old.struct_size==offsetof(alarm_service_sleep_v1,resume_sleep));
    assert(old.struct_size<ALARM_SERVICE_SLEEP_V1_SIZE);
    initial(400);reach(IDLE);(void)plan();mono_ms+=300000;wall_seconds+=303;
    /* Legacy caller does not report its successful Light boundary. */
    assert(client->refresh(NULL)==ALARM_PENDING);reach(BLOCKED);
    assert(error==ALARM_RTC&&!output_calls);
    assert(client->refresh(NULL)==ALARM_PENDING);reach(PLAYING);assert(output_calls);
    /* Native refusal grants no reanchor. Existing awake edits still fail. */
    initial(700);reach(IDLE);(void)plan();mono_ms+=1000;wall_seconds+=4;
    assert(client->refresh(NULL)==ALARM_PENDING);reach(BLOCKED);assert(error==ALARM_RTC&&!output_calls);
}
static void test_successful_sleep(void) {
    for(unsigned direction=0;direction<2;direction++) {
        initial(400);reach(IDLE);alarm_sleep_v1 p=plan();
        mono_ms+=direction?303000:300000;wall_seconds+=direction?300:303;
        unsigned reads_before=rtc_reads;
        assert(sleep_api->resume_sleep(NULL,&p)==ALARM_OK);
        assert(rtc_reads==reads_before+1&&phase==LOAD_ALARM&&!output_calls);
        assert(sleep_api->resume_sleep(NULL,&p)==ALARM_STALE&&rtc_reads==reads_before+1);
        reach(PLAYING);assert(output_calls&&desired.deadline==100400);
    }
    /* Hybrid can prepare again immediately after the reanchor, before Deep. */
    initial(700);reach(IDLE);alarm_sleep_v1 first=plan();mono_ms+=300000;wall_seconds+=303;
    assert(sleep_api->resume_sleep(NULL,&first)==ALARM_OK);
    alarm_sleep_v1 second=plan();assert(second.deadline==100700&&second.rtc_seconds==100403&&second.snapshot>first.snapshot);
    assert(sleep_api->resume_sleep(NULL,&first)==ALARM_STALE);
    /* A second successful Light cycle needs the new one-use decision. */
    mono_ms+=300000;wall_seconds+=303;assert(sleep_api->resume_sleep(NULL,&second)==ALARM_OK);
    reach(PLAYING);assert(output_calls);
    /* A late wake still expires the durable occurrence without output. */
    initial(400);reach(IDLE);first=plan();mono_ms+=300000;wall_seconds+=370;
    assert(sleep_api->resume_sleep(NULL,&first)==ALARM_OK);reach(IDLE);
    assert(!output_calls&&occurrences[0].state==ALARM_OCC_EXPIRED);
    /* True Deep resets RAM; the original durable recovery path stays intact. */
    initial(400);reach(IDLE);(void)plan();wall_seconds=100403;mono_ms=0;restart();
    reach(PLAYING);assert(output_calls);
}
static void test_invalid_resume(void) {
    for(unsigned fault=0;fault<5;fault++) {
        initial(700);reach(IDLE);alarm_sleep_v1 p=plan();
        mono_ms+=300000;wall_seconds+=300;
        if(fault==0)bad_read=true;
        if(fault==1)bad_calendar=true;
        if(fault==2)bad_weekday=true;
        if(fault==3)wall_seconds=p.rtc_seconds-1;
        if(fault==4)mono_ms=previous_ms-1;
        assert(sleep_api->resume_sleep(NULL,&p)==ALARM_RTC);
        assert(phase==BLOCKED&&!output_calls&&!put_calls);
        assert(sleep_api->resume_sleep(NULL,&p)==ALARM_STALE);
    }
    initial(700);reach(IDLE);alarm_sleep_v1 p=plan(),wrong=p;
    unsigned before=rtc_reads;assert(sleep_api->resume_sleep(NULL,NULL)==ALARM_INVALID);
    wrong.struct_size--;assert(sleep_api->resume_sleep(NULL,&wrong)==ALARM_INVALID);
    wrong=p;wrong.snapshot++;assert(sleep_api->resume_sleep(NULL,&wrong)==ALARM_STALE);
    wrong=p;wrong.rtc_seconds++;assert(sleep_api->resume_sleep(NULL,&wrong)==ALARM_STALE);
    wrong=p;wrong.deadline++;assert(sleep_api->resume_sleep(NULL,&wrong)==ALARM_STALE);
    assert(rtc_reads==before);
    /* Read-only status does not consume the decision; dependency reentry fails. */
    (void)view_copy();mono_ms+=300000;wall_seconds+=303;reentry_ticket=p;reenter=true;
    assert(sleep_api->resume_sleep(NULL,&p)==ALARM_OK&&!reenter);
    assert(rtc_reads==before+1);
}
static void test_ticket_invalidation(void) {
    for(unsigned action=0;action<6;action++) {
        initial(700);reach(IDLE);alarm_sleep_v1 p=plan();
        if(action==0)(void)client->step(NULL);
        if(action==1)(void)client->refresh(NULL);
        if(action==2){alarm_token_v1 stale={0};assert(client->acknowledge(NULL,&stale)==ALARM_STALE);}
        if(action==3)assert(client->stop_only(NULL)==ALARM_OK);
        if(action==4){alarm_sleep_v1 next={.struct_size=sizeof(next)};assert(client->prepare_sleep(NULL,&next)==ALARM_PENDING);}
        if(action==5)restart();
        unsigned before=rtc_reads;assert(sleep_api->resume_sleep(NULL,&p)==ALARM_STALE);assert(rtc_reads==before);
    }
    initial(700);reach(IDLE);alarm_sleep_v1 p=plan();assert(provider->quiesce());
    assert(sleep_api->resume_sleep(NULL,&p)==ALARM_INVALID);
}
static void test_awake_guards_after_resume(void) {
    for(unsigned backward=0;backward<2;backward++) {
        initial(700);reach(IDLE);alarm_sleep_v1 p=plan();mono_ms+=300000;wall_seconds+=303;
        assert(sleep_api->resume_sleep(NULL,&p)==ALARM_OK);reach(IDLE);
        mono_ms+=1000;wall_seconds=backward?wall_seconds-1:wall_seconds+4;
        assert(client->refresh(NULL)==ALARM_PENDING);reach(BLOCKED);assert(error==ALARM_RTC&&!output_calls);
    }
    /* The separate activation sample still catches edits after durable PENDING. */
    initial(400);reach(IDLE);alarm_sleep_v1 p=plan();mono_ms+=300000;wall_seconds+=303;
    assert(sleep_api->resume_sleep(NULL,&p)==ALARM_OK);reach(ACTIVATE_RTC);
    wall_seconds+=3;pump();assert(phase==BLOCKED&&error==ALARM_RTC&&!output_calls);
}
#ifdef POINTS_IN_TIME_SERVICE
static void test_point_deadline_resume(void) {
    initial(0);
    points_config point={.revision=2,.created=wall_seconds-60};
    point.points[0]=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=ALARM_MODE_VIBRATE,
        .weekdays=127,.hour=13,.minute=0};
    assert(points_config_valid(&point));points_config_encode(&point,blobs[5]);sizes[5]=64;
    reach(IDLE);alarm_sleep_v1 p=plan();assert(p.deadline>wall_seconds+3);
    mono_ms+=(uint64_t)(p.deadline-wall_seconds-3)*1000;wall_seconds=p.deadline;
    assert(sleep_api->resume_sleep(NULL,&p)==ALARM_OK);
    for(unsigned i=0;i<1000;i++) {
        pump();
        if(points_occ.state==ALARM_OCC_ACKED&&phase==IDLE)break;
    }
    assert(points_occ.state==ALARM_OCC_ACKED&&points_occ.deadline==p.deadline&&phase==IDLE);
    assert(output_calls&&view_copy().state==ALARM_STATE_READY&&!view_copy().occurrence.generation);
}
#endif
int main(void) {
    test_old_client_and_refusal();test_successful_sleep();test_invalid_resume();
    test_ticket_invalidation();test_awake_guards_after_resume();
#ifdef POINTS_IN_TIME_SERVICE
    test_point_deadline_resume();
#endif
    assert(provider->quiesce());
    puts("Alarm sleep-resume source guards passed: independent clocks, one-use tickets, refusal, invalid/backward RTC, awake jumps, activation, expiry and Deep restart");
    return 0;
}
