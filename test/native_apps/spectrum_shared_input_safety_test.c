/* Production Spectrum controller + unchanged bounded client. Only provider,
 * presentation handoff and hardware callbacks are fixtures. No host rendering. */
#include "RiscRuntimeV1.h"
const risc_runtime_api_v1 *safety_runtime_get_api(uint32_t);
#define risc_runtime_get_api safety_runtime_get_api
#include "PortableTextInputClient.h"
#undef risc_runtime_get_api
#define PORTABLE_SPECTRUM_SHARED_TEXT_INPUT
#ifndef PORTABLE_CONTEXTS_CLIENT
/* Controller-only fixture has no Contexts grant or custody callbacks. */
#define PORTABLE_CONTEXTS_LIVE() true
#define PORTABLE_CONTEXTS_CHECK_LIVE() do{}while(0)
#define portable_contexts_callback(value) (value)
#endif
#define main original_spectrum_fixture_main
#include "audio_spectrum_test.c"
#undef main

static unsigned host_opens,host_polls,host_closes,host_acquires,host_releases;
static unsigned suspends,resumes,retains,host_yields,editing_steps,closing_steps;
static unsigned expected_page,host_state,host_flags,fail_stage;
static bool host_live,host_grant,adapter_suspended,adapter_retained,runtime_missing,deny_host;
static bool malformed_result,oversize_result,inject_retention;
static char host_text[72],initial_text[17];
static unsigned snapshot_reads,snapshot_frames,snapshot_polls,snapshot_puts;
static uint32_t snapshot_transforms;
static risc_text_entry_request_v1 copied_request;
static risc_text_entry_api_v1_home_reason host_api;
static void require_live(void){assert(!adapter_retained&&!runtime_missing);}
static bool stage(unsigned n){if(fail_stage!=n)return false;if(inject_retention)adapter_retained=true;else runtime_missing=true;return true;}
const risc_runtime_api_v1 *safety_runtime_get_api(uint32_t v){assert(v==1);return runtime_missing?NULL:&rt;}
int portable_text_adapter_suspend(void){require_live();suspends++;assert(host_grant&&!adapter_suspended);if(stage(2))return inject_retention?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;adapter_suspended=true;return 0;}
int portable_text_adapter_resume(void){require_live();resumes++;assert(!host_live&&!host_grant&&adapter_suspended);if(stage(6))return RISC_TEXT_ENTRY_RETAINED;adapter_suspended=false;return 0;}
int portable_text_adapter_attention(void){require_live();return 0;}
void portable_text_adapter_retain(void){adapter_retained=true;retains++;}
bool portable_text_adapter_retained(void){return adapter_retained;}
static int32_t host_open(void*c,const risc_text_entry_request_v1*q,uint64_t*s){
 (void)c;require_live();assert(host_grant&&adapter_suspended&&!host_live);host_opens++;
 assert(q->api_version==1&&q->struct_size==sizeof(*q)&&q->capacity==17);
 assert(q->reserved==RISC_TEXT_ENTRY_REQUEST_HOME_REASON);copied_request=*q;host_live=true;*s=7;
 stage(3);return inject_retention&&adapter_retained?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;
}
static int32_t host_poll(void*c,uint64_t s,risc_text_entry_state_v1*out){
 (void)c;require_live();assert(host_grant&&host_live&&adapter_suspended&&s==7);host_polls++;
 if(stage(4))return inject_retention?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;
 assert(!strcmp(editing.name,initial_text));
 if(editing_steps){editing_steps--;*out=(risc_text_entry_state_v1){.struct_size=sizeof(*out),.state=RISC_TEXT_ENTRY_EDITING,.revision=host_polls,.flags=RISC_TEXT_ENTRY_PRESENTING};strcpy(out->text,"in progress");return 0;}
 *out=(risc_text_entry_state_v1){.struct_size=sizeof(*out),.state=host_state,.revision=host_polls,.flags=host_flags};
 if(malformed_result)out->revision=0;
 strcpy(out->text,host_text);if(oversize_result)memset(out->text,'X',17);
 return 0;
}
static int32_t host_close(void*c,uint64_t s){
 (void)c;require_live();assert(host_grant&&host_live&&adapter_suspended&&s==7);host_closes++;
 assert(!strcmp(editing.name,initial_text));
 if(running&&host_closes>1)assert(reads>snapshot_reads+64+host_closes-1);
 if(stage(5))return inject_retention?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;
 if(closing_steps){closing_steps--;return RISC_TEXT_ENTRY_AGAIN;}
 host_live=false;return 0;
}
static bool safety_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 require_live();if(!strcmp(n,"rtc.clock"))return false;
 if(strcmp(n,RISC_TEXT_ENTRY_CAPABILITY))return acquire(n,v,i,g);
 assert(v==1&&!i&&!host_grant);host_acquires++;
 if(deny_host)return false;
 host_grant=true;g->api=&host_api;g->slot=4;g->generation=2;
 stage(1);return true;
}
static bool safety_release(risc_runtime_capability_v1*g){
 require_live();if(g->api!=&host_api)return release(g);
 assert(host_grant&&!host_live);host_releases++;
 if(fail_stage==7){if(inject_retention)adapter_retained=true;else return false;return true;}
 host_grant=false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static void safety_yield(uint32_t n){require_live();assert(n==(running?1u:10u));host_yields++;now+=n;}
static bool safety_launch(const char*n){require_live();assert(!host_live&&!host_grant&&!adapter_suspended);return launch(n);}
static void safety_present(bool full){require_live();assert(!text_active&&!host_live&&!adapter_suspended);present(full);}
static bool safety_poll(t5_app_input_t*out,uint32_t wait){require_live();assert(!text_active&&!host_live&&!adapter_suspended);return poll(out,wait);}
static bool safety_read(void*c,int16_t*p,size_t cap,size_t*n){require_live();return read_mic(c,p,cap,n);}
static void fresh_safety(void){
 reset();host_opens=host_polls=host_closes=host_acquires=host_releases=0;suspends=resumes=retains=host_yields=0;
 editing_steps=closing_steps=fail_stage=0;host_state=RISC_TEXT_ENTRY_ACCEPTED;host_flags=0;
 host_live=host_grant=adapter_suspended=adapter_retained=runtime_missing=deny_host=false;
 malformed_result=oversize_result=inject_retention=false;
 strcpy(host_text,"ABCDEFGHIJKLMNOP");strcpy(initial_text,"old draft");
 host_api=(risc_text_entry_api_v1_home_reason){.base={1,sizeof(host_api),NULL,host_open,host_poll,host_close},.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG,.home_reason_version=RISC_TEXT_ENTRY_HOME_REASON_VERSION};
 rt.acquire=safety_acquire;rt.release=safety_release;rt.yield_ms=safety_yield;rt.request_launch=safety_launch;api.present=safety_present;api.poll=safety_poll;mic.read=safety_read;
}
static void begin_on_page(unsigned p){
 expected_page=p;set_page(p);strcpy(editing.name,initial_text);
 if(p==PAGE_SHARED_EDIT)strcpy(shared_name,initial_text);
 snapshot_puts=puts_count;snapshot_frames=frames;snapshot_reads=reads;snapshot_polls=polls;snapshot_transforms=spectrum.transforms;
 keyboard_begin();
 if(text_active)assert(!portable_app_before_launch("springboard.elf"));
}
static void direct_setup(void){fresh_safety();app=&api;runtime=&rt;text_client=(portable_text_client){0};text_active=text_closing=text_home=false;shared_keyboard=false;event_initialized=false;running=false;}
static void assert_closed(unsigned p,const char*name){assert(!text_active&&!text_closing&&!text_client.retained&&!text_client.active&&!text_client.acquired&&!text_client.suspended);assert(!host_live&&!host_grant&&!adapter_suspended);assert(page==p&&!strcmp(editing.name,name)&&!back_exits);assert(host_opens==1&&host_closes>=1&&host_releases==1&&suspends==1&&resumes==1);assert(puts_count==snapshot_puts);assert(portable_app_before_launch("springboard.elf"));}
static void direct_drafts(void){
 const unsigned pages[]={PAGE_LABEL,PAGE_SIGNATURE_EDIT,PAGE_EVENT_LABEL,PAGE_SHARED_EDIT};
 const char*captions[]={"LABEL NAME","SIGNATURE NAME","EVENT NAME","SIGNAL NAME"};
 for(unsigned i=0;i<4;i++){
  direct_setup();begin_on_page(pages[i]);assert(text_active&&page==PAGE_KEYBOARD);assert(!strcmp(copied_request.label,captions[i])&&!strcmp(copied_request.text,initial_text));
  unsigned before=frames;draw();assert(frames==before);editing_steps=3;closing_steps=3;
  for(unsigned j=0;j<3;j++){text_step();assert(text_active&&!text_closing&&!strcmp(editing.name,initial_text));}
  for(unsigned j=0;j<3;j++){text_step();assert(text_active&&text_closing&&!strcmp(editing.name,initial_text)&&!resumes&&!host_releases);}
  text_step();assert_closed(pages[i],host_text);assert(strlen(editing.name)==16&&editing.name[16]==0);
  if(i==3)assert(!strcmp(shared_name,host_text));
  direct_setup();begin_on_page(pages[i]);host_state=RISC_TEXT_ENTRY_CANCELLED;text_step();assert_closed(pages[i],initial_text);
  direct_setup();begin_on_page(pages[i]);host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;text_step();assert_closed(pages[i],initial_text);assert(text_home);
 }
 direct_setup();deny_host=true;begin_on_page(PAGE_LABEL);assert(!text_active&&page==PAGE_LABEL&&!strcmp(editing.name,initial_text)&&!suspends&&!host_opens&&toast_message);
 deny_host=false;begin_on_page(PAGE_LABEL);text_step();assert_closed(PAGE_LABEL,host_text);
 direct_setup();host_api.base.struct_size=1;begin_on_page(PAGE_LABEL);assert(!text_active&&page==PAGE_LABEL&&!suspends&&!host_opens&&!host_grant&&toast_message);
}
static void direct_fail_closed(void){
 for(unsigned stage_id=1;stage_id<=7;stage_id++)for(unsigned retained_case=0;retained_case<2;retained_case++){
  /* Adapter suspend/resume return retention themselves; their internals are
   * covered by the production adapter suite, not this provider fixture. */
  if((stage_id==2||stage_id==6)&&!retained_case)continue;
  direct_setup();fail_stage=stage_id;inject_retention=retained_case!=0;begin_on_page(PAGE_LABEL);
  if(!text_client.retained)text_step();
  assert(text_client.retained&&adapter_retained);
  unsigned callbacks=host_polls+host_closes+host_releases+host_opens+suspends+resumes;
  text_step();draw();keyboard_begin();assert(callbacks==host_polls+host_closes+host_releases+host_opens+suspends+resumes);
  assert(!strcmp(editing.name,initial_text)&&puts_count==snapshot_puts);
  assert(!portable_audio_services_safe()&&!portable_audio_suspend()&&!portable_app_before_launch("springboard.elf"));
  bool toggle_requested=false,freeze_requested=false;
  assert(!tap_action(20,20,&toggle_requested,&freeze_requested)&&!toggle_requested&&!freeze_requested);
  assert(!request_root_exit());
 }
 for(unsigned bad=0;bad<2;bad++){direct_setup();begin_on_page(PAGE_LABEL);malformed_result=!bad;oversize_result=bad!=0;text_step();assert(text_client.retained&&!host_closes&&!resumes&&!strcmp(editing.name,initial_text));}
}
static void begin_loop(void){begin_on_page(PAGE_LABEL);}
static void loop_accepted(void){assert_closed(PAGE_LABEL,host_text);assert(running&&owned&&reads>=snapshot_reads+68&&spectrum.transforms>snapshot_transforms&&signature_audio.transforms>0);assert(frames<=snapshot_frames+1);assert(host_polls==65&&host_closes==5&&host_yields==68);set_page(PAGE_MAIN);}
static void prepare_shared_coalesced(void){set_page(PAGE_SHARED_EDIT);shared_focus=0;shared_selected=-1;strcpy(shared_name,initial_text);}
static void shared_coalesced_return(void){assert(!text_active&&!host_live&&!host_grant&&page==PAGE_SHARED_EDIT&&!launches);set_page(PAGE_MAIN);}
static unsigned blocking_kind;
static void begin_blocked_home(void){
 begin_on_page(PAGE_LABEL);
 if(blocking_kind==0)cfa_learning_kind=CF_ROOM;
 else if(blocking_kind==1)signature_goal=1;
 else{event_edit_slot=0;event_training.ready=true;}
}
static void check_blocked_home(void){
 assert(!text_active&&!host_live&&!host_grant&&!strcmp(editing.name,initial_text));
 assert(page==(blocking_kind==0?PAGE_SHARED_CAPTURE:blocking_kind==1?PAGE_SIGNATURES:PAGE_EVENT_CAPTURE));
 assert(!launches&&!text_home);cfa_learning_kind=0;signature_goal=0;event_training.ready=false;set_page(PAGE_MAIN);
}
static void loop_progress(void){
 fresh_safety();check(prepare_shared_coalesced);tap(20,20,0);events[count-1].input.buttons=T5_APP_BUTTON_CONFIRM;check(shared_coalesced_return);back();run();assert(host_opens==1&&host_releases==1&&!launches&&!store_live);
 fresh_safety();editing_steps=64;closing_steps=4;start();check(begin_loop);check(loop_accepted);back();run();
 assert(!live&&!grant_live&&!store_live&&opens==1&&closes==1&&host_releases==1);
 fresh_safety();host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;start();check(begin_loop);run();assert(!live&&!grant_live&&!store_live&&!text_active&&index_event==2&&opens==1&&closes==1);
 for(blocking_kind=0;blocking_kind<3;blocking_kind++){
  fresh_safety();host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;check(begin_blocked_home);check(check_blocked_home);back();run();assert(host_releases==1&&!store_live);
 }
 for(unsigned fail=1;fail<=7;fail++){
  fresh_safety();fail_stage=fail;inject_retention=true;start();check(begin_loop);run();assert(text_client.retained&&adapter_retained&&live&&grant_live&&store_live&&!closes&&!releases&&!store_releases);
 }
}
static void geometry_same(void){
 shared_input_layout();unsigned n=shared_targets;unsigned char copy[sizeof(shared_bounds)];memcpy(copy,shared_bounds,sizeof(copy));
 draw_shared();assert(n==shared_targets&&!memcmp(copy,shared_bounds,n*sizeof(shared_bounds[0])));
}
static void rapid_navigation(void){
 direct_setup();memset(&cfa_bank,0,sizeof(cfa_bank));for(unsigned i=0;i<CF_PROFILES;i++){cfa_bank.profiles[i].kind=i%2?CF_EVENT:CF_ROOM;snprintf(cfa_bank.profiles[i].name,17,"Signal %u",i);}
 for(unsigned kind=CF_ROOM;kind<=CF_EVENT;kind++){
  shared_kind=kind;for(unsigned offset=0;offset<10;offset++){page=PAGE_SHARED;shared_offset=offset;geometry_same();}
  page=PAGE_SHARED_EDIT;shared_selected=-1;geometry_same();shared_selected=0;geometry_same();page=PAGE_SHARED_CAPTURE;geometry_same();
 }
 memset(&cfa_bank,0,sizeof(cfa_bank));shared_kind=CF_EVENT;page=PAGE_SHARED;shared_offset=shared_focus=shared_targets=0;
 /* No draw occurs through new item -> name. Stale zero targets must not trap. */
 shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus==1);shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus==2);shared_buttons(T5_APP_BUTTON_CONFIRM);assert(page==PAGE_SHARED_EDIT);
 shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus==0);initial_text[0]=0;shared_buttons(T5_APP_BUTTON_CONFIRM);assert(text_active&&page==PAGE_KEYBOARD);
 host_state=RISC_TEXT_ENTRY_CANCELLED;text_step();assert(page==PAGE_SHARED_EDIT);
 for(unsigned i=0;i<1000;i++){shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus<shared_targets);shared_buttons(T5_APP_BUTTON_UP);assert(shared_focus<shared_targets);}
}
int main(void){direct_drafts();direct_fail_closed();loop_progress();rapid_navigation();puts("Spectrum shared-input safety passed: four draft routes, 16-byte capacity, copied acceptance only after clean close, Back/Home and pending-work exit guards, coalesced-input custody, missing/malformed host, malformed/oversized results, retained/runtime-loss callbacks, capture/analysis continuity, no presenter overlap, and redraw-independent shared targets.");return 0;}
