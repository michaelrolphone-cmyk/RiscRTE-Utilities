#define ALARM_DND_CONTROL
#define ALARM_VOLUME_CONTROL
#define main existing_points_fixture
#include "points_service_test.c"
#undef main
static void dnd(bool on){blobs[8][0]=on;sizes[8]=1;}
static void begin(unsigned kind,bool mute){
 uint32_t noon=civil(2026,10,4,12,0);rtc_base=noon;boot(true);
 blobs[2][0]=3;sizes[2]=1;blobs[7][0]=80;sizes[7]=1;dnd(mute);
 if(kind==3)save(catalog(noon,POINTS_CUSTOM_1,0,3,false,false));
 else {
  save((points_config){.revision=1,.created=noon-3600});
  alarm_config c={.revision=1,.deadline=noon,.created=noon-60,.duration=kind==2?60:0,.kind=kind,.enabled=1};
  alarm_config_encode(&c,blobs[kind-1]);sizes[kind-1]=32;
 }
}
static void playing(void){for(unsigned i=0;i<100;i++){pump(1);if(active&&phase==PLAYING&&!persistence_pending)return;}assert(!"visual playback missing");}
static void confirmed_mute(void){for(unsigned i=0;i<100;i++){pump(1);if(active&&phase==PLAYING&&!persistence_pending&&desired.silenced)return;}assert(!"mute did not settle");}
static unsigned output_calls(void){return opens+writes+effects;}
int main(void){
 for(unsigned kind=1;kind<=3;kind++) {
  begin(kind,true);playing();assert(desired.silenced&&!output_calls());
  if(kind<3)assert(snapshot().state==ALARM_STATE_ALERT&&snapshot().occurrence.kind==kind);
  else assert(snapshot().state==ALARM_STATE_CUE);
  dnd(false);client->refresh(NULL);pump(20);assert(!output_calls());
  boot(false);playing();assert(desired.silenced&&!output_calls());
  if(kind==3)assert(points_occ.silenced);else assert(occurrences[kind-1].silenced);
  begin(kind,false);playing();assert(output_calls()>0);
  uint64_t began=alert_started;alarm_token_v1 before=token(&desired);
  unsigned calls=output_calls();dnd(true);client->refresh(NULL);confirmed_mute();
  assert(alert_started==began&&desired.generation==before.generation);
  assert(output_calls()==calls&&!audio_uncertain&&!haptic_uncertain);
  dnd(false);client->refresh(NULL);pump(20);assert(output_calls()==calls);
  boot(false);playing();assert(desired.silenced&&!output_calls());
  /* Restart during output cleanup preserves persistent DND; the pending
   * occurrence is muted before any new output and stays muted after Off. */
  begin(kind,false);playing();dnd(true);client->refresh(NULL);pump(1);assert(desired.silenced);
  boot(false);playing();assert(desired.silenced&&!output_calls());dnd(false);pump(20);assert(!output_calls());
  begin(kind,true);blobs[8][0]=2;pump(30);assert(phase==BLOCKED&&error==ALARM_STORAGE&&!output_calls());
 }
 for(unsigned volume=0;volume<=100;volume+=25) {
  begin(3,false);blobs[7][0]=volume;playing();assert(alert_limit_ms==350);
  assert(writes==(volume?1:0)&&effects==1); /* PCM assertion uses chosen level. */
 }
 puts("DND all: due/active visual state, confirmed mute, off/restart no replay and interrupted cleanup passed");
 return 0;
}
