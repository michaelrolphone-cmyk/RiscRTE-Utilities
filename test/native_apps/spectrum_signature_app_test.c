#define main existing_controller_main
#include "audio_spectrum_test.c"
#undef main
static void new_event(void){assert(signature_audio.available);signature_open(-1,SPECTRUM_SIGNATURE_EVENT);strcpy(editing.name,"Door");signature_capture_begin();assert(signatures[0].kind==2&&signatures[0].frames==1&&!signature_pending);draw();}
static void more_event(void){signature_open(0,0);signature_capture_begin();assert(signatures[0].frames==2&&!signature_pending);signature_capture_begin();assert(signatures[0].frames==2);}
static void event_restored(void){signature_restore();assert(signatures[0].kind==2&&signatures[0].frames==2&&!strcmp(signatures[0].name,"Door")&&!signature_errors);}
static void new_room(void){signature_open(-1,1);strcpy(editing.name,"Office");signature_capture_begin();assert(signature_goal==64);draw();}
static void room_saved(void){assert(!signature_goal&&!signature_pending&&signatures[0].frames==64&&signatures[0].kind==1);signature_mode=1;draw();}
static void add_room(void){signature_open(0,0);signature_capture_begin();assert(signature_goal==64);}
static void room_average(void){assert(!signature_goal&&signatures[0].frames==128&&!signature_pending);}
static void put_failure(void){fail_put=true;signature_open(-1,2);strcpy(editing.name,"Motor");signature_capture_begin();assert(signature_pending==1&&signatures[0].frames==1);assert(!signature_exit_ready());assert(page==PAGE_SIGNATURES&&!back_exits);signature_save();assert(signature_pending==1&&signatures[0].frames==1);}
static void retry_put(void){fail_put=false;signature_save();assert(!signature_pending&&signatures[0].frames==1);signature_restore();assert(signatures[0].frames==1);}
static int32_t uncertain_put(void *c,const char*k,const void*b,uint32_t n){int32_t result=put_value(c,k,b,n);return result==0?RISC_KEY_VALUE_IO:result;}
static void uncertain_saved(void){kv.put=uncertain_put;signature_open(-1,2);strcpy(editing.name,"Motor");signature_capture_begin();assert(signature_pending==1&&signatures[0].frames==1);kv.put=put_value;signature_save();signature_restore();assert(signatures[0].frames==1&&!signature_pending);}
static void unread(void){assert(signature_errors==1&&!signatures[0].kind);signature_load();assert(signature_errors==1);signature_open(-1,2);assert(signature_slot==1);strcpy(editing.name,"Other");signature_capture_begin();assert(signatures[1].frames==1&&signature_errors==1);assert(cells[0].bytes[0]==255);}
static void cancel_sample(void){assert(signature_goal&&signature_capture.frames>0);assert(!signature_exit_ready());bool t=false;signature_tap(170,125,&t);assert(!signature_goal&&!signatures[0].kind&&!signature_pending);}
static void paused_sample(void){assert(signature_goal&&signature_capture.frames>0);assert(portable_audio_suspend());unsigned n=signature_capture.frames;assert(!running&&signature_capture.frames==n&&signature_goal==64);draw();}
static void sample_keyboard(void){signature_open(-1,2);strcpy(editing.name,"Before");signature_keyboard=true;keyboard_begin();keyboard_activate(0);keyboard_cancel();assert(page==PAGE_SIGNATURE_EDIT&&!strcmp(editing.name,"Before"));signature_keyboard=true;keyboard_begin();keyboard_activate(PWK_DONE);assert(page==PAGE_SIGNATURE_EDIT);draw();set_page(PAGE_MAIN);}
static void frozen_capture(void){freeze();assert(!running&&signature_audio.available);new_event();}
static void capacity(void){for(unsigned i=0;i<8;i++){signatures[i].kind=2;strcpy(signatures[i].name,"Taken");signatures[i].frames=1;}signature_open(-1,2);assert(signature_slot==-1&&strstr(toast_message,"FULL"));}
static void discard_failed(void){assert(signature_pending);signature_open(0,0);signature_delete();assert(signature_pending&&signature_delete_confirm);signature_delete();assert(!signature_pending&&!signatures[0].kind&&!signature_errors);}
static void failed_delete_discard(void){
 bool t=false,f=false;signature_open(0,0);fail_put=true;tap_action(50,210,&t,&f);assert(signature_delete_confirm&&signatures[0].kind==2&&!toast_message);tap_action(50,210,&t,&f);assert(!signatures[0].kind&&signature_pending==1&&page==PAGE_SIGNATURES);
 /* Pending tombstones are visible and openable; no forced-exit trap. */
 toast_message=NULL;tap_action(70,120,&t,&f);assert(page==PAGE_SIGNATURE_EDIT);draw();tap_action(50,210,&t,&f);assert(signature_delete_confirm);tap_action(50,210,&t,&f);assert(!signature_pending&&signatures[0].kind==2&&signatures[0].frames==1&&page==PAGE_SIGNATURES);
}
static void full_room_count(void){signatures[0].kind=1;signatures[0].frames=SPECTRUM_SIGNATURE_MAX_FRAMES-63;strcpy(signatures[0].name,"Full");signature_open(0,0);signature_capture_begin();assert(!signature_goal&&signatures[0].frames==SPECTRUM_SIGNATURE_MAX_FRAMES-63);set_page(PAGE_MAIN);}
static void history_invalidated(void){signature_mode=2;manual_room=0;signatures[0].kind=1;history_count=20;signature_open(0,0);signature_delete();signature_delete();assert(!history_count&&signature_filter()==-1);}
static void frozen_filter_redraw(void){
 assert(spectrum.has_transform&&signature_audio.available);freeze();uint32_t raw[SPECTRUM_DSP_MAX_FFT/2+1];memcpy(raw,spectrum.amplitude_q24,sizeof(raw));signatures[0].kind=1;signatures[0].frames=64;strcpy(signatures[0].name,"Snapshot");memcpy(signature_means[0],signature_audio.power,sizeof(signature_audio.power));
 set_page(PAGE_SIGNATURES);bool t=false;signature_tap(200,120,&t);assert(signature_filter()==0&&history_count==1&&!running);assert(!memcmp(raw,spectrum.amplitude_q24,sizeof(raw)));signature_tap(40,60,&t);assert(signature_filter()==-1&&history_count==1&&!running);assert(!memcmp(raw,spectrum.amplitude_q24,sizeof(raw)));
}
static void advance(unsigned n){for(unsigned i=0;i<n;i++)add(EVENT_INPUT,0,-1);}
int main(void){
 reset();start();advance(4);check(new_event);advance(4);check(more_event);check(event_restored);back();back();back();run();assert(!store_live&&!live&&!signature_pending);
 reset();start();advance(4);check(new_room);advance(140);check(room_saved);check(add_room);advance(140);check(room_average);back();back();back();run();assert(!store_live&&!live&&puts_count==2);
 reset();start();advance(4);check(put_failure);check(retry_put);back();back();back();run();assert(!store_live&&!live);
 reset();start();advance(4);check(uncertain_saved);back();back();back();run();assert(!store_live&&!live);
 reset();strcpy(cells[0].key,"spectrum_s0");memset(cells[0].bytes,255,SPECTRUM_SIGNATURE_RECORD_SIZE);cells[0].size=SPECTRUM_SIGNATURE_RECORD_SIZE;start();advance(4);check(unread);back();back();back();run();assert(!store_live&&!live);
 reset();start();advance(4);check(new_room);advance(12);check(paused_sample);check(cancel_sample);back();back();back();run();assert(!store_live&&!live&&!puts_count);
 reset();start();advance(4);check(sample_keyboard);back();run();assert(!store_live&&!live&&!puts_count);
 reset();start();advance(4);check(frozen_capture);back();back();back();run();assert(!store_live&&!live&&!signature_pending);
 reset();check(capacity);back();run();assert(!store_live&&!live&&!puts_count);
 reset();start();advance(4);check(put_failure);check(discard_failed);back();back();back();run();assert(!store_live&&!live&&!signature_pending);
 reset();start();advance(4);check(new_event);check(failed_delete_discard);back();back();back();run();assert(!store_live&&!live&&!signature_pending);
 reset();start();advance(4);check(full_room_count);back();run();assert(!store_live&&!live);
 reset();start();advance(4);check(new_event);check(history_invalidated);back();back();back();run();assert(!store_live&&!live);
 reset();start();advance(24);check(frozen_filter_redraw);back();back();back();run();assert(!store_live&&!live);
 reset();kv.api_version=1;back();run();assert(!storage&&!store_live&&!live&&signature_errors==255&&!puts_count);
 puts("Signature controller: labeled capture/add/reload, room64 averaging, frozen frame, full capacity, unread slot protection, uncertain-save retry without double count, discard confirmation, keyboard cancel, pause/cancel, exit guard and cleanup passed");
}
