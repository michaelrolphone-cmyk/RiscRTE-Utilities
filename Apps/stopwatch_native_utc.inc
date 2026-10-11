/* Separate native UTC controller. Legacy Watch/raw controller is untouched. */
#include "T5AppApi.h"
#include "utility_frame.h"
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "PortableRealtimeClient.h"
#include "PortableNativeCustody.h"
#if !defined(PORTABLE_NATIVE_TIME_TOOLBAR) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE) || !defined(PORTABLE_APP_LAUNCH_GUARD) || !defined(ALARM_SERVICE_TAGGED_V2)
#error Native Stopwatch requires native toolbar, custody fence, launch guard and alarm API2
#endif
#include "stopwatch_native_utc_core.h"
#include "daily_draw.h"
#ifdef PORTABLE_NOVA_UI
#include "PortableNovaUi.h"
#endif
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#include <stddef.h>
#include <string.h>
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_key_value_v1 *storage;
static risc_runtime_capability_v1 store_grant;
static portable_realtime_client native_reader;
static bool save_uncertain;
static sw_record pending_record;
static sw_clock clock_state;
static bool loaded,reset_armed,pending_pause,store_acquired;
static const char *status;
static bool halted(void){return portable_adapter_retained();}
static int native_guard(void *unused){(void)unused;return halted()?PORTABLE_REALTIME_GUARD_RETAINED:PORTABLE_REALTIME_GUARD_SAFE;}
static bool native_halted(int result){
 if(result==PORTABLE_REALTIME_CONTEXT||result==PORTABLE_REALTIME_UNCERTAIN||result==PORTABLE_REALTIME_RETAINED)portable_adapter_retain();
 return halted();
}
static bool utc_now(uint32_t*out) {
 if(halted())return false;
 int result=portable_realtime_open(&native_reader,runtime,PORTABLE_REALTIME_READER,PORTABLE_REALTIME_TIMER_ONLY,native_guard,NULL);
 if(native_halted(result)||result!=PORTABLE_REALTIME_OK)return false;
 risc_realtime_snapshot_v1 sample;
 result=portable_realtime_read(&native_reader,&sample);
 if(native_halted(result))return false;
 int closed=portable_realtime_close(&native_reader);
 if(native_halted(closed)||closed!=PORTABLE_REALTIME_OK||result!=PORTABLE_REALTIME_OK||sample.epoch_seconds<INT64_C(946684800))return false;
 *out=(uint32_t)(sample.epoch_seconds-INT64_C(946684800));return true;
}
static bool kv_halted(int32_t result){
 switch(result){case RISC_KEY_VALUE_OK:case RISC_KEY_VALUE_NOT_FOUND:case RISC_KEY_VALUE_BUFFER_SMALL:case RISC_KEY_VALUE_INVALID:case RISC_KEY_VALUE_IO:break;default:portable_adapter_retain();}
 return halted();
}
__attribute__((visibility("hidden"))) bool portable_app_before_launch(const char *destination){
 (void)destination;
 if(save_uncertain||pending_pause){status="SAVE UNCONFIRMED - RETRY";return false;}
 return !halted();
}
static bool read_record(sw_record*out) {
 uint8_t bytes[SW_RECORD_SIZE];uint32_t n=0;
 if(halted())return false;
 int32_t result=storage->get(storage->context,"stopwatch_utc",bytes,sizeof(bytes),&n);
 if(kv_halted(result))return false;
 if(result==RISC_KEY_VALUE_NOT_FOUND){*out=(sw_record){0};return true;}
 if(result!=RISC_KEY_VALUE_OK){status="STORAGE READ FAILED";return false;}
 if(!sw_decode(out,bytes,n)){status="SAVED DATA INVALID";return false;}
 return true;
}
static void restore(void) {
 if(halted())return;
 sw_record saved;pending_pause=false;loaded=read_record(&saved);
 if(!loaded){clock_state.running=false;return;}
 uint32_t seconds=0;bool ok=utc_now(&seconds);
 if(halted())return;
 sw_restore(&clock_state,&saved,ok,seconds,app->millis());
 if(clock_state.clock_changed)status=ok?"UTC MOVED BACK - RESET":"UTC UNAVAILABLE - RETRY";
 else if(clock_state.limit)status="99 HOUR LIMIT - RESET";
 else status=clock_state.approximate?"RECOVERED - APPROXIMATE":"";
}
static bool save(sw_record record) {
 if(halted())return false;
 pending_record=record;save_uncertain=true;
 uint8_t bytes[SW_RECORD_SIZE],verify[SW_RECORD_SIZE];uint32_t used=0;sw_encode(&record,bytes);
 int32_t put=storage->put(storage->context,"stopwatch_utc",bytes,sizeof(bytes));
 if(kv_halted(put))return false;
 int32_t get=storage->get(storage->context,"stopwatch_utc",verify,sizeof(verify),&used);
 if(kv_halted(get))return false;
 if(get!=RISC_KEY_VALUE_OK||used!=sizeof(bytes)||memcmp(bytes,verify,sizeof(bytes))) {
  clock_state.running=false;pending_pause=true;status="SAVE UNCONFIRMED - RETRY";return false;
 }
 clock_state.saved=record;loaded=true;save_uncertain=false;return true;
}
static void retry_save(void){
 sw_record record=pending_record;
 if(!save(record)||halted())return;
 uint32_t seconds=0;bool ok=utc_now(&seconds);
 if(halted())return;
 sw_restore(&clock_state,&record,ok,seconds,app->millis());pending_pause=false;
 status=clock_state.clock_changed?"UTC UNAVAILABLE OR CHANGED":clock_state.running?"RUNNING - APPROXIMATE":"PAUSED";
}
static void toggle(void) {
 if(halted())return;
 reset_armed=false;
 if(save_uncertain){retry_save();return;}
 if((!loaded||clock_state.clock_changed)&&!pending_pause){status="RETRY OR RESET REQUIRED";return;}
 uint32_t now=app->millis();sw_tick(&clock_state,now);
 if(clock_state.running||pending_pause) {
  sw_record record={clock_state.elapsed_ms,0,false,clock_state.approximate};
  if(save(record)){clock_state.running=false;clock_state.clock_changed=false;pending_pause=false;status=record.approximate?"PAUSED - APPROXIMATE":"PAUSED";}
 } else {
  uint32_t seconds=0;
  if(clock_state.elapsed_ms==SW_MAX_MS){status="99 HOUR LIMIT - RESET";return;}
  if(!utc_now(&seconds)){status="UTC UNAVAILABLE - RETRY";return;}
  uint32_t started=app->millis();
  sw_record record={clock_state.elapsed_ms,seconds,true,clock_state.approximate};
  if(save(record)) {
   clock_state.running=true;clock_state.last_ms=clock_state.ms_anchor=started;clock_state.rtc_anchor=seconds;
   status=record.approximate?"RUNNING - APPROXIMATE":"RUNNING";
  }
 }
}
static void reset(void) {
 if(halted())return;
 if(save_uncertain){status="SAVE UNCONFIRMED - RETRY";return;}
 if(!reset_armed){reset_armed=true;status="TAP RESET AGAIN TO CLEAR";return;}
 reset_armed=false;
 sw_record record={0};
 if(save(record)){pending_pause=false;sw_restore(&clock_state,&record,false,0,app->millis());status="RESET";}
}
#ifdef PORTABLE_PAPER_UTILITIES
#include "stopwatch_paper.inc"
#endif
static void draw(void) {
 if(!utility_frame_begin(app))return;
 if(halted())return;
#ifdef PORTABLE_PAPER_UTILITIES
 if(utility_paper){stopwatch_paper_draw();return;}
#endif
#ifdef PORTABLE_NOVA_UI
 portable_nova_begin();portable_nova_header("STOPWATCH");
 char value[12],large[9];sw_format(clock_state.elapsed_ms,value);
#ifdef PORTABLE_UNPADDED_HOURS
 const char *fraction=value;while(*fraction&&*fraction!='.')++fraction;
 size_t whole=(size_t)(fraction-value);memcpy(large,value,whole);large[whole]=0;
#else
 const char *fraction=value+8;memcpy(large,value,8);large[8]=0;
#endif
 portable_nova_center(4,16,68,208,large,NOVA_CYAN);
 portable_nova_center(0,16,111,208,fraction,NOVA_TEXT);
 portable_nova_center(1,16,135,208,clock_state.running?"Running":loaded?"Paused":"Storage error",NOVA_CAP);
 if(status && strcmp(status,"RUNNING") && strcmp(status,"PAUSED"))portable_nova_center(2,16,157,208,status,NOVA_CYAN);
 portable_nova_button(16,180,64,48,pending_pause?"Save":clock_state.running?"Pause":"Start",false);
 portable_nova_button(88,180,64,48,reset_armed?"Confirm":"Reset",reset_armed);
 portable_nova_button(160,180,64,48,"Retry",false);app->present(false);
#else
 int w=app->screen_width(),h=app->screen_height();
 app->clear();app->draw_text(8,16,"BACK");app->draw_label(52,16,w-104,"STOPWATCH");
 char value[12];sw_format(clock_state.elapsed_ms,value);
 int scale=w>=220?3:2;
 int size=daily_draw_width(value,scale);
 daily_draw_text(app,(w-size)/2,64,value,scale);
 app->draw_label(4,105,w-8,clock_state.running?"RUNNING":loaded?"PAUSED":"STORAGE ERROR");
 app->draw_label(4,124,w-8,status?status:"");
 app->fill_rect(8,h-84,w/2-12,40,true);app->fill_rect(w/2+4,h-84,w/2-12,40,true);
 app->fill_rect(10,h-82,w/2-16,36,false);app->fill_rect(w/2+6,h-82,w/2-16,36,false);
 app->draw_label(10,h-67,w/2-16,pending_pause?"SAVE PAUSE":clock_state.running?"PAUSE":"START");
 app->draw_label(w/2+6,h-67,w/2-16,reset_armed?"CONFIRM RESET":"RESET");
 app->draw_label(8,h-20,w-16,"RETRY STORAGE");app->present(false);
#endif
}
static bool open_state(void) {
 runtime=risc_runtime_get_api(1);
 if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release)return false;
 store_acquired=false;store_grant=(risc_runtime_capability_v1){.struct_size=sizeof(store_grant)};
 bool ok=runtime->acquire("storage.key-value",1,2,&store_grant);
 if(halted())return false;
 if(!ok||store_grant.struct_size!=sizeof(store_grant)||!store_grant.api||!store_grant.slot||!store_grant.generation){portable_adapter_retain();return false;}
 store_acquired=true;storage=store_grant.api;
 return storage->api_version==1&&storage->struct_size>=sizeof(*storage)&&storage->get&&storage->put;
}
static void close_state(void) {
 if(halted()||!runtime)return;
 if(store_acquired){risc_runtime_capability_v1 released=store_grant;
  if(!runtime->release(&released)||released.struct_size!=sizeof(released)||released.api||released.slot||released.generation){portable_adapter_retain();return;}
  store_grant=released;store_acquired=false;
 }
 storage=NULL;runtime=NULL;
}
void app_main(void) {
 utility_frame_reset();
 app=t5_app_get_api(1);status="";loaded=reset_armed=pending_pause=save_uncertain=false;clock_state=(sw_clock){0};
 if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label)||!app->poll||!app->millis||!app->screen_width||!app->screen_height||!app->clear||!app->draw_text||!app->draw_label||!app->fill_rect||!app->present)return;
 if(app->screen_width()<160||app->screen_width()>1024||app->screen_height()<240||app->screen_height()>1024)return;
#ifdef PORTABLE_PAPER_UTILITIES
 up_open(app);
#endif
 if(!open_state()){if(halted())return;status="UTC OR STORAGE UNAVAILABLE";draw();
  for(;;){if(utility_frame_pending)draw();t5_app_input_t input={0};if(!app->poll(&input,50)) {
#ifdef PORTABLE_ALARM_CLIENT
   if(portable_app_sleep_retained())return;
#endif
   break;
  }
#ifdef PORTABLE_PAPER_UTILITIES
   if(input.exit_requested&&!(input.buttons&T5_APP_BUTTON_BACK))break;
   if(utility_paper){up_input(&input);if(input.tapped&&up_hit(input.touch_x,input.touch_y,32,688,416,88))input.buttons|=T5_APP_BUTTON_BACK;}
   if(input.buttons&T5_APP_BUTTON_BACK){if(portable_app_before_launch("springboard.elf")&&up_return())break;}
#else
   if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK))break;
#endif
  }
  close_state();return;
 }
 restore();if(halted())return;draw();if(halted())return;uint32_t rendered=app->millis(),checked=rendered;
 for(;;) {
  t5_app_input_t input={0};if(!app->poll(&input,20)) {
#ifdef PORTABLE_ALARM_CLIENT
   if(portable_app_sleep_retained())return;
#endif
   break;
  }
#ifdef PORTABLE_PAPER_UTILITIES
  if(input.exit_requested&&!(input.buttons&T5_APP_BUTTON_BACK))break;
  if(utility_paper){up_input(&input);if(input.tapped&&up_hit(input.touch_x,input.touch_y,32,688,416,88))input.buttons|=T5_APP_BUTTON_BACK;}
  if(input.buttons&T5_APP_BUTTON_BACK){if(portable_app_before_launch("springboard.elf")&&up_return())break;status="Return unavailable - retry";draw();continue;}
#else
  if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK))break;
#endif
  uint32_t now=app->millis();bool was_running=clock_state.running;sw_tick(&clock_state,now);bool dirty=false;
  if(was_running&&!clock_state.running){status="99 HOUR LIMIT - RESET";dirty=true;}
  if(clock_state.running&&(uint32_t)(now-checked)>=1000) {
   checked=now;uint32_t seconds=0;bool ok=utc_now(&seconds);
   if(halted())return;
   if(!sw_clock_consistent(&clock_state,ok,seconds,now)) {
    clock_state.running=false;clock_state.clock_changed=true;pending_pause=true;
    status=ok?"UTC CHANGED - SAVE PAUSE":"UTC LOST - SAVE PAUSE";dirty=true;
   }
  }
  if(input.buttons&T5_APP_BUTTON_CONFIRM){toggle();dirty=true;}
  if(input.tapped) {
#ifdef PORTABLE_NOVA_UI
   int x=input.touch_x,y=input.touch_y;
   if(
#ifdef PORTABLE_PAPER_UTILITIES
      utility_paper?up_hit(x,y,32,584,132,88):
#endif
      portable_nova_hit(x,y,16,180,64,48)){toggle();dirty=true;}
   else if(
#ifdef PORTABLE_PAPER_UTILITIES
      utility_paper?up_hit(x,y,174,584,132,88):
#endif
      portable_nova_hit(x,y,88,180,64,48)){reset();dirty=true;}
   else if(
#ifdef PORTABLE_PAPER_UTILITIES
      utility_paper?up_hit(x,y,316,584,132,88):
#endif
      portable_nova_hit(x,y,160,180,64,48)) {
#else
   int x=input.touch_x,y=input.touch_y,w=app->screen_width(),h=app->screen_height();
   if(x>=8&&x<w-8&&y>=h-84&&y<h-44){if(x<w/2-4)toggle();else if(x>=w/2+4)reset();dirty=true;}
   else if(x>=8&&x<w-8&&y>=h-32&&y<h){
#endif
    if(save_uncertain)retry_save();
    else if(pending_pause)toggle();
    else if(clock_state.running)status="RUNNING - NO RETRY NEEDED";
    else restore();
    reset_armed=false;dirty=true;
   }
  }
  if(halted())return;
  if(dirty||utility_frame_pending||(clock_state.running&&(uint32_t)(now-rendered)>=
#ifdef PORTABLE_PAPER_UTILITIES
    (utility_paper?1000u:50u)
#else
    50u
#endif
    )){draw();if(!utility_frame_pending)rendered=app->millis();}
 }
 close_state();
}
