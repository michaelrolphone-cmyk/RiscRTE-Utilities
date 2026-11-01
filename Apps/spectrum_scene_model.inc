/* Audio Spectrum domain extracted from published bf111791.
 * Sampling/history capacities below are retained data bounds, not display size.
 * Presentation, physical input and lifecycle are supplied by the shared shell. */
#include "RiscRuntimeV1.h"
#include "AudioInputV1.h"
#include "RiscKeyValueV1.h"
#include "spectrum_dsp.h"
#include "spectrum_store.h"
#include "spectrum_signature_store.h"
#include "spectrum_temporal_files.h"
#include "spectrum_neural_store.h"
#include "spectrum_speech.h"
#include <stdio.h>
#define SPECTRUM_HISTORY_FRAMES 228u
#define SPECTRUM_HISTORY_BANDS 166u
#define FRAME_MS 33u
#define PREFERRED_RATE 16000u
static const struct spectrum_domain_clock { uint32_t (*millis)(void); } *app;
static const risc_runtime_api_v1 *runtime;
static bool spectrum_domain_alive(void);
static void spectrum_domain_retain(void);
static void keyboard_begin(void);
static bool spectrum_domain_storage_boundary(void);
#include "spectrum_scene_io.h"
static const twatch_audio_in_api_v1 *microphone;
static const risc_key_value_v1 *storage;
static risc_runtime_capability_v1 grant,store_grant;
static spectrum_dsp_state spectrum;
static spectrum_background ambient;
static spectrum_speech speech;
static uint32_t foreground_power[128];
static spectrum_preferences prefs;
static spectrum_label labels[SPECTRUM_LABEL_MAX],editing,deleted;
static spectrum_dsp_config dsp_config;
static bool acquired,owned,uncertain,running,frozen,dirty,store_acquired,capture_error,input_waiting,input_gap,capture_requested;
static uint32_t last_pcm_at;
static bool lab_edit,cursor_visible,started;
static bool label_active[SPECTRUM_LABEL_MAX],level_valid[SPECTRUM_LABEL_MAX];
static int16_t label_db[SPECTRUM_LABEL_MAX],label_snr[SPECTRUM_LABEL_MAX];
static uint16_t spec_levels[SPECTRUM_HISTORY_FRAMES],fall_levels[SPECTRUM_HISTORY_BANDS],peak_levels[SPECTRUM_HISTORY_FRAMES];
static int16_t spec_db[SPECTRUM_HISTORY_FRAMES],fall_db[SPECTRUM_HISTORY_BANDS];
static uint8_t history[SPECTRUM_HISTORY_FRAMES][SPECTRUM_HISTORY_BANDS];
static uint16_t history_next,history_count,pending_save,load_errors;
static uint32_t rendered,recorded_transform,sample_rate;
static unsigned view,page,list_scroll;
static int edit_slot,undo_slot;
static uint16_t cursor_hz;
static const char *message,*store_message,*toast_message;
static uint32_t toast_until;
static uint32_t palette[256];
static const uint32_t label_colors[8]={0xffd24au,0xff7a1au,0xff3d71u,0xb24dffu,0x6d7bffu,0x3d9bffu,0x3dff9au,0x9be15du};
static const char *palette_names[5]={"NOVA","INFERNO","VIRIDIS","GRAY","JET"};
static const char *window_names[5]={"RECT","HANN","HAMMING","BLACKMAN","FLAT TOP"};
enum {PAGE_MAIN,PAGE_CONTROLS,PAGE_LABEL,PAGE_KEYBOARD,PAGE_SIGNATURES,PAGE_SIGNATURE_EDIT,PAGE_EVENTS,PAGE_EVENT_LABEL,PAGE_EVENT_CAPTURE,PAGE_EVENT_EXAMPLES,PAGE_SHARED,PAGE_SHARED_EDIT,PAGE_SHARED_CAPTURE};
static int clamp_int(int n,int low,int high){return n<low?low:n>high?high:n;}
static int abs_int(int n){return n<0?-n:n;}
static inline void freq_text(unsigned hz,char out[20]){if(hz>=1000)snprintf(out,20,"%u.%02u kHz",hz/1000,(hz%1000)/10);else snprintf(out,20,"%u Hz",hz);}
static inline void short_freq(unsigned hz,char out[20]){if(hz>=1000 && !(hz%1000))snprintf(out,20,"%uk",hz/1000);else snprintf(out,20,"%u",hz);}
static void notify(const char *s){toast_message=s;toast_until=app->millis()+4200u;dirty=true;}
static bool acquire_storage(void);
static void set_page(unsigned next);
static void keyboard_begin(void);
static void clear_analysis(void);
static void refresh_analysis(void);
static void stop(void);
static void pause_capture(void);
static void toggle(void);
void portable_audio_capture_resume(void);
static bool event_keyboard;
static void temporal_observe(void);
static void detect_frequency_labels(void);
static void temporal_suspend(void);
static bool temporal_retained(void);
static bool temporal_exit_blocked(void);
static bool temporal_exit_ready(void);

static void cfa_restore(void);
static const uint32_t*cfa_background(void);
static bool cfa_filter_select(unsigned,const char*);
static void shared_open(unsigned);
static bool shared_back(void);
static void shared_keyboard_done(void);
static bool shared_keyboard;
static void cfa_arm(void);
static void cfa_begin(void);
static void cfa_save(void);
static bool cfa_unsaved(void);
static bool cfa_has_events(void);
static const char*cfa_name(unsigned,const char*);
static void cfa_retry(void);
static void cfa_discard(void);
static void cfa_forget(unsigned);
static void cfa_rename_event(unsigned,const char*);
static bool cfa_confirm(unsigned,const char*,unsigned);
static bool cfa_save_event(unsigned,const char*,bool);
static void cfa_event_boundary(bool,bool,bool);
static void cfa_apply(void);
static void set_page(unsigned next){page=next;dirty=true;}
static void retain(void){spectrum_domain_retain();}
#include "spectrum_scene_signatures.inc"
#include "spectrum_scene_temporal.inc"
#define CFA_SOURCE 0
#define CFA_RESUME portable_audio_capture_resume
#define CFA_MATCH_REASON ST_RESULT_MATCH
#define CFA_UNKNOWN_REASON ST_RESULT_UNKNOWN
#include "spectrum_scene_fingerprint.inc"
bool portable_audio_suspend(void){
 if(uncertain)return false;
 bool was_running=running;running=false;input_waiting=false;spectrum.used=0;signature_suspend();temporal_suspend();dirty=true;
 if(!owned){if(was_running)message="STOPPED / MIC OFF";return true;}
 if(!microphone->close(microphone->context)){uncertain=true;return false;}
 owned=false;message="STOPPED / MIC OFF";return true;
}
static void pause_capture(void){if(!portable_audio_suspend())retain();}
static void stop(void){capture_requested=false;pause_capture();}
void portable_audio_capture_resume(void){if(capture_requested&&!running&&!uncertain&&!temporal_retained())toggle();}
static bool acquire_microphone(void){
 if(!spectrum_domain_alive())return false;
 if(microphone)return true;
 if(acquired)return false;
 if(!spectrum_acquire("audio.input",1,0,&grant))return false;
 if(!spectrum_domain_alive())return false;
 acquired=true;
 const twatch_audio_in_api_v1 *p=grant.api;if(!p||p->api_version!=1||p->struct_size<sizeof(*p)||!p->open||!p->read||!p->close)return false;microphone=spectrum_guard_microphone(p);return true;
}
static bool acquire_storage(void){
 if(!spectrum_domain_alive())return false;
 if(storage)return true;
 if(store_acquired)return false;
 if(!spectrum_acquire("storage.key-value",2,0,&store_grant))return false;
 if(!spectrum_domain_alive())return false;
 store_acquired=true;
 const risc_key_value_v1 *p=store_grant.api;if(!p||p->api_version!=2||p->struct_size<sizeof(*p)||!p->get||!p->put)return false;storage=spectrum_guard_storage(p);return true;
}
static void label_key(unsigned slot,char out[16]){snprintf(out,16,"spectrum_l%u",slot);}
static void reload_storage(void){
 if(!load_errors)return;
 if(!acquire_storage()){store_message="STORAGE UNAVAILABLE";return;}
 uint8_t bytes[32];uint32_t n=0;int32_t r;
 if(load_errors&1u){
  spectrum_preferences loaded=spectrum_preferences_default();
  r=storage->get(storage->context,"spectrum_cfg",bytes,sizeof(bytes),&n);
  if(r==RISC_KEY_VALUE_NOT_FOUND || (r==RISC_KEY_VALUE_OK && spectrum_preferences_decode(&loaded,bytes,n))){prefs=loaded;load_errors&=(uint16_t)~1u;}
 }
 for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(load_errors&(2u<<i)){
  char key[16];label_key(i,key);n=0;spectrum_label loaded=spectrum_label_default(i);
  r=storage->get(storage->context,key,bytes,sizeof(bytes),&n);
  if(r==RISC_KEY_VALUE_NOT_FOUND || (r==RISC_KEY_VALUE_OK && spectrum_label_decode(&loaded,bytes,n))){labels[i]=loaded;load_errors&=(uint16_t)~(2u<<i);}
 }
 store_message=load_errors?"SAVED DATA UNREAD / RETRY":pending_save?"UNSAVED CHANGES":NULL;
}
static void restore(void){
 prefs=spectrum_preferences_default();memset(labels,0,sizeof(labels));
 load_errors=(uint16_t)((1u<<(SPECTRUM_LABEL_MAX+1u))-1u);reload_storage();
}
static void persist(void){
 if(!pending_save)return;
 if(pending_save&load_errors){store_message="RETRY STORAGE FIRST";notify(store_message);return;}
 if(!acquire_storage()){store_message="UNSAVED / NO STORAGE";notify("UNSAVED / NO STORAGE");return;}
 uint8_t bytes[32];bool failed=false;
 if(pending_save&1){spectrum_preferences_encode(&prefs,bytes);if(storage->put(storage->context,"spectrum_cfg",bytes,sizeof(bytes))==RISC_KEY_VALUE_OK)pending_save&=(uint16_t)~1u;else failed=true;}
 for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(pending_save&(2u<<i)){char key[16];label_key(i,key);spectrum_label_encode(&labels[i],bytes);if(storage->put(storage->context,key,bytes,sizeof(bytes))==RISC_KEY_VALUE_OK)pending_save&=(uint16_t)~(2u<<i);else failed=true;}
 if(failed){store_message="SAVE UNCONFIRMED";notify("SAVE UNCONFIRMED");}else store_message=load_errors?"SAVED DATA UNREAD / RETRY":NULL;
}
static void clear_analysis(void){signature_display_reset();memset(history,0,sizeof(history));memset(spec_levels,0,sizeof(spec_levels));memset(fall_levels,0,sizeof(fall_levels));memset(peak_levels,0,sizeof(peak_levels));memset(label_active,0,sizeof(label_active));memset(level_valid,0,sizeof(level_valid));for(unsigned i=0;i<SPECTRUM_HISTORY_FRAMES;i++)spec_db[i]=-12000;for(unsigned i=0;i<SPECTRUM_HISTORY_BANDS;i++)fall_db[i]=-12000;history_next=history_count=0;recorded_transform=0;}
static void configure_dsp(void){
 dsp_config=spectrum_dsp_defaults();dsp_config.sample_rate=sample_rate;dsp_config.fft_size=prefs.fft_size;dsp_config.window=(spectrum_dsp_window)prefs.window;dsp_config.low_hz=prefs.low_hz;dsp_config.high_hz=prefs.high_hz>sample_rate/2?sample_rate/2:prefs.high_hz;dsp_config.log_frequency=prefs.log_frequency;dsp_config.log_amplitude=prefs.log_amplitude;dsp_config.gain_db=prefs.gain_db;dsp_config.threshold_db=prefs.threshold_db;
 (void)spectrum_dsp_configure(&spectrum,&dsp_config);clear_analysis();if(cursor_hz>dsp_config.high_hz||cursor_hz<(dsp_config.log_frequency&&dsp_config.low_hz<10?10:dsp_config.low_hz))cursor_visible=false;
}
static void toggle(void){
 if(running){stop();frozen=false;return;}
 capture_requested=true;
 sample_rate=PREFERRED_RATE;temporal_start();spectrum_room_reset(&room_tracker);signature_audio.available=false;signature_audio.used=0;event_slot=-1;
 if(!acquire_microphone()){capture_requested=false;message="MIC UNAVAILABLE";capture_error=true;notify(message);return;}
 owned=true;
 if(!microphone->open(microphone->context,sample_rate)){stop();message="MIC START FAILED";capture_error=true;notify(message);return;}
 configure_dsp();(void)spectrum_dsp_init(&spectrum,&dsp_config);capture_error=input_waiting=input_gap=false;running=true;frozen=false;started=true;last_pcm_at=rendered=app->millis();message="MIC / 16 kHz";dirty=true;
}
static void freeze(void){stop();frozen=true;message="FROZEN / MIC OFF";dirty=true;}
static void detect_frequency_labels(void){
 for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++){bool before=label_active[i];if(!labels[i].present||labels[i].frequency_hz>8000){label_active[i]=false;}else{int16_t excess_db,snr;label_active[i]=spectrum_background_label(&ambient,labels[i].frequency_hz,prefs.threshold_db,before,&excess_db,&snr);label_db[i]=excess_db;label_snr[i]=snr;level_valid[i]=true;}if(before!=label_active[i]||(view==2&&label_active[i]))dirty=true;}
}
static void refresh_analysis(void){
 if(!spectrum.has_transform)return;
 uint32_t now=app->millis();
 int selected=signature_filter();const uint32_t*shared_background=cfa_background();const uint32_t *display_amplitude=spectrum.amplitude_q24;
 if(shared_background){signature_prepare_filter(selected);for(unsigned k=0;k<=spectrum.config.fft_size/2;k++)filtered_amplitude[k]=spectrum_signature_filtered_amplitude(spectrum.amplitude_q24[k],k,spectrum.config.fft_size,signature_display_gains);display_amplitude=filtered_amplitude;}
 spectrum_dsp_resample_values(&spectrum,display_amplitude,spec_levels,spec_db,SPECTRUM_HISTORY_FRAMES);spectrum_dsp_resample_values(&spectrum,display_amplitude,fall_levels,fall_db,SPECTRUM_HISTORY_BANDS);
 for(unsigned i=0;i<SPECTRUM_HISTORY_FRAMES;i++){unsigned decay=peak_levels[i]>196?peak_levels[i]-196:0;peak_levels[i]=spec_levels[i]>decay?spec_levels[i]:(uint16_t)decay;}
 for(unsigned y=0;y<SPECTRUM_HISTORY_BANDS;y++)history[history_next][y]=(uint8_t)((uint32_t)fall_levels[y]*255/32767);
 history_next=(history_next+1)%SPECTRUM_HISTORY_FRAMES;if(history_count<SPECTRUM_HISTORY_FRAMES)history_count++;

 signature_display_reset();recorded_transform=spectrum.transforms;rendered=now;dirty=true;
}
static void capture_discontinuity(bool lost_input){
 /* A late consumer read is not evidence that the room changed. Screen work
  * can outlast the bounded RX buffers: discard contiguous signal history, but
  * retain statistics of complete room frames. Empty input is a real loss of
  * scene evidence and must restart learning. Saved samples are never changed. */
 spectrum.used=0;signature_suspend();signature_display_reset();temporal_suspend();
 temporal_reset_context();
 if(lost_input){spectrum_background_reset(&ambient);spectrum_room_reset(&room_tracker);}
 else spectrum_background_interrupt(&ambient);
 event_slot=-1;event_match.complete=false;
 memset(label_active,0,sizeof(label_active));memset(level_valid,0,sizeof(level_valid));
 input_gap=true;dirty=true;
}
static void capture(void){
 int16_t pcm[256];size_t got=0;
 bool ok=microphone->read(microphone->context,pcm,256,&got);
 if(!ok||got>256){stop();message="MIC READ FAILED";capture_error=true;notify(message);return;}
 if(!input_gap&&(uint32_t)(app->millis()-last_pcm_at)>=128u)capture_discontinuity(!got);
 /* A successful short/empty read is a bounded RX wait, not end-of-stream. */
 if(!got){if(!input_waiting&&(uint32_t)(app->millis()-last_pcm_at)>=2000u){input_waiting=true;message="WAITING FOR AUDIO";dirty=true;}return;}
 if(!spectrum_dsp_feed(&spectrum,pcm,got)||!spectrum_signature_feed(&signature_audio,pcm,got)){stop();message="MIC READ FAILED";capture_error=true;notify(message);return;}
 cfa_pcm(pcm,got,sample_rate,!input_gap);
 if(speech.available&&!spectrum_speech_feed(&speech,pcm,got))dirty=true;
 input_gap=false;
 last_pcm_at=app->millis();if(input_waiting){input_waiting=false;message="MIC / 16 kHz";dirty=true;}
 signature_observe();cfa_apply();
 uint32_t now=app->millis();if(spectrum.transforms==recorded_transform||(uint32_t)(now-rendered)<FRAME_MS)return;
 refresh_analysis();
}
typedef struct {uint8_t at;uint32_t color;} color_stop;
static void build_palette(void){
 static const color_stop nova[]={{0,0},{56,0x051e38},{128,0x0e8ea3},{191,0x19e3ff},{230,0xffd24a},{255,0xffffff}};
 static const color_stop inferno[]={{0,0x000004},{36,0x1b0c42},{71,0x57106e},{107,0x8a226a},{143,0xbc3754},{179,0xe55c30},{214,0xfb9b06},{235,0xf6d746},{255,0xfcffa4}};
 static const color_stop viridis[]={{0,0x440154},{28,0x482878},{56,0x3e4a89},{84,0x31688e},{112,0x26828e},{140,0x1f9e89},{168,0x35b779},{196,0x6dcd59},{224,0xb4de2c},{255,0xfde725}};
 static const color_stop gray[]={{0,0},{255,0xffffff}};
 static const color_stop jet[]={{0,0x00007f},{32,0x0000ff},{96,0x00ffff},{159,0xffff00},{223,0xff0000},{255,0x7f0000}};
 const color_stop *sets[]={nova,inferno,viridis,gray,jet};const unsigned counts[]={6,9,10,2,6};const color_stop *s=sets[prefs.palette];unsigned count=counts[prefs.palette],part=1;
 for(unsigned i=0;i<256;i++){while(part+1<count&&i>s[part].at)part++;unsigned span=s[part].at-s[part-1].at,a=i-s[part-1].at;uint32_t c=0;for(unsigned shift=0;shift<=16;shift+=8){unsigned lo=(s[part-1].color>>shift)&255,hi=(s[part].color>>shift)&255;unsigned v=(lo*(span-a)+hi*a)/span;c|=v<<shift;}palette[i]=c;}
}
static unsigned saved_count(void){unsigned n=0;for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)n+=labels[i].present;return n;}
static unsigned ordered_labels(unsigned order[8]){
 unsigned n=0;for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(labels[i].present)order[n++]=i;
 for(unsigned i=1;i<n;i++){unsigned key=order[i],j=i;while(j&&labels[order[j-1]].frequency_hz>labels[key].frequency_hz){order[j]=order[j-1];j--;}order[j]=key;}return n;
}
typedef struct {const char *name;uint32_t color;unsigned confidence;int16_t amplitude_db;uint8_t kind;} monitor_item;
enum {MONITOR_EVENT=1,MONITOR_LABEL=2,MONITOR_VOICE=3,MONITOR_CANDIDATE=4};
static unsigned monitor_label_confidence(unsigned slot){
 int snr=label_snr[slot];if(snr<=0)return 0;unsigned confidence=(unsigned)snr/12u;return confidence>100u?100u:confidence;
}
static int16_t monitor_scene_db(void){return spectrum_background_db(spectrum_signature_total(ambient.raw));}
static unsigned monitor_room_count(void){unsigned n=0;for(unsigned i=0;i<CF_PROFILES;i++)n+=cfa_bank.profiles[i].kind==CF_ROOM;return n;}
static unsigned monitor_items(monitor_item items[17]){
 unsigned n=0,temporal=0;
 if(cfa_event.slot>=0){items[n++]=(monitor_item){cfa_bank.profiles[cfa_event.slot].name,0x3dff9au,(unsigned)(cfa_event.confidence*100.f),0,MONITOR_EVENT};temporal=1;}
 if(!temporal&&event_match.complete)for(unsigned i=0;i<ST_LABELS;i++)if(event_library.labels[i].present&&event_match.positive[i]>=ST_MATCH_MIN&&((event_match.reason==ST_RESULT_MATCH&&event_match.selected==(int)i)||event_match.negative[i]+60u<event_match.positive[i])&&(event_match.selected<0||event_match.selected==(int)i)){
  bool confirmed=event_match.reason==ST_RESULT_MATCH&&event_match.selected==(int)i;
  items[n++]=(monitor_item){event_library.labels[i].name,confirmed?0x3dff9au:0xffd24au,event_match.positive[i]/10u,event_match.query.peak_db,confirmed?MONITOR_EVENT:MONITOR_CANDIDATE};temporal++;
 }
 if(!temporal&&event_slot>=0&&event_slot<(int)SPECTRUM_SIGNATURE_SLOTS&&signatures[event_slot].kind==SPECTRUM_SIGNATURE_EVENT){
  items[n++]=(monitor_item){signatures[event_slot].name,0x3dff9au,event_confidence,spectrum_background_db(spectrum_signature_total(foreground_power)),MONITOR_EVENT};
 }
 for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(labels[i].present&&label_active[i]){
  items[n++]=(monitor_item){labels[i].name,label_colors[labels[i].color],monitor_label_confidence(i),label_db[i],MONITOR_LABEL};
 }
 if(running&&!input_waiting&&speech.active)items[n++]=(monitor_item){"MAYBE SPEECH",0xffd24au,speech.confidence,speech.amplitude_db,MONITOR_VOICE};
 for(unsigned i=1;i<n;i++){monitor_item key=items[i];unsigned j=i;while(j&&(items[j-1].confidence<key.confidence||(items[j-1].confidence==key.confidence&&items[j-1].amplitude_db<key.amplitude_db))){items[j]=items[j-1];j--;}items[j]=key;}
 return n;
}
/* Resolve the visible list before presentation or input. A denied frame must
 * not retain an obsolete origin after detections or saved labels disappear. */
static inline void normalize_monitor(void){
 if(page!=PAGE_MAIN||view!=2)return;
 unsigned n,rows=lab_edit?2u:3u;
 if(lab_edit)n=saved_count();else{monitor_item items[17];n=monitor_items(items);}
 if(list_scroll>=n){unsigned next=n>rows?n-rows:0;if(list_scroll!=next){list_scroll=next;dirty=true;}}
}
static void save_label(void){
 unsigned start=0,end=(unsigned)strlen(editing.name);while(start<end&&editing.name[start]==' ')start++;while(end>start&&editing.name[end-1]==' ')end--;if(start)for(unsigned i=0;i<end-start;i++)editing.name[i]=editing.name[start+i];editing.name[end-start]=0;
 if(!spectrum_label_valid(&editing)){notify("ENTER A LABEL NAME");return;}
 int slot=edit_slot;if(slot<0)for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(!labels[i].present && !(load_errors&(2u<<i))){slot=(int)i;break;}if(slot<0){notify(load_errors?"RETRY STORAGE FIRST":"8 LABEL LIMIT / DELETE ONE");return;}
 if(load_errors&(2u<<(unsigned)slot)){notify("RETRY STORAGE FIRST");return;}
 labels[slot]=editing;label_active[slot]=level_valid[slot]=false;pending_save|=(uint16_t)(2u<<slot);persist();set_page(PAGE_MAIN);
}
static void delete_label(int slot){
 if(slot<0||slot>=(int)SPECTRUM_LABEL_MAX||!labels[slot].present)return;
 if(load_errors&(2u<<(unsigned)slot)){notify("RETRY STORAGE FIRST");return;}
 deleted=labels[slot];undo_slot=slot;labels[slot]=(spectrum_label){0};label_active[slot]=level_valid[slot]=false;pending_save|=(uint16_t)(2u<<slot);persist();set_page(PAGE_MAIN);notify(pending_save?"DELETE NOT SAVED":"LABEL DELETED");
}
static void undo_delete(void){
 if(undo_slot<0)return;
 int slot=undo_slot;if(labels[slot].present){slot=-1;for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(!labels[i].present && !(load_errors&(2u<<i))){slot=(int)i;break;}}
 if(slot<0){notify("8 LABEL LIMIT / CANNOT UNDO");return;}labels[slot]=deleted;pending_save|=(uint16_t)(2u<<slot);undo_slot=-1;toast_message=NULL;persist();dirty=true;
}
static void open_label(int slot,uint16_t frequency){
 signature_keyboard=false;event_keyboard=false;
 if(slot<0){unsigned tolerance=2*sample_rate/prefs.fft_size;for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(labels[i].present){unsigned t=labels[i].frequency_hz*3/100;if(t<tolerance)t=tolerance;if(abs_int((int)labels[i].frequency_hz-frequency)<=(int)t){slot=(int)i;break;}}}
 edit_slot=slot;
 if(slot>=0)editing=labels[slot];else{if(saved_count()==SPECTRUM_LABEL_MAX){notify("8 LABEL LIMIT / DELETE ONE");return;}editing=(spectrum_label){.present=true,.frequency_hz=frequency};for(unsigned c=0;c<8;c++){bool used=false;for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++)if(labels[i].present&&labels[i].color==c)used=true;if(!used){editing.color=(uint8_t)c;break;}}}
 toast_message=NULL;set_page(PAGE_LABEL);
}
static void settings_change(unsigned setting,int direction,int segment){
 if(setting==13){shared_open(2);return;}
 if(setting==12){shared_open(1);return;}
 if(setting==0)return; /* Reserved legacy source setting is no longer exposed. */
 if(setting!=11 && (load_errors&1u)){notify("RETRY STORAGE FIRST");return;}
 static const uint16_t lows[]={0,20,50,100,200,500,1000},highs[]={1000,2000,5000,8000};
 switch(setting){
 case 1:case 2:{const uint16_t *values=setting==1?lows:highs;unsigned count=setting==1?7:4,current=setting==1?prefs.low_hz:prefs.high_hz,i=0;for(;i<count&&values[i]!=current;i++);int next=clamp_int((int)i+direction,0,(int)count-1);uint16_t value=values[next];if((setting==1&&value>=prefs.high_hz)||(setting==2&&value<=prefs.low_hz))return;if(setting==1)prefs.low_hz=value;else prefs.high_hz=value;break;}
 case 3:prefs.log_frequency=segment==0;break;case 4:prefs.log_amplitude=segment==0;break;case 5:prefs.show_labels=segment==0;break;case 6:prefs.palette=(uint8_t)clamp_int((int)prefs.palette+direction,0,4);build_palette();break;case 7:prefs.gain_db=(int8_t)clamp_int(prefs.gain_db+direction*3,-24,60);break;case 8:prefs.window=(uint8_t)clamp_int((int)prefs.window+direction,0,4);break;case 9:prefs.fft_size=(uint16_t)clamp_int(direction>0?prefs.fft_size*2:prefs.fft_size/2,256,8192);break;case 10:prefs.threshold_db=(int8_t)clamp_int(prefs.threshold_db+direction*5,-90,-20);break;case 11:{bool resume=capture_requested;pause_capture();capture_requested=false;if(event_files.pending>=0||event_files.ready!=3)event_retry();if(temporal_retained())return;signature_load();signature_save();if(load_errors){reload_storage();configure_dsp();build_palette();}persist();capture_requested=resume;portable_audio_capture_resume();notify(store_message?store_message:(event_files.pending>=0||event_files.ready!=3)?(event_message?event_message:"APP DATA UNAVAILABLE"):"SAVED DATA LOADED");dirty=true;return;}default:return;
 }
 configure_dsp();pending_save|=1;persist();dirty=true;
}

static bool spectrum_model_exit_ready(void){
 stop();if(!spectrum_domain_alive()||temporal_retained())return false;
 if(cfa_learning_kind){notify("SAVE OR CANCEL LEARNING");set_page(PAGE_SHARED_CAPTURE);return false;}
 return signature_exit_ready()&&temporal_exit_ready();
}
static void spectrum_model_init(void){
 microphone=NULL;storage=NULL;memset(&spectrum,0,sizeof(spectrum));
 acquired=owned=uncertain=running=frozen=store_acquired=capture_error=input_waiting=input_gap=capture_requested=false;
 lab_edit=cursor_visible=started=false;dirty=true;pending_save=load_errors=0;view=page=list_scroll=0;
 rendered=recorded_transform=last_pcm_at=toast_until=0;store_message=toast_message=NULL;
 event_keyboard=shared_keyboard=false;
 sample_rate=PREFERRED_RATE; edit_slot=undo_slot=-1;message="READY / MIC OFF";
 restore();if(!spectrum_domain_alive())return;signature_restore();if(!spectrum_domain_alive())return;
 event_restore();if(temporal_retained()||!spectrum_domain_alive())return;cfa_restore();
 if(temporal_retained()||!spectrum_domain_alive())return;
 configure_dsp();build_palette();set_page(PAGE_MAIN);
}
static void spectrum_model_step(void){
 if(!spectrum_domain_alive()||temporal_retained())return;
 if(running)capture();
 if(!spectrum_domain_alive()||temporal_retained())return;
 if(running){temporal_tick();cfa_apply();}
 if(!spectrum_domain_alive()||temporal_retained())return;
 event_neural_tick();
}
