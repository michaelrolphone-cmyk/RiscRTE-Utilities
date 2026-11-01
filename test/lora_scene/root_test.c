/* Actual LoRa production controller; only service endpoints are doubles. */
#include "lora_scene.c"
#include <assert.h>
static risc_runtime_api_v1 test_rt,other_rt;
static const risc_runtime_api_v1*identity;
static unsigned io_count,retained_count,radio_cancels,radio_sends,radio_receives,checkpoints,opens,releases;
static uint32_t test_now,scene_flags,text_flags,attention_bits;
static int query_rc,apply_rc,close_rc,text_close_rc,text_poll_rc,attention_rc;
static bool cancel_ok,no_ui,dirty_again;
static unsigned pauses,resumes;
static risc_text_entry_state_v1_long result;
static risc_scene_event_v1 queue[4];
static unsigned qi,queued;
static risc_scene_components_api_v1 component_api;
static risc_text_entry_api_v1_checkpoint editor;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?identity:NULL;
}
static void io(void){assert(!terminal);
io_count++;
}
static bool retain_endpoint(void){retained_count++;
return true;
}
static void yield_endpoint(uint32_t n){io();
test_now+=n;
}
static bool clock_endpoint(uint32_t*n){io();
*n=test_now;
return true;
}
static bool health_endpoint(risc_runtime_health_v1*h){io();
h->uptime_ms=test_now;
return true;
}
static bool acquire_endpoint(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){(void)v;
(void)i;
io();
const void*a=NULL;
if(!no_ui&&!strcmp(n,RISC_SCENE_CAPABILITY))a=&component_api;
if(!no_ui&&!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY))a=&editor;
if(!a)return false;
g->api=a;
g->slot=1;
g->generation=1;
return true;
}
static bool release_endpoint(risc_runtime_capability_v1*g){io();
releases++;
*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
return true;
}
static int ui_next(void*c,uint64_t t,risc_scene_event_v1*e){(void)c;
(void)t;
io();
if(qi==queued)return RISC_SCENE_IDLE;
*e=queue[qi++];
return 0;
}
static int ui_snapshot(void*c,uint64_t t,risc_scene_navigation_v1*n,uint32_t*flags){(void)c;
(void)t;
io();
n->depth=1;
n->routes[0]=1;
*flags=scene_flags;
return 0;
}
static int ui_close(void*c,uint64_t t){(void)c;
(void)t;
io();
return close_rc;
}
static int ui_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){(void)c;
(void)d;
(void)n;
io();
opens++;
*t=1;
return 0;
}
static int ui_update(void*c,uint64_t t,const risc_components_document_v1*d){(void)c;
(void)t;
(void)d;
io();
return 0;
}
static int ui_configure(void*c,uint64_t t,uint32_t flags){(void)c;
(void)t;
(void)flags;
io();
return 0;
}
static bool radio_cancel(void*c){(void)c;
io();
radio_cancels++;
return cancel_ok;
}
static bool radio_info(void*c,twatch_lora_profile_info_v1*q){(void)c;
io();
*q=(twatch_lora_profile_info_v1){sizeof(*q),15,3,0};
return true;
}
static bool radio_select(void*c,uint32_t n){(void)c;
(void)n;
io();
return true;
}
static bool radio_configure(void*c,const twatch_lora_config_v2*p){(void)c;
(void)p;
io();
return true;
}
static bool radio_send(void*c,const uint8_t*p,size_t n,uint32_t timeout){(void)c;
(void)p;
(void)n;
(void)timeout;
io();
radio_sends++;
return true;
}
static bool radio_receive(void*c,uint32_t timeout){(void)c;
(void)timeout;
io();
radio_receives++;
return true;
}
static bool radio_poll(void*c,twatch_lora_status_v2*q){(void)c;
io();
*q=(twatch_lora_status_v2){.state=TW_LORA_RX};
return true;
}
static bool radio_read(void*c,uint8_t*p,size_t n,size_t*out){(void)c;
(void)p;
(void)n;
(void)out;
io();
return false;
}
static twatch_radio_api_v2 radio_api={2,sizeof(radio_api),NULL,radio_configure,radio_send,radio_receive,radio_poll,radio_read,radio_cancel,radio_info,radio_select};
static int editor_open(void*c,const risc_text_entry_request_v1_long*q,uint64_t*t){(void)c;
io();
assert(q->capacity==241||q->capacity==12);
*t=9;
return 0;
}
static int editor_poll(void*c,uint64_t t,risc_text_entry_state_v1_long*q){(void)c;
assert(t==9);
io();
if(!text_poll_rc||dirty_again)*q=result;
return text_poll_rc;
}
static int editor_open_extended(void*c,const risc_text_entry_request_v1_extended*q,uint64_t*t){(void)c;
(void)q;
(void)t;
assert(0);
return -1;
}
static int editor_poll_extended(void*c,uint64_t t,risc_text_entry_state_v1_extended*q){(void)c;
(void)t;
(void)q;
assert(0);
return -1;
}
static int editor_close(void*c,uint64_t t){(void)c;
assert(t==9);
io();
return text_close_rc;
}
static int editor_enable(void*c,uint64_t t){(void)c;
(void)t;
io();
return 0;
}
static int editor_snapshot(void*c,uint64_t t,risc_text_entry_checkpoint_state_v1*q){(void)c;
(void)t;
io();
q->flags=text_flags;
return 0;
}
static int editor_pause(void*c,uint64_t t){(void)c;
(void)t;
io();
assert(!radio.model.owned);
pauses++;
return 0;
}
static int editor_resume(void*c,uint64_t t,uint32_t flags){(void)c;
(void)t;
io();
assert(flags==3);
resumes++;
return 0;
}
static int checkpoint_endpoint(uint64_t t,const risc_resident_request_v1*q,risc_resident_reply_v1*r){(void)t;
(void)r;
io();
checkpoints++;
assert(!radio.model.owned&&!ui_token&&(!text_token||text_paused));
assert(q->reason!=RISC_RESIDENT_CHECKPOINT_POLL&&q->reason!=RISC_RESIDENT_CHECKPOINT_POLICY);
if(q->reason==RISC_RESIDENT_QUERY_IDLE_ONLY||q->reason==RISC_RESIDENT_QUERY_ALARM_ONLY)return query_rc;
if(q->reason==RISC_RESIDENT_APPLY_IDLE_ONLY)return apply_rc;
return 0;
}
static int attention_query(uint64_t t,risc_resident_attention_v1*q){(void)t;
io();
if(!attention_rc){q->supported_flags=7;
q->pending_flags=attention_bits;
q->monotonic_ms=test_now;
}return attention_rc;
}
static int attention_reject(uint64_t t,uint32_t flags){(void)t;
io();
assert(flags&&(flags&~6u)==0);
return 0;
}
static void reset(void){
 terminal=leaving=false;
io_count=retained_count=radio_cancels=radio_sends=radio_receives=checkpoints=opens=releases=pauses=resumes=0;
 test_now=100;
scene_flags=text_flags=attention_bits=0;
query_rc=apply_rc=close_rc=text_close_rc=text_poll_rc=attention_rc=0;
cancel_ok=true;
no_ui=dirty_again=false;
 test_rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(test_rt),.acquire=acquire_endpoint,.release=release_endpoint,.health=health_endpoint,.monotonic_ms=clock_endpoint,.yield_ms=yield_endpoint,.retain_invocation=retain_endpoint};
identity=rt=&test_rt;
 ui_grant=text_grant=radio_grant=profile_grant=pref_grant=clock_grant=(risc_runtime_capability_v1){0};
 component_api=(risc_scene_components_api_v1){.tag=RISC_COMPONENTS_TAG,.version=1,.open=ui_open,.update=ui_update};
 component_api.lifecycle.base=(risc_scene_api_v1){.api_version=1,.struct_size=sizeof(component_api),.next=ui_next,.snapshot=ui_snapshot,.close=ui_close};
component_api.lifecycle.configure=ui_configure;
 scene=&component_api.lifecycle.base;
components=&component_api;
lifecycle=&component_api.lifecycle;
 editor=(risc_text_entry_api_v1_checkpoint){.checkpoint_snapshot=editor_snapshot,.pause_for_focus_change=editor_pause,.pause=editor_pause,.resume=editor_resume,.checkpoint_enable=editor_enable};
 editor.base.base.base.base.base=(risc_text_entry_api_v1){.api_version=1,.struct_size=sizeof(editor),.close=editor_close};
 editor.base.base.base.base.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG;
editor.base.base.base.base.home_reason_version=1;
 editor.base.base.base.masked_tag=RISC_TEXT_ENTRY_MASKED_TAG;
editor.base.base.base.masked_version=1;
 editor.base.base.extended_tag=RISC_TEXT_ENTRY_EXTENDED_TAG;
editor.base.base.extended_version=1;
editor.base.base.open_extended=editor_open_extended;
editor.base.base.poll_extended=editor_poll_extended;
 editor.base.long_tag=RISC_TEXT_ENTRY_LONG_TAG;
editor.base.long_version=1;
editor.base.open_long=editor_open;
editor.base.poll_long=editor_poll;
 editor.checkpoint_tag=RISC_TEXT_ENTRY_CHECKPOINT_TAG;
editor.checkpoint_version=1;
 ui_token=1;
text_token=sequence=0;
revision=1;
qi=queued=0;
dirty=explicit_refresh=true;
 pending_edit=text_closing=text_paused=text_pausing=text_presenting=false;
text_api=NULL;
text_long=NULL;
text_checkpoint=NULL;
 attention_available=attention_checked=manual_neutral=false;
attention_pending=handoff_reason=0;
idle_pending=idle_queried=text_idle_applied=host_alarm_supported=false;
idle_at=100;
idle_ms=60000;
 resident=(risc_scene_resident_v1){0};
attention=(risc_resident_attention_client_v1){0};
 memset(draft,0,sizeof(draft));
memset(entry,0,sizeof(entry));
notice_text[0]=0;
edit_field=6;
picker_return=LSP_HOME;
 lss_init(&radio,alive,retain,NULL);
assert(lss_bind(&radio,&radio_api));
 radio.model.profile=(twatch_lora_config_v2){915000000,125000,8,7,5,10,{0}};
radio.model.choice=3;
radio.model.profile_valid=true;
 view=(lora_scene_view){.model=&radio.model,.draft=draft,.notice=notice_text,.page=LSP_HOME,.radio_available=true,.text_available=true};
 document=(risc_components_document_v1){0};
scanner_alarm_init(&alarm_client,rt,alive,retain,NULL);
alarm_ui=false;
 memset(&scanner_bg,0,sizeof(scanner_bg));
assert(scanner_background_init(rt,alive,retain,NULL));
 result=(risc_text_entry_state_v1_long){.struct_size=sizeof(result),.revision=1,.state=RISC_TEXT_ENTRY_CANCELLED};
}
static void owned(void){radio.model.owned=radio.model.listening=true;
radio.model.state=LM_LISTENING;
}
static void text_owned(void){text_token=9;
text_api=&editor.base.base.base.base.base;
text_long=&editor.base;
text_checkpoint=&editor;
text_grant=(risc_runtime_capability_v1){.struct_size=sizeof(text_grant),.api=&editor,.slot=1,.generation=1};
}
static void attention_setup(void){resident.enabled=true;
resident.client=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(resident.client),.invocation=1,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint_endpoint};
attention.base=resident.client;
attention.attention=attention_query;
attention.reject_manual=attention_reject;
attention_available=host_alarm_supported=true;
}
int main(int argc,char**argv){assert(argc==2);
reset();
const char*c=argv[1];
 if(!strcmp(c,"quiet-attention")){attention_setup();
owned();
assert(attention_step()&&radio.model.owned&&!radio_cancels&&!checkpoints);
}
 else if(!strcmp(c,"alarm-attention")){attention_setup();
owned();
attention_bits=1;
assert(attention_step()&&!radio.model.owned&&radio_cancels==1&&checkpoints==1&&!leaving);
}
 else if(!strcmp(c,"cancel-failure")){attention_setup();
owned();
attention_bits=1;
cancel_ok=false;
assert(!attention_step()&&terminal&&!checkpoints);
unsigned n=io_count;
assert(!attention_step()&&n==io_count);
}
 else if(!strcmp(c,"short-back")){attention_setup();
view.page=LSP_COMPOSE;
manual_neutral=true;
attention_bits=2;
assert(attention_step()&&view.page==LSP_HOME&&!leaving);
}
 else if(!strcmp(c,"long-key")){attention_setup();
manual_neutral=true;
attention_bits=4;
assert(attention_step()&&!leaving&&!checkpoints);
}
 else if(!strcmp(c,"unknown-attention")){attention_setup();
attention_rc=-99;
assert(!attention_step()&&terminal&&!checkpoints);
}
 else if(!strcmp(c,"text-open")){pending_edit=true;
begin_edit();
assert(text_token==9&&text_checkpoint&&!terminal);
}
 else if(!strcmp(c,"text-accept")){text_owned();
result.state=RISC_TEXT_ENTRY_ACCEPTED;
memset(result.text,'x',240);
result.text[240]=0;
text_step();
assert(!text_token&&strlen(draft)==240&&!radio_sends);
}
 else if(!strcmp(c,"text-cancel")){strcpy(draft,"old");
text_owned();
text_step();
assert(!text_token&&!strcmp(draft,"old"));
}
 else if(!strcmp(c,"text-home")){text_owned();
result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;
text_step();
assert(!text_token&&leaving);
}
 else if(!strcmp(c,"text-close-again")){text_owned();
text_closing=true;
text_close_rc=RISC_TEXT_ENTRY_AGAIN;
text_step();
assert(text_token&&!terminal&&!releases);
text_close_rc=0;
text_step();
assert(!text_token&&releases==1);
}
 else if(!strcmp(c,"text-clean-again")){text_owned();
text_poll_rc=RISC_TEXT_ENTRY_AGAIN;
text_step();
assert(text_token&&!terminal&&!releases);
}
 else if(!strcmp(c,"text-dirty-again")){text_owned();
text_poll_rc=RISC_TEXT_ENTRY_AGAIN;
dirty_again=true;
text_step();
assert(terminal&&text_token&&!releases);
}
 else if(!strcmp(c,"text-overlong")){text_owned();result.state=RISC_TEXT_ENTRY_ACCEPTED;memset(result.text,'x',255);result.text[255]=0;text_step();assert(terminal&&text_token&&!releases&&!draft[0]);}
 else if(!strcmp(c,"field-overlong")){text_owned();edit_field=0;result.state=RISC_TEXT_ENTRY_ACCEPTED;strcpy(result.text,"915000000000");text_step();assert(terminal&&text_token&&!releases);}
 else if(!strcmp(c,"text-invalid")){text_owned();
result.struct_size=0;
text_step();
assert(terminal&&text_token&&!releases);
}
 else if(!strcmp(c,"rf-exact")){edit_field=0;
strcpy(entry,"915000000");
assert(field_apply()&&view.editing.frequency_hz==915000000);
strcpy(entry,"9150000000");
assert(!field_apply());
}
 else if(!strcmp(c,"rf-negative")){edit_field=4;
strcpy(entry,"-9");
assert(field_apply()&&view.editing.power_dbm==-9);
strcpy(entry,"-10");
assert(!field_apply());
}
 else if(!strcmp(c,"history-identity")){lm_add(&radio.model,(const uint8_t*)"first",5,LM_IN);
view.page=LSP_HISTORY;
assert(publish());
for(unsigned i=0;i<20;i++)lm_add(&radio.model,(const uint8_t*)"later",5,LM_IN);
action(LSP_MESSAGE_BASE);
assert(!strcmp(view.detail.text,"first"));
}
 else if(!strcmp(c,"root-back")){owned();
action(LSP_BACK);
assert(leaving&&radio.model.owned);
assert(stop_owners()&&!radio.model.owned);
}
 else if(!strcmp(c,"idle-radio")){attention_setup();
owned();
idle_pending=true;
assert(idle_step()&&!radio.model.owned&&!idle_pending&&checkpoints==2);
}
 else if(!strcmp(c,"idle-ui-pending")){attention_setup();
owned();
idle_pending=true;
close_rc=RISC_SCENE_AGAIN;
assert(idle_step()&&!checkpoints&&!radio.model.owned);
close_rc=0;
assert(idle_step()&&checkpoints==2);
}
 else if(!strcmp(c,"idle-busy")){attention_setup();
idle_pending=true;
apply_rc=RISC_RESIDENT_BUSY;
assert(idle_step()&&idle_pending&&checkpoints==2);
apply_rc=0;
assert(idle_step()&&!idle_pending&&checkpoints==3);
}
 else if(!strcmp(c,"idle-exit")){attention_setup();
idle_pending=true;
apply_rc=RISC_RESIDENT_EXIT;
assert(idle_step()&&leaving);
}
 else if(!strcmp(c,"idle-text")){attention_setup();
text_owned();
ui_token=0;
idle_at=0;
idle_ms=5;
assert(text_policy()&&pauses==1&&resumes==1&&!text_paused&&!leaving&&!radio_sends);
}
 else if(!strcmp(c,"idle-held")){attention_setup();
text_owned();
ui_token=0;
idle_at=0;
idle_ms=5;
text_flags=RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY;
assert(text_policy()&&!pauses&&!checkpoints);
}
 else if(!strcmp(c,"identity-lost")){unsigned before=io_count;
identity=&other_rt;
assert(!close_ui()&&terminal&&io_count==before);
}
 else if(!strcmp(c,"short-runtime")){unsigned before=io_count;test_rt.struct_size=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE;app_main();assert(io_count==before);}
 else if(!strcmp(c,"no-ui")){no_ui=true;
unsigned before=io_count;
app_main();
assert(io_count==before+1&&!terminal);
}
 else assert(0);
 printf("%s PASS\n",c);
return 0;
}
