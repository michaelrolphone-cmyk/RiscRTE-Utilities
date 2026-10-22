#define SPECTRUM_TEST_EVENTS 5000
#define SPECTRUM_TEMPORAL_MAIN legacy_temporal_tests
#include "spectrum_temporal_app_test.c"
#undef SPECTRUM_TEMPORAL_MAIN
static cf_bank roundtrip;
static unsigned room_slot;
static cf_bank before_add, pending_add;
static cf_feature prior_feature;
static void begin_room(void){
 shared_open(CF_ROOM);shared_tap(110,210);assert(page==PAGE_SHARED_EDIT);
 shared_tap(40,80);assert(shared_keyboard&&page==PAGE_KEYBOARD);
 strcpy(editing.name,"Office");keyboard_activate(PWK_DONE);
 assert(!shared_keyboard&&page==PAGE_SHARED_EDIT&&!strcmp(shared_name,"Office"));
 draw_shared();assert(shared_targets>=3);shared_focus=1;shared_buttons(T5_APP_BUTTON_CONFIRM);
 assert(running&&cfa_learning_kind==CF_ROOM);
}
static void save_room(void){
 assert(cfa_learn_ready());shared_tap(110,160);assert(!cfa_learning_kind&&!cfa_changed&&page==PAGE_SHARED);
 assert(cf_decode_source(&roundtrip,CF_AUDIO,event_disk[3],event_sizes[3]));
 int slot=cf_profile_find(&roundtrip,CF_ROOM,"Office");assert(slot>=0);room_slot=(unsigned)slot;
 const cf_feature*f=&roundtrip.profiles[slot].feature[CF_AUDIO];
 assert(f->windows>=3&&(f->flags&CF_BACKGROUND)&&(f->flags&CF_SPECTRAL));
 shared_tap(40,100);assert(page==PAGE_SHARED_EDIT);shared_tap(110,175);
 assert(page==PAGE_MAIN&&signature_mode==2&&cfa_background());
 uint32_t raw[128];memcpy(raw,signature_audio.power,sizeof(raw));
 signature_prepare_filter(-1);bool attenuated=false;for(unsigned i=0;i<128;i++){assert(signature_display_gains[i]<=65536);attenuated|=signature_display_gains[i]<65536;}
 assert(attenuated&&!memcmp(raw,signature_audio.power,sizeof(raw)));
 signature_mode=0;cfa_manual_name[0]=0;cfa_filter_load();
 assert(signature_mode==2&&!strcmp(cfa_manual_name,"Office")&&cfa_background());
 fail_put=true;assert(!cfa_filter_select(0,NULL));assert(signature_mode==2);fail_put=false;
 assert(cfa_filter_select(0,NULL));assert(!cfa_background());
 signature_mode=2;cfa_filter_load();assert(signature_mode==0);
 signature_mode=1;cfa_room.slot=-1;room_tracker.selected=0;
 assert(!cfa_background()); /* Unknown never subtracts a guessed legacy room. */
 signature_mode=0;
 /* Correlated slot numbers are not an identity. The same name updates its
  * shared profile even when a legacy capture chooses a different slot. */
 cf_publish(&cfa_fusion,CF_AUDIO,&cfa_window,cfa_time_us);
 assert(cfa_confirm(5,"Office",CF_ROOM));assert(cfa_bank.profiles[room_slot].kind==CF_ROOM&&!cfa_bank.profiles[5].kind);
 cfa_save();assert(!cfa_changed);
 /* Unknown commit is resolved by exact readback, without training twice. */
 cfa_changed=true;unknown_write=true;cfa_save();assert(!cfa_changed);unknown_write=false;
 pause_capture();cfa_loaded=false;memset(&cfa_bank,0,sizeof(cfa_bank));cfa_load();assert(cf_profile_find(&cfa_bank,CF_ROOM,"Office")>=0);CFA_RESUME();
}
/* Repeated observations update the selected name, not a second profile.
 * Check the existing shared-profile adaptation separately from the legacy
 * frame-weighted arithmetic mean. All samples below come from the PCM path. */
static void begin_more_room(void){
 before_add=cfa_bank;prior_feature=cfa_bank.profiles[room_slot].feature[CF_AUDIO];
 shared_open(CF_ROOM);shared_tap(40,100);assert(page==PAGE_SHARED_EDIT);
 assert(shared_selected==(int)room_slot&&!strcmp(shared_name,"Office"));
 extra_audio=true;shared_tap(110,130);
 assert(cfa_learning_kind==CF_ROOM&&!strcmp(cfa_learning.name,"Office"));
 assert(!memcmp(&before_add,&cfa_bank,sizeof(before_add)));
}
static void save_more_room(void){
 assert(cfa_learn_ready()&&cfa_learning_samples>=3);extra_audio=false;
 cf_feature captured=cfa_learning.feature[CF_AUDIO];
 assert(captured.power_mean!=prior_feature.power_mean);
 uint64_t prior_revision=file_revisions[3];
 file_error=RISC_APP_DATA_NO_SPACE;shared_tap(110,160);
 assert(page==PAGE_SHARED_CAPTURE&&cfa_learning_saved&&cfa_changed&&cfa_learning_kind==CF_ROOM);
 assert(file_revisions[3]==prior_revision&&cfa_bank.generation==before_add.generation+1);
 const cf_feature*f=&cfa_bank.profiles[room_slot].feature[CF_AUDIO];
 assert(f->windows==prior_feature.windows+1);
 assert(f->power_mean==prior_feature.power_mean*.95f+captured.power_mean*.05f);
 for(unsigned i=0;i<CF_BANDS;i++)assert(f->background[i]==prior_feature.background[i]*.95f+captured.background[i]*.05f);
 for(unsigned i=0;i<CF_PROFILES;i++)if(i!=room_slot)assert(!memcmp(&before_add.profiles[i],&cfa_bank.profiles[i],sizeof(cf_profile)));
 pending_add=cfa_bank;
 shared_tap(110,160); /* Retry must not blend or count twice. */
 assert(cfa_learning_saved&&cfa_changed&&!memcmp(&pending_add,&cfa_bank,sizeof(pending_add))&&file_revisions[3]==prior_revision);
 file_error=0;unknown_write=true;shared_tap(110,160);unknown_write=false;
 assert(page==PAGE_SHARED&&!cfa_learning_saved&&!cfa_changed&&!cfa_learning_kind);
 assert(!memcmp(&pending_add,&cfa_bank,sizeof(pending_add))&&file_revisions[3]>prior_revision);
 assert(cf_decode_source(&roundtrip,CF_AUDIO,event_disk[3],event_sizes[3]));
 assert(cf_profile_find(&roundtrip,CF_ROOM,"Office")==(int)room_slot);
 assert(roundtrip.profiles[room_slot].feature[CF_AUDIO].power_mean==f->power_mean);
 pause_capture();cfa_loaded=false;memset(&cfa_bank,0,sizeof(cfa_bank));cfa_load();
 assert(cfa_loaded&&!cfa_failed&&cf_profile_find(&cfa_bank,CF_ROOM,"Office")==(int)room_slot);
 assert(cfa_bank.profiles[room_slot].feature[CF_AUDIO].windows==prior_feature.windows+1);
 assert(cfa_bank.profiles[room_slot].feature[CF_AUDIO].power_mean==pending_add.profiles[room_slot].feature[CF_AUDIO].power_mean);
 CFA_RESUME();set_page(PAGE_MAIN);
}
static void begin_event(void){shared_open(CF_EVENT);shared_tap(110,210);strcpy(shared_name,"Door");shared_tap(110,130);assert(cfa_learning_kind==CF_EVENT);}
static void signal_on(void){extra_audio=true;}
static void signal_off(void){extra_audio=false;}
static void save_event_shared(void){
 assert(cfa_learning_samples>=3&&cfa_learn_ready());shared_tap(110,160);assert(!cfa_changed&&!cfa_learning_kind);
 assert(cf_decode_source(&roundtrip,CF_AUDIO,event_disk[3],event_sizes[3]));
 assert(cf_profile_find(&roundtrip,CF_ROOM,"Office")>=0&&cf_profile_find(&roundtrip,CF_EVENT,"Door")>=0);
 /* Unknown remains unknown when no matching temporal/spectral evidence exists. */
 cfa_event.slot=-1;cfa_apply();assert(!strcmp(cfa_name(CF_EVENT,"UNKNOWN"),"UNKNOWN"));
 set_page(PAGE_MAIN);
}
int main(void){
 fresh();check(begin_room);advance(1900);check(save_room);
 check(begin_more_room);advance(1900);check(save_more_room);
 check(begin_event);advance(100);
 for(unsigned i=0;i<3;i++){check(signal_on);advance(80);check(signal_off);advance(80);}
 check(save_event_shared);back();run();assert(!files_live&&!store_live&&!live);
 puts("Shared signal library: real PCM room/event training, named profiles, repeated-label accumulation, full-storage/unknown-save retry without duplicate blending, readback/reopen, background subtraction without raw mutation PASS");return 0;
}
