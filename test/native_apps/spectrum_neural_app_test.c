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
 *s=neural_size;*v=file_revision;return RISC_APP_DATA_OK;
}
static int32_t neural_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*s,uint64_t*v){
 if(!neural_name(n))return file_read(c,n,expected,out,cap,s,v);
 neural_guard();assert(expected==file_revision&&cap>=neural_size);neural_reads++;memcpy(out,neural_disk,neural_size);*s=neural_size;*v=file_revision;return RISC_APP_DATA_OK;
}
static int32_t neural_replace(void*c,const char*n,uint64_t expected,const void*in,uint32_t size){
 if(!neural_name(n))return file_replace(c,n,expected,in,size);
 neural_guard();neural_writes++;assert(size==SN_RECORD_SIZE&&expected==(neural_size?file_revision:0));
 if(neural_error){if(neural_error==RISC_APP_DATA_RETAINED)file_retained=true;return neural_error;}
 memcpy(neural_disk,in,size);neural_size=size;file_revision++;return neural_unknown?RISC_APP_DATA_COMMIT_UNKNOWN:RISC_APP_DATA_OK;
}
static void neural_fresh(void){
 fresh();file_api.stat=neural_stat;file_api.read=neural_read;file_api.replace=neural_replace;
 neural_size=neural_reads=neural_writes=0;neural_error=0;neural_unknown=neural_read_error=false;memset(neural_disk,0,sizeof(neural_disk));
 st_library seeded={0};for(unsigned i=0;i<2;i++){
  st_label *l=&seeded.labels[i];l->present=true;l->next_id=6;snprintf(l->name,sizeof(l->name),"Door %u",i);
  for(unsigned j=0;j<3;j++)l->examples[j]=sn_fixture_example(i,j,ST_POSITIVE);
  l->examples[3]=sn_fixture_example(2,3,ST_NEGATIVE);l->examples[4]=sn_fixture_example(2,4,ST_NEGATIVE);
 }
 for(unsigned b=0;b<2;b++){seeded.generation[b]=1;event_sizes[b]=(uint32_t)st_bank_encode(&seeded,b,event_disk[b],sizeof(event_disk[b]));assert(event_sizes[b]);}
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
int main(void){
 neural_fresh();check(check_idle_guards);check(check_active_reload);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();check(check_full_cache);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();check(check_unknown_cache);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();neural_size=SN_RECORD_SIZE;memset(neural_disk,0xff,sizeof(neural_disk));check(check_corrupt_cache);back();run();assert(!files_live&&!live);
 neural_fresh();neural_read_error=true;check(check_read_failure);back();run();assert(!files_live&&!live);
 neural_fresh();start();advance(164);check(check_live_guard);back();run();assert(!files_live&&!store_live&&!live);
 neural_fresh();check(check_retained_cache);run();assert(files_live&&file_retained&&!file_releases&&!store_releases&&!live);
 puts("Neural app: idle-only learning, temporal fallback, actual Monitor selection, one checkpoint, exact reload/edit invalidation, full/unknown/corrupt/read failures, unchanged source banks, capture resume and retained cleanup PASS");return 0;
}
