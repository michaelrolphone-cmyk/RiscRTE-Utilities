/* Exercise recovery through the actual app, using the established provider fixture. */
#define main original_controller_main
#include "audio_spectrum_test.c"
#undef main
static uint8_t saved_config[32],saved_label[32];
static int32_t fail_startup_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size){
 if(!strcmp(key,"spectrum_cfg")||!strcmp(key,"spectrum_l0")){*size=0;return RISC_KEY_VALUE_IO;}
 return get_value(ctx,key,out,cap,size);
}
static void recover_reads(void){
 assert(load_errors==3u && store_message && !pending_save);
 assert(prefs.gain_db==12 && !labels[0].present);
 settings_change(7,1,0);assert(!puts_count && prefs.gain_db==12);
 kv.get=get_value;settings_change(11,0,0);
 assert(!puts_count && !load_errors && !store_message && !running);
 assert(prefs.gain_db==24 && prefs.source==1 && labels[0].present && !strcmp(labels[0].name,"Saved label"));
 assert(!memcmp(cells[0].bytes,saved_config,32)&&!memcmp(cells[1].bytes,saved_label,32));
}
static void corrupt_label_retry(void){
 assert(load_errors==2u && !pending_save && !labels[0].present);
 unsigned before=puts_count;settings_change(11,0,0);
 assert(load_errors==2u && store_message && puts_count==before);
 editing=(spectrum_label){true,777,2,"New label"};edit_slot=-1;save_label();
 assert(!labels[0].present && labels[4].present && load_errors==2u && store_message);
 assert(puts_count==before+1 && !strcmp(cells[1].key,"spectrum_l4"));
 for(unsigned i=0;i<32;i++)assert(cells[0].bytes[i]==255);
 settings_change(7,1,0);assert(load_errors==2u && store_message);
 assert(prefs.gain_db==15); /* unrelated valid config remains editable */
}
static void recover_denied_store(void){
 assert(load_errors==511u && !store_live && !puts_count);
 deny_store=false;settings_change(11,0,0);
 assert(!load_errors && !store_message && !puts_count && labels[0].present && prefs.high_hz==8000);
}
static void corrupt_config_retry(void){
 assert(load_errors==1u && !puts_count);
 settings_change(11,0,0);assert(load_errors==1u&&store_message&&!puts_count);
 settings_change(0,0,1);assert(!puts_count&&prefs.source==0);
 bool toggle_requested=false,freeze_requested=false;started=true;tap_action(180,26,&toggle_requested,&freeze_requested);
 assert(!puts_count&&prefs.show_labels);
}
static void recover_live_capture(void){
 assert(running&&owned&&live&&load_errors==3u);
 kv.get=get_value;settings_change(11,0,0);
 assert(!running&&!owned&&!live&&closes==1&&!puts_count&&!load_errors&&prefs.source==1);
}
int main(void){
 spectrum_preferences p=spectrum_preferences_default();p.source=1;p.gain_db=24;spectrum_preferences_encode(&p,saved_config);
 spectrum_label l={true,600,2,"Saved label"};spectrum_label_encode(&l,saved_label);
 reset();preload("spectrum_cfg",saved_config);preload("spectrum_l0",saved_label);kv.get=fail_startup_get;check(recover_reads);back();run();clean(0,0,0);
 reset();preload("spectrum_cfg",saved_config);preload("spectrum_l0",saved_label);kv.get=fail_startup_get;start();check(recover_live_capture);back();run();clean(1,1,1);
 uint8_t invalid[32];memset(invalid,255,32);
 reset();preload("spectrum_l0",invalid);check(corrupt_label_retry);back();run();clean(0,0,0);
 reset();preload("spectrum_cfg",invalid);check(corrupt_config_retry);back();run();clean(0,0,0);
 reset();deny_store=true;check(recover_denied_store);back();run();assert(!escaped&&!live&&!store_live&&store_acquires==2&&store_releases==1&&!puts_count);
 puts("Spectrum recovery: no writes on read retry, saved settings/labels restored, unresolved slots reserved, per-record errors retained, and live microphone closed before reload passed");
 return 0;
}
