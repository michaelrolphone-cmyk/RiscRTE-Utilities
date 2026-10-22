#define SPECTRUM_TEMPORAL_MAIN original_temporal_app_main
#include "spectrum_temporal_app_test.c"
#include "../../tests/spectrum_neural_fixture.h"
static uint8_t neural_disk[SN_RECORD_SIZE],saved_banks[2][ST_BANK_MAX];
static uint32_t neural_size;static unsigned neural_reads,neural_writes;
static int32_t neural_error;static bool neural_unknown,neural_read_error;
static bool neural_name(const char *name){return !strcmp(name,event_neural_file());}
static void neural_guard(void){assert(files_live&&!file_retained&&!running&&!owned&&!live);}
static int32_t neural_stat(void*c,const char*n,uint32_t*s,uint64_t*v){
 if(!neural_name(n))return file_stat(c,n,s,v);
 neural_guard();*s=0;*v=0;neural_reads++;
 if(neural_read_error)return RISC_APP_DATA_IO;
 if(!neural_size)return RISC_APP_DATA_NOT_FOUND;
 *s=neural_size;*v=file_revisions[2];return RISC_APP_DATA_OK;
}
static int32_t neural_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*s,uint64_t*v){
 if(!neural_name(n))return file_read(c,n,expected,out,cap,s,v);
 neural_guard();assert(expected==file_revisions[2]&&cap>=neural_size);neural_reads++;memcpy(out,neural_disk,neural_size);*s=neural_size;*v=file_revisions[2];return RISC_APP_DATA_OK;
}
static int32_t neural_replace(void*c,const char*n,uint64_t expected,const void*in,uint32_t size){
 if(!neural_name(n))return file_replace(c,n,expected,in,size);
 neural_guard();neural_writes++;assert(size==SN_RECORD_SIZE&&expected==(neural_size?file_revisions[2]:0));
 if(neural_error){if(neural_error==RISC_APP_DATA_RETAINED)file_retained=true;return neural_error;}
 memcpy(neural_disk,in,size);neural_size=size;file_revisions[2]=++file_revision;return neural_unknown?RISC_APP_DATA_COMMIT_UNKNOWN:RISC_APP_DATA_OK;
}
static void neural_fresh(void){
 fresh();file_api.stat=neural_stat;file_api.read=neural_read;file_api.replace=neural_replace;
 neural_size=neural_reads=neural_writes=0;neural_error=0;neural_unknown=neural_read_error=false;memset(neural_disk,0,sizeof(neural_disk));
 st_library seeded={0};for(unsigned i=0;i<2;i++){
  st_label *l=&seeded.labels[i];l->present=true;l->next_id=6;snprintf(l->name,sizeof(l->name),"Door %u",i);
  for(unsigned j=0;j<3;j++)l->examples[j]=sn_fixture_example(i,j,ST_POSITIVE);
  l->examples[3]=sn_fixture_example(2,3,ST_NEGATIVE);l->examples[4]=sn_fixture_example(2,4,ST_NEGATIVE);
 }
 for(unsigned b=0;b<2;b++){seeded.generation[b]=1;event_sizes[b]=(uint32_t)st_bank_encode(&seeded,b,event_disk[b],sizeof(event_disk[b]));assert(event_sizes[b]);file_revisions[b]=++file_revision;}
 memcpy(saved_banks,event_disk,sizeof(saved_banks));
}
static void finish_training(void){
 unsigned ticks=0;while(event_neural.state>=SN_PREPARING&&event_neural.state<=SN_CHECKING){event_neural_tick();assert(++ticks<3000);}
 assert(event_neural.state==SN_ACTIVE&&event_neural.has_active&&!memcmp(saved_banks,event_disk,sizeof(saved_banks))&&file_writes==0);
}
static void check_idle_guards(void){
 assert(event_neural.state==SN_PREPARING);unsigned ticks=event_neural.ticks;
 event_armed=true;event_neural_tick();event_armed=false;
 event_training.ready=true;event_neural_tick();event_training.ready=false;
 event_match.running=true;event_neural_tick();event_match.running=false;
 event_segment.collecting=true;event_neural_tick();event_segment.collecting=false;
 signature_goal=64;event_neural_tick();signature_goal=0;
 event_files.pending=0;event_neural_tick();event_files.pending=-1;
 assert(event_neural.ticks==ticks&&!neural_writes);
}
static void check_active_reload(void){
 finish_training();assert(neural_writes==1&&sn_record_valid(neural_disk,neural_size));
 unsigned writes=neural_writes;event_retry();assert(event_neural.has_active&&event_neural.state==SN_ACTIVE&&!event_neural.updates&&neural_writes==writes);
 st_match_begin(&event_match,&event_library.labels[0].examples[2]);while(event_match.running)temporal_tick();
 assert(event_match.selected==0&&event_match.reason==ST_RESULT_MATCH&&event_neural_used);
 monitor_item items[17];assert(monitor_items(items)==1&&items[0].kind==MONITOR_EVENT&&!strcmp(items[0].name,"Door 0"));
 event_open(0);strcpy(editing.name,"New name");event_save_name();assert(!event_neural.has_active&&event_neural.state==SN_PREPARING&&file_writes==1);set_page(PAGE_MAIN);
}
static void check_full_cache(void){
 neural_error=RISC_APP_DATA_NO_SPACE;finish_training();assert(neural_writes==1&&event_neural_cache_blocked&&!neural_size);for(unsigned i=0;i<100;i++)event_neural_tick();assert(neural_writes==1);draw_events();
}
static void check_unknown_cache(void){
 neural_unknown=true;finish_training();assert(neural_writes==1&&event_neural_cache_blocked);neural_unknown=false;event_retry();assert(event_neural.has_active&&neural_writes==1&&!event_neural_cache_blocked);
}
static void check_corrupt_cache(void){
 assert(event_neural_cache_blocked);finish_training();assert(!neural_writes&&event_neural_cache_blocked&&neural_disk[0]==0xff);
}
static void check_read_failure(void){assert(event_neural_cache_blocked);finish_training();assert(!neural_writes&&event_neural_cache_blocked);}
static void check_retained_cache(void){
 neural_error=RISC_APP_DATA_RETAINED;unsigned ticks=0;
 while(!temporal_retained()){event_neural_tick();assert(++ticks<3000);}
 assert(file_retained&&event_files.retained&&neural_writes==1&&!file_writes&&!memcmp(saved_banks,event_disk,sizeof(saved_banks)));
}
static void check_live_guard(void){
 unsigned ticks=event_neural.ticks;ambient.ready=true;ambient.foreground=true;memset(foreground_power,0,sizeof(foreground_power));foreground_power[0]=1000000;
 event_neural_tick();assert(event_neural.ticks==ticks);
 /* A tiny high-SNR bin is not an audible event and must not starve learning.
  * Use the same absolute salient-power gate as the temporal segmenter. */
 foreground_power[0]=1;assert(spectrum_background_db(spectrum_signature_total(foreground_power))<prefs.threshold_db*100);
 event_neural_tick();assert(event_neural.ticks==ticks+1);ambient.foreground=false;
 finish_training();assert(running&&owned&&live&&neural_writes==1);set_page(PAGE_MAIN);
}
static unsigned shared_test_kind;
static bool shared_test_save,shared_test_discard;
static void shared_neural_quiet(void){
 ambient.ready=true;ambient.foreground=false;input_waiting=false;
 event_match.running=false;event_segment.collecting=event_segment.ready=false;
}
static void check_shared_learning_guard(void){
 sn_reset(&event_neural,&event_library,255);event_neural_checkpoint=false;
 /* The save cases begin one tick before a genuinely trained candidate can
  * promote, not with a fabricated active model or pending cache write. */
 if(shared_test_save){
  unsigned ticks=0;
  while(event_neural.full_checks<10){sn_tick(&event_neural,&event_library);assert(++ticks<3000);}
  assert(event_neural.state==SN_CHECKING&&!event_neural.has_active);
 }
 shared_kind=shared_test_kind;shared_selected=-1;shared_discard_confirm=false;strcpy(shared_name,"Shared capture");
 set_page(PAGE_SHARED_EDIT);shared_tap(120,130);
 assert(page==PAGE_SHARED_CAPTURE&&cfa_learning_kind==shared_test_kind&&running&&owned&&live);
 assert(!signature_goal&&!event_armed&&!event_training.ready);
 shared_neural_quiet();
 unsigned ticks=event_neural.ticks,writes=neural_writes,closed=closes;
 for(unsigned i=0;i<3000;i++)event_neural_tick();
 if(event_neural.ticks!=ticks||neural_writes!=writes||closes!=closed)
  fprintf(stderr,"shared capture kind %u: neural ticks %u -> %u, checkpoint writes %u -> %u, microphone closes %u -> %u\n",shared_test_kind,ticks,event_neural.ticks,writes,neural_writes,closed,closes);
 assert(event_neural.ticks==ticks&&!event_neural.has_active&&neural_writes==writes&&closes==closed&&running&&live);
 /* Pause retains the explicit capture intent, so idle work must still wait. */
 toggle();assert(!running&&cfa_learning_kind==shared_test_kind);closed=closes;
 event_neural_tick();assert(event_neural.ticks==ticks&&neural_writes==writes&&closes==closed);
 toggle();assert(running&&live);shared_neural_quiet();
 if(shared_test_save){
  /* Bounded canonical feature fixture, not a physical microphone recording. */
  cf_feature feature={.flags=CF_TEMPORAL,.identity=sample_rate,.windows=1,.bins=1,.coverage=1.f};
  feature.repetition[0]=(cf_bin){100,1.f};assert(cf_feature_valid(&feature));
  for(unsigned i=0;i<3;i++)cfa_learn_observe(&feature,shared_test_kind==CF_EVENT);
  now+=30000;assert(cfa_learn_ready());
  file_error=RISC_APP_DATA_NO_SPACE;assert(!cfa_learn_save());
  assert(cfa_learning_kind==shared_test_kind&&cfa_learning_saved&&running&&live);
  shared_neural_quiet();closed=closes;
  event_neural_tick();assert(event_neural.ticks==ticks&&!event_neural.has_active&&neural_writes==writes&&closes==closed);
  file_error=0;
  if(shared_test_discard){
   shared_tap(120,210);assert(shared_discard_confirm&&cfa_learning_kind==shared_test_kind);
   shared_tap(120,210);assert(!cfa_learning_kind&&!cfa_learning_saved&&!event_sizes[3]);
  }else{
   assert(cfa_learn_save());assert(!cfa_learning_kind&&!cfa_learning_saved&&event_sizes[3]);
  }
 }else{
  shared_tap(120,210);assert(!cfa_learning_kind&&page==PAGE_SHARED_EDIT);
 }
 /* Both terminal paths release the guard and permit the original one-time
  * checkpoint, without changing either authoritative event bank. */
 shared_neural_quiet();closed=closes;
 finish_training();assert(neural_writes==writes+1&&closes==closed+1&&running&&owned&&live);
 set_page(PAGE_MAIN);
}
int main(void){
 for(shared_test_kind=CF_ROOM;shared_test_kind<=CF_EVENT;shared_test_kind++)for(unsigned end=0;end<3;end++){
  shared_test_save=end!=0;shared_test_discard=end==2;neural_fresh();start();advance(164);check(check_shared_learning_guard);back();run();assert(!files_live&&!store_live&&!live);
 }
 neural_fresh();check(check_idle_guards);check(check_active_reload);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();check(check_full_cache);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();check(check_unknown_cache);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();neural_size=SN_RECORD_SIZE;memset(neural_disk,0xff,sizeof(neural_disk));check(check_corrupt_cache);back();run();assert(!files_live&&!live);
 neural_fresh();neural_read_error=true;check(check_read_failure);back();run();assert(!files_live&&!live);
 neural_fresh();start();advance(164);check(check_live_guard);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();check(check_retained_cache);run();assert(files_live&&file_retained&&file_releases+1==file_acquires&&!store_releases&&!live);
 puts("Neural app: shared room/event capture and pause fence idle training/promotion/checkpoint; cancel/save resume; temporal fallback, Monitor selection, one checkpoint, reload/edit invalidation, full/unknown/corrupt/read failures, unchanged banks and retained cleanup PASS");return 0;
}
