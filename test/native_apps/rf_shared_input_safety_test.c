/* Production RF controller and bounded shared-input client. Only Runtime,
 * receiver, storage, input, and presentation handoff callbacks are fixtures.
 * This is safety/behavior coverage, not a physical-device or renderer test. */
#define PORTABLE_RF_SHARED_TEXT_INPUT
#define PORTABLE_APP_HOME_GUARD
#define PORTABLE_RETURN_APP "springboard.elf"
#ifdef RF_TEST_HOME_APP
#define PORTABLE_HOME_APP RF_TEST_HOME_APP
#else
#define PORTABLE_HOME_APP "springboard.elf"
#endif
#include "../../Apps/waterfall.c"
#include <assert.h>
#include <stdlib.h>
#include <setjmp.h>

static t5_app_api_v1 api;
static risc_runtime_api_v1 rt;
static risc_text_entry_api_v1_home_reason host_api;
static unsigned host_opens,host_polls,host_closes,host_acquires,host_releases;
static unsigned suspends,resumes,retains,host_yields,editing_steps,closing_steps,poll_again;
static int open_refusal;
static unsigned host_state,host_flags,fail_stage,attention;
static bool host_live,host_grant,adapter_suspended,adapter_retained,runtime_missing,deny_host;
static bool malformed_result,oversize_result,inject_retention,old_host,fail_put,fail_launch;
static const char *expected_launch;
static char host_text[72],initial_text[17];
static risc_text_entry_request_v1 copied_request;
static uint32_t tick;
static unsigned frames,polls,captures,radio_suspends,launches,puts_count,store_gets;
static unsigned data_acquires,data_releases,data_stats,data_writes,io_calls;
static unsigned snapshot_frames,snapshot_polls,snapshot_captures,snapshot_puts,snapshot_data_writes,last_close_captures,snapshot_data_acquires,snapshot_store_gets;
static uint32_t snapshot_transforms;
static bool radio_grant,store_live,policy_live,data_live,back_exits,deny_data;
static bool radio_failure,radio_custody,storage_release_failure,storage_custody;
static jmp_buf custody_jump;
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && defined(RF_TEST_HOME_APP)
#define HOME_LAUNCHES 0u
#else
#define HOME_LAUNCHES 1u
#endif
static struct {char key[16];uint8_t bytes[2048];uint32_t size;} cells[24];
static struct {char name[32];uint8_t bytes[72000];uint32_t size;uint64_t revision;} files[4];
static void require_live(void){assert(!adapter_retained&&!runtime_missing&&!radio_custody&&!storage_custody);io_calls++;}
static bool stage(unsigned n){if(fail_stage!=n)return false;if(inject_retention)adapter_retained=true;else runtime_missing=true;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return runtime_missing?NULL:&rt;}
const t5_app_api_v1 *t5_app_get_api(uint32_t v){assert(v==1);return &api;}
int portable_text_adapter_suspend(void){require_live();suspends++;assert(host_grant&&!adapter_suspended);if(stage(2))return RISC_TEXT_ENTRY_RETAINED;adapter_suspended=true;return 0;}
int portable_text_adapter_resume(void){require_live();resumes++;assert(!host_live&&!host_grant&&adapter_suspended);if(stage(6))return RISC_TEXT_ENTRY_RETAINED;adapter_suspended=false;return 0;}
int portable_text_adapter_attention(void){require_live();return (int)attention;}
void portable_text_adapter_retain(void){adapter_retained=true;retains++;}
bool portable_text_adapter_retained(void){return adapter_retained;}
static int32_t host_open(void*c,const risc_text_entry_request_v1*q,uint64_t*s){
 (void)c;require_live();assert(host_grant&&adapter_suspended&&!host_live&&!data_live);host_opens++;
 assert(q->api_version==1&&q->struct_size==sizeof(*q)&&q->capacity==17);
 assert(q->reserved==(old_host?0u:RISC_TEXT_ENTRY_REQUEST_HOME_REASON));copied_request=*q;if(open_refusal)return open_refusal;host_live=true;*s=7;
 stage(3);return inject_retention&&adapter_retained?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;
}
static int32_t host_poll(void*c,uint64_t s,risc_text_entry_state_v1*out){
 (void)c;require_live();assert(host_grant&&host_live&&adapter_suspended&&s==7&&!data_live);host_polls++;tick+=40;
 if(stage(4))return inject_retention?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;
 assert(!strcmp(editing.name,initial_text));
 if(poll_again){poll_again--;return RISC_TEXT_ENTRY_AGAIN;}
 if(editing_steps){editing_steps--;*out=(risc_text_entry_state_v1){.struct_size=sizeof(*out),.state=RISC_TEXT_ENTRY_EDITING,.revision=host_polls,.flags=RISC_TEXT_ENTRY_PRESENTING};strcpy(out->text,"in progress");return 0;}
 *out=(risc_text_entry_state_v1){.struct_size=sizeof(*out),.state=host_state,.revision=host_polls,.flags=host_flags};
 if(malformed_result)out->revision=0;
 strcpy(out->text,host_text);if(oversize_result)memset(out->text,'X',17);
 return 0;
}
static int32_t host_close(void*c,uint64_t s){
 (void)c;require_live();assert(host_grant&&host_live&&adapter_suspended&&s==7);host_closes++;
 assert(!strcmp(editing.name,initial_text));
 if(running&&host_closes>1)assert(captures>last_close_captures);
 last_close_captures=captures;tick+=120;
 if(stage(5))return inject_retention?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_OK;
 if(closing_steps){closing_steps--;return RISC_TEXT_ENTRY_AGAIN;}
 host_live=false;return 0;
}
static int32_t get_value(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){
 require_live();*n=0;
 if(c==(void*)1){assert(policy_live&&!strcmp(k,"quick_radio")&&cap==4);uint8_t bytes[]={0x51,1,3,3^0xa5};memcpy(out,bytes,4);*n=4;return 0;}
 assert(store_live);store_gets++;for(unsigned i=0;i<24;i++)if(!strcmp(cells[i].key,k)){*n=cells[i].size;if(*n>cap)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,cells[i].bytes,*n);return 0;}return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t put_value(void*c,const char*k,const void*b,uint32_t n){
 (void)c;require_live();assert(store_live&&strlen(k)<16&&n<=2048);puts_count++;if(fail_put)return RISC_KEY_VALUE_IO;
 unsigned i=0;for(;i<24&&cells[i].key[0]&&strcmp(cells[i].key,k);i++);assert(i<24);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;
}
static const risc_key_value_v1 kv={2,sizeof(kv),NULL,get_value,put_value},policy={1,sizeof(policy),(void*)1,get_value,put_value};
static unsigned file_index(const char*n){for(unsigned i=0;i<4;i++)if(!strcmp(files[i].name,n))return i;for(unsigned i=0;i<4;i++)if(!files[i].name[0]){assert(strlen(n)<32);strcpy(files[i].name,n);return i;}abort();}
static int32_t stat_data(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;require_live();assert(data_live);data_stats++;unsigned i=file_index(n);*s=files[i].size;*r=*s?files[i].revision:0;return *s?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t read_data(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*s,uint64_t*r){(void)c;require_live();assert(data_live);unsigned i=file_index(n);*s=*r=0;if(expected!=files[i].revision)return RISC_APP_DATA_STALE;assert(cap>=files[i].size);*s=files[i].size;*r=expected;memcpy(b,files[i].bytes,*s);return 0;}
static int32_t replace_data(void*c,const char*n,uint64_t expected,const void*b,uint32_t z){(void)c;require_live();assert(data_live&&!owned&&!running);unsigned i=file_index(n);assert(z<=sizeof(files[i].bytes)&&expected==(files[i].size?files[i].revision:0));memcpy(files[i].bytes,b,z);files[i].size=z;files[i].revision++;data_writes++;return 0;}
static const risc_app_data_v1 data_api={1,sizeof(data_api),NULL,stat_data,read_data,replace_data};
static bool clean_radio(void*c){(void)c;assert(!adapter_retained&&!runtime_missing&&!storage_custody&&radio_grant);io_calls++;radio_suspends++;radio_custody=radio_failure;return !radio_failure;}
static int legacy_burst(void*c,uint32_t*p,uint32_t n){(void)c;require_live();assert(radio_grant&&n==256);for(unsigned i=0;i<n;i++)p[i]=(i%4==0)?256u:(i%4==1)?(256u<<10):(i%4==2)?768u:(768u<<10);captures++;return 0;}
static bool radio_caps(void*c,risc_radio_iq_capabilities_v1*out){(void)c;require_live();assert(radio_grant);*out=(risc_radio_iq_capabilities_v1){.struct_size=sizeof(*out),.min_pairs=1,.max_pairs=8192};return true;}
static int configured_burst(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){
 int rc=legacy_burst(c,p,n);*f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=RISC_RADIO_IQ_FLAG_COHERENT_BURST,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=n,.sample_format=RISC_RADIO_IQ_FORMAT_S10_I0_Q10,.component_bits=10,.component_full_scale=512};return rc;
}
static const risc_radio_iq_extended_api_v1 radio={.base={.base={1,sizeof(radio),NULL,legacy_burst,clean_radio}},.capabilities=radio_caps,.capture_configured=configured_burst};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 require_live();assert(g->struct_size==sizeof(*g));
 if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)){assert(v==1&&!i&&!host_grant);host_acquires++;if(deny_host)return false;host_grant=true;g->api=&host_api;g->slot=4;g->generation=2;stage(1);return true;}
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){if(v==1){assert(i==1&&!policy_live);policy_live=true;g->api=&policy;}else{assert(v==2&&i==RF_STORAGE_INSTANCE&&!store_live);store_live=true;g->api=&kv;}}
 else if(!strcmp(n,RISC_APP_DATA_CAPABILITY)){assert(v==1&&i==RF_APP_DATA_INSTANCE&&!data_live);if(deny_data)return false;data_acquires++;data_live=true;g->api=&data_api;}
 else if(!strcmp(n,"radio.iq")){assert(v==1&&!i&&!radio_grant);radio_grant=true;g->api=&radio;}
 else return false;
 g->slot=1;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){
 require_live();assert(g->api);if(home_exit_requested)assert(!signature_goal&&!event_armed&&!event_training.ready&&!cfa_learning_kind&&!editing.name[0]&&!shared_name[0]);
 if(g->api==&host_api){assert(host_grant&&!host_live);host_releases++;if(fail_stage==7){if(inject_retention)adapter_retained=true;else return false;return true;}host_grant=false;}
 else if(g->api==&kv){assert(store_live);if(storage_release_failure){storage_custody=true;return false;}store_live=false;}
 else if(g->api==&policy){assert(policy_live);policy_live=false;}
 else if(g->api==&data_api){assert(data_live);data_live=false;data_releases++;}
 else{assert(g->api==&radio&&radio_grant&&!running&&!owned);radio_grant=false;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static int32_t screen_size(void){require_live();return 240;}
static uint32_t millis(void){require_live();return tick;}
static void clear(void){require_live();assert(!text_active&&!host_live&&!adapter_suspended);}
static void rect(int32_t x,int32_t y,int32_t w,int32_t h,bool black){(void)black;require_live();assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=240&&y+h<=240&&!text_active&&!adapter_suspended);}
static void present(bool full){require_live();assert(!full&&!text_active&&!host_live&&!adapter_suspended);frames++;}
static void set_back(bool exits){require_live();back_exits=exits;}
static void yield_ms(uint32_t n){if(storage_custody){assert(n==50);longjmp(custody_jump,1);}require_live();assert(n==(running?1u:10u));host_yields++;tick+=n;}
static bool launch(const char*n){require_live();assert(!host_live&&!host_grant&&!adapter_suspended&&!running&&!owned&&!strcmp(n,expected_launch));launches++;return !fail_launch;}
typedef struct {t5_app_input_t input;void(*check)(void);bool end;} event;
static event events[128];static unsigned count,index_event;
static bool poll(t5_app_input_t*out,uint32_t wait){require_live();assert(!text_active&&!host_live&&!adapter_suspended&&wait==(running?1u:30u)&&index_event<count);polls++;tick+=wait;event e=events[index_event++];if(e.check)e.check();*out=e.input;return !e.end;}
static void check(void(*fn)(void)){assert(count<128);events[count++]=(event){.check=fn};}
static void back(void){assert(count<128);events[count++]=(event){.input={.buttons=T5_APP_BUTTON_BACK}};}
static void fresh(void){
 host_opens=host_polls=host_closes=host_acquires=host_releases=suspends=resumes=retains=host_yields=0;
 editing_steps=closing_steps=fail_stage=attention=poll_again=0;open_refusal=0;host_state=RISC_TEXT_ENTRY_ACCEPTED;host_flags=0;
 host_live=host_grant=adapter_suspended=adapter_retained=runtime_missing=deny_host=false;
 malformed_result=oversize_result=inject_retention=old_host=fail_put=fail_launch=false;expected_launch="springboard.elf";
 strcpy(host_text,"ABCDEFGHIJKLMNOP");strcpy(initial_text,"old draft");
 tick=1000;frames=polls=captures=radio_suspends=launches=puts_count=store_gets=0;data_acquires=data_releases=data_stats=data_writes=io_calls=last_close_captures=0;
 radio_grant=store_live=policy_live=data_live=back_exits=deny_data=false;radio_failure=radio_custody=storage_release_failure=storage_custody=false;count=index_event=0;memset(cells,0,sizeof(cells));memset(files,0,sizeof(files));
 host_api=(risc_text_entry_api_v1_home_reason){.base={1,sizeof(host_api),NULL,host_open,host_poll,host_close},.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG,.home_reason_version=RISC_TEXT_ENTRY_HOME_REASON_VERSION};
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.yield_ms=yield_ms,.request_launch=launch};
 api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(api),.screen_width=screen_size,.screen_height=screen_size,.clear=clear,.fill_rect=rect,.present=present,.poll=poll,.millis=millis,.set_back_exits_app=set_back};
}
static void direct_setup(void){
 fresh();app=&api;runtime=waterfall_runtime=&rt;waterfall_api=&api;rf_bulk=&rf_resident_bulk;memset(rf_bulk,0,sizeof(*rf_bulk));
 storage=NULL;waterfall_radio=NULL;preferences=NULL;radio_acquired=preferences_acquired=store_acquired=false;
 waterfall_uncertain=uncertain=running=owned=waterfall_enabled=capture_requested=false;paper=NULL;ui_height=240;
 text_client=(portable_text_client){0};text_state=(risc_text_entry_state_v1){0};text_active=text_closing=text_home=home_exit_requested=home_refusal_pending=shared_keyboard=false;
 pending_save=load_errors=0;edit_slot=undo_slot=-1;page=PAGE_MAIN;toast_message=NULL;
 restore();signature_restore();event_restore();cfa_restore();configure_dsp();build_palette();set_page(PAGE_MAIN);assert(!data_live&&data_acquires==data_releases);
}
static void begin_on_page(unsigned p){
 set_page(p);strcpy(editing.name,initial_text);event_keyboard=p==PAGE_EVENT_LABEL;signature_keyboard=p==PAGE_SIGNATURE_EDIT;
 if(p==PAGE_SHARED_EDIT)strcpy(shared_name,initial_text);
 snapshot_puts=puts_count;snapshot_data_writes=data_writes;snapshot_data_acquires=data_acquires;snapshot_store_gets=store_gets;snapshot_frames=frames;snapshot_polls=polls;snapshot_captures=captures;snapshot_transforms=spectrum.transforms;
 keyboard_begin();if(text_active)assert(!portable_app_before_launch("springboard.elf"));
}
static void assert_closed(unsigned p,const char*name){
 assert(!text_active&&!text_closing&&!text_client.retained&&!text_client.active&&!text_client.acquired&&!text_client.suspended);
 assert(!host_live&&!host_grant&&!adapter_suspended&&!data_live&&data_acquires==data_releases);
 assert(page==p&&!strcmp(editing.name,name)&&!back_exits);
 assert(host_opens==1&&host_closes>=1&&host_releases==1&&suspends==1&&resumes==1);
 assert(puts_count==snapshot_puts&&data_writes==snapshot_data_writes);
}
static void direct_drafts(void){
 const unsigned pages[]={PAGE_LABEL,PAGE_SIGNATURE_EDIT,PAGE_EVENT_LABEL,PAGE_SHARED_EDIT};
 const char*captions[]={"LABEL NAME","SIGNATURE NAME","EVENT NAME","SIGNAL NAME"};
 for(unsigned i=0;i<4;i++){
  direct_setup();begin_on_page(pages[i]);assert(text_active&&page==PAGE_KEYBOARD&&!strcmp(copied_request.label,captions[i])&&!strcmp(copied_request.text,initial_text));
  unsigned before=frames;draw();assert(frames==before);editing_steps=3;closing_steps=3;
  for(unsigned j=0;j<3;j++){text_step();assert(text_active&&!text_closing&&!strcmp(editing.name,initial_text));}
  for(unsigned j=0;j<3;j++){text_step();assert(text_active&&text_closing&&!strcmp(editing.name,initial_text)&&!resumes&&!host_releases);}
  text_step();assert_closed(pages[i],host_text);assert(strlen(editing.name)==16&&editing.name[16]==0);if(i==3)assert(!strcmp(shared_name,host_text));
  direct_setup();begin_on_page(pages[i]);host_state=RISC_TEXT_ENTRY_CANCELLED;text_step();assert_closed(pages[i],initial_text);assert(!text_home);
  direct_setup();begin_on_page(pages[i]);host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;text_step();assert_closed(pages[i],initial_text);assert(text_home);
 }
 direct_setup();deny_host=true;begin_on_page(PAGE_LABEL);assert(!text_active&&page==PAGE_LABEL&&!strcmp(editing.name,initial_text)&&!suspends&&!host_opens&&toast_message);
 deny_host=false;begin_on_page(PAGE_LABEL);text_step();assert_closed(PAGE_LABEL,host_text);
 for(unsigned bad=0;bad<5;bad++){
  direct_setup();if(bad==0)host_api.base.struct_size=1;else if(bad==1)host_api.base.api_version=99;else if(bad==2)host_api.base.open=NULL;else if(bad==3)host_api.base.poll=NULL;else host_api.base.close=NULL;
  begin_on_page(PAGE_LABEL);assert(!text_active&&page==PAGE_LABEL&&!suspends&&!host_opens&&!host_grant&&toast_message);
 }
 const int refusals[]={RISC_TEXT_ENTRY_UNAVAILABLE,RISC_TEXT_ENTRY_BUSY,RISC_TEXT_ENTRY_INVALID};
 for(unsigned i=0;i<3;i++){direct_setup();open_refusal=refusals[i];begin_on_page(PAGE_LABEL);assert(!text_active&&!host_live&&!host_grant&&!adapter_suspended&&suspends==1&&resumes==1&&host_releases==1&&!host_closes&&!strcmp(editing.name,initial_text));}
 direct_setup();poll_again=2;begin_on_page(PAGE_LABEL);text_step();text_step();assert(text_active&&!text_closing&&!host_closes&&!strcmp(editing.name,initial_text));text_step();assert_closed(PAGE_LABEL,host_text);
 direct_setup();host_text[0]=0;begin_on_page(PAGE_LABEL);text_step();assert_closed(PAGE_LABEL,"");assert(!labels[0].present&&!puts_count);
 direct_setup();old_host=true;host_api.base.struct_size=sizeof(risc_text_entry_api_v1);begin_on_page(PAGE_LABEL);text_step();assert_closed(PAGE_LABEL,host_text);
 direct_setup();attention=1;begin_on_page(PAGE_LABEL);text_step();assert_closed(PAGE_LABEL,initial_text);
}
static void direct_fail_closed(void){
 for(unsigned stage_id=1;stage_id<=7;stage_id++)for(unsigned retained_case=0;retained_case<2;retained_case++){
  if((stage_id==2||stage_id==6)&&!retained_case)continue;
  direct_setup();fail_stage=stage_id;inject_retention=retained_case!=0;begin_on_page(PAGE_LABEL);if(!text_client.retained)text_step();
  assert(text_client.retained&&adapter_retained);unsigned before=io_calls;
  text_step();draw();keyboard_begin();assert(!strcmp(editing.name,initial_text)&&puts_count==snapshot_puts);
  assert(!portable_rf_services_safe()&&!portable_rf_suspend()&&!portable_app_before_launch("springboard.elf")&&!portable_app_before_home());
  bool t=false,f=false;assert(!tap_action(20,20,&t,&f)&&!t&&!f&&!process_contact()&&!request_root_exit());assert(before==io_calls);
 }
 for(unsigned bad=0;bad<2;bad++){direct_setup();begin_on_page(PAGE_LABEL);malformed_result=!bad;oversize_result=bad!=0;text_step();assert(text_client.retained&&!host_closes&&!resumes&&!strcmp(editing.name,initial_text));}
}
static void prelaunch_drafts(void){
 for(unsigned kind=0;kind<3;kind++){
  direct_setup();unsigned p=kind==0?PAGE_LABEL:kind==1?PAGE_SIGNATURE_EDIT:PAGE_EVENT_LABEL;
  begin_on_page(p);text_step();assert(!portable_app_before_launch("springboard.elf")&&!launches&&page==p&&toast_message&&!strcmp(toast_message,"SAVE OR CANCEL EDIT FIRST"));
  /* A real saved record equal to the editor permits launch; changing any
   * existing draft field still vetoes, independent of text-input migration. */
  if(kind==0){edit_slot=0;editing.present=true;labels[0]=editing;}
  else if(kind==1){signature_slot=0;editing.color=RF_SIGNATURE_ROOM;signatures[0].kind=editing.color;strcpy(signatures[0].name,editing.name);}
  else{event_edit_slot=0;event_library.labels[0].present=true;event_library.labels[0].shift_limit=(uint8_t)event_edit_shift;strcpy(event_library.labels[0].name,editing.name);}
  assert(portable_app_before_launch("springboard.elf"));
  if(kind==0)editing.color++;else if(kind==1)editing.color=RF_SIGNATURE_EVENT;else event_edit_shift++;
  assert(!portable_app_before_launch("springboard.elf"));
 }
 direct_setup();begin_on_page(PAGE_LABEL);text_step();editing.present=true;editing.frequency_hz=prefs.identity.lo_hz;editing.tolerance_bins=2;editing.color=0;save_label();assert(page==PAGE_MAIN&&labels[0].present&&!strcmp(labels[0].name,host_text)&&puts_count==1&&!pending_save&&portable_app_before_launch("springboard.elf"));
}
static void begin_loop(void){begin_on_page(PAGE_LABEL);}
static void loop_accepted(void){assert_closed(PAGE_LABEL,host_text);assert(running&&owned&&captures>snapshot_captures+10&&spectrum.transforms>snapshot_transforms&&signature_audio.transforms>0);assert(frames<=snapshot_frames+1&&polls==snapshot_polls+1);assert(host_polls==65&&host_closes==5&&host_yields==68);set_page(PAGE_MAIN);}
static void loop_cancelled(void){assert_closed(PAGE_LABEL,initial_text);assert(running&&owned&&!launches&&!text_home);}
static void returned_to_main(void){assert(page==PAGE_MAIN&&!launches&&!text_active);}
static void prepare_coalesced(void){set_page(PAGE_SHARED_EDIT);shared_focus=0;shared_selected=-1;strcpy(shared_name,initial_text);}
static void coalesced_return(void){assert(!text_active&&!host_live&&!host_grant&&page==PAGE_SHARED_EDIT&&!launches);set_page(PAGE_MAIN);}
static unsigned blocking_kind;
static void begin_cancelled_home(void){
 begin_on_page(PAGE_LABEL);
 if(blocking_kind==0)cfa_learning_kind=CF_ROOM;
 else if(blocking_kind==1){signature_goal=64;signature_capture=(rf_signature){.kind=RF_SIGNATURE_ROOM,.identity=prefs.identity};strcpy(signature_capture.name,"Room");}
 else if(blocking_kind==2){event_edit_slot=0;event_training.ready=true;}
 else if(blocking_kind==4){pending_save=2;labels[0]=(rf_label){.present=true,.frequency_hz=prefs.identity.lo_hz,.tolerance_bins=2};strcpy(labels[0].name,"Unsaved");}
 else if(blocking_kind==5){signature_pending=1;cfa_changed=true;event_files.pending=0;}
}
static void begin_empty_home(void){
#ifdef RF_TEST_HOME_APP
 expected_launch=RF_TEST_HOME_APP;
#endif
 initial_text[0]=0;begin_on_page(PAGE_LABEL);
}
static void home_retry(void){
 direct_setup();begin_empty_home();host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;text_step();assert_closed(PAGE_LABEL,initial_text);
 if(HOME_LAUNCHES){
  fail_launch=true;assert(!request_text_home()&&launches==1&&page==PAGE_MAIN&&!home_exit_requested&&!text_active&&!host_live&&!host_grant&&!strcmp(toast_message,"EXIT FAILED / RETRY"));
  fail_launch=false;assert(request_text_home()&&launches==2);
  rt.request_launch=NULL;assert(!request_text_home()&&launches==2);rt.request_launch=launch;assert(request_text_home()&&launches==3);
 }else{assert(request_text_home()&&!launches);rt.request_launch=NULL;assert(request_text_home()&&!launches);rt.request_launch=launch;}
 expected_launch="springboard.elf";assert(request_root_exit()&&launches==(HOME_LAUNCHES?4u:1u));
}
static void home_radio_cleanup(void){
 direct_setup();radio_grant=radio_acquired=true;waterfall_radio=&radio.base.base;running=owned=waterfall_enabled=capture_requested=true;
 strcpy(editing.name,"Uncommitted");cfa_learning_kind=CF_ROOM;radio_failure=true;
 unsigned before=puts_count;assert(!portable_app_before_home()&&!home_exit_requested&&uncertain&&waterfall_uncertain&&radio_custody&&!launches);
 assert(!strcmp(editing.name,"Uncommitted")&&cfa_learning_kind==CF_ROOM&&puts_count==before);
 unsigned calls=io_calls;assert(!portable_app_before_home()&&io_calls==calls);
 /* Only the existing radio cleanup path may run until custody is released. */
 radio_failure=false;waterfall_drain();uncertain=owned=running=false;assert(!radio_custody);assert(portable_app_before_home()&&home_exit_requested&&!editing.name[0]&&!cfa_learning_kind&&!running&&!owned&&!capture_requested&&puts_count==before);
}
static void check_home_refused(void){
 assert(launches==1&&page==PAGE_MAIN&&!home_exit_requested&&!home_refusal_pending&&!text_active&&!running&&!owned);
 assert(!pending_save&&!signature_pending&&event_files.pending==-1&&!cfa_changed&&!event_neural_checkpoint&&!labels[0].present&&!editing.name[0]&&!puts_count&&!data_writes);
 assert(data_acquires==snapshot_data_acquires&&store_gets==snapshot_store_gets&&!data_live);
 assert(!started&&load_errors==((1u<<(RF_LABEL_MAX+1u))-1u)&&signature_errors==255&&!event_files.ready&&!cfa_loaded&&cfa_failed);fail_launch=false;
}
static void external_home(void){
 set_page(PAGE_LABEL);strcpy(editing.name,"Uncommitted");pending_save=2;signature_pending=1;cfa_changed=true;event_files.pending=0;
 assert(portable_app_before_home()&&home_exit_requested&&!pending_save&&!signature_pending&&!cfa_changed&&event_files.pending==-1);
}
static void assert_clean_run(void){assert(!host_live&&!host_grant&&!radio_grant&&!store_live&&!policy_live&&!data_live&&!text_active&&data_acquires==data_releases&&!rf_bulk);}
static void loop_progress(void){
 fresh();check(prepare_coalesced);events[count++]=(event){.input={.tapped=true,.touch_x=20,.touch_y=20,.buttons=T5_APP_BUTTON_CONFIRM}};check(coalesced_return);back();app_main();assert_clean_run();assert(host_opens==1&&host_releases==1&&launches==1);
 fresh();editing_steps=64;closing_steps=4;check(begin_loop);check(loop_accepted);back();app_main();assert_clean_run();
 fresh();host_state=RISC_TEXT_ENTRY_CANCELLED;check(begin_loop);check(loop_cancelled);back();check(returned_to_main);back();app_main();assert_clean_run();assert(launches==1);
 fresh();host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;check(begin_empty_home);app_main();assert_clean_run();assert(index_event==1&&launches==HOME_LAUNCHES);
 for(blocking_kind=0;blocking_kind<6;blocking_kind++){
  fresh();expected_launch=PORTABLE_HOME_APP;host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;check(begin_cancelled_home);app_main();assert_clean_run();
  assert(launches==HOME_LAUNCHES&&home_exit_requested&&page==PAGE_MAIN&&!editing.name[0]&&!shared_name[0]&&!signature_goal&&!event_armed&&!cfa_learning_kind&&!running&&!owned&&!capture_requested&&!puts_count&&!data_writes);
 }
 if(HOME_LAUNCHES)for(unsigned kind=4;kind<6;kind++){
  fresh();blocking_kind=kind;fail_launch=true;host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;check(begin_cancelled_home);check(check_home_refused);back();app_main();assert_clean_run();assert(launches==2&&!puts_count&&!data_writes);
 }
 fresh();events[count++]=(event){.check=external_home,.end=true};app_main();assert_clean_run();assert(!host_opens&&!launches&&!puts_count&&!data_writes);
 /* Failed final KV cleanup retains the invocation after radio cleanup, with
  * no subsequent preference release, storage writes, drawing, or app input. */
 fresh();storage_release_failure=true;host_state=RISC_TEXT_ENTRY_CANCELLED;host_flags=RISC_TEXT_ENTRY_HOME_CANCEL;check(begin_empty_home);
 if(!setjmp(custody_jump)){app_main();assert(!"retained cleanup returned");}
 assert(storage_custody&&store_live&&policy_live&&!radio_grant&&!data_live&&rf_bulk&&!puts_count&&!data_writes&&!running&&!owned);
 for(unsigned fail=1;fail<=7;fail++){fresh();fail_stage=fail;inject_retention=true;check(prepare_coalesced);events[count++]=(event){.input={.buttons=T5_APP_BUTTON_CONFIRM}};app_main();assert(text_client.retained&&adapter_retained&&rf_bulk&&radio_grant&&store_live&&policy_live&&!data_live&&!launches&&host_releases<2);}
}
static void geometry_same(void){shared_input_layout();unsigned n=shared_targets;unsigned char copy[sizeof(shared_bounds)];memcpy(copy,shared_bounds,sizeof(copy));draw_shared();assert(n==shared_targets&&!memcmp(copy,shared_bounds,n*sizeof(shared_bounds[0])));}
static void rapid_navigation(void){
 direct_setup();memset(&cfa_bank,0,sizeof(cfa_bank));for(unsigned i=0;i<CF_PROFILES;i++){cfa_bank.profiles[i].kind=i%2?CF_EVENT:CF_ROOM;snprintf(cfa_bank.profiles[i].name,17,"Signal %u",i);}
 for(unsigned kind=CF_ROOM;kind<=CF_EVENT;kind++){shared_kind=kind;for(unsigned offset=0;offset<10;offset++){page=PAGE_SHARED;shared_offset=offset;geometry_same();}page=PAGE_SHARED_EDIT;shared_selected=-1;geometry_same();shared_selected=0;geometry_same();page=PAGE_SHARED_CAPTURE;geometry_same();}
 memset(&cfa_bank,0,sizeof(cfa_bank));shared_kind=CF_EVENT;page=PAGE_SHARED;shared_offset=shared_focus=shared_targets=0;
 shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus==1);shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus==2);shared_buttons(T5_APP_BUTTON_CONFIRM);assert(page==PAGE_SHARED_EDIT);
 shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus==0);initial_text[0]=0;shared_buttons(T5_APP_BUTTON_CONFIRM);assert(text_active&&page==PAGE_KEYBOARD);
 unsigned opens_before=host_opens;for(unsigned i=0;i<100;i++){keyboard_begin();shared_buttons(T5_APP_BUTTON_CONFIRM);}assert(host_opens==opens_before);
 host_state=RISC_TEXT_ENTRY_CANCELLED;text_step();assert(page==PAGE_SHARED_EDIT);
 for(unsigned i=0;i<1000;i++){shared_buttons(T5_APP_BUTTON_DOWN);assert(shared_focus<shared_targets);shared_buttons(T5_APP_BUTTON_UP);assert(shared_focus<shared_targets);}
 for(unsigned i=0;i<16;i++){strcpy(initial_text,editing.name);keyboard_begin();text_step();assert(!text_active&&!host_grant&&!host_live);}
}
static void storage_scopes(void){
 direct_setup();assert(data_stats>0&&data_acquires==data_releases&&!data_live&&!data_writes);
 /* Preserve opaque revision and exact bytes across independent proxy leases. */
 const char*n="context-fingerprints.cfp";unsigned i=file_index(n);files[i].size=7;files[i].revision=17;memcpy(files[i].bytes,"SAVED!",7);
 uint32_t size;uint64_t revision;uint8_t bytes[7];const risc_app_data_v1*p=event_files.api;
 assert(!p->stat(p->context,n,&size,&revision)&&size==7&&revision==17&&!data_live);
 assert(!p->read(p->context,n,revision,bytes,sizeof(bytes),&size,&revision)&&!memcmp(bytes,"SAVED!",7)&&!data_live);
 begin_on_page(PAGE_LABEL);text_step();assert(!memcmp(files[i].bytes,"SAVED!",7)&&files[i].revision==17&&!data_writes&&!data_live&&data_acquires==data_releases);
 direct_setup();deny_data=true;event_restore();assert(!event_files.ready&&!event_files.retained&&!data_live);begin_on_page(PAGE_LABEL);text_step();assert_closed(PAGE_LABEL,host_text);
}
int main(void){direct_drafts();direct_fail_closed();prelaunch_drafts();home_retry();home_radio_cleanup();loop_progress();rapid_navigation();storage_scopes();puts("RF shared-input safety passed: four draft routes, 16-byte names, close-before-commit, cancel-and-exit Home, unchanged normal launch veto, retained/runtime-loss no-I/O, missing/legacy hosts, capture continuity, repeated/coalesced input, redraw-independent targets and operation-scoped storage preservation.");return 0;}
