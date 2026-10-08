#define PORTABLE_ALARM_CLIENT
#define PORTABLE_RETURN_APP "springboard.elf"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/audio_spectrum.c"

typedef enum {EVENT_INPUT,EVENT_END,EVENT_SUSPEND,EVENT_RETAIN,EVENT_CALL} event_kind;
typedef struct {event_kind kind;t5_app_input_t input;int running_before;void (*check)(void);t5_app_contact_t contact;} event;
#ifndef SPECTRUM_TEST_EVENTS
#define SPECTRUM_TEST_EVENTS 1600
#endif
static event events[SPECTRUM_TEST_EVENTS];
static unsigned launches;static bool fail_launch;
static unsigned count,index_event,opens,reads,closes,acquires,releases,polls,frames,yields,store_acquires,store_releases,puts_count;
static int width,height;static uint32_t now;static bool deny_acquire,fail_open,fail_read,fail_close,fail_release,retained,present_failure,ui_failed,live,grant_live,escaped,store_live,deny_store,fail_put,back_exits;
static size_t read_count;static int malformed;static t5_app_api_v1 api;static risc_runtime_api_v1 rt;static twatch_audio_in_api_v1 mic;static risc_key_value_v1 kv;static jmp_buf retained_jump;static t5_app_contact_t current_contact;
static struct {char key[16];uint8_t bytes[2048];uint32_t size;} cells[24];
static uint8_t pixels[1024*1024];
static int32_t screen_width(void){return width;}static int32_t screen_height(void){return height;}static void clear(void){memset(pixels,255,sizeof(pixels));}
static void rect(int32_t x,int32_t y,int32_t w,int32_t h,bool black){assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=width&&y+h<=height);for(int row=y;row<y+h;row++)memset(pixels+row*width+x,black?0:255,(size_t)w);}
static void present(bool full){assert(!full);frames++;if(present_failure)ui_failed=true;}static uint32_t millis(void){return now;}static void set_back(bool exits){back_exits=exits;}static bool contact(t5_app_contact_t *out){*out=current_contact;return true;}
static bool open_mic(void *ctx,uint32_t rate){assert(ctx==&mic&&rate==16000&&grant_live&&!live);opens++;live=true;return !fail_open;}
static bool read_mic(void *ctx,int16_t *pcm,size_t cap,size_t *got){assert(ctx==&mic&&live&&grant_live&&!uncertain&&running&&cap==256&&pcm&&got);reads++;now+=16;size_t n=read_count>256?256:read_count;for(size_t i=0;i<n;i++)pcm[i]=(int16_t)(spectrum_dsp_sin((unsigned)(i+reads*256)*4096)>>16);*got=read_count;return !fail_read;}
static bool close_mic(void *ctx){assert(ctx==&mic&&live&&grant_live&&!uncertain);closes++;if(fail_close)return false;live=false;return true;}
static int32_t get_value(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size){assert(ctx==&kv&&store_live);*size=0;for(unsigned i=0;i<24;i++)if(!strcmp(cells[i].key,key)){*size=cells[i].size;if(*size>cap)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,cells[i].bytes,*size);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t put_value(void *ctx,const char *key,const void *in,uint32_t size){assert(ctx==&kv&&store_live&&(size==32||size==SPECTRUM_SIGNATURE_RECORD_SIZE)&&strlen(key)<16);puts_count++;if(fail_put)return RISC_KEY_VALUE_IO;unsigned i=0;for(;i<24&&cells[i].key[0]&&strcmp(cells[i].key,key);i++);assert(i<24);strcpy(cells[i].key,key);memcpy(cells[i].bytes,in,size);cells[i].size=size;return 0;}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){if(!strcmp(name,"storage.app-data")){assert(version==1&&instance==2);return false;}assert(version==(!strcmp(name,"storage.key-value")?2u:1u)&&instance==0&&out->struct_size==sizeof(*out));if(!strcmp(name,"storage.key-value")){store_acquires++;if(deny_store)return false;assert(!store_live);store_live=true;out->api=&kv;return true;}assert(!strcmp(name,"audio.input")&&!grant_live);acquires++;if(deny_acquire)return false;grant_live=true;out->api=malformed==4?NULL:&mic;return true;}
static bool release(risc_runtime_capability_v1 *in){if(in==&store_grant){assert(store_live);store_releases++;store_live=false;return true;}assert(in==&grant&&grant_live&&!live&&!uncertain);releases++;if(fail_release)return false;grant_live=false;return true;}
static bool launch(const char *name){assert(!strcmp(name,"springboard.elf")&&!running&&!owned);launches++;return !fail_launch;}
static bool diagnostic(const char *s){assert(strstr(s,"retained"));return true;}static void yield_ms(uint32_t ms){assert(ms==50);yields++;assert(fail_close||fail_release);longjmp(retained_jump,1);}bool portable_app_sleep_retained(void){return retained;}
static bool poll(t5_app_input_t *out,uint32_t wait){assert(wait==(running?1u:30u));polls++;now+=wait;if(ui_failed)return false;assert(index_event<count);event e=events[index_event++];if(e.running_before>=0)assert(running==(e.running_before!=0));if(e.check)e.check();if(e.kind==EVENT_END)return false;if(e.kind==EVENT_SUSPEND||e.kind==EVENT_RETAIN){assert(portable_audio_suspend());assert(!running&&!owned);if(e.kind==EVENT_RETAIN){retained=true;return false;}}current_contact=e.contact;*out=e.input;return true;}
const t5_app_api_v1 *t5_app_get_api(uint32_t v){assert(v==1);return &api;}const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return &rt;}
static void reset(void){launches=0;fail_launch=false;count=index_event=opens=reads=closes=acquires=releases=polls=frames=yields=store_acquires=store_releases=puts_count=0;width=height=240;now=0;deny_acquire=fail_open=fail_read=fail_close=fail_release=retained=present_failure=ui_failed=live=grant_live=escaped=store_live=deny_store=fail_put=false;read_count=256;malformed=0;memset(cells,0,sizeof(cells));current_contact=(t5_app_contact_t){0};api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(api),.screen_width=screen_width,.screen_height=screen_height,.clear=clear,.fill_rect=rect,.present=present,.poll=poll,.millis=millis,.set_back_exits_app=set_back,.touch_contact=contact};rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch};mic=(twatch_audio_in_api_v1){.api_version=1,.struct_size=sizeof(mic),.context=&mic,.open=open_mic,.read=read_mic,.close=close_mic};kv=(risc_key_value_v1){2,sizeof(kv),&kv,get_value,put_value};}
static void add(event_kind kind,uint32_t buttons,int before){assert(count<SPECTRUM_TEST_EVENTS);events[count++]=(event){.kind=kind,.input={.buttons=buttons},.running_before=before};}
static void tap(int x,int y,int before){add(EVENT_INPUT,0,before);events[count-1].input=(t5_app_input_t){.tapped=true,.touch_x=(int16_t)x,.touch_y=(int16_t)y};}
static void touch(int x,int y,bool down){add(EVENT_INPUT,0,-1);events[count-1].contact=(t5_app_contact_t){down,(int16_t)x,(int16_t)y};}
static void check(void (*fn)(void)){add(EVENT_CALL,0,-1);events[count-1].check=fn;}
static void start(void){add(EVENT_INPUT,T5_APP_BUTTON_CONFIRM,0);}static void back(void){add(EVENT_INPUT,T5_APP_BUTTON_BACK,-1);}static void run(void){if(!setjmp(retained_jump))app_main();else escaped=true;}
static void clean(unsigned o,unsigned r,unsigned c){assert(!escaped&&!live&&!grant_live&&!owned&&!running&&!uncertain&&!store_live&&!portable_audio_capture_active()&&!capture_requested);assert(opens==o&&reads==r&&closes==c);assert(acquires==releases);assert(store_acquires==store_releases);}
static void check_exit_retry(void){assert(launches==1&&!running&&!owned&&!strcmp(message,"EXIT FAILED / RETRY"));fail_launch=false;}
static void check_controls0(void){assert(page==PAGE_CONTROLS&&controls_scroll==0&&!back_exits&&prefs.high_hz==8000);}
static void check_controls1(void){assert(page==PAGE_CONTROLS&&controls_scroll==51&&puts_count==0);}
static void check_controls2(void){assert(page==PAGE_CONTROLS&&controls_scroll==CONTROLS_MAX_SCROLL&&puts_count==0);}
static void check_live_only(void){assert(prefs.source==0&&running&&owned&&opens==1);}
static void check_live_data(void){assert(spectrum.transforms>0&&history_count>0&&prefs.source==0&&reads>600);}
static void check_waiting_audio(void){assert(running&&owned&&portable_audio_capture_active()&&input_waiting&&!capture_error&&closes==0);read_count=256;}
static void check_audio_resumed(void){assert(running&&owned&&!input_waiting&&!capture_error&&opens==1&&!closes&&spectrum.transforms>0);}
static void check_cursor(void){assert(cursor_visible&&cursor_hz>=100&&cursor_hz<=8000);}
static void check_no_cursor(void){assert(!cursor_visible);}
static void check_popup(void){assert(page==PAGE_LABEL&&!back_exits&&editing.present);}
static void check_keyboard(void){assert(page==PAGE_KEYBOARD&&!back_exits);}
static void check_saved_label(void){assert(labels[4].present&&!strcmp(labels[4].name,"ae")&&labels[4].color==7&&page==PAGE_MAIN&&!pending_save);}
static void check_deleted(void){assert(!labels[0].present&&undo_slot==0);}
static void check_undone(void){assert(labels[0].present&&!strcmp(labels[0].name,"Mains hum")&&undo_slot<0);}
static void direct_edit_saved(void){open_label(0,0);strcpy(editing.name,"Power hum");editing.color=4;save_label();assert(!strcmp(labels[0].name,"Power hum")&&!pending_save);}
static void direct_unsaved(void){fail_put=true;prefs.gain_db=15;pending_save|=1;persist();assert(pending_save==1&&store_message);fail_put=false;persist();assert(!pending_save&&!store_message);}
static void direct_all_settings(void){for(unsigned k=0;k<12;k++){settings_change(k,1,1);settings_change(k,-1,0);}assert(spectrum_preferences_valid(&prefs)&&prefs.high_hz<=8000);}
static void check_restored(void){assert(prefs.source==0&&prefs.gain_db==24&&labels[0].present&&!strcmp(labels[0].name,"Saved label")&&!labels[1].present&&!running);}
static void check_continuous_controls(void){
 bool t=false,f=false;assert(page==PAGE_CONTROLS);unsigned before=puts_count;
 controls_move(51);assert(controls_scroll==51);draw();
 /* A clipped HIGH row is still HIGH; header/footer never hit a hidden row. */
 tap_action(105,70,&t,&f);assert(prefs.high_hz==5000&&puts_count==before+1);
 tap_action(215,45,&t,&f);tap_action(215,195,&t,&f);assert(puts_count==before+1);
 controls_move(7*CONTROLS_ROW_HEIGHT);assert(controls_scroll==336);draw();
 tap_action(215,70,&t,&f);assert(prefs.window==2&&puts_count==before+2);
 controls_move(10000);assert(controls_scroll==CONTROLS_MAX_SCROLL);draw();
 controls_move(-10000);assert(controls_scroll==0);draw();
 settings_change(0,0,1);assert(prefs.source==0&&puts_count==before+2);
}
static void check_bad_records(void){assert(prefs.high_hz==8000&&!labels[0].present&&store_message&&puts_count==0);}
static void preload(const char *key,const uint8_t *bytes){unsigned i=0;while(cells[i].key[0])i++;strcpy(cells[i].key,key);memcpy(cells[i].bytes,bytes,32);cells[i].size=32;}
int main(void){
 reset();back();run();clean(0,0,0);assert(acquires==0&&store_acquires==1&&puts_count==0);
 reset();tap(20,20,0);run();clean(0,0,0);assert(launches==1);
 reset();fail_launch=true;tap(20,20,0);check(check_exit_retry);tap(20,20,0);run();clean(0,0,0);assert(launches==2);
 reset();start();tap(185,228,1);tap(185,228,0);run();clean(1,1,1);assert(launches==1);
 reset();start();back();run();clean(1,1,1);
 reset();start();add(EVENT_INPUT,T5_APP_BUTTON_CONFIRM,1);back();run();clean(1,1,1);assert(!strcmp(message,"STOPPED / MIC OFF"));
 reset();start();tap(60,228,1);events[count-1].input.buttons=T5_APP_BUTTON_CONFIRM;back();run();clean(1,1,1);
 reset();start();tap(185,228,1);events[count-1].input.buttons=T5_APP_BUTTON_CONFIRM;back();run();clean(1,1,1);assert(frozen);
 reset();start();add(EVENT_INPUT,T5_APP_BUTTON_DOWN,1);start();back();run();clean(2,2,2);
 reset();start();tap(84,26,1);for(unsigned i=0;i<450;i++)add(EVENT_INPUT,0,1);tap(185,228,1);back();run();clean(1,452,1);assert(view==1&&history_count>40&&frozen);
 reset();read_count=17;start();for(unsigned i=0;i<120;i++)add(EVENT_INPUT,0,1);back();run();clean(1,121,1);assert(spectrum.transforms==1);
 reset();read_count=0;start();for(unsigned i=0;i<260;i++)add(EVENT_INPUT,0,1);check(check_waiting_audio);for(unsigned i=0;i<20;i++)add(EVENT_INPUT,0,1);check(check_audio_resumed);back();run();clean(1,283,1);
 reset();read_count=257;start();back();run();clean(1,1,1);assert(spectrum.transforms==0&&!strcmp(message,"MIC READ FAILED"));
 reset();read_count=SIZE_MAX;start();back();run();clean(1,1,1);
 reset();read_count=17;fail_read=true;start();back();run();clean(1,1,1);assert(!spectrum.used&&!spectrum.transforms);
 reset();fail_open=true;start();back();run();clean(1,0,1);assert(!strcmp(message,"MIC START FAILED"));
 reset();deny_acquire=true;start();back();run();assert(!opens&&!closes&&!releases&&acquires==1&&!store_live);
 for(int fault=1;fault<=4;fault++){reset();malformed=fault;if(fault==1)mic.api_version=2;if(fault==2)mic.struct_size=4;if(fault==3)mic.read=NULL;start();start();back();run();clean(0,0,0);assert(acquires==1);}
 reset();start();add(EVENT_END,0,1);run();clean(1,1,1);
 reset();present_failure=true;start();run();clean(0,0,0);
 reset();start();add(EVENT_SUSPEND,0,1);add(EVENT_INPUT,0,0);add(EVENT_INPUT,0,0);back();run();clean(1,1,1);
 reset();start();add(EVENT_SUSPEND,0,1);start();back();run();clean(2,2,2);
 reset();start();add(EVENT_RETAIN,0,1);run();assert(!escaped&&!live&&grant_live&&store_live&&acquires==1&&!releases&&closes==1&&reads==1);
 reset();add(EVENT_RETAIN,0,0);run();assert(!acquires&&!closes&&!releases&&store_live&&!store_releases);
 reset();fail_close=true;start();back();run();assert(escaped&&uncertain&&live&&grant_live&&closes==1&&reads==1&&!releases&&yields==1);assert(!portable_audio_suspend()&&closes==1&&!portable_audio_services_safe());
 reset();fail_open=fail_close=true;start();run();assert(escaped&&closes==1&&!reads&&!releases);
 reset();fail_read=fail_close=true;start();run();assert(escaped&&closes==1&&reads==1&&!releases);
 reset();fail_release=true;start();back();run();assert(escaped&&!live&&grant_live&&releases==1&&closes==1&&yields==1);
 reset();tap(-1,220,0);tap(240,220,0);tap(60,-1,0);tap(60,240,0);back();run();clean(0,0,0);
 reset();tap(120,211,0);check(check_controls0);touch(215,180,true);touch(215,129,true);touch(215,129,false);events[count-1].input=(t5_app_input_t){.tapped=true,.touch_x=215,.touch_y=129};check(check_controls1);touch(215,180,true);touch(215,-300,true);touch(215,-300,false);check(check_controls2);check(check_continuous_controls);tap(120,214,0);back();run();clean(0,0,0);
 reset();start();check(check_live_only);for(unsigned i=0;i<600;i++)add(EVENT_INPUT,0,1);check(check_live_data);back();run();clean(1,603,1);
 reset();start();touch(180,160,true);touch(196,155,true);touch(196,155,false);check(check_cursor);tap(40,155,1);check(check_no_cursor);back();run();assert(!escaped&&opens==1&&closes==1);
 reset();start();tap(185,228,1);tap(145,155,0);check(check_cursor);tap(140,62,0);check(check_popup);tap(120,120,0);check(check_keyboard);tap(52,87,0);tap(160,87,0);tap(191,190,0);tap(200,154,0);tap(170,183,0);check(check_saved_label);back();run();clean(1,1,1);
 reset();start();tap(136,26,1);tap(190,62,1);tap(201,105,1);check(check_deleted);tap(192,211,1);check(check_undone);back();back();run();assert(!escaped&&closes==1);
 reset();check(direct_edit_saved);check(direct_unsaved);check(direct_all_settings);back();run();clean(0,0,0);assert(puts_count>20);
 reset();spectrum_preferences p=spectrum_preferences_default();p.gain_db=24;uint8_t bytes[32];spectrum_preferences_encode(&p,bytes);bytes[12]=1;spectrum_store_put16(bytes+30,spectrum_store_checksum(bytes));preload("spectrum_cfg",bytes);spectrum_label l={true,600,2,"Saved label"};spectrum_label_encode(&l,bytes);preload("spectrum_l0",bytes);l=(spectrum_label){0};spectrum_label_encode(&l,bytes);preload("spectrum_l1",bytes);check(check_restored);start();check(check_live_only);back();run();clean(1,2,1);assert(!puts_count&&cells[0].bytes[12]==1);
 reset();spectrum_preferences_encode(&p,bytes);bytes[12]=1;spectrum_store_put16(bytes+30,spectrum_store_checksum(bytes));preload("spectrum_cfg",bytes);deny_acquire=true;start();back();run();assert(!running&&!owned&&!started&&!opens&&!reads&&capture_error&&prefs.source==0&&prefs.gain_db==24&&!puts_count&&cells[0].bytes[12]==1);
 reset();memset(bytes,0xff,32);preload("spectrum_cfg",bytes);preload("spectrum_l0",bytes);check(check_bad_records);back();run();clean(0,0,0);
 reset();deny_store=true;back();run();assert(!store_live&&!store_releases&&store_message);
 const int sizes[][2]={{240,320},{320,240},{480,480},{1024,1024}};for(unsigned i=0;i<4;i++){reset();width=sizes[i][0];height=sizes[i][1];start();back();run();clean(1,1,1);}
 reset();width=239;run();assert(!polls&&!acquires&&!store_acquires);reset();height=1025;run();assert(!polls&&!acquires);reset();api.poll=NULL;run();assert(!polls&&!acquires);reset();rt.release=NULL;run();assert(!polls&&!acquires);reset();api.struct_size=4;run();assert(!polls&&!acquires);
 puts("Spectrum controller: live-only capture lifecycle/faults/retention, three tabs, continuous controls and clipped hit tests, cursor drag/dismiss, label keyboard/color/save/delete/undo, legacy migration, persistence/retry and bounds passed");return 0;
}
