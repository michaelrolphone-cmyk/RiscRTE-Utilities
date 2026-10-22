/* Additive differential oracle for the unchanged Audio controller. Compile this
 * identical file over the RF candidate and its predecessor, then compare stdout.
 * No expected digest is baked in: stdout contains exact persisted bytes, an
 * ordered callback transcript, and explicit (padding-free) observable state.
 *
 * cc -std=c11 -O2 -Wall -Wextra -Werror \
 *   -I$SYSTEM/lib/PortableApps/include -I$SYSTEM/lib/NativeApps/include \
 *   test/native_apps/rf_candidate_audio_differential_test.c -lm -o audio-diff
 * Repeat with -DPORTABLE_SPECTRUM_SHARED_TEXT_INPUT for the shared-text build.
 * This is host synthetic-PCM evidence, not microphone or hardware qualification.
 */
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
/* Match the controller-only shared-input fixture: no Contexts provider. */
#define PORTABLE_CONTEXTS_LIVE() true
#define PORTABLE_CONTEXTS_CHECK_LIVE() do {} while (0)
#define portable_contexts_callback(value) (value)
#endif
#define main rf_diff_existing_audio_main
#include "audio_spectrum_test.c"
#undef main

#include <inttypes.h>

/* Event storage and PCM generation follow spectrum_temporal_app_test.c.
 * Wrappers record the actual callback order, not a reconstructed summary. */
static uint8_t diff_disk[4][72000];
static uint32_t diff_sizes[4];
static uint64_t diff_revisions[4],diff_revision;
static bool diff_files_live,diff_extra,diff_unknown;
static int32_t diff_file_error;
static unsigned diff_file_acquires,diff_file_releases,diff_file_writes,diff_event_writes;
static uint8_t diff_trace[262144];
static size_t diff_trace_size;
static unsigned diff_calls[32];
static const char *diff_case;
static const char diff_office[17]="Office";

enum {
 D_ACQUIRE=1,D_RELEASE,D_OPEN,D_READ,D_CLOSE,D_GET,D_PUT,D_STAT,
 D_FILE_READ,D_REPLACE,D_TEXT_SUSPEND,D_TEXT_RESUME,D_TEXT_RETAIN,
 D_TEXT_OPEN,D_TEXT_POLL,D_TEXT_CLOSE,D_TEXT_ATTENTION,D_POLL,D_PRESENT
};
static void diff_byte(unsigned v){assert(diff_trace_size<sizeof(diff_trace));diff_trace[diff_trace_size++]=(uint8_t)v;}
static void diff_u32(uint32_t v){for(unsigned i=0;i<4;i++)diff_byte(v>>(8*i));}
static void diff_u64(uint64_t v){for(unsigned i=0;i<8;i++)diff_byte((unsigned)(v>>(8*i)));}
static void diff_string(const char *s){do{diff_byte((unsigned char)*s);}while(*s++);}
static void diff_call(unsigned code){assert(code<32);diff_calls[code]++;diff_byte(code);}
static uint32_t diff_float(float v){uint32_t bits;memcpy(&bits,&v,sizeof(bits));return bits;}
static void diff_hex(const char *kind,const char *name,const uint8_t *bytes,size_t n){
 printf("%s %s %s %zu ",diff_case,kind,name,n);
 for(size_t i=0;i<n;i++)printf("%02x",bytes[i]);
 putchar('\n');
}
static unsigned diff_bank(const char *n){
 if(!strcmp(n,"context-fingerprints.cfp"))return 3;
 if(!strcmp(n,event_neural_file()))return 2;
 assert(!strcmp(n,st_file_name(0))||!strcmp(n,st_file_name(1)));
 return !strcmp(n,st_file_name(1));
}
static void diff_file_live(void){assert(diff_files_live&&!running&&!owned&&!live);}
static int32_t diff_stat(void *c,const char *n,uint32_t *s,uint64_t *v){
 (void)c;diff_file_live();unsigned b=diff_bank(n);diff_call(D_STAT);diff_byte(b);
 *s=diff_sizes[b];*v=diff_revisions[b];
 int32_t rc=*s?RISC_APP_DATA_OK:RISC_APP_DATA_NOT_FOUND;
 diff_u32((uint32_t)rc);diff_u32(*s);diff_u64(*v);return rc;
}
static int32_t diff_file_read(void *c,const char *n,uint64_t expected,void *out,uint32_t cap,uint32_t *s,uint64_t *v){
 (void)c;diff_file_live();unsigned b=diff_bank(n);diff_call(D_FILE_READ);diff_byte(b);diff_u64(expected);diff_u32(cap);
 *s=0;*v=0;int32_t rc=RISC_APP_DATA_STALE;
 if(expected==diff_revisions[b]){assert(cap>=diff_sizes[b]);memcpy(out,diff_disk[b],diff_sizes[b]);*s=diff_sizes[b];*v=diff_revisions[b];rc=RISC_APP_DATA_OK;}
 diff_u32((uint32_t)rc);diff_u32(*s);diff_u64(*v);return rc;
}
static int32_t diff_replace(void *c,const char *n,uint64_t expected,const void *in,uint32_t size){
 (void)c;diff_file_live();unsigned b=diff_bank(n);diff_call(D_REPLACE);diff_byte(b);diff_u64(expected);diff_u32(size);diff_file_writes++;if(b!=3)diff_event_writes++;
 assert(size<=sizeof(diff_disk[b]));int32_t rc=diff_file_error;
 if(expected!=(diff_sizes[b]?diff_revisions[b]:0))rc=RISC_APP_DATA_STALE;
 if(!rc){memcpy(diff_disk[b],in,size);diff_sizes[b]=size;diff_revisions[b]=++diff_revision;rc=diff_unknown?RISC_APP_DATA_COMMIT_UNKNOWN:RISC_APP_DATA_OK;}
 diff_u32((uint32_t)rc);return rc;
}
static risc_app_data_v1 diff_file_api={1,sizeof(diff_file_api),NULL,diff_stat,diff_file_read,diff_replace};

#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
static bool diff_host_grant,diff_host_live,diff_text_suspended,diff_text_retained;
static char diff_host_name[17];
static risc_text_entry_api_v1_home_reason diff_host_api;
int portable_text_adapter_suspend(void){diff_call(D_TEXT_SUSPEND);assert(!diff_text_suspended);diff_text_suspended=true;return RISC_TEXT_ENTRY_OK;}
int portable_text_adapter_resume(void){diff_call(D_TEXT_RESUME);assert(diff_text_suspended&&!diff_host_live&&!diff_host_grant);diff_text_suspended=false;return RISC_TEXT_ENTRY_OK;}
int portable_text_adapter_attention(void){diff_call(D_TEXT_ATTENTION);return 0;}
void portable_text_adapter_retain(void){diff_call(D_TEXT_RETAIN);diff_text_retained=true;}
bool portable_text_adapter_retained(void){return diff_text_retained;}
static int32_t diff_text_open(void *c,const risc_text_entry_request_v1 *q,uint64_t *s){
 (void)c;diff_call(D_TEXT_OPEN);diff_string(q->label);diff_string(q->text);diff_u32(q->capacity);
 assert(diff_host_grant&&diff_text_suspended&&!diff_host_live&&q->capacity==17);diff_host_live=true;*s=7;return 0;
}
static int32_t diff_text_poll(void *c,uint64_t s,risc_text_entry_state_v1 *out){
 (void)c;diff_call(D_TEXT_POLL);diff_u64(s);assert(diff_host_live&&s==7);
 *out=(risc_text_entry_state_v1){.struct_size=sizeof(*out),.state=RISC_TEXT_ENTRY_ACCEPTED,.revision=1};
 strcpy(out->text,diff_host_name);diff_string(out->text);return 0;
}
static int32_t diff_text_close(void *c,uint64_t s){(void)c;diff_call(D_TEXT_CLOSE);diff_u64(s);assert(diff_host_live&&s==7);diff_host_live=false;return 0;}
#endif
static bool diff_acquire(const char *n,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 diff_call(D_ACQUIRE);diff_string(n);diff_u32(version);diff_u64(instance);
 bool ok;
 if(!strcmp(n,"rtc.clock"))ok=false;
 else if(!strcmp(n,"storage.app-data")){assert(version==1&&instance==2&&!diff_files_live);diff_files_live=true;diff_file_acquires++;out->api=&diff_file_api;ok=true;}
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
 else if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)){assert(version==1&&!instance&&!diff_host_grant);diff_host_grant=true;out->api=&diff_host_api;out->slot=4;out->generation=2;ok=true;}
#endif
 else ok=acquire(n,version,instance,out);
 diff_byte(ok);return ok;
}
static bool diff_release(risc_runtime_capability_v1 *g){
 diff_call(D_RELEASE);bool ok;
 if(g->api==&diff_file_api){diff_byte(1);assert(diff_files_live);diff_files_live=false;diff_file_releases++;ok=true;}
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
 else if(g->api==&diff_host_api){diff_byte(4);assert(diff_host_grant&&!diff_host_live);diff_host_grant=false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};ok=true;}
#endif
 else{diff_byte(g==&store_grant?2:3);ok=release(g);}
 diff_byte(ok);return ok;
}
static bool diff_open(void *c,uint32_t rate){diff_call(D_OPEN);diff_u32(rate);bool ok=open_mic(c,rate);diff_byte(ok);return ok;}
static bool diff_close(void *c){diff_call(D_CLOSE);bool ok=close_mic(c);diff_byte(ok);return ok;}
static bool diff_audio_read(void *c,int16_t *pcm,size_t cap,size_t *got){
 assert(c==&mic&&live&&grant_live&&running&&cap==256);diff_call(D_READ);diff_u32((uint32_t)cap);reads++;now+=16;
 for(unsigned i=0;i<256;i++){
  unsigned sample=(reads-1u)*256+i;int32_t value=spectrum_dsp_sin(sample*1802u)>>20;
  if(diff_extra)value+=spectrum_dsp_sin((uint32_t)((uint64_t)sample*1000u*65536u/16000u))>>17;
  pcm[i]=(int16_t)value;
 }
 *got=256;diff_u32(256);return true;
}
static int32_t diff_get(void *c,const char *k,void *out,uint32_t cap,uint32_t *size){
 diff_call(D_GET);diff_string(k);diff_u32(cap);int32_t rc=get_value(c,k,out,cap,size);diff_u32((uint32_t)rc);diff_u32(*size);return rc;
}
static int32_t diff_put(void *c,const char *k,const void *in,uint32_t size){
 diff_call(D_PUT);diff_string(k);diff_u32(size);int32_t rc=put_value(c,k,in,size);diff_u32((uint32_t)rc);return rc;
}
static bool diff_poll(t5_app_input_t *out,uint32_t wait){diff_call(D_POLL);diff_u32(wait);return poll(out,wait);}
static void diff_present(bool full){diff_call(D_PRESENT);diff_byte(full);present(full);}
static void diff_reset(const char *name){
 reset();diff_case=name;memset(diff_disk,0,sizeof(diff_disk));memset(diff_sizes,0,sizeof(diff_sizes));memset(diff_revisions,0,sizeof(diff_revisions));
 diff_revision=1;diff_files_live=diff_extra=diff_unknown=false;diff_file_error=0;diff_file_acquires=diff_file_releases=diff_file_writes=diff_event_writes=0;
 diff_trace_size=0;memset(diff_calls,0,sizeof(diff_calls));
 rt.acquire=diff_acquire;rt.release=diff_release;mic.open=diff_open;mic.read=diff_audio_read;mic.close=diff_close;kv.get=diff_get;kv.put=diff_put;api.poll=diff_poll;api.present=diff_present;
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
 diff_host_grant=diff_host_live=diff_text_suspended=diff_text_retained=false;diff_host_name[0]=0;
 diff_host_api=(risc_text_entry_api_v1_home_reason){.base={1,sizeof(diff_host_api),NULL,diff_text_open,diff_text_poll,diff_text_close},.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG,.home_reason_version=RISC_TEXT_ENTRY_HOME_REASON_VERSION};
#endif
}
static void diff_snapshot(const char *stage){
 const uint32_t *background=cfa_background();
 printf("%s STATE %s now=%u run=%u own=%u request=%u page=%u gen=%u loaded=%u changed=%u failed=%u revision=%" PRIu64 " learn=%u samples=%u saved=%u ready=%u train=%u/%u segment=%u room=%d/%08x/%u/%u event=%d/%08x/%u/%u signature=%u/%u/%u temporal=%d/%u/%u/%u match=%d/%u/%u/%u background=%u mode=%u\n",
  diff_case,stage,now,running,owned,capture_requested,page,cfa_bank.generation,cfa_loaded,cfa_changed,cfa_failed,cfa_revision,cfa_learning_kind,cfa_learning_samples,cfa_learning_saved,cfa_learn_ready(),cfa_training_ready,cfa_training_saved,cfa_segment_active,
  cfa_room.slot,diff_float(cfa_room.confidence),cfa_room.sources,cfa_room.ambiguous,cfa_event.slot,diff_float(cfa_event.confidence),cfa_event.sources,cfa_event.ambiguous,
  signature_goal,signature_capture.frames,signature_pending,event_files.pending,event_armed,event_training.ready,event_training.event.count,event_match.selected,event_match.reason,event_match.score,event_match.running,background!=NULL,signature_mode);
 printf("%s BACKGROUND %s",diff_case,stage);if(background)for(unsigned i=0;i<128;i++)printf(" %08x",background[i]);putchar('\n');
 printf("%s TEMPORAL %s generation=%u,%u next=%u examples=%u ambient=%u/%u/%u room_tracker=%d/%u/%u events=%u\n",diff_case,stage,event_library.generation[0],event_library.generation[1],event_library.labels[0].next_id,st_example_count(&event_library.labels[0],0),ambient.ready,ambient.frames,ambient.foreground,room_tracker.selected,room_tracker.confidence,room_tracker.ambiguous,cfa_has_events());
 uint8_t bytes[CF_STORE_MAX];uint32_t n=cf_encode_source(&cfa_bank,CF_AUDIO,bytes,sizeof(bytes));assert(n);diff_hex("BANK",stage,bytes,n);
}
static void diff_finish(void){
 assert(!diff_files_live&&!store_live&&!live&&!grant_live&&!running&&!owned&&!uncertain&&!escaped);
 assert(diff_file_acquires==diff_file_releases&&acquires==releases&&store_acquires==store_releases);
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
 assert(!diff_host_grant&&!diff_host_live&&!diff_text_suspended&&!diff_text_retained&&!text_active);
#endif
 printf("%s COUNTS opens=%u reads=%u closes=%u acquires=%u releases=%u kv=%u/%u puts=%u files=%u/%u writes=%u polls=%u frames=%u\n",diff_case,opens,reads,closes,acquires,releases,store_acquires,store_releases,puts_count,diff_file_acquires,diff_file_releases,diff_file_writes,polls,frames);
 for(unsigned i=1;i<=D_PRESENT;i++)printf("%s CALLBACK %u %u\n",diff_case,i,diff_calls[i]);
 for(unsigned i=0;i<4;i++){
  char name[32];snprintf(name,sizeof(name),"%u-revision-%" PRIu64,i,diff_revisions[i]);diff_hex("APPDATA",name,diff_disk[i],diff_sizes[i]);
 }
 for(unsigned i=0;i<24;i++)if(cells[i].key[0])diff_hex("KV",cells[i].key,cells[i].bytes,cells[i].size);
 diff_hex("CALLBACK_ORDER","le-fields",diff_trace,diff_trace_size);
}
static void diff_drive(unsigned n){for(unsigned i=0;i<n;i++){assert(running);capture();temporal_tick();cfa_apply();}}
static void diff_name(const char *name){
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
 assert(text_active);strcpy(diff_host_name,name);text_step();assert(!text_active&&!text_client.retained);
#else
 assert(page==PAGE_KEYBOARD);strcpy(editing.name,name);keyboard_activate(PWK_DONE);
#endif
}
static void diff_shared_room(void){
 assert(cfa_loaded&&!cfa_failed);shared_open(CF_ROOM);shared_tap(100,210);shared_tap(60,80);diff_name("Office");shared_tap(100,130);
 assert(cfa_learning_kind==CF_ROOM&&running);diff_snapshot("learning-start");
 diff_drive(100);unsigned partial=cfa_learning_samples;assert(portable_audio_suspend());assert(!running&&cfa_learning_kind==CF_ROOM&&cfa_learning_samples==partial);diff_snapshot("learning-paused");
 toggle();assert(running);unsigned steps=0;while(!cfa_learn_ready()){diff_drive(1);assert(++steps<2500);}
 assert(cfa_learning_samples>=3);diff_snapshot("learning-ready");
 diff_file_error=RISC_APP_DATA_NO_SPACE;shared_tap(100,160);assert(cfa_learning_saved&&cfa_changed&&cfa_learning_kind==CF_ROOM);uint32_t generation=cfa_bank.generation;diff_snapshot("save-no-space");
 diff_file_error=0;shared_tap(100,160);assert(!cfa_learning_kind&&!cfa_learning_saved&&!cfa_changed&&cfa_bank.generation==generation);diff_snapshot("save-retried");
 assert(cfa_filter_select(2,diff_office)&&cfa_background());diff_snapshot("manual-background");
 fail_put=true;assert(!cfa_filter_select(0,diff_office)&&signature_mode==2);diff_snapshot("filter-kv-failed");fail_put=false;
 assert(cfa_filter_select(1,diff_office));diff_drive(1000);assert(cfa_room.slot>=0&&cfa_background());diff_snapshot("auto-room-match");
 assert(cfa_changed);diff_unknown=true;cfa_save();diff_unknown=false;assert(!cfa_changed);diff_snapshot("unknown-commit-verified");
 diff_drive(1000);assert(cfa_changed);diff_file_error=RISC_APP_DATA_NO_SPACE;cfa_save();assert(cfa_changed);diff_snapshot("checkpoint-failed");diff_file_error=0;cfa_retry();assert(!cfa_changed);diff_snapshot("checkpoint-retried");
 assert(cfa_filter_select(0,diff_office)&&!cfa_background());set_page(PAGE_MAIN);
}
static void diff_signature(void){
 signature_open(-1,SPECTRUM_SIGNATURE_ROOM);strcpy(editing.name,"Legacy room");signature_capture_begin();assert(running&&signature_goal==100);
 diff_drive(20);assert(signature_capture.frames>0&&signature_capture.frames<100);unsigned partial=signature_capture.frames;
 assert(portable_audio_suspend()&&signature_capture.frames==partial);diff_snapshot("signature-paused");toggle();fail_put=true;
 for(unsigned i=0;signature_goal;i++){assert(i<250);diff_drive(1);}
 assert(signature_pending==1&&signatures[0].frames==100&&cfa_changed);diff_snapshot("signature-kv-failed");
 fail_put=false;signature_save();assert(!signature_pending&&!cfa_changed&&signatures[0].frames==100);diff_snapshot("signature-retried");
 signature_restore();assert(signatures[0].frames==100&&!strcmp(signatures[0].name,"Legacy room"));diff_snapshot("signature-reloaded");
 direct_edit_saved();direct_unsaved();assert(!pending_save);diff_snapshot("preferences-label-retried");set_page(PAGE_MAIN);
}
static void diff_record(unsigned kind){
 event_open(0);event_arm(kind);assert(event_armed&&running);diff_drive(20);assert(!event_wait_quiet);diff_extra=true;diff_drive(40);diff_extra=false;diff_drive(24);
 assert(event_training.ready&&!event_armed&&event_training.event.pre==4&&!event_training.event.flags);diff_snapshot(kind==ST_POSITIVE?"positive-review":"negative-review");
 event_save_capture();assert(!event_training.ready&&running&&event_files.pending<0);diff_snapshot(kind==ST_POSITIVE?"positive-saved":"negative-saved");
}
static void diff_temporal(void){
 event_open(-1);diff_name("Door");event_save_name();assert(event_library.labels[0].present&&!running);set_page(PAGE_MAIN);toggle();diff_drive(164);
 diff_record(ST_POSITIVE);diff_drive(164);diff_record(ST_NEGATIVE);assert(st_example_count(&event_library.labels[0],0)==2);
 pause_capture();assert(!st_files_load(&event_files,&event_library,0,false));portable_audio_capture_resume();assert(st_example_count(&event_library.labels[0],0)==2);diff_snapshot("temporal-reloaded");
 event_open(0);strcpy(editing.name,"Unsaved");diff_file_error=RISC_APP_DATA_NO_SPACE;event_save_name();assert(event_files.pending==0);diff_snapshot("appdata-failed");
 diff_file_error=0;event_retry();assert(event_files.pending<0&&!strcmp(event_library.labels[0].name,"Unsaved"));diff_snapshot("appdata-retried");
 event_open(0);strcpy(editing.name,"Unknown");diff_unknown=true;event_save_name();assert(event_files.pending==0);diff_snapshot("appdata-unknown");diff_unknown=false;unsigned before=diff_event_writes;event_retry();assert(event_files.pending<0&&diff_event_writes==before);diff_snapshot("appdata-verified");
 event_known_hold=true;st_match_begin(&event_match,&event_library.labels[0].examples[0]);unsigned ticks=0;while(event_match.running){temporal_tick();assert(++ticks<128);}
 assert(event_match.reason==ST_RESULT_NEGATIVE&&!event_known_hold);diff_snapshot("negative-match-releases-background");set_page(PAGE_MAIN);
}
int main(void){
 puts("RF_CANDIDATE_AUDIO_DIFFERENTIAL_V1");
#ifdef PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
 puts("shared_text_input=1");
#else
 puts("shared_text_input=0");
#endif
 diff_reset("shared-room");check(diff_shared_room);back();run();diff_finish();
 diff_reset("legacy-signature");check(diff_signature);back();run();diff_finish();
 diff_reset("temporal-event");check(diff_temporal);back();run();diff_finish();
 puts("PASS audio differential capture completed");return 0;
}
