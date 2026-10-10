/* Minimal before/after probe against the production app's capture function.
 * No replacement capture policy; only provider, clock and policy transports. */
#include "../../Apps/waterfall.c"
#include <assert.h>
static uint32_t tick;
static unsigned calls;
static int injected;
static uint32_t now(void){return tick;}
static bool diagnostic(const char *line){assert(line);return true;}
static int32_t get_policy(void*c,const char*k,void*out,uint32_t capacity,uint32_t*used){(void)c;assert(!strcmp(k,"quick_radio")&&capacity==4);uint8_t b[]={0x51,1,3,0xa6};memcpy(out,b,4);*used=4;return RISC_KEY_VALUE_OK;}
static int32_t put_policy(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;assert(0);return -1;}
static const risc_key_value_v1 key_value={1,sizeof(key_value),NULL,get_policy,put_policy};
static bool clean(void*c){(void)c;return true;}
static int burst(void*c,uint32_t*p,uint32_t count,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){
 (void)c;++calls;assert(running&&owned&&count==256);if(injected)return injected;
 for(unsigned i=0;i<count;i++)p[i]=(i%4==0)?256u:(i%4==1)?(256u<<10):(i%4==2)?768u:(768u<<10);
 *f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=RISC_RADIO_IQ_FLAG_COHERENT_BURST,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=count,.sample_format=RISC_RADIO_IQ_FORMAT_S10_I0_Q10,.component_bits=10,.component_full_scale=512};return RISC_RADIO_IQ_OK;
}
static const risc_radio_iq_extended_api_v1 radio={.base={.base={.api_version=1,.struct_size=sizeof(radio),.suspend=clean}},.capture_configured=burst};
static const t5_app_api_v1 fake_app={.abi_version=1,.struct_size=sizeof(fake_app),.millis=now};
static const risc_runtime_api_v1 fake_runtime={.api_version=1,.struct_size=sizeof(fake_runtime),.diagnostic=diagnostic};
int main(void){
 rf_bulk=&rf_resident_bulk;memset(rf_bulk,0,sizeof(*rf_bulk));prefs=rf_preferences_default();app=&fake_app;runtime=waterfall_runtime=&fake_runtime;waterfall_radio=&radio.base.base;preferences=&key_value;receiver_extended=true;capture_requested=running=owned=waterfall_enabled=true;event_slot=-1;
 rf_signature_init(&signature_audio);configure_dsp();assert(rf_dsp_init(&spectrum,&dsp_config));
 for(unsigned i=0;i<3;i++){tick+=1000;capture();}
 assert(calls==3&&history_count==3&&portable_radio_capture_active());
 injected=RISC_RADIO_IQ_DUMP_TIMEOUT;tick+=1000;capture();
 fprintf(stderr,"RF_PROBE_AFTER_FAILURE calls=%u history=%u running=%u requested=%u active=%u\n",calls,history_count,running,capture_requested,portable_radio_capture_active());
 assert(running&&capture_requested&&portable_radio_capture_active());
 injected=0;tick+=1000;capture();assert(calls==5&&history_count==4&&!capture_error);
 puts("Three successful bursts, one clean timeout, automatic recovery PASS");return 0;
}
