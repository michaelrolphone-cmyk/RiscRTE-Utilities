/* Real service + timestamped RF provider. No learning state is mocked. */
#include "../../Services/contexts/service.c"
#include <assert.h>
#include <stdio.h>
static uint64_t now_ms=1000,read_us;
static bool live,no_data,gaps;
static unsigned starts,stops;
static uint64_t clock_now(void*c){(void)c;return now_ms;}
static bool mic_open(void*c,uint32_t rate){(void)c;(void)rate;return false;}
static bool mic_read(void*c,int16_t*p,size_t n,size_t*g){(void)c;(void)p;(void)n;*g=0;return false;}
static bool mic_close(void*c){(void)c;return true;}
static bool radio_suspend(void*c){(void)c;if(live)stops++;live=false;return true;}
static int burst(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){(void)c;(void)p;(void)n;(void)s;(void)f;assert(0);return -1;}
static int radio_begin(void*c,const risc_radio_iq_settings_v1*s,uint32_t interval,risc_radio_iq_format_v1*f){
 (void)c;assert(!live&&interval==1000);live=true;starts++;read_us=now_ms*1000;
 *f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=RISC_RADIO_IQ_FLAG_COHERENT_BURST,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=256,.sample_format=RISC_RADIO_IQ_FORMAT_S10_I0_Q10,.component_bits=10,.component_full_scale=512};return 0;
}
static int envelope(void*c,risc_radio_iq_envelope_v1*out,uint32_t cap,uint32_t*n,uint32_t*d){
 (void)c;assert(live);*n=*d=0;
 if(no_data){read_us=now_ms*1000;return 0;}
 while(*n<cap&&read_us+1000<=now_ms*1000){read_us+=1000;unsigned phase=(unsigned)(read_us/1000%100);
  out[*n]=(risc_radio_iq_envelope_v1){.timestamp_us=read_us,.magnitude=phase<8?.5f:.02f,.observed_us=3,.sequence=(uint32_t)(read_us/1000),.flags=gaps&&phase==0?RISC_RADIO_IQ_ENVELOPE_GAP:0};++*n;}
 return 0;
}
static int latest(void*c,uint32_t*p,uint32_t count,uint64_t*stamp){
 (void)c;assert(live&&count==256);if(no_data)return RISC_RADIO_IQ_BUSY;
 for(unsigned i=0;i<count;i++)p[i]=(uint32_t)((i%8<4?200:-200)&1023);
 *stamp=now_ms*1000;return 0;
}
static bool radio_end(void*c){(void)c;assert(live);live=false;stops++;return true;}
static const risc_platform_clock_api_v1 clock_ops={.api_version=1,.struct_size=sizeof(clock_ops),.monotonic_ms=clock_now};
static const twatch_audio_in_api_v1 mic_ops={.api_version=1,.struct_size=sizeof(mic_ops),.open=mic_open,.read=mic_read,.close=mic_close};
static const risc_radio_iq_temporal_api_v1 radio_ops={.base={.base={.base={.api_version=1,.struct_size=sizeof(radio_ops),.suspend=radio_suspend}},.capture_configured=burst},.temporal_abi=RISC_RADIO_IQ_TEMPORAL_ABI,.start_envelope=radio_begin,.read_envelope=envelope,.latest_iq=latest,.stop_envelope=radio_end};
static uint8_t saved[CF_STORE_MAX];static cf_bank decoded;
int main(void){
 const risc_driver_v2*d=t5_driver_get(2);risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock_ops},{"radio.iq",1,&radio_ops},{"audio.input",1,&mic_ops}};
 for(unsigned mode=0;mode<5;mode++){
  assert(d->start(deps,CONTEXTS_HAS_AUDIO?3:2));no_data=mode==1||mode==3||mode==4;gaps=mode==2;
  const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(d->capability);assert(f);
  contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=CONTEXTS_RADIO};assert(f->fingerprint(NULL,CONTEXTS_FP_CONFIG,&cfg));
  contexts_learning_v1 q={.struct_size=sizeof(q),.operation=CONTEXTS_LEARN_BEGIN,.kind=mode==4?CF_EVENT:CF_ROOM,.name="Test room"};assert(f->fingerprint(NULL,CONTEXTS_FP_LEARN,&q));
  contexts_policy_v1 policy={sizeof(policy),true,true,false,true,CONTEXTS_RADIO};uint64_t began=now_ms;
  do {
   now_ms+=10;if(mode==3&&now_ms-began>=22000)no_data=false;
   assert(f->base.step(NULL,&policy));q.operation=CONTEXTS_LEARN_POLL;assert(f->fingerprint(NULL,CONTEXTS_FP_LEARN,&q));
   if(now_ms-began<30000)assert(q.state!=CONTEXTS_LEARN_READY);
   if(now_ms-began==30000&&mode==3)assert(q.progress==100&&q.state==CONTEXTS_LEARN_RECORDING&&q.samples<3);
  }while(q.state!=CONTEXTS_LEARN_READY&&q.state!=CONTEXTS_LEARN_FAILED&&now_ms-began<(mode==4?121000u:46000u));
  assert(q.progress==(mode==4?0u:100u));
  if(mode==0||mode==3){assert(q.state==CONTEXTS_LEARN_READY&&q.samples>=3);q.operation=CONTEXTS_LEARN_SAVE;assert(f->fingerprint(NULL,CONTEXTS_FP_LEARN,&q));
   contexts_fingerprint_record_v1 record={.struct_size=sizeof(record),.source=CONTEXTS_RADIO,.capacity=sizeof(saved),.bytes=saved};assert(f->fingerprint(NULL,CONTEXTS_FP_EXPORT,&record));assert(cf_decode_source(&decoded,CF_RF,saved,record.size));int slot=cf_profile_find(&decoded,CF_ROOM,"Test room");assert(slot>=0&&decoded.profiles[slot].feature[CF_RF].windows>=3);
  }else{assert(q.state==CONTEXTS_LEARN_FAILED&&q.samples==0&&now_ms-began==(mode==4?120000u:45000u));q.operation=CONTEXTS_LEARN_SAVE;assert(!f->fingerprint(NULL,CONTEXTS_FP_LEARN,&q));}
  assert(f->base.pause(NULL)&&!live);assert(d->quiesce());d->stop();
  printf("Real RF learning mode=%u completed after %llu ms, %u windows, state=%u PASS\n",mode,(unsigned long long)(now_ms-began),q.samples,q.state);
 }
 assert(starts==stops);return 0;
}
