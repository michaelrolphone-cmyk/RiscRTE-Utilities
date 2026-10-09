/* Actual provider plus canonical saved-model fixtures. Training happens only
 * here to create owner checkpoints; production Contexts has no trainer. */
#include "../../Services/contexts/service.c"
#define main rf_existing_model_tests
#include "rf_temporal_model_test.c"
#undef main
#include "../../tests/spectrum_neural_fixture.h"
#include <math.h>
static st_library sound;
static st_library full_audio;
static rt_library full_radio;
static sn_trainer learned;
static uint8_t sound_banks[2][ST_BANK_MAX],radio_banks[2][RT_BANK_MAX],sound_checkpoint[SN_RECORD_SIZE],radio_checkpoint[RN_RECORD_SIZE];
static uint32_t sound_sizes[2],radio_sizes[2];
static uint64_t test_now=1000,producer_at,produced,consumed,queued;
static unsigned input_opens,input_closes,input_reads,overruns,model_delay,seen_work;
static bool input_live,close_refused;
static uint64_t test_clock(void*c){
 (void)c;if(ct_a.match.work_units!=seen_work){test_now+=model_delay;seen_work=ct_a.match.work_units;}return test_now;
}
static void producer(void){if(input_live){uint64_t added=(test_now-producer_at)*16u;producer_at=test_now;produced+=added;queued+=added;if(queued>512)overruns++;}}
static bool input_open(void*c,uint32_t rate){(void)c;assert(rate==16000&&!input_live);input_live=true;input_opens++;producer_at=test_now;produced=consumed=queued=0;return true;}
static bool input_read(void*c,int16_t*pcm,size_t count,size_t*got){(void)c;assert(input_live&&count==256);producer();assert(queued>=256);memset(pcm,0,count*sizeof(*pcm));*got=count;queued-=count;consumed+=count;input_reads++;return true;}
static bool input_close(void*c){(void)c;assert(input_live);input_closes++;if(close_refused)return false;input_live=false;return true;}
static bool receiver_suspend(void*c){(void)c;assert(!"no retained RF in this fixture");return false;}
static int receiver_capture(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){(void)c;(void)p;(void)n;(void)s;(void)f;return RISC_RADIO_IQ_BUSY;}
static const risc_platform_clock_api_v1 clock_table={.api_version=1,.struct_size=sizeof(clock_table),.monotonic_ms=test_clock};
static const twatch_audio_in_api_v1 input_table={.api_version=1,.struct_size=sizeof(input_table),.open=input_open,.read=input_read,.close=input_close};
static const risc_radio_iq_extended_api_v1 receiver_table={.base={.base={.api_version=1,.struct_size=sizeof(receiver_table),.suspend=receiver_suspend}},.capture_configured=receiver_capture};
static const contexts_service_v1 *provider;
static contexts_policy_v1 test_policy={sizeof(test_policy),true,true,true,false,CONTEXTS_AUDIO};
static contexts_source_status_v1 source_view(unsigned source){contexts_status_v1 out={.struct_size=sizeof(out)};assert(provider->status(NULL,&out));return source==CONTEXTS_AUDIO?out.audio:out.radio;}
static contexts_model_details_v1 details(unsigned source){contexts_model_details_v1 out={.struct_size=sizeof(out)};assert(provider->model_details(NULL,source,&out));return out;}
static void prepare_models(void){
 memset(&sound,0,sizeof(sound));sound.generation[0]=3;sound.generation[1]=5;
 for(unsigned i=0;i<2;i++){st_label*l=&sound.labels[i];l->present=true;l->next_id=6;snprintf(l->name,sizeof(l->name),"Sound %u",i);
  for(unsigned j=0;j<3;j++)l->examples[j]=sn_fixture_example(i,j,ST_POSITIVE);
  l->examples[3]=sn_fixture_example(2,3,ST_NEGATIVE);l->examples[4]=sn_fixture_example(2,4,ST_NEGATIVE);
 }
 sn_reset(&learned,&sound,255);unsigned ticks=0;
 while(learned.state>=SN_PREPARING&&learned.state<=SN_CHECKING){sn_tick(&learned,&sound);assert(++ticks<4000);}
 assert(learned.has_active);uint32_t crc[2];
 for(unsigned bank=0;bank<2;bank++){sound_sizes[bank]=(uint32_t)st_bank_encode(&sound,bank,sound_banks[bank],sizeof(sound_banks[bank]));assert(sound_sizes[bank]);crc[bank]=spectrum_signature_u32(sound_banks[bank]+sound_sizes[bank]-4);}
 assert(sn_record_encode(&learned,&sound,crc,sound_checkpoint,sizeof(sound_checkpoint)));
 setup_neural();rn_reset(&trainer,&library,255);finish_neural(&trainer);assert(trainer.has_active);
 for(unsigned bank=0;bank<2;bank++){radio_sizes[bank]=(uint32_t)rt_bank_encode(&library,bank,radio_banks[bank],sizeof(radio_banks[bank]));assert(radio_sizes[bank]);crc[bank]=rf_signature_u32(radio_banks[bank]+radio_sizes[bank]-4);}
 assert(rn_record_encode(&trainer,&library,crc,radio_checkpoint,sizeof(radio_checkpoint)));
}
static void begin_source(uint32_t source){
 assert(provider->request_export(NULL,source)&&provider->begin_export(NULL,source));uint8_t record[RF_SIGNATURE_RECORD_SIZE];
 if(source==CONTEXTS_AUDIO){spectrum_preferences p=spectrum_preferences_default();spectrum_preferences_encode(&p,record);assert(provider->export_record(NULL,source,CONTEXTS_RECORD_PREFERENCES,0,record,SPECTRUM_PREFERENCES_SIZE));}
 else{rf_preferences p=rf_preferences_default();assert(rf_preferences_encode(&p,record));assert(provider->export_record(NULL,source,CONTEXTS_RECORD_PREFERENCES,0,record,RF_PREFERENCES_SIZE));}
 for(unsigned i=0;i<8;i++){
  if(source==CONTEXTS_AUDIO){spectrum_signature s={0};assert(spectrum_signature_encode(&s,record));assert(provider->export_record(NULL,source,CONTEXTS_RECORD_SIGNATURE,i,record,SPECTRUM_SIGNATURE_RECORD_SIZE));}
  else{rf_signature s={.identity=rf_identity_default()};assert(rf_signature_encode(&s,record));assert(provider->export_record(NULL,source,CONTEXTS_RECORD_SIGNATURE,i,record,RF_SIGNATURE_RECORD_SIZE));}
 }
}
static void banks(uint32_t source){for(unsigned bank=0;bank<2;bank++)assert(provider->export_record(NULL,source,CONTEXTS_RECORD_TEMPORAL_BANK,bank,source==CONTEXTS_AUDIO?sound_banks[bank]:radio_banks[bank],source==CONTEXTS_AUDIO?sound_sizes[bank]:radio_sizes[bank]));}
static void import_models(uint32_t source,bool neural){begin_source(source);banks(source);assert(provider->export_record(NULL,source,CONTEXTS_RECORD_NEURAL,0,neural?(source==CONTEXTS_AUDIO?sound_checkpoint:radio_checkpoint):NULL,neural?(source==CONTEXTS_AUDIO?SN_RECORD_SIZE:RN_RECORD_SIZE):0));assert(provider->finish_export(NULL,source,0));}
static void match_audio(const st_example *query,bool neural_expected,int selected){
 ct_reset(0);st_match_begin(&ct_a.match,query);ct_m[0].query_at=test_now;ct_m[0].details.match_pending=true;
 unsigned ticks=0;while(ct_a.match.running){unsigned work=ct_a.match.work_units;ct_tick(0);assert(ct_a.match.work_units-work<=8&&++ticks<1000);}
 assert(ct_m[0].event_slot==selected&&ct_m[0].event_valid==(selected>=0));
 assert(ct_m[0].details.event_engine==(neural_expected?CONTEXTS_EVENT_NEURAL:CONTEXTS_EVENT_TEMPORAL));
}
static void match_radio(const rt_example *query,bool neural_expected,int selected){
 ct_reset(1);rt_match_begin(&ct_r.match,query);ct_m[1].query_at=test_now;ct_m[1].details.match_pending=true;
 unsigned ticks=0;while(ct_r.match.running){unsigned work=ct_r.match.work_units;ct_tick(1);assert(ct_r.match.work_units-work<=8&&++ticks<1000);}
 assert(ct_m[1].event_slot==selected&&ct_m[1].event_valid==(selected>=0));
 assert(ct_m[1].details.event_engine==(neural_expected?CONTEXTS_EVENT_NEURAL:CONTEXTS_EVENT_TEMPORAL));
}
static void import_tests(void){
 import_models(CONTEXTS_AUDIO,true);contexts_source_status_v1 sv=source_view(CONTEXTS_AUDIO);assert(sv.signatures_ready&&sv.temporal_ready&&sv.neural_ready);
 uint32_t room_generation=sv.model_generation;assert(!memcmp(&ct_a.library,&sound,sizeof(sound))&&!memcmp(&ct_a.neural,&learned.active,sizeof(sn_model)));
 contexts_model_details_v1 d=details(CONTEXTS_AUDIO);assert(d.positive_examples==6&&d.negative_examples==4&&d.temporal_generation);
 match_audio(&sound.labels[0].examples[2],true,0);match_audio(&sound.labels[1].examples[2],true,1);match_audio(&sound.labels[0].examples[4],false,-1);
 import_models(CONTEXTS_RADIO,true);sv=source_view(CONTEXTS_RADIO);assert(sv.temporal_ready&&sv.neural_ready&&!memcmp(&ct_r.library,&library,sizeof(library))&&!memcmp(&ct_r.neural,&trainer.active,sizeof(rn_model)));
 match_radio(&library.labels[0].examples[2],true,0);match_radio(&library.labels[1].examples[2],true,1);match_radio(&library.labels[0].examples[4],false,-1);
 import_models(CONTEXTS_AUDIO,false);assert(source_view(CONTEXTS_AUDIO).model_generation==room_generation);match_audio(&sound.labels[0].examples[2],false,-1);assert(ct_m[0].event_ambiguous);
 uint8_t bad[RN_RECORD_SIZE];memcpy(bad,sound_checkpoint,SN_RECORD_SIZE);spectrum_signature_put32(bad+28,sound.generation[0]+1);spectrum_signature_put32(bad+SN_RECORD_SIZE-4,spectrum_signature_crc(bad,SN_RECORD_SIZE-4));assert(sn_record_valid(bad,SN_RECORD_SIZE));
 begin_source(CONTEXTS_AUDIO);banks(CONTEXTS_AUDIO);assert(!provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_NEURAL,0,bad,SN_RECORD_SIZE));assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));assert(source_view(CONTEXTS_AUDIO).temporal_ready&&!source_view(CONTEXTS_AUDIO).neural_ready&&details(CONTEXTS_AUDIO).neural_state==CONTEXTS_IMPORT_FAILED);
 memcpy(bad,radio_checkpoint,RN_RECORD_SIZE);rf_capture_identity other=library.identity;other.raw_gain++;rf_identity_encode(&other,bad+64);rf_signature_put32(bad+RN_RECORD_SIZE-4,rf_signature_crc(bad,RN_RECORD_SIZE-4));assert(rn_record_valid(bad,RN_RECORD_SIZE));
 begin_source(CONTEXTS_RADIO);banks(CONTEXTS_RADIO);assert(!provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_NEURAL,0,bad,RN_RECORD_SIZE));assert(provider->finish_export(NULL,CONTEXTS_RADIO,0));assert(source_view(CONTEXTS_RADIO).temporal_ready&&!source_view(CONTEXTS_RADIO).neural_ready);
 begin_source(CONTEXTS_AUDIO);assert(provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,0,sound_banks[0],sound_sizes[0]));assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));assert(source_view(CONTEXTS_AUDIO).signatures_ready&&!source_view(CONTEXTS_AUDIO).temporal_ready&&details(CONTEXTS_AUDIO).bank_error[1]==CONTEXTS_IMPORT_INCOMPLETE);
 begin_source(CONTEXTS_AUDIO);for(unsigned bank=0;bank<2;bank++)assert(provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,bank,NULL,0));assert(provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_NEURAL,0,NULL,0));assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));assert(details(CONTEXTS_AUDIO).temporal_state==CONTEXTS_IMPORT_MISSING&&!source_view(CONTEXTS_AUDIO).temporal_ready);
 begin_source(CONTEXTS_AUDIO);sound_banks[0][20]^=1;assert(!provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,0,sound_banks[0],sound_sizes[0]));sound_banks[0][20]^=1;assert(provider->export_model_error(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,1,RISC_APP_DATA_STALE));assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));assert(!source_view(CONTEXTS_AUDIO).temporal_ready&&details(CONTEXTS_AUDIO).bank_error[1]==RISC_APP_DATA_STALE);
 begin_source(CONTEXTS_AUDIO);assert(!provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_NEURAL,0,sound_checkpoint,SN_RECORD_SIZE));banks(CONTEXTS_AUDIO);assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));assert(source_view(CONTEXTS_AUDIO).temporal_ready&&!source_view(CONTEXTS_AUDIO).neural_ready);
 begin_source(CONTEXTS_AUDIO);banks(CONTEXTS_AUDIO);assert(!provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,0,sound_banks[0],sound_sizes[0]));assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));assert(!source_view(CONTEXTS_AUDIO).temporal_ready);
}
static void import_full_audio(void){
 begin_source(CONTEXTS_AUDIO);
 for(unsigned bank=0;bank<2;bank++){size_t size=st_bank_encode(&full_audio,bank,bytes,sizeof(bytes));assert(size==ST_BANK_MAX);assert(provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_TEMPORAL_BANK,bank,bytes,(uint32_t)size));}
 assert(provider->export_record(NULL,CONTEXTS_AUDIO,CONTEXTS_RECORD_NEURAL,0,NULL,0));assert(provider->finish_export(NULL,CONTEXTS_AUDIO,0));
}
static void full_bank_tests(void){
 full_audio=(st_library){.generation={7,9}};full_radio=(rt_library){.identity=rf_identity_default(),.generation={11,13}};
 st_example audio=sound.labels[0].examples[0];rt_example radio=library.labels[0].examples[0];
 unsigned audio_count=audio.count,radio_count=radio.count;audio.count=radio.count=64;audio.flags=ST_CLIPPED|ST_CONFIRMED_END;radio.flags=RT_CLIPPED|RT_CONFIRMED_END;
 for(unsigned i=0;i<64;i++){audio.frames[i]=audio.frames[i%audio_count];radio.frames[i]=radio.frames[i%radio_count];radio.frames[i].timestamp_ms=1000u+i*100u;}
 st_summarize(&audio);rt_summarize(&radio);assert(st_example_valid(&audio)&&rt_example_valid(&radio));
 for(unsigned slot=0;slot<8;slot++){
  st_label *a=&full_audio.labels[slot];rt_label *r=&full_radio.labels[slot];a->present=r->present=true;a->next_id=r->next_id=7;
  snprintf(a->name,17,"Audio %u",slot);snprintf(r->name,17,"Radio %u",slot);
  for(unsigned i=0;i<6;i++){a->examples[i]=audio;r->examples[i]=radio;a->examples[i].id=r->examples[i].id=i+1;a->examples[i].kind=r->examples[i].kind=i<3?ST_POSITIVE:ST_NEGATIVE;}
 }
 import_full_audio();assert(!memcmp(&ct_a.library,&full_audio,sizeof(full_audio)));assert(details(CONTEXTS_AUDIO).positive_examples==24&&details(CONTEXTS_AUDIO).negative_examples==24);
 begin_source(CONTEXTS_RADIO);
 for(unsigned bank=0;bank<2;bank++){size_t size=rt_bank_encode(&full_radio,bank,bytes,sizeof(bytes));assert(size==RT_BANK_MAX);assert(provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_TEMPORAL_BANK,bank,bytes,(uint32_t)size));}
 assert(provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_NEURAL,0,NULL,0));assert(provider->finish_export(NULL,CONTEXTS_RADIO,0));assert(!memcmp(&ct_r.library,&full_radio,sizeof(full_radio)));
 uint32_t power[128]={0};power[30]=100000000u;
 ct_radio_observe(true,true,power,&full_radio.identity,UINT32_MAX-100u);assert(ct_r.segment.collecting&&ct_r.segment.event.count==1);
 ct_radio_observe(true,true,power,&full_radio.identity,(uint64_t)UINT32_MAX+1u);assert(ct_r.segment.event.count==1&&ct_r.segment.event.frames[0].timestamp_ms==0&&(ct_r.segment.event.frames[0].flags&RT_GAP_BEFORE));
 rf_capture_identity other=full_radio.identity;other.raw_gain++;
 ct_radio_observe(true,true,power,&other,(uint64_t)UINT32_MAX+101u);assert(!ct_r.segment.collecting&&!ct_r.match.running);
}
static void finite_matching_test(bool worst){
 if(worst)import_full_audio();else import_models(CONTEXTS_AUDIO,true);
 assert(provider->step(NULL,&test_policy));
 for(unsigned tick=0;tick<400;tick++){test_now+=8;assert(provider->capture_audio(NULL));if(tick%3==0)assert(provider->step(NULL,&test_policy));}
 assert(a.background.ready&&!overruns);
 st_match_begin(&ct_a.match,worst?&full_audio.labels[0].examples[2]:&sound.labels[0].examples[2]);ct_m[0].query_at=test_now;ct_m[0].details.match_pending=true;seen_work=0;model_delay=2;
 unsigned opens=input_opens,tick=0;
 while(ct_a.match.running){test_now+=8;unsigned work=ct_a.match.work_units;assert(provider->capture_audio(NULL));assert(ct_a.match.work_units==work);if(++tick%3==0)assert(provider->step(NULL,&test_policy));producer();assert(!overruns&&queued<512&&produced==consumed+queued&&tick<1000);}
 model_delay=0;assert(input_opens==opens);
 if(worst)assert(!source_view(CONTEXTS_AUDIO).event_valid&&details(CONTEXTS_AUDIO).event_age_ms>EVENT_HOLD_MS);
 else{assert(ct_m[0].event_slot==0&&ct_m[0].details.event_engine==CONTEXTS_EVENT_NEURAL);contexts_source_status_v1 s=source_view(CONTEXTS_AUDIO);assert(s.event_valid&&!strcmp(s.event_name,"Sound 0"));}
 assert(provider->pause(NULL));assert(!ct_a.match.running&&!ct_m[0].event_valid&&source_view(CONTEXTS_AUDIO).temporal_ready);
}
static void rf_only_models_test(void){
 import_models(CONTEXTS_RADIO,true);
 contexts_source_status_v1 s=source_view(CONTEXTS_RADIO);
 assert(s.signatures_ready&&s.temporal_ready&&s.neural_ready);
 assert(!memcmp(&ct_r.library,&library,sizeof(library))&&!memcmp(&ct_r.neural,&trainer.active,sizeof(rn_model)));
 assert(details(CONTEXTS_RADIO).positive_examples==6&&details(CONTEXTS_RADIO).negative_examples==4);
 assert(source_view(CONTEXTS_AUDIO).model_state==CONTEXTS_MODEL_UNAVAILABLE);
 match_radio(&library.labels[0].examples[2],true,0);
 match_radio(&library.labels[1].examples[2],true,1);
 match_radio(&library.labels[0].examples[4],false,-1);
 uint32_t generation=s.model_generation;
 import_models(CONTEXTS_RADIO,false);assert(source_view(CONTEXTS_RADIO).model_generation==generation);
 match_radio(&library.labels[0].examples[2],false,-1);assert(ct_m[1].event_ambiguous);
 uint8_t bad[RN_RECORD_SIZE];memcpy(bad,radio_checkpoint,sizeof(bad));
 rf_capture_identity other=library.identity;other.raw_gain++;rf_identity_encode(&other,bad+64);
 rf_signature_put32(bad+RN_RECORD_SIZE-4,rf_signature_crc(bad,RN_RECORD_SIZE-4));assert(rn_record_valid(bad,sizeof(bad)));
 begin_source(CONTEXTS_RADIO);banks(CONTEXTS_RADIO);
 assert(!provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_NEURAL,0,bad,sizeof(bad)));
 assert(provider->finish_export(NULL,CONTEXTS_RADIO,0));
 assert(source_view(CONTEXTS_RADIO).temporal_ready&&!source_view(CONTEXTS_RADIO).neural_ready&&details(CONTEXTS_RADIO).neural_error==CONTEXTS_IMPORT_STALE);
 begin_source(CONTEXTS_RADIO);
 assert(provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_TEMPORAL_BANK,0,radio_banks[0],radio_sizes[0]));
 assert(provider->finish_export(NULL,CONTEXTS_RADIO,0));
 assert(!source_view(CONTEXTS_RADIO).temporal_ready&&details(CONTEXTS_RADIO).bank_error[1]==CONTEXTS_IMPORT_INCOMPLETE);
 begin_source(CONTEXTS_RADIO);
 for(unsigned b=0;b<2;b++)assert(provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_TEMPORAL_BANK,b,NULL,0));
 assert(provider->export_record(NULL,CONTEXTS_RADIO,CONTEXTS_RECORD_NEURAL,0,NULL,0));
 assert(provider->finish_export(NULL,CONTEXTS_RADIO,0));assert(details(CONTEXTS_RADIO).temporal_state==CONTEXTS_IMPORT_MISSING);
 import_models(CONTEXTS_RADIO,true);
 ct_reset(1);rt_match_begin(&ct_r.match,&library.labels[0].examples[2]);ct_m[1].details.match_pending=true;ct_m[1].query_at=test_now;
 unsigned work=ct_r.match.work_units;assert(provider->capture_audio(NULL));assert(ct_r.match.work_units==work);
 contexts_policy_v1 rf_policy={sizeof(rf_policy),true,true,true,true,CONTEXTS_ALL};
 radio_active=true;radio_at=test_now; /* No new burst is due while matching this observation. */
 unsigned rounds=0;
 while(ct_r.match.running){work=ct_r.match.work_units;assert(provider->step(NULL,&rf_policy));assert(ct_r.match.work_units>work&&ct_r.match.work_units-work<=128&&++rounds<1000);}
 assert(!input_opens&&!input_reads&&!input_closes);
 puts("RF-only temporal/neural: real canonical imports, identity/stale/incomplete/missing gates, bounded matching and no Audio capture PASS");
}
int main(void){
 prepare_models();const risc_driver_v2 *driver=t5_driver_get(2);assert(driver);
 const risc_provider_dependency_v1 dependencies[]={{"platform.clock",1,&clock_table},{"audio.input",1,&input_table},{"radio.iq",1,&receiver_table}};
 const risc_provider_dependency_v1 rf_dependencies[]={dependencies[0],dependencies[2]};
 assert(driver->start(CONTEXTS_RF_ONLY?rf_dependencies:dependencies,CONTEXTS_RF_ONLY?2u:3u));provider=driver->capability;
 if(CONTEXTS_RF_ONLY)rf_only_models_test();
 else{import_tests();full_bank_tests();finite_matching_test(false);finite_matching_test(true);}assert(driver->quiesce());driver->stop();
 if(!CONTEXTS_RF_ONLY)printf("Contexts temporal/neural: copied canonical libraries, active checkpoint identity, ambiguity/negative fallback, retained room generation, incomplete/stale import, bounded matching with finite512-frame RX PASS (%zu model bytes)\n",sizeof(ct_a)+sizeof(ct_r)+sizeof(ct_m));
}
