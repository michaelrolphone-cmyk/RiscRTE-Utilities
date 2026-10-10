#define main original_spectrum_main
#include "audio_spectrum_test.c"
#undef main
static uint8_t event_disk[4][60000];static uint32_t event_sizes[4];static uint64_t file_revision;static bool files_live,file_retained,extra_audio,unknown_write,stale_write;static unsigned file_writes,file_calls,file_releases;static int32_t file_error;static unsigned audio_hz;static bool base_mix;
static unsigned file_bank(const char*name){if(!strcmp(name,"context-fingerprints.cfp"))return 3;if(!strcmp(name,event_neural_file()))return 2;assert(!strcmp(name,st_file_name(0))||!strcmp(name,st_file_name(1)));return !strcmp(name,st_file_name(1));}
static int32_t file_stat(void*c,const char*n,uint32_t*s,uint64_t*v){(void)c;assert(files_live&&!file_retained&&!running&&!owned&&!live);file_calls++;*s=0;*v=0;unsigned b=file_bank(n);if(!event_sizes[b])return RISC_APP_DATA_NOT_FOUND;*s=event_sizes[b];*v=file_revision;return 0;}
static int32_t file_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*s,uint64_t*v){(void)c;assert(files_live&&!file_retained&&!running&&!owned&&!live);file_calls++;*s=0;*v=0;if(expected!=file_revision)return RISC_APP_DATA_STALE;unsigned b=file_bank(n);assert(cap>=event_sizes[b]);memcpy(out,event_disk[b],event_sizes[b]);*s=event_sizes[b];*v=file_revision;return 0;}
static int32_t file_replace(void*c,const char*n,uint64_t expected,const void*in,uint32_t size){(void)c;assert(files_live&&!file_retained&&!owned&&!running&&!live);file_calls++;unsigned b=file_bank(n);if(b!=3)file_writes++;assert(size<=sizeof(event_disk[b]));if(stale_write){file_revision++;stale_write=false;return RISC_APP_DATA_STALE;}if(expected!=(event_sizes[b]?file_revision:0))return RISC_APP_DATA_STALE;if(file_error){if(file_error==RISC_APP_DATA_RETAINED)file_retained=true;return file_error;}memcpy(event_disk[b],in,size);event_sizes[b]=size;file_revision++;return unknown_write?RISC_APP_DATA_COMMIT_UNKNOWN:0;}
static risc_app_data_v1 file_api={1,sizeof(file_api),NULL,file_stat,file_read,file_replace};
static bool acquire_events(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out){if(strcmp(n,"storage.app-data"))return acquire(n,version,instance,out);assert(version==1&&instance==2&&!files_live);files_live=true;out->api=&file_api;return true;}
static bool release_events(risc_runtime_capability_v1*g){assert(!file_retained);if(g==&event_data_grant){assert(files_live);files_live=false;file_releases++;return true;}return release(g);}
static uint32_t guarded_millis(void){assert(!file_retained);return millis();}
static bool signal_read(void*c,int16_t*pcm,size_t cap,size_t*got){assert(c==&mic&&live&&cap==256);reads++;now+=16;for(unsigned i=0;i<256;i++){unsigned sample=(reads-1u)*256+i;int32_t value=base_mix?(spectrum_dsp_sin(sample*1802u)>>17)+(spectrum_dsp_sin(sample*4096u)>>18)+(spectrum_dsp_sin(sample*13107u)>>19):(spectrum_dsp_sin(sample*1802u)>>20);if(extra_audio)value+=spectrum_dsp_sin((uint32_t)((uint64_t)sample*audio_hz*65536u/16000u))>>17;pcm[i]=(int16_t)value;}*got=256;return true;}
static void fresh(void){reset();memset(event_disk,0,sizeof(event_disk));memset(event_sizes,0,sizeof(event_sizes));file_revision=1;files_live=file_retained=extra_audio=unknown_write=stale_write=false;file_error=0;file_writes=file_calls=file_releases=0;audio_hz=1000;base_mix=false;rt.acquire=acquire_events;rt.release=release_events;api.millis=guarded_millis;mic.read=signal_read;}
static void create_label(void){assert(event_files.ready==3);event_open(-1);strcpy(editing.name,"Door");event_save_name();assert(event_library.labels[0].present&&event_library.labels[0].next_id==1&&!running&&event_files.pending<0);set_page(PAGE_MAIN);}
static void arm_positive(void){assert(ambient.ready&&(!ambient.foreground||spectrum_background_db(spectrum_signature_total(ambient.excess))<prefs.threshold_db*100));event_open(0);event_arm(ST_POSITIVE);assert(event_armed&&page==PAGE_EVENT_CAPTURE&&event_wait_quiet);}
static void arm_negative(void){assert(ambient.ready);event_open(0);event_arm(ST_NEGATIVE);assert(event_armed);}
static void sound_on(void){assert(!event_wait_quiet);extra_audio=true;}
static void sound_off(void){extra_audio=false;}
static void check_review(void){assert(event_training.ready&&!event_armed&&event_training.event.pre==4&&!event_training.event.flags);assert(event_training.event.duration_ms>=512);draw();}
static void save_positive(void){event_save_capture();assert(!event_training.ready&&running&&event_files.pending<0&&event_library.labels[0].examples[0].kind==ST_POSITIVE&&event_library.labels[0].next_id==2);draw();set_page(PAGE_MAIN);}
static void save_negative(void){event_save_capture();assert(event_library.labels[0].examples[1].kind==ST_NEGATIVE&&event_library.labels[0].examples[0].kind==ST_POSITIVE&&event_library.labels[0].next_id==3);assert(running&&event_files.pending<0);pause_capture();assert(st_files_load(&event_files,&event_library,0,false)==0&&st_example_count(&event_library.labels[0],0)==2);portable_audio_capture_resume();set_page(PAGE_MAIN);}
static void save_failure(void){event_open(0);strcpy(editing.name,"Unsaved");file_error=RISC_APP_DATA_NO_SPACE;event_save_name();assert(event_files.pending==0&&!running);set_page(PAGE_MAIN);assert(!request_root_exit()&&page==PAGE_EVENTS);draw();}
static void retry_save(void){file_error=0;unsigned before=file_writes;event_retry();assert(event_files.pending<0&&file_writes==before+1&&!strcmp(event_library.labels[0].name,"Unsaved"));set_page(PAGE_MAIN);}
static void discard_touch(void){bool t=false,f=false;set_page(PAGE_EVENTS);toast_message=NULL;tap_action(170,200,&t,&f);assert(event_delete_example==-3&&event_files.pending==0);tap_action(170,200,&t,&f);assert(event_files.pending<0&&!strcmp(event_library.labels[0].name,"Door"));set_page(PAGE_MAIN);}
static void uncertain_save(void){event_open(0);strcpy(editing.name,"Unknown");unknown_write=true;event_save_name();assert(event_files.pending==0);unknown_write=false;unsigned before=file_writes;event_retry();assert(event_files.pending<0&&file_writes==before);set_page(PAGE_MAIN);}
static void bad_collection(void){assert(event_files.ready==2);event_open(-1);assert(event_edit_slot==4);set_page(PAGE_MAIN);}
static void keyboard_paths(void){event_open(0);event_keyboard=true;keyboard_begin();keyboard_activate(1);keyboard_cancel();assert(page==PAGE_EVENT_LABEL&&!strcmp(editing.name,"Door"));keyboard_begin();keyboard_activate(PWK_DONE);assert(page==PAGE_EVENT_LABEL);draw();set_page(PAGE_MAIN);}
static void retained_save(void){event_open(0);strcpy(editing.name,"Retained");file_error=RISC_APP_DATA_RETAINED;event_save_name();assert(temporal_retained());}
static void clipped_review(void){assert(event_training.ready&&(event_training.event.flags&ST_CLIPPED)&&event_training.event.count==64);draw();event_cancel_capture();assert(!temporal_exit_blocked());set_page(PAGE_MAIN);}
static void cancel_armed(void){bool t=false,f=false;toast_message=NULL;tap_action(175,202,&t,&f);assert(!event_armed&&!event_training.ready&&page==PAGE_EVENT_LABEL);set_page(PAGE_MAIN);}
static void stop_before_onset(void){bool t=false,f=false;toast_message=NULL;tap_action(60,202,&t,&f);assert(!event_armed&&!event_training.ready&&page==PAGE_EVENT_LABEL&&file_writes==1);set_page(PAGE_MAIN);}
static void rejected_event_releases_background(void){event_known_hold=true;st_match_begin(&event_match,&event_library.labels[0].examples[0]);unsigned ticks=0;while(event_match.running){temporal_tick();assert(++ticks<128);}assert(event_match.reason==ST_RESULT_NEGATIVE&&!event_known_hold);}
static void confirm_manual_whole_event(void){bool t=false,f=false;toast_message=NULL;tap_action(50,202,&t,&f);assert(!event_armed&&event_training.ready&&st_partial(&event_training.event));draw();tap_action(100,163,&t,&f);assert((event_training.event.flags&(ST_CLIPPED|ST_CONFIRMED_END))==(ST_CLIPPED|ST_CONFIRMED_END));tap_action(50,202,&t,&f);assert(!event_training.ready&&event_files.pending<0&&event_library.labels[0].examples[0].flags==3);set_page(PAGE_MAIN);}
static void snapshot_ambient_legacy(void){signature_open(-1,SPECTRUM_SIGNATURE_EVENT);strcpy(editing.name,"Ambient");signature_capture_begin();assert(signature_goal==100);set_page(PAGE_MAIN);}
static void assert_ambient_is_not_event(void){assert(event_slot<0&&!event_segment.collecting&&!event_segment.ready);for(unsigned i=0;i<8;i++)assert(!label_active[i]);}
static void assert_short_frequency_peak(void){assert(prefs.fft_size==8192&&label_active[2]);}
static void use_slow_display(void){prefs.fft_size=8192;configure_dsp();}
static void check_monitor_context(void){
 signatures[1]=(spectrum_signature){.frames=SPECTRUM_SIGNATURE_ROOM_FRAMES,.kind=SPECTRUM_SIGNATURE_ROOM};strcpy(signatures[1].name,"Office");room_tracker.selected=room_tracker.candidate=1;room_tracker.confidence=93;ambient.ready=true;ambient.raw[0]=100000;
 labels[2]=(spectrum_label){.present=true,.frequency_hz=1000,.color=2};strcpy(labels[2].name,"Hum");label_active[2]=true;label_snr[2]=840;label_db[2]=-3300;
 event_library.labels[0].present=true;strcpy(event_library.labels[0].name,"Door");event_library.labels[1].present=true;strcpy(event_library.labels[1].name,"Window");event_match.complete=true;event_match.reason=ST_RESULT_AMBIGUOUS;event_match.selected=-1;event_match.positive[0]=910;event_match.positive[1]=860;event_match.query.peak_db=-1800;
 monitor_item items[17];unsigned n=monitor_items(items);assert(n==3);assert(items[0].kind==MONITOR_CANDIDATE&&!strcmp(items[0].name,"Door")&&items[0].confidence==91&&items[0].amplitude_db==-1800);assert(items[1].kind==MONITOR_CANDIDATE&&!strcmp(items[1].name,"Window")&&items[1].confidence==86);assert(items[2].kind==MONITOR_LABEL&&!strcmp(items[2].name,"Hum")&&items[2].confidence==70&&items[2].amplitude_db==-3300);assert(monitor_room_count()==1&&monitor_scene_db()>-12000);view=2;lab_edit=false;draw_monitor();
}
static void advance(unsigned n){for(unsigned i=0;i<n;i++)add(EVENT_INPUT,0,-1);}
#ifndef SPECTRUM_TEMPORAL_MAIN
#define SPECTRUM_TEMPORAL_MAIN main
#endif
int SPECTRUM_TEMPORAL_MAIN(void){
 fresh();check(create_label);start();advance(164);check(arm_positive);advance(20);check(sound_on);advance(40);check(sound_off);advance(24);check(check_review);check(save_positive);advance(164);check(arm_negative);advance(20);check(sound_on);advance(40);check(sound_off);advance(24);check(check_review);check(save_negative);check(rejected_event_releases_background);back();run();assert(!files_live&&!store_live&&!live&&file_writes==3&&file_releases==1);
 fresh();check(create_label);check(save_failure);check(retry_save);back();run();assert(!files_live&&!store_live&&!live);
 fresh();check(create_label);check(save_failure);check(discard_touch);back();run();assert(!files_live&&!store_live&&!live);
 fresh();check(create_label);check(uncertain_save);back();run();assert(!files_live&&!store_live&&!live);
 fresh();event_sizes[0]=ST_BANK_MIN;memset(event_disk[0],255,ST_BANK_MIN);check(bad_collection);back();run();assert(!files_live&&!file_writes);
 fresh();check(create_label);check(keyboard_paths);back();run();assert(!files_live&&!live);
 fresh();check(create_label);start();advance(164);check(arm_positive);advance(20);check(sound_on);advance(270);check(clipped_review);back();run();assert(!files_live&&!live&&file_writes==1);
 fresh();check(create_label);start();advance(164);check(arm_positive);check(cancel_armed);back();run();assert(!files_live&&!live&&file_writes==1);
 fresh();check(create_label);start();advance(164);check(arm_positive);check(stop_before_onset);back();run();assert(!files_live&&!live&&file_writes==1);
 fresh();check(create_label);start();advance(164);check(arm_positive);advance(20);check(sound_on);advance(20);check(confirm_manual_whole_event);back();run();assert(!files_live&&!live&&file_writes==2);
 fresh();base_mix=true;start();advance(164);check(snapshot_ambient_legacy);advance(220);for(unsigned n=0;n<60;n++){advance(16);check(assert_ambient_is_not_event);}back();run();assert(!files_live&&!live);
 fresh();start();advance(164);check(use_slow_display);extra_audio=false;check(sound_on);advance(4);check(assert_short_frequency_peak);check(sound_off);back();run();assert(!files_live&&!live);
 fresh();check(check_monitor_context);back();run();assert(!files_live&&!store_live&&!live);
 fresh();check(create_label);check(retained_save);run();assert(file_retained&&files_live&&store_live&&!file_releases&&!store_releases&&!running&&!live);
 puts("Temporal app: live onset/precontext positive+negative capture, multi-example save/reload, clipping/cancel, keyboard nesting, ranked room/event/label Monitor context, pending exit guard, full/unknown/retry/discard, reserved corrupt banks, mic-off file transactions and retained return without UI/release passed");return 0;
}
