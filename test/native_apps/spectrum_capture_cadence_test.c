/* Real app against a clocked, bounded 2 x 256-frame RX and blocking display.
 * DMA replacement drops the oldest unread PCM; no invented continuous stream. */
#define SPECTRUM_TEST_EVENTS 12000
#define SPECTRUM_TEMPORAL_MAIN prior_temporal_cadence_main
#include "spectrum_temporal_app_test.c"
#undef SPECTRUM_TEMPORAL_MAIN

static unsigned paint_ms,model_drops,model_room,model_partial;
static uint32_t model_clock,model_produced,model_consumed,model_last_read;
static bool model_empty,model_error,force_paint,check_progress;
static unsigned previous_warmup,complete_frames,max_paint_gap;
static uint32_t last_paint;
static uint8_t saved_rooms[2][SPECTRUM_SIGNATURE_RECORD_SIZE];
static int16_t model_sample(uint32_t at,unsigned room){
 unsigned a=room?1500u:437u,b=room?2500u:1000u;
 if(room==2)return 0;
 return (int16_t)((spectrum_dsp_sin((uint32_t)((uint64_t)at*a*65536u/16000u))>>17)+
                  (spectrum_dsp_sin((uint32_t)((uint64_t)at*b*65536u/16000u))>>19));
}
static void model_produce(void){
 uint32_t elapsed=now-model_clock;model_clock=now;
 if(model_empty){model_consumed=model_produced;return;}
 model_produced+=elapsed*16u;
 if(model_produced-model_consumed>512u){model_drops+=model_produced-model_consumed-512u;model_consumed=model_produced-512u;}
}
static bool model_read(void*c,int16_t*p,size_t cap,size_t*got){
 assert(c==&mic&&live&&cap==256);reads++;model_produce();
 if(model_error){*got=0;return false;}
 if(model_empty){now+=40;model_produce();*got=0;return true;}
 unsigned want=model_partial?model_partial:256;
 unsigned available=model_produced-model_consumed;
 if(available<want){now+=(want-available+15)/16;model_produce();}
 for(unsigned i=0;i<want;i++){
  uint32_t at=model_consumed++;int32_t sample=model_sample(at,model_room);
  if(extra_audio)sample+=spectrum_dsp_sin((uint32_t)((uint64_t)at*1800u*65536u/16000u))>>16;
  p[i]=(int16_t)sample;
 }
 *got=want;model_last_read=now;return true;
}
static void model_present(bool full){
 present(full);
 if(running){
  unsigned gap=now-last_paint;if(gap>max_paint_gap)max_paint_gap=gap;
  last_paint=now;now+=paint_ms;
 }
}
static bool model_poll(t5_app_input_t*out,uint32_t wait){
 bool ok=poll(out,wait);
 if(check_progress&&running){assert(ambient.frames>=previous_warmup);previous_warmup=ambient.frames;}
 if(force_paint)dirty=true;
 return ok;
}
static void seed_room(unsigned slot){
 spectrum_signature s={.kind=SPECTRUM_SIGNATURE_ROOM};
 strcpy(s.name,slot?"Kitchen":"Office");spectrum_signature_analyzer a;spectrum_signature_init(&a);
 int16_t pcm[256];
 for(unsigned n=0;n<128;n++){
  for(unsigned i=0;i<256;i++)pcm[i]=model_sample(n*256+i,slot);
  assert(spectrum_signature_feed(&a,pcm,256));if(!(n&1))continue;
  assert(spectrum_signature_add(&s,a.power));
 }
 assert(s.frames==64);snprintf(cells[slot].key,sizeof(cells[slot].key),"spectrum_s%u",slot);
 assert(spectrum_signature_encode(&s,cells[slot].bytes));cells[slot].size=SPECTRUM_SIGNATURE_RECORD_SIZE;
}
static void model_fresh(unsigned delay,unsigned fft,unsigned partial){
 fresh();paint_ms=delay;model_drops=model_room=0;model_partial=partial;
 model_clock=model_produced=model_consumed=model_last_read=last_paint=0;
 model_empty=model_error=false;force_paint=check_progress=true;
 previous_warmup=complete_frames=max_paint_gap=0;
 api.present=model_present;api.poll=model_poll;mic.read=model_read;
 seed_room(0);seed_room(1);for(unsigned i=0;i<2;i++)memcpy(saved_rooms[i],cells[i].bytes,sizeof(saved_rooms[i]));
 spectrum_preferences p=spectrum_preferences_default();p.fft_size=(uint16_t)fft;
 uint8_t bytes[32];spectrum_preferences_encode(&p,bytes);preload("spectrum_cfg",bytes);
}
static void monitor_page(void){view=2;dirty=true;}
static void waterfall_page(void){view=1;dirty=true;}
static void room_is_matched(void){
 assert(running&&owned&&ambient.ready&&ambient.frames==64);
 assert(room_tracker.selected==(int)model_room&&room_tracker.confidence>=80&&!room_tracker.ambiguous);
 assert(signatures[0].frames==64&&signatures[1].frames==64&&!puts_count);
 for(unsigned i=0;i<2;i++){uint8_t bytes[SPECTRUM_SIGNATURE_RECORD_SIZE];assert(spectrum_signature_encode(&signatures[i],bytes));assert(!memcmp(bytes,saved_rooms[i],sizeof(bytes))&&!memcmp(cells[i].bytes,saved_rooms[i],sizeof(bytes)));}
 assert(!input_waiting&&!capture_error&&opens==closes+1);
 if(paint_ms>32)assert(model_drops>0);
 assert(frames>8&&max_paint_gap<1300u);complete_frames=signature_audio.transforms;
}
static void plot_is_live(void){room_is_matched();assert(spectrum.transforms>2&&history_count>2);}
static void stop_for_retry(void){check_progress=false;stop();assert(!running&&!capture_requested);}
static void restarted(void){assert(running&&opens==2&&ambient.frames<64&&room_tracker.selected<0);check_progress=true;previous_warmup=ambient.frames;}
static void begin_empty(void){check_progress=false;model_empty=true;}
static void waiting_without_scene(void){assert(input_waiting&&input_gap&&running&&!ambient.ready&&!ambient.frames&&room_tracker.selected<0);assert(opens==1&&!closes);model_empty=false;previous_warmup=0;check_progress=true;}
static void recovered(void){assert(signature_audio.transforms>complete_frames);room_is_matched();}
static void change_room(void){model_room=1;}
static void wrong_manual_filter(void){signature_mode=2;manual_room=1;}
static void ambiguous_saved_rooms(void){
 signatures[1]=signatures[0];strcpy(signatures[1].name,"Similar");spectrum_signature_mean(&signatures[1],signature_means[1]);
}
static void ambiguous_room(void){assert(ambient.ready&&room_tracker.ambiguous&&room_tracker.selected<0&&!puts_count);}
static void silent_room(void){assert(ambient.ready&&!room_tracker.ambiguous&&room_tracker.selected<0&&!puts_count);}
static void fast_paint(void){paint_ms=0;force_paint=false;}
static void interrupt_recording(void){assert(event_armed&&event_training.collecting);paint_ms=160;force_paint=dirty=true;}
static void clipped_recording(void){
 assert(running&&!event_armed&&event_training.ready&&(event_training.event.flags&ST_CLIPPED));
 unsigned n=event_training.event.count;for(unsigned i=0;i<8;i++){now+=1;capture();}
 assert(event_training.event.count==n);event_cancel_capture();set_page(PAGE_MAIN);
}
static void fail_next_read(void){check_progress=false;model_error=true;}
static void failed_read(void){assert(!running&&!owned&&capture_error&&!capture_requested&&closes==1);assert(!strcmp(message,"MIC READ FAILED"));model_error=false;}
static void sparse_input(void){check_progress=false;model_partial=1;previous_warmup=ambient.frames;}
static void sparse_paints_remain_bounded(void){assert(frames>8&&max_paint_gap<1300u&&running);}
static void stop_sparse(void){stop();assert(!running);}
#ifndef SPECTRUM_PRE_FIX_REGRESSION
static void adaptation_across_paints(void){
 spectrum_background uninterrupted={0},interrupted={0};uint32_t p[128]={0};p[10]=1000;
 for(unsigned i=0;i<64;i++){spectrum_background_observe(&uninterrupted,p);spectrum_background_observe(&interrupted,p);}
 p[10]=10000;
 for(unsigned i=0;i<1800;i++){
  if(!(i%2))spectrum_background_interrupt(&interrupted);
  spectrum_background_observe(&uninterrupted,p);spectrum_background_observe(&interrupted,p);
  assert(!memcmp(&uninterrupted,&interrupted,sizeof(interrupted)));
  if(i<1023)assert(interrupted.slow[10]==1000);
 }
 assert(interrupted.slow[10]>1000);
 /* Both SNR hysteresis thresholds remain unchanged across display gaps. */
 p[10]=interrupted.slow[10]*5u;spectrum_background_observe(&interrupted,p);assert(interrupted.active[10]);
 p[10]=interrupted.slow[10]*3u;spectrum_background_observe(&interrupted,p);assert(interrupted.active[10]);
 spectrum_background_interrupt(&interrupted);assert(!interrupted.foreground&&!interrupted.raw[10]&&!interrupted.excess[10]);
 spectrum_background_observe(&interrupted,p);assert(interrupted.active[10]);
 p[10]=0;spectrum_background_observe(&interrupted,p);assert(!interrupted.active[10]&&!interrupted.age[10]);
}
#endif
int main(void){
#ifndef SPECTRUM_PRE_FIX_REGRESSION
 adaptation_across_paints();
#endif
 /* Every paint is slow, and every loop requests another paint. Both halves
  * of a canonical frame must still arrive before it can be interrupted. */
 const unsigned sizes[]={256,512,1024,2048,4096,8192};
 for(unsigned f=0;f<6;f++){unsigned fft=sizes[f];
  model_fresh(160,fft,0);check(monitor_page);start();advance(600);check(room_is_matched);back();run();assert(!live&&!files_live&&!store_live);
 }
 model_fresh(16,2048,0);check(monitor_page);start();advance(400);check(room_is_matched);back();run();
 model_fresh(160,2048,0);now=model_clock=last_paint=UINT32_MAX-300u;check(monitor_page);start();advance(400);check(room_is_matched);back();run();
 model_fresh(160,2048,63);check(monitor_page);start();advance(1400);check(room_is_matched);back();run();
 model_fresh(160,2048,17);check(monitor_page);start();advance(4500);check(room_is_matched);back();run();
 model_fresh(160,2048,0);check(monitor_page);check(wrong_manual_filter);start();advance(400);check(room_is_matched);back();run();
 model_fresh(160,2048,0);model_room=1;check(monitor_page);start();advance(400);check(room_is_matched);back();run();
 model_fresh(160,2048,0);model_room=2;check(monitor_page);start();advance(400);check(silent_room);back();run();
 model_fresh(160,2048,0);check(monitor_page);check(ambiguous_saved_rooms);start();advance(400);check(ambiguous_room);back();run();
 model_fresh(160,2048,0);check(monitor_page);start();advance(400);check(room_is_matched);check(change_room);advance(9000);check(room_is_matched);back();run();
 model_fresh(160,2048,64);check(monitor_page);start();advance(1300);check(room_is_matched);back();run();
 model_fresh(160,2048,0);start();advance(600);check(plot_is_live);back();run();
 model_fresh(160,8192,0);check(waterfall_page);start();advance(600);check(plot_is_live);back();run();
 model_fresh(160,2048,0);check(monitor_page);start();advance(400);check(room_is_matched);check(begin_empty);advance(70);check(waiting_without_scene);advance(400);check(recovered);back();run();
 model_fresh(160,2048,0);check(monitor_page);start();advance(400);check(room_is_matched);check(stop_for_retry);start();check(restarted);advance(400);check(room_is_matched);back();run();
 model_fresh(160,2048,0);check(create_label);check(monitor_page);start();advance(400);check(fast_paint);check(arm_positive);advance(20);check(sound_on);advance(20);check(interrupt_recording);advance(20);check(clipped_recording);back();run();assert(!live&&!files_live&&!store_live);
 model_fresh(160,2048,0);check(monitor_page);start();advance(400);check(room_is_matched);check(fail_next_read);check(failed_read);start();check(restarted);advance(400);check(room_is_matched);back();run();
 model_fresh(160,2048,0);check(monitor_page);start();advance(100);check(sparse_input);advance(1400);check(sparse_paints_remain_bounded);check(stop_sparse);back();run();
 model_fresh(160,2048,0);check(monitor_page);start();advance(100);check(sparse_input);add(EVENT_INPUT,T5_APP_BUTTON_DOWN,1);back();run();assert(frozen&&!running&&closes==1);
 puts("Clocked 2x256 RX + slow display: room warmup/matching, partial input, 2048/8192 plot, empty gap recovery and restart PASS");
 return 0;
}
