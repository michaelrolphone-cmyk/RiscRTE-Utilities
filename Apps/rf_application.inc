#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "rf_dsp.h"
#include "rf_presentation.h"
#include "rf_store.h"
#include "rf_signature_store.h"
#include "rf_temporal_files.h"
#include "rf_neural_store.h"
#include "PortableWatchKeyboard.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_NOVA_UI
#include "PortableNovaUi.h"
#else
#include "daily_draw.h"
#define NOVA_CYAN 0x19e3ffu
#define NOVA_DIM 0x0e4f5cu
#define NOVA_LINE 0x12262bu
#define NOVA_CAP 0x6b8288u
#define NOVA_TEXT 0xcfe9eeu
#define NOVA_WHITE 0xffffffu
#endif
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif

#define PLOT_X 6
#define PLOT_Y 46
#define PLOT_W 228
#define PLOT_H (ui_height-74)
#define RF_PLOT_MAX_H 438
static int ui_height=240;
static const paper_presentation *paper;
static springboard_contact paper_contact;
#define FRAME_MS 100u
#define PREFERRED_RATE 80000000u
#ifndef RF_RETURN_APP
#ifdef PORTABLE_RETURN_APP
#define RF_RETURN_APP PORTABLE_RETURN_APP
#endif
#endif
#ifndef RF_STORAGE_INSTANCE
#define RF_STORAGE_INSTANCE 0u
#endif
#ifndef RF_APP_DATA_INSTANCE
#define RF_APP_DATA_INSTANCE 3u
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_key_value_v1 *storage;
static risc_runtime_capability_v1 store_grant;
static risc_radio_iq_capabilities_v1 receiver_caps;
static bool receiver_extended;
static unsigned receiver_scroll;
static uint32_t burst_at,last_burst_at;
static bool burst_gap;
typedef struct {
 rf_dsp_state spectrum;
 uint8_t history[PLOT_W][RF_PLOT_MAX_H];
 uint32_t pairs[RF_DSP_MAX_FFT],filtered_amplitude[RF_DSP_MAX_FFT];
 rt_library event_library;
 rt_files event_files;
 rt_segmenter event_segment,event_training;
 rt_matcher event_match;
 rn_trainer event_neural;
} rf_bulk_state;
static rf_bulk_state *rf_bulk;
/* The selected Runtime loads module BSS in PSRAM. Legacy adapters without
 * optional bulk allocation use this bounded resident block, never heap malloc. */
static rf_bulk_state rf_resident_bulk;
static bool rf_bulk_allocated;
#define spectrum (rf_bulk->spectrum)
#define history (rf_bulk->history)
#define burst_pairs (rf_bulk->pairs)
#define filtered_amplitude (rf_bulk->filtered_amplitude)
#define event_library (rf_bulk->event_library)
#define event_files (rf_bulk->event_files)
#define event_segment (rf_bulk->event_segment)
#define event_training (rf_bulk->event_training)
#define event_match (rf_bulk->event_match)
#define event_neural (rf_bulk->event_neural)

static rf_background ambient;
static uint32_t foreground_power[128];
static rf_preferences prefs;
static rf_label labels[RF_LABEL_MAX],editing,deleted;
static rf_dsp_config dsp_config;
static bool urgent_draw;
static bool owned,uncertain,running,frozen,dirty,store_acquired,capture_error,input_waiting,input_gap,capture_requested;
static uint32_t last_pcm_at;
static bool lab_edit,cursor_visible,contact_down,contact_plot,contact_drag,contact_moved,started;
static bool label_active[RF_LABEL_MAX],level_valid[RF_LABEL_MAX];
static int16_t label_db[RF_LABEL_MAX],label_snr[RF_LABEL_MAX];
static uint16_t spec_levels[PLOT_W],fall_levels[RF_PLOT_MAX_H],peak_levels[PLOT_W];
static int16_t spec_db[PLOT_W],fall_db[RF_PLOT_MAX_H];
static uint16_t history_next,history_count,pending_save,load_errors;
static uint32_t history_times[PLOT_W];
static uint32_t rendered,recorded_transform,sample_rate,painted;
static int controls_scroll,controls_touch_y,controls_touch_scroll;
static bool contact_controls;
static unsigned view,page,list_scroll,key_page,key_choice;
static int edit_slot,undo_slot,drag_origin,pill_x,pill_y,pill_w,pill_h;
static uint32_t cursor_hz;
static const char *message,*store_message,*toast_message;
static uint32_t toast_until;
static uint32_t palette[256];
static const uint32_t label_colors[8]={0xffd24au,0xff7a1au,0xff3d71u,0xb24dffu,0x6d7bffu,0x3d9bffu,0x3dff9au,0x9be15du};
static const char *palette_names[5]={"NOVA","INFERNO","VIRIDIS","GRAY","JET"};
static const char *window_names[5]={"RECT","HANN","HAMMING","BLACKMAN","FLAT TOP"};
enum {PAGE_MAIN,PAGE_CONTROLS,PAGE_LABEL,PAGE_KEYBOARD,PAGE_SIGNATURES,PAGE_SIGNATURE_EDIT,PAGE_EVENTS,PAGE_EVENT_LABEL,PAGE_EVENT_CAPTURE,PAGE_EVENT_EXAMPLES,PAGE_RECEIVER};
static int clamp_int(int n,int low,int high){return n<low?low:n>high?high:n;}
static int abs_int(int n){return n<0?-n:n;}
static bool hit(int x,int y,int l,int t,int w,int h){return x>=l && y>=t && x<l+w && y<t+h;}
static uint32_t fade(uint32_t c,unsigned a){return (((((c>>16)&255)*a/255)<<16)|((((c>>8)&255)*a/255)<<8)|((c&255)*a/255));}
static bool ui_stretched(void){return page==PAGE_MAIN?(view==2||!started):page!=PAGE_CONTROLS;}
static int ui_scale(void){return paper?app->screen_width()/240:1;}
static int ui_x(int x){return x*ui_scale()+(paper?0:(app->screen_width()-240)/2);}
static int ui_y(int y){return paper?(ui_stretched()?y*app->screen_height()/240:y*ui_scale()):y+(app->screen_height()-240)/2;}
static int ui_unx(int x){return (x-(paper?0:(app->screen_width()-240)/2))/ui_scale();}
static int ui_uny(int y){return paper?(ui_stretched()?y*240/app->screen_height():y/ui_scale()):y-(app->screen_height()-240)/2;}
static void fill(int x,int y,int w,int h,uint32_t c){
 if(w<=0||h<=0)return;
 if(paper){int left=ui_x(x),top=ui_y(y),right=ui_x(x+w),bottom=ui_y(y+h);if(left<0)left=0;if(top<0)top=0;if(right>app->screen_width())right=app->screen_width();if(bottom>app->screen_height())bottom=app->screen_height();if(right>left&&bottom>top){unsigned l=((c>>16&255u)*77u+(c>>8&255u)*150u+(c&255u)*29u)>>8;uint8_t tone=!c?T5_APP_TONE_WHITE:l>185?T5_APP_TONE_BLACK:l>85?T5_APP_TONE_DARK_GRAY:T5_APP_TONE_LIGHT_GRAY;if(app->struct_size>=offsetof(t5_app_api_v1,fill_rounded_rect_tone)+sizeof(app->fill_rounded_rect_tone)&&app->fill_rounded_rect_tone)app->fill_rounded_rect_tone(left,top,right-left,bottom-top,0,tone);else app->fill_rect(left,top,right-left,bottom-top,c!=0);}return;}
#ifdef PORTABLE_NOVA_UI
 portable_nova_fill(x,y,w,h,c);
#else
 x=ui_x(x);y=ui_y(y);if(x<0||y<0||x+w>app->screen_width()||y+h>app->screen_height())return;app->fill_rect(x,y,w,h,c!=0);
#endif
}
static void round_rect(int x,int y,int w,int h,int r,uint32_t c){
 if(paper){fill(x,y,w,h,c);return;}
#ifdef PORTABLE_NOVA_UI
 portable_nova_round(x,y,w,h,r,c);
#else
 (void)r;fill(x,y,w,h,c);
#endif
}
static int text_width(unsigned face,const char *s){
 if(paper&&paper->measure)return (paper->measure(s,face==0)+ui_scale()-1)/ui_scale();
#ifdef PORTABLE_NOVA_UI
 return portable_nova_measure(face,s);
#else
 (void)face;return daily_draw_width(s,1);
#endif
}
static void text(unsigned face,int x,int y,int w,const char *s,uint32_t c){
 if(paper&&paper->text){paper->text(ui_x(x),ui_y(y),w*ui_scale(),s,PAPER_TEXT_LITERAL|1u,face==0,c!=0);return;}
#ifdef PORTABLE_NOVA_UI
 portable_nova_text(face,x,y,w,s,c);
#else
 (void)face;(void)c;char clipped[64];unsigned n=(unsigned)(w/6);if(n>=sizeof(clipped))n=sizeof(clipped)-1;unsigned k=0;for(;k<n&&s[k];k++)clipped[k]=s[k];clipped[k]=0;daily_draw_text(app,ui_x(x),ui_y(y),clipped,1);
#endif
}
static void center(unsigned face,int x,int y,int w,const char *s,uint32_t c){int n=text_width(face,s);text(face,x+(n<w?(w-n)/2:0),y,w,s,c);}
/* These are the original baked 15px Rajdhani glyphs, never resized bitmaps. */
static void pill(int x,int y,int w,int h,const char *s,bool selected){if(paper){fill(x,y,w,h,NOVA_WHITE);if(!selected)fill(x+1,y+1,w-2,h-2,0);center(1,x+2,y+(h-15)/2-2,w-4,s,selected?0:NOVA_WHITE);return;}round_rect(x,y,w,h,h/2,selected?NOVA_CYAN:NOVA_DIM);if(!selected)round_rect(x+1,y+1,w-2,h-2,(h-2)/2,0);center(1,x+2,y+(h-15)/2-2,w-4,s,selected?0x001418u:NOVA_CYAN);}
static void line(int x0,int y0,int x1,int y1,uint32_t c){int dx=abs_int(x1-x0),sx=x0<x1?1:-1,dy=-abs_int(y1-y0),sy=y0<y1?1:-1,e=dx+dy;for(;;){fill(x0,y0,1,1,c);if(x0==x1&&y0==y1)break;int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}}
static void freq_text(unsigned hz,char out[20]){snprintf(out,20,"%u.%03u MHz",hz/1000000u,(hz%1000000u)/1000u);}

static void short_freq(unsigned hz,char out[20]){snprintf(out,20,"%u.%u",hz/1000000u,(hz%1000000u)/100000u);}

static bool optional_contact(void){return app->struct_size>=offsetof(t5_app_api_v1,touch_contact)+sizeof(app->touch_contact)&&app->touch_contact;}
static void notify(const char *s){toast_message=s;toast_until=app->millis()+4200u;dirty=urgent_draw=true;}

static bool acquire_storage(void);
static void set_page(unsigned next);
static void keyboard_begin(void);
static void clear_analysis(void);
static void refresh_analysis(void);
static void stop(void);
static void pause_capture(void);
static void toggle(void);
void portable_rf_capture_resume(void);
static bool event_keyboard;
static void temporal_observe(void);
static void detect_frequency_labels(void);
static void temporal_suspend(void);
static bool temporal_retained(void);
static bool temporal_exit_blocked(void);
static bool temporal_exit_ready(void);
static void persist(void);
static bool receiver_change(unsigned,int);
static void draw_receiver(void);
static void receiver_tap(int,int);
#include "rf_signature_app.inc"
#include "rf_temporal_app.inc"

static bool rf_services_safe(void){return !rf_bulk||(!uncertain&&!temporal_retained());}
static void rf_radio_suspended(void){if(!rf_bulk)return;running=owned=false;input_waiting=false;signature_suspend();temporal_suspend();dirty=true;}
bool portable_rf_services_safe(void){return rf_services_safe();}
bool portable_rf_capture_active(void){return capture_requested&&running&&owned&&!uncertain;}
bool portable_rf_suspend(void){
 bool was_running=running;running=false;input_waiting=false;signature_suspend();temporal_suspend();dirty=true;contact_down=contact_plot=false;
 if(!portable_radio_suspend()){uncertain=true;return false;}uncertain=owned=false;if(was_running)message="STOPPED / RF OFF";return true;
}

static void retain(void){waterfall_log("SDR cleanup-unconfirmed; invocation retained");for(;;)runtime->yield_ms(50);}

static void pause_capture(void){if(!portable_rf_suspend()){waterfall_drain();uncertain=false;owned=false;}}

static void stop(void){capture_requested=false;pause_capture();}
void portable_rf_capture_resume(void){if(capture_requested&&!running&&!uncertain&&!temporal_retained())toggle();}
static bool acquire_receiver(void){
 if(!waterfall_policy())return false;
 if(!waterfall_open_radio())return false;
 const risc_radio_iq_extended_api_v1 *api=(const void*)waterfall_radio;
 receiver_extended=waterfall_radio->struct_size>=sizeof(*api)&&api->capabilities&&api->capture_configured;
 receiver_caps=(risc_radio_iq_capabilities_v1){.struct_size=sizeof(receiver_caps)};
 if(receiver_extended&&!api->capabilities(waterfall_radio->context,&receiver_caps))receiver_extended=false;
 return true;
}

static bool acquire_storage(void){
 if(storage)return true;
 if(store_acquired)return false;
 store_grant=(risc_runtime_capability_v1){.struct_size=sizeof(store_grant)};
 if(!runtime->acquire("storage.key-value",2,RF_STORAGE_INSTANCE,&store_grant))return false;
 store_acquired=true;
 const risc_key_value_v1 *p=store_grant.api;if(!p||p->api_version!=2||p->struct_size<sizeof(*p)||!p->get||!p->put)return false;storage=p;return true;
}
static void label_key(unsigned slot,char out[16]){snprintf(out,16,"rf_l%u",slot);}
/* Unread records remain authoritative. A failed read is never permission to
 * replace a saved record with launch defaults. Retry only unresolved reads. */
static void reload_storage(void){
 if(!load_errors)return;
 if(!acquire_storage()){store_message="STORAGE UNAVAILABLE";return;}
 uint8_t bytes[RF_PREFERENCES_SIZE];uint32_t n=0;int32_t r;
 if(load_errors&1u){
  rf_preferences loaded=rf_preferences_default();
  r=storage->get(storage->context,"rf_cfg",bytes,sizeof(bytes),&n);
  if(r==RISC_KEY_VALUE_NOT_FOUND || (r==RISC_KEY_VALUE_OK && rf_preferences_decode(&loaded,bytes,n))){prefs=loaded;load_errors&=(uint16_t)~1u;}
 }
 for(unsigned i=0;i<RF_LABEL_MAX;i++)if(load_errors&(2u<<i)){
  char key[16];label_key(i,key);n=0;rf_label loaded=rf_label_default(i);
  r=storage->get(storage->context,key,bytes,sizeof(bytes),&n);
  if(r==RISC_KEY_VALUE_NOT_FOUND || (r==RISC_KEY_VALUE_OK && rf_label_decode(&loaded,bytes,n))){labels[i]=loaded;load_errors&=(uint16_t)~(2u<<i);}
 }
 store_message=load_errors?"SAVED DATA UNREAD / RETRY":pending_save?"UNSAVED CHANGES":NULL;
}
static void restore(void){
 prefs=rf_preferences_default();memset(labels,0,sizeof(labels));
 load_errors=(uint16_t)((1u<<(RF_LABEL_MAX+1u))-1u);reload_storage();
}
static void persist(void){
 if(!pending_save)return;
 if(pending_save&load_errors){store_message="RETRY STORAGE FIRST";notify(store_message);return;}
 if(!acquire_storage()){store_message="UNSAVED / NO STORAGE";notify("UNSAVED / NO STORAGE");return;}
 uint8_t bytes[RF_PREFERENCES_SIZE];bool failed=false;
 if(pending_save&1){if(rf_preferences_encode(&prefs,bytes)&&storage->put(storage->context,"rf_cfg",bytes,RF_PREFERENCES_SIZE)==RISC_KEY_VALUE_OK)pending_save&=(uint16_t)~1u;else failed=true;}
 for(unsigned i=0;i<RF_LABEL_MAX;i++)if(pending_save&(2u<<i)){char key[16];label_key(i,key);if(rf_label_encode(&labels[i],bytes)&&storage->put(storage->context,key,bytes,RF_LABEL_RECORD_SIZE)==RISC_KEY_VALUE_OK)pending_save&=(uint16_t)~(2u<<i);else failed=true;}
 if(failed){store_message="SAVE UNCONFIRMED";notify("SAVE UNCONFIRMED");}else store_message=load_errors?"SAVED DATA UNREAD / RETRY":NULL;
}
static void clear_analysis(void){signature_display_reset();memset(history,0,sizeof(history));memset(spec_levels,0,sizeof(spec_levels));memset(fall_levels,0,sizeof(fall_levels));memset(peak_levels,0,sizeof(peak_levels));memset(label_active,0,sizeof(label_active));memset(level_valid,0,sizeof(level_valid));for(unsigned i=0;i<PLOT_W;i++)spec_db[i]=-12000;for(unsigned i=0;i<(unsigned)PLOT_H;i++)fall_db[i]=-12000;history_next=history_count=0;memset(history_times,0,sizeof(history_times));recorded_transform=0;}
static void configure_dsp(void){
 dsp_config=(rf_dsp_config){prefs.identity,prefs.low_hz,prefs.high_hz,prefs.fft_size,(rf_dsp_window)prefs.window,prefs.log_frequency,prefs.log_amplitude,prefs.gain_db,prefs.threshold_db};sample_rate=prefs.identity.sample_rate_hz;
 (void)rf_dsp_configure(&spectrum,&dsp_config);clear_analysis();if(cursor_hz<dsp_config.low_hz||cursor_hz>dsp_config.high_hz)cursor_visible=false;
}

static void toggle(void){
 if(running){stop();frozen=false;return;}
 capture_requested=true;
 if(!acquire_receiver()){capture_requested=false;message=waterfall_status==WATERFALL_AIRPLANE?"AIRPLANE MODE":waterfall_status==WATERFALL_NO_POLICY?"RADIO POLICY UNAVAILABLE":"RF UNAVAILABLE / START RETRY";capture_error=true;notify(message);return;}
 if(!receiver_extended){rf_capture_identity defaults=rf_identity_default();if(prefs.fft_size!=256||!rf_identity_equal(&prefs.identity,&defaults)){capture_requested=false;message="LEGACY: 256 / DEFAULT RF ONLY";capture_error=true;notify(message);return;}}
 configure_dsp();(void)rf_dsp_init(&spectrum,&dsp_config);temporal_start();rf_room_reset(&room_tracker);signature_audio.available=false;event_slot=-1;
 owned=waterfall_enabled=running=started=true;capture_error=input_waiting=input_gap=frozen=false;last_pcm_at=rendered=app->millis();last_burst_at=0;message=receiver_extended?"NOMINAL RF / UNCALIBRATED":"LEGACY @1 / DEFAULT RF / 256";dirty=true;
}

static void freeze(void){stop();frozen=true;message="FROZEN / RF OFF";dirty=true;}
/* Each real coherent burst independently feeds the fixed canonical detector.
 * Nothing describes unobserved time between scheduled receiver captures. */
static void detect_frequency_labels(void){
 for(unsigned i=0;i<RF_LABEL_MAX;i++){bool before=label_active[i];bool valid=signature_audio.available&&labels[i].present&&rf_label_valid(&labels[i])&&labels[i].frequency_hz>=rf_identity_low_hz(&prefs.identity)&&labels[i].frequency_hz<=rf_identity_high_hz(&prefs.identity);int16_t excess_db=RF_DSP_FLOOR_DB,snr=0;label_active[i]=valid&&rf_background_label(&ambient,&prefs.identity,labels[i].frequency_hz,labels[i].tolerance_hz,labels[i].tolerance_bins,prefs.fft_size,prefs.threshold_db,before,&excess_db,&snr);label_db[i]=excess_db;label_snr[i]=snr;level_valid[i]=valid;if(before!=label_active[i]||(view==2&&label_active[i]))dirty=true;}
}

static void refresh_analysis(void){
 if(!spectrum.has_transform)return;
 uint32_t now=app->millis();
 int selected=signature_filter();const uint32_t *display_amplitude=spectrum.amplitude_q24;
 if(selected>=0){signature_prepare_filter(selected);for(unsigned k=0;k<spectrum.config.fft_size;k++)filtered_amplitude[k]=rf_signature_filtered_amplitude(spectrum.amplitude_q24[k],k,spectrum.config.fft_size,signature_display_gains);display_amplitude=filtered_amplitude;}
 rf_dsp_resample_values(&spectrum,display_amplitude,spec_levels,spec_db,PLOT_W);rf_dsp_resample_values(&spectrum,display_amplitude,fall_levels,fall_db,PLOT_H);
 for(unsigned i=0;i<PLOT_W;i++){unsigned decay=peak_levels[i]>196?peak_levels[i]-196:0;peak_levels[i]=spec_levels[i]>decay?spec_levels[i]:(uint16_t)decay;}
 for(unsigned y=0;y<(unsigned)PLOT_H;y++)history[history_next][y]=(uint8_t)((uint32_t)fall_levels[y]*255/32767);
 history_times[history_next]=burst_at;history_next=(history_next+1)%PLOT_W;if(history_count<PLOT_W)history_count++;

 signature_display_reset();recorded_transform=spectrum.transforms;rendered=now;dirty=true;
}
static void capture_discontinuity(bool lost_input){
 signature_suspend();signature_display_reset();temporal_suspend();temporal_reset_context();if(lost_input){rf_background_reset(&ambient);rf_room_reset(&room_tracker);}else rf_background_interrupt(&ambient);event_slot=-1;event_match.complete=false;memset(label_active,0,sizeof(label_active));memset(level_valid,0,sizeof(level_valid));input_gap=true;dirty=true;
}

static void capture(void){
 uint32_t now=app->millis();if(last_burst_at&&(uint32_t)(now-last_burst_at)<FRAME_MS)return;
 if(!waterfall_policy()){unsigned reason=waterfall_status;stop();message=reason==WATERFALL_AIRPLANE?"AIRPLANE MODE":"RADIO POLICY UNAVAILABLE";capture_error=true;return;}
 unsigned count=prefs.fft_size;waterfall_capture_pairs=count;int result;const risc_radio_iq_extended_api_v1 *extended=(const void*)waterfall_radio;
 risc_radio_iq_settings_v1 settings={sizeof(settings),prefs.identity.lo_hz,prefs.identity.sample_rate_hz,prefs.identity.width_hz,prefs.identity.raw_gain,prefs.identity.rf_gain,prefs.identity.bb_gain,prefs.identity.filter,{prefs.identity.dc[0],prefs.identity.dc[1],prefs.identity.dc[2],prefs.identity.dc[3]},prefs.identity.iq_correction};
 risc_radio_iq_format_v1 format={.struct_size=sizeof(format)};
 if(!trace_capture_seen)waterfall_log("SDR capture begin");
 burst_at=app->millis();burst_gap=true; /* Every independent burst leaves unobserved RF time. */
 if(receiver_extended){if(!trace_capture_seen&&extended->capture_configured_traced)result=extended->capture_configured_traced(waterfall_radio->context,burst_pairs,count,&settings,&format,waterfall_trace_stage,NULL);else result=extended->capture_configured(waterfall_radio->context,burst_pairs,count,&settings,&format);}
 else{const risc_radio_iq_diagnostics_api_v1 *legacy=(const void*)waterfall_radio;if(!trace_capture_seen&&waterfall_radio->struct_size>=sizeof(*legacy)&&legacy->capture_burst_traced)result=legacy->capture_burst_traced(waterfall_radio->context,burst_pairs,count,waterfall_trace_stage,NULL);else result=waterfall_radio->capture_burst(waterfall_radio->context,burst_pairs,count);}
 last_burst_at=burst_at;
 if(result==RISC_RADIO_IQ_CLEANUP_RETAINED){waterfall_uncertain=uncertain=true;running=waterfall_enabled=false;waterfall_log("SDR cleanup ok=0 retained=1");waterfall_drain();uncertain=owned=false;capture_error=true;message="CLEANUP RECOVERED / START";dirty=true;return;}
 waterfall_trace_capture(result);waterfall_status=(unsigned)result;
 if(result!=RISC_RADIO_IQ_OK){running=waterfall_enabled=capture_requested=false;capture_error=true;capture_discontinuity(true);message=result==RISC_RADIO_IQ_BUSY?"RADIO BUSY / START RETRY":"CAPTURE FAILED / START RETRY";dirty=true;return;}
 if(receiver_extended&&(format.struct_size<sizeof(format)||format.component_bits!=10u||format.component_full_scale!=512u||format.pair_count!=count||format.sample_rate_hz!=prefs.identity.sample_rate_hz||format.bandwidth_hz!=prefs.identity.width_hz||format.sample_format!=RISC_RADIO_IQ_FORMAT_S10_I0_Q10||!(format.flags&RISC_RADIO_IQ_FLAG_COHERENT_BURST))){stop();capture_error=true;message="INCOMPATIBLE CAPTURE FORMAT";dirty=true;return;}
 rf_capture_identity identity=prefs.identity;if(receiver_extended)identity.lo_hz=format.center_hz;
 if(!rf_identity_equal(&identity,&prefs.identity)){/* Quantized nominal LO defines compatibility; live calibration values do not. */prefs.identity=identity;prefs.low_hz=rf_identity_low_hz(&identity);prefs.high_hz=rf_identity_high_hz(&identity);bool empty=true;for(unsigned i=0;i<8;i++)if(event_library.labels[i].present)empty=false;if(empty&&!event_files.exists)event_library.identity=identity;configure_dsp();temporal_start();rf_room_reset(&room_tracker);pending_save|=1;}
 if(!rf_signature_burst(&signature_audio,&identity,burst_pairs,count)||!rf_dsp_burst(&spectrum,burst_pairs,count)){stop();capture_error=true;message="INVALID COHERENT BURST";dirty=true;return;}
 input_waiting=input_gap=capture_error=false;signature_observe();refresh_analysis();
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
static void draw_gear(void){
 round_rect(213,14,24,24,12,NOVA_DIM);round_rect(214,15,22,22,11,0);round_rect(220,21,10,10,5,NOVA_CYAN);round_rect(222,23,6,6,3,0);
 for(unsigned i=0;i<4;i++){int x=i==0?224:i==1?233:i==2?224:215,y=i==0?17:i==1?25:i==2?33:25;fill(x,y,3,3,NOVA_CYAN);}
}
static void tab(int x,int width,const char *name,bool selected){
 if(paper){pill(x,14,width,24,name,selected);return;}
 round_rect(x,14,width,24,12,selected?NOVA_CYAN:NOVA_DIM);if(!selected)round_rect(x+1,15,width-2,22,11,0);unsigned face=0;while(face<2&&text_width(face,name)>width-6)++face;center(face,x+3,17,width-6,name,selected?0x001418u:NOVA_CYAN);
}
static void draw_tabs(void){
 tab(4,51,"SPEC",view==0);tab(58,50,"FALL",view==1);tab(111,74,"MONITOR",view==2);
 if(view!=2){round_rect(188,14,22,24,11,prefs.show_labels?NOVA_CYAN:NOVA_DIM);if(!prefs.show_labels)round_rect(189,15,20,22,10,0);uint32_t c=prefs.show_labels?0x001418u:NOVA_CYAN;line(193,20,201,20,c);line(201,20,206,26,c);line(206,26,200,32,c);line(200,32,193,25,c);line(193,25,193,20,c);fill(195,22,2,2,c);}
 draw_gear();
}
static void draw_axes(bool waterfall,bool foreground){
 unsigned last=0;bool have=false;int last_right=-10;unsigned dimension=waterfall?PLOT_H:PLOT_W;
 for(unsigned i=0;i<6u;i++){
  unsigned f=rf_dsp_frequency_at(&dsp_config,i,6);
  unsigned p=rf_dsp_column_at(&dsp_config,f,dimension);if(waterfall&&have&&p-last<19u)continue;last=p;have=true;
  char name[20];short_freq(f,name);
  if(waterfall){for(int x=0;x<PLOT_W;x+=4)fill(PLOT_X+x,PLOT_Y+(int)p,2,1,NOVA_DIM);text(1,PLOT_X+2,PLOT_Y+clamp_int((int)p-6,0,PLOT_H-16),42,name,0x9fb4bb);}
  else{if(!foreground)fill(PLOT_X+(int)p,PLOT_Y,1,PLOT_H,NOVA_LINE);else{int tw=text_width(1,name),tx=clamp_int((int)p-tw/2,0,PLOT_W-tw);char end_name[20];short_freq(dsp_config.high_hz,end_name);int end_left=PLOT_W-text_width(1,end_name);if(tx>=last_right+5&&(f==dsp_config.high_hz||tx+tw+5<=end_left)){text(1,PLOT_X+tx,PLOT_Y+PLOT_H-16,tw,name,NOVA_CAP);last_right=tx+tw;}}}
 }
 if(!waterfall){for(unsigned i=0;i<5;i++){int y;char name[12];if(prefs.log_amplitude){int db=-(int)i*20;y=PLOT_Y+PLOT_H-(db+90)*PLOT_H/90;snprintf(name,sizeof(name),"%d",db);}else{if(i==4)continue;unsigned pct=100-i*25;y=PLOT_Y+PLOT_H-(int)pct*PLOT_H/100;snprintf(name,sizeof(name),"%u%%",pct);}y=clamp_int(y,PLOT_Y,PLOT_Y+PLOT_H-1);if(!foreground)fill(PLOT_X,y,PLOT_W,1,NOVA_LINE);else text(1,PLOT_X+2,clamp_int(y-16,PLOT_Y,PLOT_Y+PLOT_H-33),39,name,0x73949b);}}
}
static void draw_plot(void){
 if(view==1){for(unsigned x=0;x<history_count;x++){unsigned source=(history_next+PLOT_W-history_count+x)%PLOT_W;unsigned at=PLOT_W-history_count+x;unsigned alpha=at<12?at*255/12:at>PLOT_W-13?(PLOT_W-1-at)*255/12:255;for(unsigned y=0;y<(unsigned)PLOT_H;y++)fill(PLOT_X+(int)at,PLOT_Y+(int)y,1,1,fade(palette[history[source][y]],alpha));}draw_axes(true,true);char span[48];unsigned first=(history_next+PLOT_W-history_count)%PLOT_W,last=(history_next+PLOT_W-1u)%PLOT_W;uint32_t elapsed=history_count>1?history_times[last]-history_times[first]:0;snprintf(span,sizeof(span),"%u BURSTS / %u.%u s",history_count,elapsed/1000u,elapsed%1000u/100u);char high_name[20];short_freq(dsp_config.high_hz,high_name);int span_left=text_width(1,high_name)+10;fill(PLOT_X+span_left,PLOT_Y+PLOT_H-18,PLOT_W-span_left,18,0);text(1,PLOT_X+span_left+2,PLOT_Y+PLOT_H-18,PLOT_W-span_left-4,span,NOVA_CAP);}
 else{
  draw_axes(false,false);text(1,PLOT_X+48,PLOT_Y+1,48,"dBFS",NOVA_CAP);int prev_y=PLOT_Y+PLOT_H-1,prev_peak=prev_y;
  for(int x=0;x<PLOT_W;x++){int y=PLOT_Y+PLOT_H-1-(int)((uint32_t)spec_levels[x]*(PLOT_H-1)/32767),peak_y=PLOT_Y+PLOT_H-1-(int)((uint32_t)peak_levels[x]*(PLOT_H-1)/32767);
   for(int fy=y+1;fy<PLOT_Y+PLOT_H-17;fy++)fill(PLOT_X+x,fy,1,1,fade(NOVA_CYAN,(unsigned)(PLOT_Y+PLOT_H-fy)*140/PLOT_H));
   if(x){line(PLOT_X+x-1,prev_peak,PLOT_X+x,peak_y,paper?NOVA_DIM:0x8c7429);line(PLOT_X+x-1,prev_y,PLOT_X+x,y,paper?NOVA_WHITE:NOVA_CYAN);}prev_y=y;prev_peak=peak_y;
  }
  fill(PLOT_X,PLOT_Y+PLOT_H-17,PLOT_W,17,0);draw_axes(false,true);
 }
 if(prefs.show_labels)for(unsigned i=0;i<RF_LABEL_MAX;i++){
  rf_label *l=&labels[i];if(!l->present||l->frequency_hz<(dsp_config.log_frequency&&dsp_config.low_hz<10?10:dsp_config.low_hz)||l->frequency_hz>dsp_config.high_hz)continue;
  int p=(int)rf_dsp_column_at(&dsp_config,l->frequency_hz,view==1?PLOT_H:PLOT_W);uint32_t color=label_active[i]?label_colors[l->color]:fade(label_colors[l->color],102);
  int limit=view==1?PLOT_W:PLOT_H-18;for(int k=0;k<limit;k+=6){if(view==1)fill(PLOT_X+k,PLOT_Y+p,3,1,color);else fill(PLOT_X+p,PLOT_Y+k,1,3,color);}
  for(int k=0;k<5;k++)if(view==1)fill(PLOT_X+k,PLOT_Y+clamp_int(p-4+k,0,PLOT_H-1),1,clamp_int(9-2*k,1,PLOT_H-clamp_int(p-4+k,0,PLOT_H-1)),color);else fill(PLOT_X+clamp_int(p-4+k,0,PLOT_W-1),PLOT_Y+k,clamp_int(9-2*k,1,PLOT_W-clamp_int(p-4+k,0,PLOT_W-1)),1,color);
  if(label_active[i]){int y=view==1?PLOT_Y+clamp_int(p-18,1,PLOT_H-17):PLOT_Y+20+(int)(i%3)*17;int tw=text_width(1,l->name);if(tw>132)tw=132;int x=view==1?PLOT_X+PLOT_W-8-tw:PLOT_X+clamp_int(p>PLOT_W-90?p-4-tw:p+4,0,PLOT_W-tw);fill(x,y,tw,17,0);text(1,x,y,tw,l->name,color);}
 }
 pill_w=pill_h=0;
 if(event_slot>=0&&!event_match.complete){char event_name[32];snprintf(event_name,sizeof(event_name),"%.16s %u%%",signatures[event_slot].name,event_confidence);fill(PLOT_X+35,PLOT_Y+PLOT_H-37,190,18,0);text(1,PLOT_X+37,PLOT_Y+PLOT_H-37,185,event_name,0x3dff9au);}
 if(event_match.complete&&event_match.selected>=0){char event_name[48];int shift=event_match.shift[event_match.selected];snprintf(event_name,sizeof(event_name),"%.16s %u%% %+d bands",event_library.labels[event_match.selected].name,event_match.score/10,shift);fill(PLOT_X+5,PLOT_Y+PLOT_H-37,218,18,0);text(1,PLOT_X+7,PLOT_Y+PLOT_H-37,214,event_name,0x3dff9au);}
 if(signature_filter()>=0)text(1,PLOT_X+112,PLOT_Y+1,113,"BG FILTER",0xffd24au);
 if(cursor_visible){int p=(int)rf_dsp_column_at(&dsp_config,cursor_hz,view==1?PLOT_H:PLOT_W);int db=view==1?fall_db[p]:spec_db[p];char f[20],caption[40];freq_text(cursor_hz,f);snprintf(caption,sizeof(caption),"%s %d dBFS",f,db/100);
  if(view==1)fill(PLOT_X,PLOT_Y+p,PLOT_W,1,NOVA_WHITE);else fill(PLOT_X+p,PLOT_Y,1,PLOT_H-18,NOVA_WHITE);
  int px=view==1?PLOT_X+PLOT_W-9:PLOT_X+p,py=view==1?PLOT_Y+p:PLOT_Y+PLOT_H-1-(int)((uint32_t)spec_levels[p]*(PLOT_H-1)/32767);round_rect(px-4,py-4,9,9,4,NOVA_CYAN);round_rect(px-2,py-2,5,5,2,NOVA_WHITE);
  pill_w=clamp_int(text_width(1,caption)+16,112,PLOT_W-4);pill_h=28;pill_x=view==1?PLOT_X+PLOT_W-pill_w-6:PLOT_X+clamp_int(p-pill_w/2,2,PLOT_W-pill_w-2);pill_y=view==1?PLOT_Y+clamp_int(p<38?p+8:p-36,2,PLOT_H-30):PLOT_Y+4;
  round_rect(pill_x,pill_y,pill_w,pill_h,13,NOVA_CYAN);round_rect(pill_x+1,pill_y+1,pill_w-2,pill_h-2,12,0);center(1,pill_x+5,pill_y+5,pill_w-10,caption,NOVA_CYAN);
 }
}
static unsigned saved_count(void){unsigned n=0;for(unsigned i=0;i<RF_LABEL_MAX;i++)n+=labels[i].present;return n;}
static unsigned ordered_labels(unsigned order[8]){
 unsigned n=0;for(unsigned i=0;i<RF_LABEL_MAX;i++)if(labels[i].present)order[n++]=i;
 for(unsigned i=1;i<n;i++){unsigned key=order[i],j=i;while(j&&labels[order[j-1]].frequency_hz>labels[key].frequency_hz){order[j]=order[j-1];j--;}order[j]=key;}return n;
}
typedef struct {const char *name;uint32_t color;unsigned confidence;int16_t amplitude_db;uint8_t kind;} monitor_item;
enum {MONITOR_EVENT=1,MONITOR_LABEL=2,MONITOR_UNCLASSIFIED=3,MONITOR_CANDIDATE=4};
static unsigned monitor_label_confidence(unsigned slot){
 int snr=label_snr[slot];if(snr<=0)return 0;unsigned confidence=(unsigned)snr/12u;return confidence>100u?100u:confidence;
}
static int16_t monitor_scene_db(void){return rf_background_db(rf_signature_total(ambient.raw));}
static unsigned monitor_room_count(void){unsigned n=0;for(unsigned i=0;i<RF_SIGNATURE_SLOTS;i++)if(signatures[i].kind==RF_SIGNATURE_ROOM)n++;return n;}
static unsigned monitor_items(monitor_item items[17]){
 unsigned n=0,temporal=0;
 if(event_match.complete)for(unsigned i=0;i<RT_LABELS;i++)if(event_library.labels[i].present&&event_match.positive[i]>=RT_MATCH_MIN&&((event_match.reason==RT_RESULT_MATCH&&event_match.selected==(int)i)||event_match.negative[i]+60u<event_match.positive[i])&&(event_match.selected<0||event_match.selected==(int)i)){
  bool confirmed=event_match.reason==RT_RESULT_MATCH&&event_match.selected==(int)i;
  items[n++]=(monitor_item){event_library.labels[i].name,confirmed?0x3dff9au:0xffd24au,event_match.positive[i]/10u,event_match.query.peak_db,confirmed?MONITOR_EVENT:MONITOR_CANDIDATE};temporal++;
 }
 if(!temporal&&event_slot>=0&&event_slot<(int)RF_SIGNATURE_SLOTS&&signatures[event_slot].kind==RF_SIGNATURE_EVENT){
  items[n++]=(monitor_item){signatures[event_slot].name,0x3dff9au,event_confidence,rf_background_db(rf_signature_total(foreground_power)),MONITOR_EVENT};
 }
 for(unsigned i=0;i<RF_LABEL_MAX;i++)if(labels[i].present&&label_active[i]){
  items[n++]=(monitor_item){labels[i].name,label_colors[labels[i].color],monitor_label_confidence(i),label_db[i],MONITOR_LABEL};
 }
 if(!n&&running&&!input_waiting&&ambient.ready&&ambient.foreground&&rf_background_db(rf_signature_total(foreground_power))>=prefs.threshold_db*100)items[n++]=(monitor_item){"UNCLASSIFIED RF",NOVA_CAP,0,rf_background_db(rf_signature_total(foreground_power)),MONITOR_UNCLASSIFIED};
 for(unsigned i=1;i<n;i++){monitor_item key=items[i];unsigned j=i;while(j&&(items[j-1].confidence<key.confidence||(items[j-1].confidence==key.confidence&&items[j-1].amplitude_db<key.amplitude_db))){items[j]=items[j-1];j--;}items[j]=key;}
 return n;
}
static void draw_monitor(void){
 if(lab_edit){
  unsigned order[8],n=ordered_labels(order);char header[40];text(0,20,46,143,"EDIT LABELS",NOVA_CYAN);if(pending_save)snprintf(header,sizeof(header),"UNSAVED CHANGES");else snprintf(header,sizeof(header),"%u SAVED",saved_count());text(1,20,64,145,header,NOVA_CAP);pill(166,48,54,28,"DONE",false);fill(20,84,200,1,NOVA_DIM);
  if(!n){center(1,20,112,200,"NO LABELS SAVED",NOVA_CAP);center(1,20,133,200,"DRAG A FREQUENCY LINE",NOVA_CAP);center(1,20,154,200,"THEN TAP ITS PILL TO NAME",NOVA_CAP);}
  if(list_scroll>=n)list_scroll=n>2?n-2:0;
  for(unsigned row=0;row<2&&row+list_scroll<n;row++){unsigned slot=order[row+list_scroll];rf_label *l=&labels[slot];int y=88+(int)row*55;uint32_t color=label_colors[l->color];fill(20,y,2,50,color);text(1,30,y,152,l->name,NOVA_WHITE);char frequency[20];freq_text(l->frequency_hz,frequency);text(1,30,y+20,117,frequency,NOVA_CAP);pill(184,y+4,32,32,"X",false);fill(30,y+51,190,1,NOVA_LINE);}
  if(n>2){pill(21,204,44,24,"<",false);char count[24];snprintf(count,sizeof(count),"%u-%u / %u",list_scroll+1,list_scroll+2<n?list_scroll+2:n,n);center(1,66,207,108,count,NOVA_CAP);pill(176,204,44,24,">",false);}return;
 }
 monitor_item items[17];unsigned n=monitor_items(items),rooms=monitor_room_count();char header[48],room_detail[48];text(0,20,46,143,"MONITOR",NOVA_CYAN);
 if(!running)snprintf(header,sizeof(header),"%s / LAST CONTEXT",frozen?"FROZEN":"STOPPED");else if(!ambient.ready)snprintf(header,sizeof(header),"LEARNING ROOM / %u%%",ambient.frames*100u/RF_BG_WARMUP);else if(event_match.complete&&event_match.reason==RT_RESULT_AMBIGUOUS)snprintf(header,sizeof(header),"EVENT / AMBIGUOUS");else if(event_match.complete&&event_match.reason==RT_RESULT_NEGATIVE)snprintf(header,sizeof(header),"EVENT / NONMATCH");else snprintf(header,sizeof(header),"%u ACTIVE ITEM%s%s",n,n==1?"":"S",event_neural_used&&event_match.complete?" / NEURAL":"");
 text(1,20,64,145,header,NOVA_CAP);pill(166,48,54,28,"EDIT",false);fill(20,84,200,1,NOVA_DIM);
 text(1,20,89,38,"ROOM",NOVA_CAP);bool verifying=room_tracker.candidate>=0&&room_tracker.candidate!=room_tracker.selected;bool holding=room_tracker.selected>=0&&(room_tracker.misses||room_tracker.ambiguous);int room=verifying?room_tracker.candidate:room_tracker.selected;
 const char *room_name=!rooms?"NO ROOM SAMPLES":!ambient.ready?"LEARNING...":room>=0&&room<(int)RF_SIGNATURE_SLOTS&&signatures[room].kind==RF_SIGNATURE_ROOM?signatures[room].name:room_tracker.ambiguous?"AMBIGUOUS":"UNKNOWN";
 text(0,62,88,158,room_name,room>=0?NOVA_WHITE:NOVA_TEXT);
 if(!rooms)snprintf(room_detail,sizeof(room_detail),"CAPTURE ROOM IN CONTROLS > SAMPLES");
 else if(!ambient.ready)snprintf(room_detail,sizeof(room_detail),"RAW ROOM PROFILE / WAIT FOR BASELINE");
 else if(room>=0&&holding)snprintf(room_detail,sizeof(room_detail),"HOLD / %s",room_tracker.ambiguous?"SIMILAR ROOM ACTIVITYS":"CHECKING ROOM");
 else if(room>=0)snprintf(room_detail,sizeof(room_detail),"%s %u%%  AMP %d dB",verifying?"VERIFY":"MATCH",room_tracker.confidence,monitor_scene_db()/100);
 else snprintf(room_detail,sizeof(room_detail),"%s %u%%  AMP %d dB",room_tracker.ambiguous?"AMBIGUOUS":"NO MATCH",room_tracker.confidence,monitor_scene_db()/100);
 text(1,20,106,200,room_detail,NOVA_CAP);fill(20,126,200,1,NOVA_DIM);
 pill(10,211,28,27,"<",false);pill(45,211,65,27,running?"STOP":"START",running);pill(117,211,78,27,"LEARN",false);pill(202,211,28,27,">",false);
 if(!n){center(1,20,145,200,input_waiting?"WAITING FOR RF INPUT":running?"NO ACTIVITY IDENTIFIED":"START RF TO MONITOR",NOVA_CAP);center(1,20,166,200,running?"LEARN A ACTIVITY BELOW":"UNCLASSIFIED RF ACTIVITY",NOVA_CAP);list_scroll=0;return;}
 if(list_scroll>=n)list_scroll=n>3?n-3:0;
 for(unsigned row=0;row<3&&row+list_scroll<n;row++){monitor_item *item=&items[row+list_scroll];int y=132+(int)row*26;fill(20,y+1,2,22,item->color);text(1,28,y,40,item->kind==MONITOR_EVENT?"EVENT":item->kind==MONITOR_CANDIDATE?"MAYBE":item->kind==MONITOR_UNCLASSIFIED?"RF":"LABEL",NOVA_CAP);text(1,70,y,99,item->name,NOVA_WHITE);char confidence[12],amplitude[20];if(item->kind==MONITOR_UNCLASSIFIED)snprintf(confidence,sizeof(confidence),"RAW");else snprintf(confidence,sizeof(confidence),"%u%%",item->confidence);snprintf(amplitude,sizeof(amplitude),"AMP %d dB",item->amplitude_db/100);text(1,176,y,44,confidence,item->color);text(1,70,y+13,100,amplitude,NOVA_CAP);}
}
static void draw_footer(void){
 if(view==2)return;
 const char *label=running?"BURSTS / STOP":frozen?"FROZEN / START":"START";
 text(1,8,ui_height-22,140,label,NOVA_CYAN);text(1,183,ui_height-22,51,running?"FREEZE":"EXIT",NOVA_CAP);
 if(pending_save||signature_pending||event_files.pending>=0)fill(150,ui_height-15,4,4,0xff6a5f);
}
static void draw_start(void){
 center(0,20,46,200,"RF SPECTRUM",NOVA_CYAN);center(1,20,68,200,"NOVA-7",NOVA_CAP);if(capture_error)center(1,12,85,216,message,0xff6a5f);pill(68,108,104,38,"START",true);
 center(1,20,156,200,"LIVE RF RECEIVER",NOVA_CAP);center(1,20,174,200,"NOMINAL MHz / DIGITAL dBFS",NOVA_CAP);pill(77,202,86,28,"CONTROLS",false);text(1,16,12,40,"EXIT",NOVA_CAP);
}
#include "rf_controls.inc"
static void draw_popup(void){
 round_rect(20,44,200,155,18,NOVA_DIM);round_rect(21,45,198,153,17,0);center(1,30,54,180,"LABEL FREQUENCY",NOVA_CAP);char f[20];freq_text(editing.frequency_hz,f);center(0,28,76,184,f,NOVA_CYAN);
 round_rect(31,104,178,32,10,NOVA_DIM);round_rect(32,105,176,30,9,0);text(1,40,112,161,editing.name[0]?editing.name:"NAME THIS ACTIVITY",editing.name[0]?NOVA_WHITE:NOVA_CAP);
 for(unsigned i=0;i<8;i++){int x=32+(int)i*23;if(i==editing.color)round_rect(x-2,144,19,19,9,NOVA_WHITE);round_rect(x,146,15,15,7,label_colors[i]);}
 pill(28,173,59,26,"CANCEL",false);if(edit_slot>=0)pill(91,173,59,26,"DELETE",false);pill(edit_slot>=0?154:126,173,edit_slot>=0?59:87,26,"SAVE",true);
 center(1,10,207,220,"TAP NAME TO TYPE / 16 CHAR MAX",NOVA_CAP);
}
static void draw_keyboard(void){
 text(1,18,15,40,"<",NOVA_CYAN);text(1,45,15,176,"LABEL NAME",NOVA_CAP);fill(18,40,204,1,NOVA_DIM);
 text(1,18,46,204,editing.name,NOVA_CYAN);
 for(unsigned key=0;key<PWK_COUNT;key++){
  portable_watch_key_rect r;(void)portable_watch_key_bounds(key,&r);
  bool selected=key_choice==key;fill(r.x,r.y,r.w,r.h,selected?NOVA_DIM:NOVA_LINE);fill(r.x,r.y,r.w,1,selected?NOVA_CYAN:NOVA_DIM);
  unsigned ch=portable_watch_key_character(key_page,key);char label[2]={(char)(ch?ch:' '),0};
  const char *caption=key==PWK_PAGE?"ABC/#":key==PWK_DELETE?"DELETE":key==PWK_DONE?"DONE":ch==32?"_":label;
  text(1,r.x+5,r.y+(key<PWK_CHARACTERS?3:5),r.w-10,caption,NOVA_CYAN);
 }
 char status[40];snprintf(status,sizeof(status),"%u / 16 CHARACTERS",(unsigned)strlen(editing.name));text(1,18,211,204,status,NOVA_CAP);
}
static int toast_top(void){if(page==PAGE_MAIN&&view<2&&started)return ui_height-42;return page==PAGE_EVENT_LABEL?177:page==PAGE_EVENTS?28:page==PAGE_EVENT_CAPTURE?58:page==PAGE_EVENT_EXAMPLES?32:198;}
static void draw(void){
if(paper&&paper->begin)paper->begin();else {
#ifdef PORTABLE_NOVA_UI
 portable_nova_begin();
#else
 app->clear();
#endif
 }
 if(page==PAGE_RECEIVER)draw_receiver();else if(page==PAGE_EVENTS)draw_events();else if(page==PAGE_EVENT_LABEL)draw_event_label();else if(page==PAGE_EVENT_CAPTURE)draw_event_capture();else if(page==PAGE_EVENT_EXAMPLES)draw_event_examples();else if(page==PAGE_SIGNATURES)draw_signatures();else if(page==PAGE_SIGNATURE_EDIT)draw_signature_edit();else if(page==PAGE_CONTROLS)draw_controls();else if(page==PAGE_LABEL)draw_popup();else if(page==PAGE_KEYBOARD)draw_keyboard();else if(!started)draw_start();else{draw_tabs();if(view==2)draw_monitor();else draw_plot();draw_footer();if(capture_error||input_waiting){round_rect(25,166,190,29,10,NOVA_DIM);round_rect(26,167,188,27,9,0);center(1,30,172,180,message,0xff6a5f);}}
 if(toast_message&&page!=PAGE_KEYBOARD){int y=toast_top();round_rect(14,y,212,28,14,NOVA_DIM);round_rect(15,y+1,210,26,13,0);text(1,23,y+5,undo_slot>=0?145:194,toast_message,NOVA_TEXT);if(undo_slot>=0)text(1,174,y+5,45,"UNDO",NOVA_CYAN);}
 app->present(false);painted=app->millis();dirty=false;
}

static char keyboard_before[17];
static void set_page(unsigned next){if(next==PAGE_CONTROLS&&page!=PAGE_CONTROLS)controls_scroll=0;page=next;urgent_draw=true;contact_down=contact_plot=contact_controls=false;if(app->struct_size>=offsetof(t5_app_api_v1,set_back_exits_app)+sizeof(app->set_back_exits_app)&&app->set_back_exits_app)app->set_back_exits_app(false);dirty=true;}
static void keyboard_begin(void){
 memcpy(keyboard_before,editing.name,sizeof(keyboard_before));key_page=PWK_INITIAL_PAGE;key_choice=0;toast_message=NULL;set_page(PAGE_KEYBOARD);
}
static void keyboard_cancel(void){memcpy(editing.name,keyboard_before,sizeof(editing.name));key_choice=0;set_page(event_keyboard?PAGE_EVENT_LABEL:signature_keyboard?PAGE_SIGNATURE_EDIT:PAGE_LABEL);}
static void keyboard_activate(unsigned key){
 if(page!=PAGE_KEYBOARD || key>=PWK_COUNT)return;
 size_t n=strlen(editing.name);
 if(key<PWK_CHARACTERS){unsigned ch=portable_watch_key_character(key_page,key);if(ch && n<RF_LABEL_NAME_MAX){editing.name[n]=(char)ch;editing.name[n+1]=0;}}
 else if(key==PWK_PAGE)key_page=(key_page+1)%PWK_PAGES;
 else if(key==PWK_DELETE){if(n)editing.name[n-1]=0;}
 else if(key==PWK_DONE){key_choice=0;set_page(event_keyboard?PAGE_EVENT_LABEL:signature_keyboard?PAGE_SIGNATURE_EDIT:PAGE_LABEL);}
 dirty=true;
}
static void save_label(void){
 unsigned start=0,end=(unsigned)strlen(editing.name);while(start<end&&editing.name[start]==' ')start++;while(end>start&&editing.name[end-1]==' ')end--;if(start)for(unsigned i=0;i<end-start;i++)editing.name[i]=editing.name[start+i];editing.name[end-start]=0;
 if(!rf_label_valid(&editing)){notify("ENTER A LABEL NAME");return;}
 int slot=edit_slot;if(slot<0)for(unsigned i=0;i<RF_LABEL_MAX;i++)if(!labels[i].present && !(load_errors&(2u<<i))){slot=(int)i;break;}if(slot<0){notify(load_errors?"RETRY STORAGE FIRST":"8 LABEL LIMIT / DELETE ONE");return;}
 if(load_errors&(2u<<(unsigned)slot)){notify("RETRY STORAGE FIRST");return;}
 labels[slot]=editing;label_active[slot]=level_valid[slot]=false;pending_save|=(uint16_t)(2u<<slot);persist();set_page(PAGE_MAIN);
}
static void delete_label(int slot){
 if(slot<0||slot>=(int)RF_LABEL_MAX||!labels[slot].present)return;
 if(load_errors&(2u<<(unsigned)slot)){notify("RETRY STORAGE FIRST");return;}
 deleted=labels[slot];undo_slot=slot;labels[slot]=(rf_label){0};label_active[slot]=level_valid[slot]=false;pending_save|=(uint16_t)(2u<<slot);persist();set_page(PAGE_MAIN);notify(pending_save?"DELETE NOT SAVED":"LABEL DELETED");
}
static void undo_delete(void){
 if(undo_slot<0)return;
 int slot=undo_slot;if(labels[slot].present){slot=-1;for(unsigned i=0;i<RF_LABEL_MAX;i++)if(!labels[i].present && !(load_errors&(2u<<i))){slot=(int)i;break;}}
 if(slot<0){notify("8 LABEL LIMIT / CANNOT UNDO");return;}labels[slot]=deleted;pending_save|=(uint16_t)(2u<<slot);undo_slot=-1;toast_message=NULL;persist();dirty=true;
}
static void open_label(int slot,uint32_t frequency){
 signature_keyboard=false;event_keyboard=false;
 if(slot<0){unsigned tolerance=2*sample_rate/prefs.fft_size;for(unsigned i=0;i<RF_LABEL_MAX;i++)if(labels[i].present){unsigned t=labels[i].tolerance_hz;if(t<tolerance)t=tolerance;uint32_t difference=labels[i].frequency_hz>frequency?labels[i].frequency_hz-frequency:frequency-labels[i].frequency_hz;if(difference<=t){slot=(int)i;break;}}}
 edit_slot=slot;
 if(slot>=0)editing=labels[slot];else{if(saved_count()==RF_LABEL_MAX){notify("8 LABEL LIMIT / DELETE ONE");return;}editing=(rf_label){.present=true,.frequency_hz=frequency,.tolerance_bins=2};for(unsigned c=0;c<8;c++){bool used=false;for(unsigned i=0;i<RF_LABEL_MAX;i++)if(labels[i].present&&labels[i].color==c)used=true;if(!used){editing.color=(uint8_t)c;break;}}}
 toast_message=NULL;set_page(PAGE_LABEL);
}
static void settings_change(unsigned setting,int direction,int segment){
 if(setting==13){event_load();if(!temporal_retained())set_page(PAGE_EVENTS);return;}
 if(setting==12){signature_load();set_page(PAGE_SIGNATURES);return;}
 if(setting==0)return; /* Reserved legacy source setting is no longer exposed. */
 if(setting!=11 && (load_errors&1u)){notify("RETRY STORAGE FIRST");return;}

 switch(setting){
 case 1:case 2:{uint32_t min=rf_identity_low_hz(&prefs.identity),max=rf_identity_high_hz(&prefs.identity),step=prefs.identity.sample_rate_hz/32u;uint32_t current=setting==1?prefs.low_hz:prefs.high_hz;int64_t v=(int64_t)current+direction*(int64_t)step;if(v<min)v=min;if(v>max)v=max;if((setting==1&&v>=prefs.high_hz)||(setting==2&&v<=prefs.low_hz))return;if(setting==1)prefs.low_hz=(uint32_t)v;else prefs.high_hz=(uint32_t)v;break;}
 case 3:prefs.log_frequency=segment==0;break;case 4:prefs.log_amplitude=segment==0;break;case 5:prefs.show_labels=segment==0;break;case 6:prefs.palette=(uint8_t)clamp_int((int)prefs.palette+direction,0,4);build_palette();break;case 7:prefs.gain_db=(int8_t)clamp_int(prefs.gain_db+direction*3,-24,60);break;case 8:prefs.window=(uint8_t)clamp_int((int)prefs.window+direction,0,4);break;case 9:if(receiver_extended&&direction>0&&prefs.fft_size*2u>receiver_caps.max_pairs){notify("RECEIVER COHERENT BURST LIMIT");return;}if(!receiver_extended&&direction>0){notify("LEGACY MAX 256 / RECEIVER");return;}prefs.fft_size=(uint16_t)clamp_int(direction>0?prefs.fft_size*2:prefs.fft_size/2,256,8192);break;case 10:prefs.threshold_db=(int8_t)clamp_int(prefs.threshold_db+direction*5,-90,-20);break;case 11:{bool resume=capture_requested;pause_capture();capture_requested=false;if(event_files.pending>=0||event_files.ready!=3)event_retry();if(temporal_retained())return;signature_load();signature_save();if(load_errors){reload_storage();configure_dsp();build_palette();}persist();capture_requested=resume;portable_rf_capture_resume();notify(store_message?store_message:(event_files.pending>=0||event_files.ready!=3)?(event_message?event_message:"APP DATA UNAVAILABLE"):"SAVED DATA LOADED");dirty=true;return;}default:return;
 }
 configure_dsp();pending_save|=1;persist();dirty=true;
}
/* Paper presentation owns source-aware touch sampling. Its adapter deliberately
 * does not also publish T5 touch taps. Consume that one source exactly once and
 * let this controller own every hit target, including its top-left chrome. */
static void normalize_input(t5_app_input_t *input){
 paper_contact=(springboard_contact){0};
 if(!paper||!paper->contact)return;
 paper->contact(&paper_contact);
 bool release=paper_contact.valid&&paper_contact.released&&!paper_contact.cancelled&&paper_contact.tap_eligible;
 input->tapped=release;
 if(release){input->touch_x=paper_contact.x;input->touch_y=paper_contact.y;}
 if(release&&paper_contact.x<44&&paper_contact.y<40&&input->exit_requested&&(input->buttons&T5_APP_BUTTON_BACK)){
  input->exit_requested=false;input->buttons&=~T5_APP_BUTTON_BACK;
 }
}
static bool process_contact(void){
 t5_app_contact_t contact={0};
 if(paper&&paper->contact){
  if(paper_contact.cancelled){contact_down=contact_plot=contact_controls=contact_drag=contact_moved=false;return true;}
  if(!paper_contact.valid)return false;
  contact=(t5_app_contact_t){paper_contact.down,paper_contact.x,paper_contact.y};
  if(contact.down&&!contact_down&&!paper_contact.tap_eligible)return false;
 }else{if(!optional_contact()||!app->touch_contact(&contact))return false;}
 int x=ui_unx(contact.x),y=ui_uny(contact.y);
 bool consumed=false;
 if(contact.down){
  if(!contact_down){contact_down=true;contact_controls=page==PAGE_CONTROLS&&hit(x,y,0,CONTROLS_TOP,240,CONTROLS_BOTTOM-CONTROLS_TOP);controls_touch_y=y;controls_touch_scroll=controls_scroll;contact_plot=page==PAGE_MAIN&&started&&view<2&&hit(x,y,PLOT_X,PLOT_Y,PLOT_W,PLOT_H)&&!(cursor_visible&&hit(x,y,pill_x,pill_y,pill_w,pill_h));contact_moved=false;
   if(contact_plot){int coordinate=view==1?y-PLOT_Y:x-PLOT_X;drag_origin=coordinate;int cursor=(int)rf_dsp_column_at(&dsp_config,cursor_hz,view==1?PLOT_H:PLOT_W);contact_drag=!cursor_visible||abs_int(cursor-coordinate)<=14;if(contact_drag){cursor_hz=rf_dsp_frequency_at(&dsp_config,(unsigned)coordinate,view==1?PLOT_H:PLOT_W);cursor_visible=true;dirty=true;}}
  }else if(contact_controls){if(abs_int(y-controls_touch_y)>6)contact_moved=true;if(contact_moved){controls_move(controls_touch_scroll+controls_touch_y-y);consumed=true;}}
  else if(contact_plot){int coordinate=clamp_int(view==1?y-PLOT_Y:x-PLOT_X,0,view==1?PLOT_H-1:PLOT_W-1);if(abs_int(coordinate-drag_origin)>6){contact_moved=true;contact_drag=true;}if(contact_drag){cursor_hz=rf_dsp_frequency_at(&dsp_config,(unsigned)coordinate,view==1?PLOT_H:PLOT_W);cursor_visible=true;dirty=true;}}
 }else if(contact_down){consumed=contact_plot||(contact_controls&&contact_moved);if(contact_plot&&!contact_drag&&!contact_moved){cursor_visible=false;dirty=true;}contact_down=contact_plot=contact_controls=false;}
 return consumed;
}
static bool request_root_exit(void){
 stop();if(!signature_exit_ready()||!temporal_exit_ready())return false;
#ifdef RF_RETURN_APP
 if(!runtime->request_launch||!runtime->request_launch(RF_RETURN_APP)){message="EXIT FAILED / RETRY";notify(message);return false;}
#endif
 return true;
}
static bool tap_action(int x,int y,bool *toggle_requested,bool *freeze_requested){
 if(x<0||y<0||x>=240||y>=(ui_stretched()?240:ui_height))return false;
 if(page==PAGE_RECEIVER){receiver_tap(x,y);return false;}
 if(page!=PAGE_KEYBOARD&&toast_message&&hit(x,y,14,toast_top(),212,28)){if(undo_slot>=0&&x>=167)undo_delete();else{toast_message=NULL;dirty=true;}return false;}
 if(page==PAGE_EVENTS||page==PAGE_EVENT_LABEL||page==PAGE_EVENT_CAPTURE||page==PAGE_EVENT_EXAMPLES){event_tap(x,y,toggle_requested);return false;}
 if(page==PAGE_SIGNATURES||page==PAGE_SIGNATURE_EDIT){signature_tap(x,y,toggle_requested);return false;}
 if(page==PAGE_KEYBOARD){if(hit(x,y,8,4,48,36)){keyboard_cancel();return false;}int key=portable_watch_key_hit(x,y);if(key>=0){key_choice=(unsigned)key;keyboard_activate((unsigned)key);}return false;}
 if(page==PAGE_LABEL){if(hit(x,y,31,104,178,32)){keyboard_begin();}else if(y>=140&&y<166&&x>=28&&x<216){editing.color=(uint8_t)clamp_int((x-28)/23,0,7);dirty=true;}else if(y>=169&&y<203){if(x>=24&&x<89)set_page(PAGE_MAIN);else if(edit_slot>=0&&x>=89&&x<152)delete_label(edit_slot);else if(x>=(edit_slot>=0?152:122)&&x<218)save_label();}return false;}
 if(page==PAGE_CONTROLS){if(y>=21&&y<45){receiver_scroll=0;set_page(PAGE_RECEIVER);return false;}if(hit(x,y,79,ui_height-39,82,32)){set_page(PAGE_MAIN);return false;}if(y<CONTROLS_TOP||y>=CONTROLS_BOTTOM)return false;unsigned position=(unsigned)(y-CONTROLS_TOP+controls_scroll),setting=position/CONTROLS_ROW_HEIGHT+1;int row_y=(int)(position%CONTROLS_ROW_HEIGHT);if(setting>CONTROLS_COUNT||row_y<4||row_y>=44)return false;if(controls_segment(setting)){if(x>=110&&x<165)settings_change(setting,0,0);else if(x>=171&&x<232)settings_change(setting,0,1);}else if(setting==11||setting==12||setting==13){if(x>=110&&x<232)settings_change(setting,0,0);}else if(x>=90&&x<122)settings_change(setting,-1,0);else if(x>=200&&x<232)settings_change(setting,1,0);return false;}
 if(!started){if(hit(x,y,68,102,104,48))*toggle_requested=true;else if(hit(x,y,72,196,98,38)){list_scroll=0;set_page(PAGE_CONTROLS);}else if(hit(x,y,6,4,50,34))return request_root_exit();return false;}
 if(y>=8&&y<42){if(x>=4&&x<55){view=0;list_scroll=0;lab_edit=false;}else if(x>=58&&x<108){view=1;list_scroll=0;lab_edit=false;}else if(x>=111&&x<185){view=2;list_scroll=0;lab_edit=false;}else if(x>=188&&x<210&&view!=2){if(load_errors&1u)notify("RETRY STORAGE FIRST");else{prefs.show_labels=!prefs.show_labels;pending_save|=1;persist();}}else if(x>=213&&x<240){list_scroll=0;set_page(PAGE_CONTROLS);}if(page==PAGE_MAIN)set_page(PAGE_MAIN);dirty=true;return false;}
 if(view==2){if(hit(x,y,161,44,64,37)){lab_edit=!lab_edit;list_scroll=0;set_page(PAGE_MAIN);return false;}if(lab_edit){unsigned order[8],n=ordered_labels(order);if(y>=88&&y<198){unsigned row=(unsigned)(y-88)/55+list_scroll;if(row<n){if(x>=182&&x<220)delete_label((int)order[row]);else if(x>=20&&x<181)open_label((int)order[row],0);}}if(y>=200&&y<235){if(x>=16&&x<68&&list_scroll)list_scroll--;else if(x>=173&&x<224&&list_scroll+2<n)list_scroll++;}}else if(y>=210){monitor_item items[17];unsigned n=monitor_items(items);if(x<39&&list_scroll)list_scroll--;else if(x>=45&&x<111)*toggle_requested=true;else if(x>=117&&x<196){event_list_scroll=0;toast_message=NULL;set_page(PAGE_EVENTS);}else if(x>=202&&list_scroll+3<n)list_scroll++;}else if(hit(x,y,20,85,200,41)){toast_message=NULL;set_page(PAGE_SIGNATURES);}dirty=true;return false;}
 if(cursor_visible&&hit(x,y,pill_x,pill_y,pill_w,pill_h)){open_label(-1,cursor_hz);return false;}
 if(hit(x,y,PLOT_X,PLOT_Y,PLOT_W,PLOT_H)){int coordinate=view==1?y-PLOT_Y:x-PLOT_X;int current=(int)rf_dsp_column_at(&dsp_config,cursor_hz,view==1?PLOT_H:PLOT_W);if(cursor_visible&&abs_int(current-coordinate)>14)cursor_visible=false;else{cursor_visible=true;cursor_hz=rf_dsp_frequency_at(&dsp_config,(unsigned)coordinate,view==1?PLOT_H:PLOT_W);}dirty=true;return false;}
 if(y>=ui_height-24&&y<ui_height){if(x>=6&&x<150)*toggle_requested=true;else if(x>=154&&x<240){if(running)*freeze_requested=true;else return request_root_exit();}}return false;
}
static bool rf_bulk_create(void){
 rf_bulk_allocated=app->struct_size>=offsetof(t5_app_api_v1,psram_free)+sizeof(app->psram_free)&&app->psram_alloc&&app->psram_free;
 rf_bulk=rf_bulk_allocated?app->psram_alloc(sizeof(*rf_bulk)):&rf_resident_bulk;if(!rf_bulk)return false;memset(rf_bulk,0,sizeof(*rf_bulk));return true;
}
void app_main(void){
 app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);waterfall_api=app;waterfall_runtime=runtime;
 if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,millis)+sizeof(app->millis)||!app->poll||!app->millis||!app->screen_width||!app->screen_height||!app->clear||!app->fill_rect||!app->present||!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms)return;
 if(app->screen_width()<240||app->screen_width()>1024||app->screen_height()<240||app->screen_height()>1024)return;
 paper=paper_presentation_get();ui_height=paper?app->screen_height()/(app->screen_width()/240):240;if(ui_height<240||ui_height>512)return;
 if(!rf_bulk_create()){waterfall_log("SDR bulk memory unavailable");return;}
 waterfall_radio=NULL;preferences=NULL;storage=NULL;waterfall_grant=(risc_runtime_capability_v1){0};store_grant=(risc_runtime_capability_v1){0};radio_acquired=preferences_acquired=waterfall_uncertain=false;waterfall_enabled=false;trace_capture_seen=trace_cleanup_seen=false;waterfall_status=WATERFALL_STOPPED;
 waterfall_preferences=(risc_runtime_capability_v1){.struct_size=sizeof(waterfall_preferences)};if(runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&waterfall_preferences)){preferences_acquired=true;preferences=waterfall_preferences.api;}
 capture_requested=input_gap=input_waiting=capture_error=owned=uncertain=running=frozen=store_acquired=lab_edit=cursor_visible=contact_down=contact_plot=contact_drag=contact_moved=started=false;dirty=urgent_draw=true;receiver_extended=false;
 pending_save=load_errors=0;page=PAGE_MAIN;view=list_scroll=key_page=key_choice=0;edit_slot=undo_slot=-1;message="READY / RF OFF";store_message=toast_message=NULL;last_pcm_at=rendered=recorded_transform=toast_until=painted=0;sample_rate=PREFERRED_RATE;pill_w=pill_h=0;controls_scroll=controls_touch_y=controls_touch_scroll=0;contact_controls=false;
 waterfall_log("SDR app enter");restore();signature_restore();signature_mode=prefs.background_mode;manual_room=prefs.manual_room;event_restore();if(temporal_retained())retain();configure_dsp();build_palette();set_page(PAGE_MAIN);toggle();if(store_message)notify(store_message);
 for(;;){
  if(waterfall_uncertain){waterfall_drain();uncertain=owned=running=false;}
  if(temporal_retained())retain();
  uint32_t now=app->millis();if(dirty&&(!running||urgent_draw||(uint32_t)(now-painted)>=(paper?900u:100u))){draw();urgent_draw=false;}
  t5_app_input_t input={0};
  if(!app->poll(&input,running?1:30)){
#ifdef PORTABLE_ALARM_CLIENT
   if(portable_app_sleep_retained())return;
#endif
   if(!signature_exit_ready()||!temporal_exit_ready())continue;
   break;
  }
  if(waterfall_uncertain)continue;
  if(temporal_retained())retain();
  normalize_input(&input);
  if(input.exit_requested){stop();if(signature_exit_ready()&&temporal_exit_ready())break;continue;}
  if(input.buttons&T5_APP_BUTTON_BACK){if(page==PAGE_KEYBOARD)keyboard_cancel();else if(page==PAGE_EVENT_CAPTURE){if(event_armed||event_training.ready)notify("SAVE OR CANCEL THE WINDOW");else set_page(PAGE_EVENT_LABEL);}else if(page==PAGE_EVENT_EXAMPLES)set_page(PAGE_EVENT_LABEL);else if(page==PAGE_EVENT_LABEL)set_page(PAGE_EVENTS);else if(page==PAGE_EVENTS||page==PAGE_SIGNATURES||page==PAGE_RECEIVER)set_page(PAGE_CONTROLS);else if(page==PAGE_SIGNATURE_EDIT)set_page(PAGE_SIGNATURES);else if(page!=PAGE_MAIN){list_scroll=0;set_page(PAGE_MAIN);}else if(lab_edit){lab_edit=false;set_page(PAGE_MAIN);}else if(request_root_exit())break;continue;}
  now=app->millis();if(event_slot>=0&&(uint32_t)(now-last_event_at)>2000u){event_slot=-1;dirty=true;}if(toast_message&&(int32_t)(now-toast_until)>=0){toast_message=NULL;undo_slot=-1;dirty=true;}
  bool consumed=process_contact(),toggle_requested=false,freeze_requested=false;
  uint32_t buttons=input.buttons;if(buttons||input.tapped||consumed)urgent_draw=true;
  if(page==PAGE_MAIN){if(buttons&T5_APP_BUTTON_CONFIRM)toggle_requested=true;else if(buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT)){view=(view+(buttons&T5_APP_BUTTON_LEFT?2u:1u))%3;list_scroll=0;lab_edit=false;set_page(PAGE_MAIN);}else if(buttons&T5_APP_BUTTON_DOWN)freeze_requested=true;}
  else if(page==PAGE_CONTROLS){if(buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))controls_move(controls_scroll-CONTROLS_ROW_HEIGHT);else if(buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))controls_move(controls_scroll+CONTROLS_ROW_HEIGHT);}
  else if(page==PAGE_RECEIVER){if(buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT)){if(receiver_scroll)receiver_scroll--;dirty=true;}else if(buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)){if(receiver_scroll+3<RF_RECEIVER_ROWS)receiver_scroll++;dirty=true;}}
  else if(page==PAGE_LABEL&&(buttons&T5_APP_BUTTON_CONFIRM))save_label();
  else if(page==PAGE_KEYBOARD){if(buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP))key_choice=key_choice?key_choice-1:PWK_COUNT-1;else if(buttons&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_DOWN))key_choice=(key_choice+1)%PWK_COUNT;else if(buttons&T5_APP_BUTTON_CONFIRM)keyboard_activate(key_choice);if(buttons)dirty=true;}
  if(input.tapped&&!consumed&&tap_action(ui_unx(input.touch_x),ui_uny(input.touch_y),&toggle_requested,&freeze_requested))break;
  if(temporal_retained())retain();
  if(freeze_requested)freeze();else if(toggle_requested)toggle();if(running)capture();if(running)temporal_tick();
  if(!input.buttons&&!input.tapped&&!contact_down&&!consumed)event_neural_tick();
 }
 stop();persist();if(event_data_acquired&&!runtime->release(&event_data_grant))retain();event_data_acquired=false;event_files.api=NULL;waterfall_release(&waterfall_grant,&radio_acquired);waterfall_radio=NULL;if(store_acquired&&!runtime->release(&store_grant))retain();store_acquired=false;storage=NULL;waterfall_release(&waterfall_preferences,&preferences_acquired);preferences=NULL;if(rf_bulk_allocated)app->psram_free(rf_bulk);rf_bulk=NULL;waterfall_log("SDR app exit");
}
