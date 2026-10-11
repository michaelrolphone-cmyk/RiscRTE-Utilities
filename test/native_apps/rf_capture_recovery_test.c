/* Production RF capture path. Only receiver/policy/clock I/O is simulated.
 * The selected resident build defines and exact compiled SDK come from a receipt.
 * No hardware, firmware, driver, or timing qualification is claimed here. */
#include "../../Apps/waterfall.c"
#include <assert.h>

static uint32_t tick;
static unsigned calls,suspends,diags,policy_reads,cleanup_failures;
static unsigned retry_logs,recovered_logs,stopped_logs;
static int injected,format_fault;
static uint8_t policy_flags;
static bool policy_ok,retained,diagnostic_ok;
static uint32_t capture_duration;
static char last_retry[128],last_recovered[128],last_stopped[128];

static uint32_t now(void){return tick;}
static void wait_ms(uint32_t ms){assert(retained);tick+=ms;}
static bool diagnostic(const char *line){
 assert(line);++diags;
 if(strstr(line,"SDR retry ")){++retry_logs;snprintf(last_retry,sizeof(last_retry),"%s",line);}
 if(strstr(line,"SDR recovered ")){++recovered_logs;snprintf(last_recovered,sizeof(last_recovered),"%s",line);}
 if(strstr(line,"SDR stopped ")){++stopped_logs;snprintf(last_stopped,sizeof(last_stopped),"%s",line);}
 return diagnostic_ok;
}
static int32_t get_policy(void*c,const char*k,void*out,uint32_t capacity,uint32_t*used){
 (void)c;assert(!retained&&!strcmp(k,"quick_radio")&&capacity==4);++policy_reads;
 if(!policy_ok)return RISC_KEY_VALUE_IO;
 uint8_t bytes[]={0x51,1,policy_flags,(uint8_t)(policy_flags^0xa5)};
 memcpy(out,bytes,4);*used=4;return RISC_KEY_VALUE_OK;
}
static int32_t put_policy(void*c,const char*k,const void*b,uint32_t n){
 (void)c;(void)k;(void)b;(void)n;assert(!"capture must not write policy");return -1;
}
static const risc_key_value_v1 key_value={1,sizeof(key_value),NULL,get_policy,put_policy};
static bool clean(void*c){
 (void)c;++suspends;
 if(retained){assert(!running&&!waterfall_enabled&&uncertain&&waterfall_uncertain);if(cleanup_failures){--cleanup_failures;return false;}retained=false;}
 return true;
}
static bool capabilities(void*c,risc_radio_iq_capabilities_v1*out){
 (void)c;assert(!retained);*out=(risc_radio_iq_capabilities_v1){.struct_size=sizeof(*out),.min_pairs=1,.max_pairs=8192};return true;
}
static int burst(void*c,uint32_t*p,uint32_t count,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){
 (void)c;++calls;assert(!retained&&running&&owned&&capture_requested&&count==prefs.fft_size);tick+=capture_duration;
 if(injected){if(injected==RISC_RADIO_IQ_CLEANUP_RETAINED)retained=true;return injected;}
 for(unsigned i=0;i<count;i++)p[i]=(i%4==0)?256u:(i%4==1)?(256u<<10):(i%4==2)?768u:(768u<<10);
 *f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=RISC_RADIO_IQ_FLAG_COHERENT_BURST,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=count,.sample_format=RISC_RADIO_IQ_FORMAT_S10_I0_Q10,.component_bits=10,.component_full_scale=512};
 if(format_fault)f->component_bits=9;
 return RISC_RADIO_IQ_OK;
}
static const risc_radio_iq_extended_api_v1 radio={
 .base={.base={.api_version=1,.struct_size=sizeof(radio),.suspend=clean}},
 .capabilities=capabilities,.capture_configured=burst
};
static const t5_app_api_v1 fake_app={.abi_version=1,.struct_size=sizeof(fake_app),.millis=now};
static const risc_runtime_api_v1 fake_runtime={.api_version=1,.struct_size=sizeof(fake_runtime),.yield_ms=wait_ms,.diagnostic=diagnostic};
static void setup(void){
 rf_bulk=&rf_resident_bulk;memset(rf_bulk,0,sizeof(*rf_bulk));prefs=rf_preferences_default();
 app=&fake_app;runtime=waterfall_runtime=&fake_runtime;waterfall_radio=&radio.base.base;preferences=&key_value;
 receiver_extended=true;capture_requested=running=owned=waterfall_enabled=true;uncertain=waterfall_uncertain=false;
 last_burst_at=tick=0;calls=suspends=diags=policy_reads=cleanup_failures=0;injected=format_fault=0;capture_duration=0;
 retry_logs=recovered_logs=stopped_logs=0;memset(last_retry,0,sizeof(last_retry));memset(last_recovered,0,sizeof(last_recovered));memset(last_stopped,0,sizeof(last_stopped));
 policy_flags=3;policy_ok=diagnostic_ok=true;retained=false;trace_capture_seen=trace_cleanup_seen=false;
 event_slot=-1;event_initialized=false;event_armed=false;signature_goal=signature_seen=0;signature_mode=0;manual_room=-1;
 capture_error=input_waiting=input_gap=frozen=false;capture_retry_reset();capture_ok_count=capture_fault_count=0;
 rf_signature_init(&signature_audio);rf_room_reset(&room_tracker);configure_dsp();assert(rf_dsp_init(&spectrum,&dsp_config));
 message="NOMINAL RF / UNCALIBRATED";
}
static void advance(uint32_t ms){tick+=ms;capture();}
static void three(void){for(unsigned i=0;i<3;i++)advance(1000);assert(calls==3&&history_count==3&&portable_radio_capture_active());}
static void assert_stopped(void){assert(!running&&!capture_requested&&!waterfall_enabled&&!owned&&!portable_radio_capture_active()&&!capture_retry_delay);}

static void test_clean_recovery(void){
 static const int codes[]={RISC_RADIO_IQ_PLL_FAILED,RISC_RADIO_IQ_PBUS_FAILED,RISC_RADIO_IQ_DUMP_TIMEOUT,RISC_RADIO_IQ_BUSY};
 for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++){
  setup();three();unsigned normal_diags=diags;
  /* Steady successful capture does not create per-burst diagnostics. */
  advance(1000);assert(diags==normal_diags);unsigned history_before=history_count;
  injected=codes[i];capture_duration=700;advance(1000);
  assert(running&&capture_requested&&portable_radio_capture_active()&&capture_error&&input_waiting&&input_gap);
  assert(history_count==history_before&&capture_ok_count==4&&capture_fault_count==1&&capture_retries==1);
  assert(capture_retry_delay==250&&capture_retry_at==tick&&retry_logs==1&&strstr(last_retry,"ok=4 faults=1"));
  unsigned before=calls,reads=policy_reads,logs=diags;
  for(unsigned n=0;n<249;n++)advance(1);
  assert(calls==before&&policy_reads==reads&&diags==logs);
  injected=0;capture_duration=0;advance(1);
  assert(calls==before+1&&running&&history_count==history_before+1&&!capture_error&&!input_waiting&&!input_gap);
  assert(!capture_retries&&!capture_retry_delay&&capture_ok_count==5&&capture_fault_count==1&&recovered_logs==1);
  assert(strstr(last_recovered,"ok=5 faults=1")&&!strstr(message,"RETRY"));
 }
 puts("PASS clean PLL/PBUS/timeout/busy recovery, completion-based backoff, no false samples or per-burst logging");
}
static void test_bound(void){
 setup();three();injected=RISC_RADIO_IQ_DUMP_TIMEOUT;advance(1000);
 for(unsigned retry=1;retry<=RF_CAPTURE_RETRY_LIMIT;retry++){
  unsigned before=calls;uint32_t delay=250u<<(retry-1);
  assert(capture_retries==retry&&capture_retry_delay==delay);
  advance(delay-1);assert(calls==before);advance(1);assert(calls==before+1);
 }
 assert_stopped();assert(calls==7&&history_count==3&&capture_fault_count==4&&retry_logs==3&&stopped_logs==1&&suspends==1);
 assert(strstr(last_stopped,"retry=3/3")&&strstr(last_stopped,"ok=3 faults=4"));
 unsigned before=calls;injected=0;advance(100000);portable_rf_capture_resume();assert(calls==before);assert_stopped();
 toggle();assert(running&&!capture_retries&&!capture_fault_count&&!capture_ok_count);advance(1000);assert(calls==before+1&&history_count==1);
 puts("PASS three-retry bound, stopped state, no delayed restart, explicit Start reentry reset");
}
static void test_cancel(void){
 for(unsigned kind=0;kind<3;kind++){
  setup();three();injected=RISC_RADIO_IQ_BUSY;advance(1000);unsigned before=calls;
  if(kind==0)stop();else if(kind==1)freeze();else assert(portable_radio_suspend());
  assert_stopped();injected=0;advance(100000);portable_rf_capture_resume();assert(calls==before);assert_stopped();
  if(kind==1)assert(frozen);
 }
 puts("PASS Stop, Freeze and typed suspend cancel pending recovery without foreground restart");
}
static void test_policy_and_fatal(void){
 for(unsigned kind=0;kind<3;kind++){
  setup();three();injected=RISC_RADIO_IQ_DUMP_TIMEOUT;advance(1000);unsigned before=calls;
  if(kind==0)policy_flags=PORTABLE_RADIO_AIRPLANE;else if(kind==1)policy_ok=false;else policy_flags=0xff;
  advance(250);assert_stopped();assert(calls==before&&capture_error&&suspends==1);
 }
 static const int fatal[]={RISC_RADIO_IQ_BAD_ARGUMENT,RISC_RADIO_IQ_NOT_RUNNING,99,-1};
 for(unsigned i=0;i<sizeof(fatal)/sizeof(*fatal);i++){
  setup();three();injected=fatal[i];advance(1000);assert_stopped();assert(retry_logs==0&&stopped_logs==1&&capture_error&&history_count==3);
 }
 setup();three();injected=RISC_RADIO_IQ_DUMP_TIMEOUT;advance(1000);injected=0;format_fault=1;advance(250);assert_stopped();assert(!strcmp(message,"INCOMPATIBLE CAPTURE FORMAT")&&history_count==3&&recovered_logs==0);
 setup();three();injected=RISC_RADIO_IQ_DUMP_TIMEOUT;advance(1000);injected=0;signature_audio.window_power_q16=0;advance(250);assert_stopped();assert(!strcmp(message,"INVALID COHERENT BURST")&&history_count==3&&recovered_logs==0);
 puts("PASS Airplane/unreadable/invalid policy, fatal/unknown codes and invalid capture data remain fail-closed");
}
static void test_retained(void){
 setup();three();injected=RISC_RADIO_IQ_DUMP_TIMEOUT;advance(1000);unsigned before=calls;
 injected=RISC_RADIO_IQ_CLEANUP_RETAINED;cleanup_failures=3;advance(250);
 assert_stopped();assert(!retained&&!uncertain&&!waterfall_uncertain&&suspends==4&&calls==before+1&&recovered_logs==0);
 assert(!strcmp(message,"CLEANUP RECOVERED / START"));
 injected=0;advance(100000);portable_rf_capture_resume();assert(calls==before+1);assert_stopped();
 puts("PASS retained cleanup uses suspend-only drain; no capture/policy access while custody is retained");
}
static void test_wrap_reset_and_diagnostics(void){
 setup();tick=UINT32_MAX-100;injected=RISC_RADIO_IQ_DUMP_TIMEOUT;capture();assert(capture_retry_at==UINT32_MAX-100);
 unsigned before=calls;advance(249);assert(calls==before);injected=0;advance(1);assert(calls==before+1&&running&&!capture_retries);
 setup();injected=RISC_RADIO_IQ_DUMP_TIMEOUT;capture();assert(capture_retry_at==0&&capture_retry_delay==250);
 advance(249);assert(calls==1);injected=0;advance(1);assert(calls==2&&history_count==1);
 /* Successful bursts reset consecutive budget, not total failure evidence. */
 for(unsigned i=0;i<8;i++){injected=RISC_RADIO_IQ_BUSY;advance(1000);assert(capture_retries==1);injected=0;advance(250);assert(!capture_retries&&running);}
 assert(capture_fault_count==9&&capture_ok_count==9);
 setup();diagnostic_ok=false;three();injected=RISC_RADIO_IQ_BUSY;advance(1000);injected=0;advance(250);assert(running&&history_count==4);
 puts("PASS millis wrap/zero, independent error episodes and rejected diagnostics do not break recovery");
}
int main(void){
 test_clean_recovery();test_bound();test_cancel();test_policy_and_fatal();test_retained();test_wrap_reset_and_diagnostics();
 puts("RF production capture recovery tests PASS (simulated provider outcomes; no hardware qualification)");return 0;
}
