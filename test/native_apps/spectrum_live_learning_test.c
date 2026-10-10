#define SPECTRUM_TEMPORAL_MAIN prior_temporal_controller_main
#include "spectrum_temporal_app_test.c"
#undef SPECTRUM_TEMPORAL_MAIN

static unsigned partial_room, interrupted_columns;
static bool gap_read(void*c,int16_t*p,size_t cap,size_t*got){
 if(!read_count){reads++;now+=40;*got=0;return true;}
 return signal_read(c,p,cap,got);
}
static void begin_gap(void){assert(event_training.collecting&&event_armed);label_active[0]=true;level_valid[0]=true;read_count=0;}
static void end_gap(void){
 assert(input_waiting&&running&&!event_armed&&event_training.ready);
 assert((event_training.event.flags&ST_CLIPPED)&&opens==1&&!closes);
 for(unsigned i=0;i<8;i++)assert(!label_active[i]&&!level_valid[i]);
 interrupted_columns=event_training.event.count;read_count=256;
}
static void gap_preserved(void){
 assert(event_training.event.count==interrupted_columns&&(event_training.event.flags&ST_CLIPPED));
 assert(!input_waiting&&running&&opens==1&&!closes);event_cancel_capture();set_page(PAGE_MAIN);
}
static void room_from_stopped(void){
 assert(!running&&!signature_audio.available);signature_open(-1,SPECTRUM_SIGNATURE_ROOM);
 strcpy(editing.name,"Office");signature_capture_begin();
 assert(running&&owned&&opens==1&&signature_goal==100&&!signature_capture.frames);
 /* Repeated Add must not restart the active collection. */
 signature_capture_begin();assert(signature_goal==100&&opens==1);
}
static void room_pause(void){partial_room=signature_capture.frames;assert(partial_room&&partial_room<100);assert(portable_audio_suspend());assert(!running&&signature_goal==100&&signature_capture.frames==partial_room);}
static void room_resume(void){bool t=false;signature_tap(60,125,&t);assert(t);toggle();assert(running&&opens==2&&signature_capture.frames==partial_room);}
static void room_committed(void){assert(signatures[0].frames==100&&!signature_goal&&puts_count==1);signature_capture_begin();assert(signature_goal==100);}
static void room_cancel(void){bool t=false;signature_tap(170,125,&t);assert(!signature_goal&&signatures[0].frames==100&&puts_count==1);set_page(PAGE_MAIN);}
static void room_open_failure(void){
 fail_open=true;signature_open(-1,SPECTRUM_SIGNATURE_ROOM);strcpy(editing.name,"Office");signature_capture_begin();
 assert(!running&&!signature_goal&&!signature_pending&&page==PAGE_SIGNATURE_EDIT);fail_open=false;
 signature_capture_begin();assert(running&&opens==2&&signature_goal==100);
}
static void room_failed_cancel(void){bool t=false;signature_tap(170,125,&t);assert(!signature_goal&&!signatures[0].kind&&!puts_count);set_page(PAGE_MAIN);}
static void auto_record(void){
 event_open(-1);assert(page==PAGE_KEYBOARD);strcpy(editing.name,"Door");keyboard_activate(PWK_DONE);
 event_arm(ST_POSITIVE);assert(event_armed&&running&&event_library.labels[0].present&&file_writes==1&&!ambient.ready);
}
static void cancelled_record(void){assert(event_armed&&!event_wait_quiet);event_cancel_capture();assert(!event_armed&&!event_training.ready&&file_writes==1);set_page(PAGE_MAIN);}
static void rejected_shift(void){
 event_open(0);event_library.generation[0]=UINT32_MAX;event_edit_shift=2;
 unsigned writes=file_writes;event_arm(ST_POSITIVE);
 assert(!event_armed&&!running&&file_writes==writes&&event_library.labels[0].shift_limit==1);set_page(PAGE_MAIN);
}
int main(void){
 fresh();mic.read=gap_read;check(create_label);start();advance(164);check(arm_positive);advance(20);check(sound_on);advance(20);check(begin_gap);advance(80);check(end_gap);advance(20);check(sound_off);advance(24);check(gap_preserved);back();run();assert(!files_live&&!store_live&&!live&&opens==1&&closes==1);
 /* Pause/resume plus separate KV and fingerprint checkpoint transactions. */
 fresh();check(room_from_stopped);advance(20);check(room_pause);advance(10);check(room_resume);advance(220);check(room_committed);advance(20);check(room_cancel);back();run();assert(!files_live&&!store_live&&!live&&opens==4&&closes==4&&puts_count==1);
 fresh();check(room_open_failure);advance(20);check(room_failed_cancel);back();run();assert(!files_live&&!store_live&&!live&&opens==2&&closes==2&&!puts_count);
 fresh();check(auto_record);advance(200);check(cancelled_record);back();run();assert(!files_live&&!live&&file_writes==1&&opens==1&&closes==1);
 fresh();check(create_label);check(rejected_shift);back();run();assert(!files_live&&!live&&file_writes==1&&!opens);
 puts("Live learning: empty-gap clipped temporal review, continuous MIC, Add Sample auto-start/repeat/pause/resume/cancel/open retry, Record auto-name/start/warmup and refused shift-save guard PASS");return 0;
}
