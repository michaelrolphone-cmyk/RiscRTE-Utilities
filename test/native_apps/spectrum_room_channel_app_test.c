#define SPECTRUM_TEMPORAL_MAIN original_temporal_app_main
#include "spectrum_temporal_app_test.c"
static spectrum_signature rooms_before[SPECTRUM_SIGNATURE_SLOTS];
static void room_observation(unsigned band,uint32_t power){
 memset(signature_audio.power,0,sizeof(signature_audio.power));signature_audio.power[band]=power;
 signature_audio.transforms++;now+=32;signature_observe();
}
static void check_room_transition(void){
 for(unsigned fft=256;fft<=8192;fft*=2)for(unsigned mode=0;mode<3;mode++){
  spectrum_background_reset(&ambient);temporal_reset_context();spectrum_room_reset(&room_tracker);
  memset(signatures,0,sizeof(signatures));memset(signature_means,0,sizeof(signature_means));signature_mode=mode;manual_room=0;prefs.fft_size=fft;configure_dsp();
  for(unsigned slot=0;slot<2;slot++){
   signatures[slot].kind=SPECTRUM_SIGNATURE_ROOM;signatures[slot].frames=SPECTRUM_SIGNATURE_ROOM_FRAMES;
   strcpy(signatures[slot].name,slot?"Workshop":"Office");unsigned band=slot?40:4;
   signatures[slot].sums[band]=64000;signature_means[slot][band]=1000;
  }
  memcpy(rooms_before,signatures,sizeof(signatures));
  for(unsigned i=0;i<128;i++)room_observation(4,1000);
  assert(ambient.ready&&room_tracker.selected==0);
  /* A short loud impact remains a transient, never a room switch. */
  for(unsigned i=0;i<3;i++)room_observation(40,100000);
  assert(room_tracker.selected==0);
  for(unsigned i=0;i<128;i++)room_observation(4,1000);
  event_known_hold=true;
  for(unsigned i=0;i<240;i++)room_observation(40,1000);
  assert(ambient.slow[40]==0&&ambient.foreground);
  assert(room_tracker.selected==1&&room_tracker.confidence>=99);
  assert(signature_filter()==(mode==0?-1:mode==1?1:0));
  assert(!memcmp(signatures,rooms_before,sizeof(signatures))&&!signature_pending&&!puts_count);
  /* Real silence clears the room even while the event-floor EMA is stale. */
  for(unsigned i=0;i<64;i++)room_observation(0,0);
  assert(room_tracker.selected==-1);
  signatures[2]=(spectrum_signature){.kind=SPECTRUM_SIGNATURE_ROOM,.frames=SPECTRUM_SIGNATURE_ROOM_FRAMES,.name="Quiet room"};
  signatures[2].sums[80]=1024;signature_means[2][80]=16;
  for(unsigned i=0;i<480;i++)room_observation(80,16);
  assert(room_tracker.selected==2&&room_tracker.mean[4]==0&&room_tracker.mean[40]==0&&room_tracker.mean[80]==16);
 }
 set_page(PAGE_MAIN);
}
int main(void){fresh();check(check_room_transition);back();run();assert(!files_live&&!store_live&&!live);puts("Room channel: new saved room recognized independently of held event floor, burst rejection, silence release, all display FFTs/modes and unchanged saved profiles PASS");return 0;}
