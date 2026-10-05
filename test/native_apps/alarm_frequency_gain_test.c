/* Sequential real-service/real-app regression. Production sources are linked
 * separately; only the runtime, app input and physical providers are modeled. */
#include "AlarmRecords.h"
#include "AlarmOutputV1.h"
#include "AlarmVolume.h"
#include "PortableRtcClock.h"
#include "RiscBoundKeyValueV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include "T5AppApi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern const risc_driver_v2 *t5_driver_get(uint32_t);
static const char *const keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,
    ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,"points_cfg","points_occ",ALARM_VOLUME_KEY};
static uint8_t bytes[8][64];
static uint32_t sizes[8];
static uint64_t milliseconds;
static unsigned physical_gain=17,physical_maximum=100;
static unsigned alarm_percent,alarm_opens,alarm_writes,alarm_peak;
static unsigned frequency_opens,frequency_writes,frequency_pcm_peak,frequency_wire_peak;
static unsigned gain_calls,alarm_closes,frequency_closes,app_grants,polls,presents;
static bool stream_live,frequency_stage;

static int key_index(const char *key) {
    for(unsigned i=0;i<8;i++)if(!strcmp(key,keys[i]))return (int)i;
    assert(!"unexpected storage binding");return 0;
}
static int32_t storage_get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *size) {
    (void)context;int i=key_index(key);*size=sizes[i];
    if(!sizes[i])return RISC_BOUND_KEY_VALUE_NOT_FOUND;
    if(capacity<sizes[i])return RISC_BOUND_KEY_VALUE_BUFFER_SMALL;
    memcpy(out,bytes[i],sizes[i]);return RISC_BOUND_KEY_VALUE_OK;
}
static int32_t storage_put(void *context,const char *key,const void *value,uint32_t size) {
    (void)context;int i=key_index(key);assert(i==3 && size==ALARM_RECORD_SIZE);
    memcpy(bytes[i],value,size);sizes[i]=size;return RISC_BOUND_KEY_VALUE_OK;
}
static uint64_t monotonic(void *context) {(void)context;return milliseconds;}
static bool rtc_read(void *context,twatch_rtc_time_v1 *out) {
    (void)context;unsigned seconds=101+(unsigned)(milliseconds/1000);
    *out=(twatch_rtc_time_v1){2000,1,1,6,0,(uint8_t)(seconds/60),(uint8_t)(seconds%60)};
    return true;
}
static bool haptic_effect(void *context,uint8_t effect) {
    (void)context;(void)effect;assert(!"sound-only test must not vibrate");return false;
}
static bool haptic_stop(void *context) {(void)context;return true;}
static bool audio_open(void *context,uint32_t rate,uint8_t channels) {
    (void)context;assert(!stream_live && channels==1); /* No stream overlap. */
    assert(rate==(frequency_stage?16000u:8000u));stream_live=true;
    if(frequency_stage){assert(physical_gain==100 && physical_maximum==100);frequency_opens++;}
    else alarm_opens++;
    return true; /* Opening deliberately does not reset persistent gain. */
}
static unsigned magnitude(int32_t sample) {return sample<0?(unsigned)-sample:(unsigned)sample;}
static bool audio_write(void *context,const int16_t *pcm,size_t frames) {
    (void)context;assert(stream_live && pcm && frames==256);
    assert(physical_gain==100 && physical_maximum==100);
    for(size_t i=0;i<frames;i++) {
        unsigned raw=magnitude(pcm[i]);
        int32_t wire=(int32_t)pcm[i]*(int32_t)physical_gain/(int32_t)physical_maximum;
        assert(wire==pcm[i]);
        if(frequency_stage) {
            assert(raw<=1638);
            if(raw>frequency_pcm_peak)frequency_pcm_peak=raw;
            if(magnitude(wire)>frequency_wire_peak)frequency_wire_peak=magnitude(wire);
        } else {
            assert(raw==32767u*alarm_percent/100u);
            if(raw>alarm_peak)alarm_peak=raw;
        }
    }
    if(frequency_stage)frequency_writes++;else alarm_writes++;
    return true;
}
static bool audio_gain(void *context,uint16_t gain,uint16_t maximum) {
    (void)context;assert(stream_live && !frequency_stage && gain==100 && maximum==100);
    physical_gain=gain;physical_maximum=maximum;gain_calls++;return true;
}
static bool audio_silence(void *context) {(void)context;return true;}
static bool audio_close(void *context) {
    (void)context;
    if(stream_live){if(frequency_stage)frequency_closes++;else alarm_closes++;}
    stream_live=false;return true; /* Closing also retains physical gain. */
}
static const twatch_audio_out_api_v1 audio_api={1,sizeof(audio_api),NULL,
    audio_open,audio_write,audio_gain,audio_silence,audio_close};
static const risc_bound_key_value_v1 storage_api={1,sizeof(storage_api),NULL,storage_get,storage_put};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,monotonic,NULL};
static const twatch_rtc_api_v1 rtc_api={2,sizeof(rtc_api),NULL,rtc_read,NULL,NULL,NULL};
static const twatch_haptic_api_v1 haptic_api={1,sizeof(haptic_api),NULL,haptic_effect,haptic_stop};
static const risc_provider_dependency_v1 dependencies[]={
    {"storage.key-value.bound",1,&storage_api},{"platform.clock",1,&clock_api},
    {"rtc.clock",2,&rtc_api},{"haptic.effect",1,&haptic_api},{"audio.output",1,&audio_api}};

static bool acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant) {
    assert(frequency_stage && !strcmp(name,"audio.output") && version==1 && !id);
    assert(grant->struct_size==sizeof(*grant) && !app_grants);
    grant->api=&audio_api;app_grants++;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
    assert(!stream_live && app_grants==1 && grant->api==&audio_api);
    grant->api=NULL;app_grants--;return true;
}
static bool diagnostic(const char *message) {(void)message;assert(!"unexpected retained failure");return false;}
static void yield_ms(uint32_t value) {(void)value;assert(!"unexpected retained failure");}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),
    .yield_ms=yield_ms,.diagnostic=diagnostic,.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {assert(version==1);return &runtime_api;}
static int32_t dimension(void) {return 240;}
static uint32_t app_millis(void) {return (uint32_t)milliseconds;}
static void clear(void) {}
static void text(int32_t x,int32_t y,const char *value) {assert(x>=0 && y>=0 && value);}
static void label(int32_t x,int32_t y,int32_t width,const char *value) {assert(x>=0 && y>=0 && width>0 && x+width<=240 && value);}
static void rect(int32_t x,int32_t y,int32_t width,int32_t height,bool black) {
    (void)black;assert(x>=0 && y>=0 && width>0 && height>0 && x+width<=240 && y+height<=240);
}
static void present(bool full) {assert(!full);presents++;}
static bool poll(t5_app_input_t *input,uint32_t wait) {
    assert(wait==0 || wait==30);assert(++polls<=6);milliseconds+=16;
    memset(input,0,sizeof(*input));
    if(polls==1 || polls==5)input->buttons=T5_APP_BUTTON_CONFIRM;
    if(polls==6)input->buttons=T5_APP_BUTTON_BACK;
    return true;
}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),
    .screen_width=dimension,.screen_height=dimension,.clear=clear,.draw_text=text,
    .draw_label=label,.fill_rect=rect,.present=present,.poll=poll,.millis=app_millis};
const t5_app_api_v1 *t5_app_get_api(uint32_t version) {assert(version==1);return &app_api;}
static alarm_status_v1 status(const alarm_service_v1 *service) {
    alarm_status_v1 out={.struct_size=sizeof(out)};assert(service->status(NULL,&out)==ALARM_OK);return out;
}
static void step(const alarm_service_v1 *service) {assert(service->step(NULL)==ALARM_OK);milliseconds++;}
static void run(unsigned percent) {
    assert(!stream_live && !app_grants);memset(bytes,0,sizeof(bytes));memset(sizes,0,sizeof(sizes));
    milliseconds=0;frequency_stage=false;alarm_percent=percent;
    alarm_opens=alarm_writes=alarm_peak=frequency_opens=frequency_writes=0;
    frequency_pcm_peak=frequency_wire_peak=gain_calls=alarm_closes=frequency_closes=polls=presents=0;
    /* Prove the service actively neutralizes an old provider gain. */
    physical_gain=17;physical_maximum=100;
    alarm_config config={.revision=1,.deadline=101,.created=100,.kind=ALARM_KIND_ALARM,.enabled=1};
    alarm_config_encode(&config,bytes[0]);sizes[0]=ALARM_RECORD_SIZE;
    bytes[2][0]=ALARM_MODE_SOUND;sizes[2]=1;bytes[7][0]=(uint8_t)percent;sizes[7]=1;
    const risc_driver_v2 *driver=t5_driver_get(2);assert(driver && driver->start(dependencies,5));
    const alarm_service_v1 *service=driver->capability;
    for(unsigned i=0;!alarm_writes && i<64;i++)step(service);
    alarm_status_v1 active=status(service);
    assert(active.state==ALARM_STATE_ALERT && active.occurrence.generation && alarm_writes==1);
    assert(alarm_opens==1 && gain_calls==1 && alarm_peak==(percent==10?3276u:29490u));
    assert(service->acknowledge(NULL,&active.occurrence)==ALARM_PENDING);
    for(unsigned i=0;status(service).state!=ALARM_STATE_READY && i<64;i++)step(service);
    alarm_status_v1 stopped=status(service);
    assert(stopped.state==ALARM_STATE_READY && !stopped.output_uncertain && !stream_live);
    assert(stopped.schedules[0].state==ALARM_SCHEDULE_DISMISSED && alarm_closes==1);
    alarm_occurrence durable;assert(alarm_occurrence_decode(&durable,bytes[3],sizes[3],ALARM_KIND_ALARM));
    assert(durable.state==ALARM_OCC_ACKED && physical_gain==100 && physical_maximum==100);
    frequency_stage=true;app_main(); /* Actual default 440 Hz / 5% Start, Stop, Back. */
    assert(frequency_opens==1 && frequency_writes==4 && frequency_closes==1 && polls==6 && presents>=2);
    assert(!stream_live && !app_grants && gain_calls==1);
    assert(frequency_pcm_peak>=1637 && frequency_pcm_peak<=1638);
    assert(frequency_wire_peak==frequency_pcm_peak && physical_gain==100 && physical_maximum==100);
    assert(driver->quiesce());
    printf("Alarm %u%% peak %u -> ACK/cleanup -> real Frequency 5%% peak %u: unity gain, no overlap, cleanup passed\n",
        percent,alarm_peak,frequency_wire_peak);
}
int main(void) {run(10);run(90);return 0;}
