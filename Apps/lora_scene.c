/* One LoRa application: semantic intents and copied model only.
 * ui.scene/text-input own presentation; providers own physical lifecycle. */
#include "lora_scene_session.h"
#include "lora_scene_presenter.h"
#include "scanner_alarm.h"
#include "scanner_background.h"
#include "RiscSceneResidentV1.h"
#include "RiscResidentAttentionV1.h"
#include "RiscResidentIdleV1.h"
#include "RiscResidentAlarmV1.h"
#include "RiscTextEntryV1.h"
#include "RiscCivilClockV1.h"
#include "PortableTimeFormat.h"
#include <stdio.h>
static const risc_runtime_api_v1*rt;
static bool terminal,leaving,dirty,explicit_refresh;
static lora_scene_session radio;
static lora_scene_view view;
static scanner_alarm alarm_client;
static bool alarm_ui,host_alarm_supported;
static alarm_token_v1 committed_alarm;
enum { LORA_ALARM_ACK=20,LORA_ALARM_RETRY=21 };
static risc_runtime_capability_v1 ui_grant,text_grant,radio_grant,profile_grant,pref_grant,clock_grant;
static const risc_scene_api_v1*scene;
static const risc_scene_components_api_v1*components;
static const risc_scene_lifecycle_api_v1*lifecycle;
static const risc_text_entry_api_v1*text_api;
static const risc_text_entry_api_v1_long*text_long;
static const risc_text_entry_api_v1_checkpoint*text_checkpoint;
static risc_components_document_v1 document,candidate;
static risc_scene_navigation_v1 navigation;
static uint64_t ui_token,text_token,sequence;
static uint32_t revision,ui_flags,attention_at,attention_pending,handoff_reason,idle_at,idle_ms;
static bool attention_available,attention_checked,manual_neutral,idle_pending,idle_queried,text_idle_applied;
static bool text_closing,text_paused,text_pausing,text_presenting,pending_edit;
static unsigned edit_field,picker_return,committed_count;
static lm_message committed_messages[LM_HISTORY_MAX];
static char draft[LM_TEXT_MAX+1],entry[LM_TEXT_MAX+1],notice_text[72];
static risc_text_entry_state_v1_long text_state;
static risc_scene_resident_v1 resident;
static risc_resident_attention_client_v1 attention;
static bool alive(void*p){(void)p;
if(terminal)return false;
if(risc_runtime_get_api(1)!=rt){terminal=true;
return false;
}return true;
}
static void retain(void*p){(void)p;
if(terminal)return;
terminal=true;
if(risc_runtime_get_api(1)==rt)(void)rt->retain_invocation();
}
static bool fail(void){retain(NULL);
return false;
}
static void notice(const char*s){snprintf(notice_text,sizeof(notice_text),"%s",s);
dirty=explicit_refresh=true;
}
static uint32_t millis(void){uint32_t n=0;
if(!alive(NULL))return 0;
bool ok=rt->monotonic_ms(&n);
if(alive(NULL)&&!ok)retain(NULL);
return n;
}
static bool empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation&&(g->struct_size==0||g->struct_size==sizeof(*g));
}
static int acquire(const char*name,uint32_t version,uint32_t instance,risc_runtime_capability_v1*g){
 if(!alive(NULL))return -1;
if(!empty(g))return 1;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
bool ok=rt->acquire(name,version,instance,g);
 if(!alive(NULL))return -1;
if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):!empty(g))){fail();
return -1;
}return ok?1:0;
}
static bool release(risc_runtime_capability_v1*g){
 if(!alive(NULL))return false;
if(empty(g))return true;
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation)return fail();
 risc_runtime_capability_v1 copy=*g;
bool ok=rt->release(&copy);
 if(!alive(NULL))return false;
if(!ok||copy.struct_size!=sizeof(copy)||!empty(&copy))return fail();
*g=copy;
return true;
}
static bool kv_result(int rc){if(!alive(NULL))return false;
if(rc>RISC_KEY_VALUE_OK||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT)return fail();
return true;
}
static const risc_key_value_v1*kv_open(risc_runtime_capability_v1*g,uint32_t instance,bool write){
 int a=acquire(RISC_KEY_VALUE_CAPABILITY,1,instance,g);
if(a<=0)return NULL;
 const risc_key_value_v1*p=g->api;
if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->get||(write&&!p->put)){fail();
return NULL;
}return p;
}
static void policy_read(void){
 const risc_key_value_v1*p=kv_open(&pref_grant,1,false);
view.policy_allowed=false;
idle_ms=60000;
 if(!p)return;
uint8_t b[8]={0};
uint32_t n=0;
int rc=p->get(p->context,PORTABLE_RADIO_KEY,b,sizeof(b),&n);
if(!kv_result(rc))return;
 uint8_t flags=0;
view.policy_allowed=rc==RISC_KEY_VALUE_NOT_FOUND||(!rc&&portable_radio_decode(b,n,&flags)&&!(flags&PORTABLE_RADIO_AIRPLANE));
 n=0;
rc=p->get(p->context,PORTABLE_TIME_FORMAT_KEY,b,sizeof(b),&n);
if(!kv_result(rc))return;
 view.time_format=!rc&&n==4&&b[0]==0x54&&b[1]==1&&b[2]<=1&&b[3]==(uint8_t)(b[2]^0xa5)?b[2]:0;
 n=0;
rc=p->get(p->context,"sleep_idle",b,sizeof(b),&n);
if(!kv_result(rc))return;
 uint32_t seconds=(uint32_t)b[2]|(uint32_t)b[3]<<8;
 if(!rc&&n==5&&b[0]==0x54&&b[1]==1&&b[4]==(uint8_t)(b[2]^b[3]^0xa5)&&seconds>=5&&seconds<=3600)idle_ms=seconds*1000u;
 (void)release(&pref_grant);
}
static void profile_load(void){
 const risc_key_value_v1*p=kv_open(&profile_grant,LM_STORE_INSTANCE,true);
if(!p){view.save_state=1;
return;
}
 uint8_t b[LM_PROFILE_BYTES];
uint32_t n=0;
int rc=p->get(p->context,LM_PROFILE_KEY,b,sizeof(b),&n);
if(!kv_result(rc))return;
 if(!rc){if(!lm_profile_decode(&radio.model.profile,&radio.model.choice,&radio.model.profile_valid,b,n))notice("Saved RF invalid; explicit setup required");
else if(!radio.model.choice)notice("Choose radio; saved RF retained");
}
 else if(rc!=RISC_KEY_VALUE_NOT_FOUND)notice("Saved RF unreadable; retry or explicit setup");
 (void)release(&profile_grant);
}
static bool profile_save(void){
 if(!scanner_background_pause())return false;
 const risc_key_value_v1*p=kv_open(&profile_grant,LM_STORE_INSTANCE,true);
if(!p){view.save_state=1;
notice("RF active for this session only");
return false;
}
 uint8_t b[LM_PROFILE_BYTES],check[LM_PROFILE_BYTES];
uint32_t n=0;
 lm_profile_encode(&radio.model.profile,radio.model.choice,radio.model.profile_valid,b);
 int rc=p->put(p->context,LM_PROFILE_KEY,b,sizeof(b));
if(!kv_result(rc))return false;
 int rr=p->get(p->context,LM_PROFILE_KEY,check,sizeof(check),&n);
if(!kv_result(rr))return false;
 bool saved=!rc&&!rr&&n==sizeof(b)&&!memcmp(b,check,sizeof(b));
view.save_state=saved?0:2;
 if(!release(&profile_grant))return false;
 notice(saved?"RF saved; radio remains off":"RF active; save unconfirmed");
return saved;
}
static bool radio_open(void){
 if(radio.provider)return true;
int a=acquire(TWATCH_RADIO_CAPABILITY,2,0,&radio_grant);
 if(a<=0){view.radio_available=false;
notice("Radio unavailable; retry");
return false;
}
 if(!lm_selector_valid(radio_grant.api)){release(&radio_grant);
view.radio_available=false;
notice("Radio picker unavailable; update provider");
return false;
}
 view.radio_available=lss_bind(&radio,radio_grant.api);
return view.radio_available;
}
static bool radio_stop(void){return lss_stop(&radio);
}
static bool stop_owners(void){return radio_stop()&&scanner_background_pause();
}
static void stamp(lm_message*m){
 if(!m||!alive(NULL))return;
int a=acquire(RISC_CIVIL_CLOCK_CAPABILITY,1,0,&clock_grant);
if(a<=0)return;
 const risc_civil_clock_api_v1*p=clock_grant.api;
if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->read){fail();
return;
}
 risc_civil_time_v1 t={.struct_size=sizeof(t)};
int rc=p->read(p->context,&t);
if(!alive(NULL))return;
 if(rc!=-1&&rc!=0){fail();
return;
}
 if(!rc){if(t.struct_size!=sizeof(t)||t.day<0||t.day>36524||t.minute>=1440||!memchr(t.zone,0,sizeof(t.zone))){fail();
return;
}
  unsigned day=(unsigned)t.day,year=2000,month=1;
for(;;){unsigned days=365+(!(year%4)&&(year%100||!(year%400)));
if(day<days)break;
day-=days;
year++;
}
  static const unsigned months[]={31,28,31,30,31,30,31,31,30,31,30,31};
  while(month<=12){unsigned days=months[month-1]+(month==2&&!(year%4)&&(year%100||!(year%400)));
if(day<days)break;
day-=days;
month++;
}
  m->year=(uint16_t)year;
m->month=(uint8_t)month;
m->day=(uint8_t)(day+1);
m->hour=(uint8_t)(t.minute/60);
m->minute=(uint8_t)(t.minute%60);
m->clock_valid=true;
 }(void)release(&clock_grant);
}
static bool snapshot(void){
 if(!ui_token)return true;
uint32_t flags=0;
risc_scene_navigation_v1 n={.api_version=1,.struct_size=sizeof(n)};
 int rc=scene->snapshot(scene->context,ui_token,&n,&flags);
if(!alive(NULL))return false;
 if(rc||n.api_version!=1||n.struct_size!=sizeof(n)||n.reserved||n.depth!=1||n.routes[0]!=1||(flags&~15u))return fail();
 navigation=n;
ui_flags=flags;
if(flags&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))resident.activity=true;
return true;
}
static bool close_ui(void){
 if(!alive(NULL))return false;
if(!ui_token)return true;
 int rc=scene->close(scene->context,ui_token);
if(!alive(NULL))return false;
if(rc==RISC_SCENE_AGAIN)return false;
 if(rc!=RISC_SCENE_OK)return fail();
ui_token=0;
ui_flags=0;
return true;
}
static bool publish(void){
 if(!alive(NULL)||text_token)return false;
 if(ui_token&&(!snapshot()||(ui_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))))return false;
 if(!dirty&&ui_token)return true;
if(revision==UINT32_MAX)return fail();
 if(alarm_ui){
  candidate=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(candidate),.revision=revision+1,.root=1,.route_count=1,.screen_key=100,.flags=RISC_COMPONENTS_ALERT};
  bool alert=alarm_client.status.state==ALARM_STATE_ALERT;
unsigned action=alert?LORA_ALARM_ACK:LORA_ALARM_RETRY;
candidate.cancel_action=action;
  candidate.routes[0]=(risc_scene_route_v1){.id=1,.back_action=action,.title="ALARM"};
  lsp_node(&candidate,RISC_SCENE_TEXT_NODE,0,alert?"Alarm":"Alarm service",alert?alarm_client.status.label:"Reconciliation pending");
  lsp_node(&candidate,RISC_SCENE_ACTION,action,alert?"Dismiss":"Retry","");
 }else if(!lsp_build(&candidate,&view,revision+1))return fail();
 if(ui_token){candidate.revision=document.revision;
if(!memcmp(&candidate,&document,sizeof(candidate))){dirty=false;
return true;
}candidate.revision=revision+1;
}
 uint64_t opened=0;
int rc=ui_token?components->update(scene->context,ui_token,&candidate):components->open(scene->context,&candidate,NULL,&opened);
 if(!alive(NULL))return false;
if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY){if(opened)return fail();
return false;
}if(rc)return fail();
 bool fresh=!ui_token;
if(fresh){if(!opened)return fail();
ui_token=opened;
sequence=0;
}
 document=candidate;
revision=document.revision;
dirty=explicit_refresh=false;
 if(alarm_ui)committed_alarm=alarm_client.status.occurrence;
 committed_count=radio.model.count;
for(unsigned i=0;i<committed_count;i++)committed_messages[i]=*lm_at(&radio.model,i);
 if(fresh&&lifecycle){rc=lifecycle->configure(scene->context,ui_token,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);
if(!alive(NULL))return false;
if(rc)return fail();
}
 return true;
}
static bool field_apply(void){
 int64_t n;
if(!lm_number(entry,edit_field==4,&n))return false;
 twatch_lora_config_v2*p=&view.editing;
 switch(edit_field){
 case 0:if(n<0||n>UINT32_MAX||!lm_frequency_matches((uint32_t)n,radio.model.choice))return false;
p->frequency_hz=(uint32_t)n;
break;
 case 1:if(radio.model.choice==4?(n!=203125&&n!=406250&&n!=812500&&n!=1625000):(n!=125000&&n!=250000&&n!=500000))return false;
p->bandwidth_hz=(uint32_t)n;
break;
 case 2:if(n<5||n>12)return false;
p->sf=(uint8_t)n;
break;
 case 3:if(n<5||n>8)return false;
p->coding_rate=(uint8_t)n;
break;
 case 4:if(radio.model.choice==4?(n<-18||n>13):(n<-9||n>22))return false;
p->power_dbm=(int8_t)n;
break;
 case 5:if(n<8||n>4096)return false;
p->preamble=(uint16_t)n;
break;
 default:return false;
 }view.fields|=1u<<edit_field;
return true;
}
static const char*field_label(unsigned n){static const char*s[]={"Frequency Hz","Bandwidth Hz","Spreading factor","Coding rate 4/N","Power dBm","Preamble symbols","Compose message"};
return s[n<7?n:6];
}
static bool text_close(void){
 if(!text_token)return true;
int rc=text_api->close(text_api->context,text_token);
if(!alive(NULL))return false;
 if(rc==RISC_TEXT_ENTRY_AGAIN)return false;
if(rc)return fail();
 text_token=0;
text_api=NULL;
text_long=NULL;
text_checkpoint=NULL;
text_paused=text_pausing=false;
idle_pending=idle_queried=false;
return release(&text_grant);
}
static void text_step(void){
 if(!text_token||!alive(NULL))return;
 if(!text_closing){risc_text_entry_state_v1_long v={.struct_size=sizeof(v)};
int rc=text_long->poll_long(text_api->context,text_token,&v);
if(!alive(NULL))return;
  if(rc==RISC_TEXT_ENTRY_AGAIN){risc_text_entry_state_v1_long empty={.struct_size=sizeof(empty)};
if(memcmp(&v,&empty,sizeof(v)))fail();
return;
}
  unsigned capacity=edit_field<6?12:241;
  if(rc||v.struct_size!=sizeof(v)||!v.revision||v.state>RISC_TEXT_ENTRY_CANCELLED||(v.flags&~7u)||((v.flags&RISC_TEXT_ENTRY_HOME_CANCEL)&&v.state!=RISC_TEXT_ENTRY_CANCELLED)||!memchr(v.text,0,capacity)){fail();
return;
}
  for(unsigned i=0;v.text[i];i++)if((unsigned char)v.text[i]<32||(unsigned char)v.text[i]>126){fail();
return;
}
  text_state=v;
text_presenting=!!(v.flags&RISC_TEXT_ENTRY_PRESENTING);
if(v.state==RISC_TEXT_ENTRY_EDITING)return;
text_closing=true;
 }
 if(!text_close())return;
text_closing=false;
dirty=explicit_refresh=true;
 if(text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL){leaving=true;
return;
}
 if(text_state.state==RISC_TEXT_ENTRY_ACCEPTED){const char*end=memchr(text_state.text,0,sizeof(entry));
if(!end){fail();return;}
memset(entry,0,sizeof(entry));
memcpy(entry,text_state.text,(size_t)(end-text_state.text));
  if(edit_field==6){memcpy(draft,entry,sizeof(draft));
notice("Draft updated; Send remains explicit");
}
  else if(!field_apply())notice("RF value rejected; edit again with a matching value");
  else notice("Field updated; Apply RF remains explicit");
 }else notice("Edit cancelled; previous value retained");
}
static void begin_edit(void){
 if(!pending_edit||text_token||!close_ui())return;
 pending_edit=false;
 int a=acquire(RISC_TEXT_ENTRY_CAPABILITY,1,0,&text_grant);
if(a<=0){view.text_available=false;
notice("Text input unavailable; retry");
return;
}
 text_api=text_grant.api;
text_long=risc_text_entry_long(text_api);
 if(!text_long||!text_api->close){release(&text_grant);
text_api=NULL;
notice("Long text input unavailable; no truncating fallback");
view.text_available=false;
return;
}
 risc_text_entry_request_v1_long q={.api_version=1,.struct_size=sizeof(q),.capacity=edit_field<6?12:241,.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON};
 snprintf(q.label,sizeof(q.label),"%s",field_label(edit_field));
snprintf(q.text,sizeof(q.text),"%s",entry);
 int rc=text_long->open_long(text_api->context,&q,&text_token);
if(!alive(NULL))return;
 if(rc){if(text_token||(rc!=RISC_TEXT_ENTRY_UNAVAILABLE&&rc!=RISC_TEXT_ENTRY_BUSY&&rc!=RISC_TEXT_ENTRY_INVALID)){fail();
return;
}release(&text_grant);
text_api=NULL;
notice("Text input unavailable; retry");
return;
}
 if(!text_token){fail();
return;
}view.text_available=true;
text_closing=text_presenting=false;
text_state=(risc_text_entry_state_v1_long){.struct_size=sizeof(text_state)};
 text_checkpoint=risc_text_entry_checkpoint(text_api);
if(text_checkpoint){rc=text_checkpoint->checkpoint_enable(text_api->context,text_token);
if(!alive(NULL))return;
if(rc)fail();
}
}
static void open_picker(void){
 if(!radio_stop())return;
picker_return=view.page;
view.page=LSP_PICKER;
view.mask=0;
dirty=true;
 if(!radio_open())return;
twatch_lora_profile_info_v1 info;
 if(!lm_profile_info(&radio.facade,&info)){if(!lss_result(&radio,false)&&!alive(NULL))return;
notice("Radio choices unavailable; retry");
return;
}view.mask=info.supported_profiles;
}
static void back(void){
 if(alarm_ui){if(alarm_client.status.state==ALARM_STATE_ALERT)(void)scanner_alarm_ack(&alarm_client,&committed_alarm);
return;
}
 if(text_token){text_state=(risc_text_entry_state_v1_long){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
text_closing=true;
return;
}
 if(view.page==LSP_HOME)leaving=true;
else view.page=view.page==LSP_DETAIL?LSP_HISTORY:view.page==LSP_PICKER?picker_return:LSP_HOME;
dirty=true;
}
static void action(uint32_t a){
 explicit_refresh=dirty=true;
resident.activity=true;
 if(alarm_ui){if(a==LORA_ALARM_ACK)(void)scanner_alarm_ack(&alarm_client,&committed_alarm);
else if(a==LORA_ALARM_RETRY)(void)scanner_alarm_refresh(&alarm_client);
return;
}
 if(a==LSP_BACK){back();
return;
}
 if(a==LSP_OPEN_COMPOSE){view.page=LSP_COMPOSE;
return;
}
 if(a==LSP_OPEN_HISTORY){view.page=LSP_HISTORY;
return;
}
 if(a==LSP_OPEN_RF){if(!radio_stop())return;
view.editing=radio.model.profile;
view.fields=radio.model.profile_valid?63:0;
view.page=LSP_RF;
return;
}
 if(a==LSP_OPEN_PICKER){open_picker();
return;
}
 if(a>=LSP_CHOOSE_BASE+1&&a<=LSP_CHOOSE_BASE+4){
  unsigned choice=a-LSP_CHOOSE_BASE;
if(!(view.mask&(1u<<(choice-1))))return;
  if(lss_choose(&radio,choice)){view.page=picker_return;
(void)profile_save();
}else if(alive(NULL))notice("Choice rejected; retry");
return;
 }
 if(a>=LSP_MESSAGE_BASE&&a<LSP_MESSAGE_BASE+committed_count){view.detail=committed_messages[a-LSP_MESSAGE_BASE];
view.page=LSP_DETAIL;
return;
}
 if(a==LSP_EDIT_MESSAGE||(a>=LSP_FIELD_BASE&&a<LSP_FIELD_BASE+6)){
  edit_field=a==LSP_EDIT_MESSAGE?6:a-LSP_FIELD_BASE;
  if(edit_field==6)memcpy(entry,draft,sizeof(entry));
  else{const int64_t values[]={view.editing.frequency_hz,view.editing.bandwidth_hz,view.editing.sf,view.editing.coding_rate,view.editing.power_dbm,view.editing.preamble};
if(view.fields&(1u<<edit_field))snprintf(entry,sizeof(entry),"%ld",(long)values[edit_field]);
else entry[0]=0;
}
  pending_edit=true;
return;
 }
 if(a==LSP_APPLY_RF){
  if(view.fields!=63||!lm_profile_matches(&view.editing,radio.model.choice)){notice("Set all six valid matching RF values");
return;
}
  if(!radio_stop())return;
radio.model.profile=view.editing;
radio.model.profile_valid=true;
(void)profile_save();
view.page=LSP_HOME;
return;
 }
 if(a==LSP_SEND||a==LSP_LISTEN){
  if((a==LSP_LISTEN&&radio.model.owned)||(a==LSP_SEND&&radio.model.state==LM_SENDING)){if(radio_stop())notice("Stopped; any unfinished send is unconfirmed");
return;
}
  if(!radio.model.choice){open_picker();
return;
}
  if(!radio.model.profile_valid||!lm_profile_matches(&radio.model.profile,radio.model.choice)){notice("Set matching RF before using radio");
return;
}
  policy_read();
if(!alive(NULL))return;
if(!view.policy_allowed){notice("Radio policy unavailable or Airplane mode");
return;
}
  if(!radio_open())return;
unsigned before=radio.model.next;
uint32_t now=millis();
if(!alive(NULL))return;
  bool ok=a==LSP_SEND?lss_transmit(&radio,draft,now):lss_listen(&radio,now);
if(!alive(NULL))return;
  if(before!=radio.model.next)stamp(&radio.model.history[radio.model.active]);
  notice(ok?(a==LSP_SEND?"Sending; no automatic retry":"Listening; no encryption or receipts"):"Radio action rejected or send unconfirmed");
return;
 }
}
static bool bind_resident(void){attention=(risc_resident_attention_client_v1){0};
resident=(risc_scene_resident_v1){0};
if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!rt->resident_shell)return true;
 attention.base.struct_size=sizeof(attention);
bool ok=rt->resident_shell(&attention.base);
if(!alive(NULL))return false;
 if(!ok)return true;
 risc_resident_client_v1*c=&attention.base;
if(c->api_version!=1||c->struct_size<sizeof(*c)||!c->invocation||c->role!=RISC_RESIDENT_ROLE_FOREGROUND||!c->checkpoint)return fail();
 resident.client=*c;
resident.enabled=true;
attention_available=risc_resident_attention_available_v1(&attention);
return true;
}
static int checkpoint(uint32_t reason,uint32_t flags,uint32_t*out){if(!alive(NULL)||!resident.enabled||!resident.client.checkpoint)return RISC_RESIDENT_RETAINED;
risc_resident_request_v1 q={sizeof(q),reason,flags,0};
risc_resident_reply_v1 r={sizeof(r),0};
int rc=resident.client.checkpoint(resident.client.invocation,&q,&r);
if(!alive(NULL))return RISC_RESIDENT_RETAINED;
if(r.struct_size!=sizeof(r)||(r.flags&~(RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED|RISC_RESIDENT_REPLY_POLICY_REQUEST))||(rc!=RISC_RESIDENT_OK&&r.flags)){fail();
return RISC_RESIDENT_RETAINED;
}if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF){fail();
return RISC_RESIDENT_RETAINED;
}if(out)*out=r.flags;
return rc;
}
static void idle_activity(void){idle_at=millis();
resident.activity=false;
}
static bool idle_due(void){
 if(resident.activity)idle_activity();
 return alive(NULL)&&idle_ms&&(uint32_t)(millis()-idle_at)>=idle_ms;
}
static bool negotiate_alarm(void){
 host_alarm_supported=false;
if(!resident.enabled)return true;
 uint32_t flags=0;
int rc=checkpoint(RISC_RESIDENT_QUERY_ALARM_ONLY,0,&flags);
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK||flags)return fail();
 host_alarm_supported=true;
return true;
}
static bool text_policy(void){
 if(!alive(NULL))return false;
 if(!text_token||!text_checkpoint||text_closing||attention_pending)return true;
 int rc;
 if(!text_pausing&&!text_paused){
  risc_text_entry_checkpoint_state_v1 q={.struct_size=sizeof(q)};
  rc=text_checkpoint->checkpoint_snapshot(text_api->context,text_token,&q);
  if(!alive(NULL))return false;
  if(rc!=RISC_TEXT_ENTRY_OK||q.struct_size!=sizeof(q)||(q.flags&~15u))return fail();
  text_presenting=!!(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING);
  if(q.flags&(RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY))resident.activity=true;
  if(resident.activity)idle_activity();
  if(text_presenting)return true;
  if(!scanner_background_step(true)||!scanner_background_capture())return false;
  if(!resident.enabled||(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)||!idle_due())return true;
  idle_pending=true;
idle_queried=false;
text_idle_applied=false;
text_pausing=true;
 }
 if(text_pausing){
  if(!stop_owners())return !terminal;
  rc=text_checkpoint->pause_for_focus_change(text_api->context,text_token);
if(!alive(NULL))return false;
  if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
  if(rc==RISC_TEXT_ENTRY_BUSY){text_pausing=false;
idle_pending=false;
idle_activity();
return true;
}
  if(rc!=RISC_TEXT_ENTRY_OK)return fail();
  text_pausing=false;
text_paused=true;
 }
 uint32_t flags=0;
 if(!idle_queried){
  rc=checkpoint(RISC_RESIDENT_QUERY_IDLE_ONLY,0,&flags);
  if(rc==RISC_RESIDENT_BUSY)goto resume_editor;
  if(rc!=RISC_RESIDENT_OK||flags)return fail();
  idle_queried=true;
 }
 if(!text_idle_applied) {
  rc=checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY,0,&flags);
  if(rc==RISC_RESIDENT_BUSY)return true;
  if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){
   text_state=(risc_text_entry_state_v1_long){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED,.flags=RISC_TEXT_ENTRY_HOME_CANCEL};
text_closing=true;
return true;
  }
  if(rc!=RISC_RESIDENT_OK||(flags&RISC_RESIDENT_REPLY_POLICY_REQUEST))return fail();
  text_idle_applied=true;
 }
 if(host_alarm_supported) {
  rc=checkpoint(RISC_RESIDENT_QUERY_LAST_IDLE_ALARM,0,&flags);
  if(rc==RISC_RESIDENT_BUSY)return true;
  if(rc!=RISC_RESIDENT_OK||(flags&~RISC_RESIDENT_REPLY_REDRAW))return fail();
  if(flags&RISC_RESIDENT_REPLY_REDRAW) {
   text_state=(risc_text_entry_state_v1_long){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
   text_closing=true;
idle_pending=idle_queried=text_idle_applied=false;
idle_activity();
return true;
  }
 }
resume_editor:
 rc=text_checkpoint->resume(text_api->context,text_token,RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
 if(!alive(NULL))return false;
 if(rc!=RISC_TEXT_ENTRY_OK)return fail();
 text_paused=false;
idle_pending=idle_queried=text_idle_applied=false;
manual_neutral=false;
idle_activity();
policy_read();
return alive(NULL);
}
static bool idle_step(void){
 if(!alive(NULL))return false;
 if(!idle_pending)return true;
 if(!stop_owners()||!close_ui())return !terminal;
 uint32_t flags=0;
int rc;
 if(!idle_queried){
  rc=checkpoint(RISC_RESIDENT_QUERY_IDLE_ONLY,0,&flags);
  if(rc==RISC_RESIDENT_BUSY){idle_pending=false;
idle_activity();
notice("Automatic idle unavailable");
return true;
}
  if(rc!=RISC_RESIDENT_OK||flags)return fail();
  idle_queried=true;
 }
 rc=checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY,0,&flags);
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)return fail();
 if(flags&RISC_RESIDENT_REPLY_POLICY_REQUEST)return fail();
 idle_pending=idle_queried=false;
manual_neutral=false;
idle_activity();
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){leaving=true;
return true;
}
 policy_read();
if(!alive(NULL))return false;
notice("Paused - start radio manually");
explicit_refresh=true;
return true;
}
static bool attention_step(void){
 if(!alive(NULL)||!resident.enabled)return alive(NULL);
 if(!attention_pending&&attention_available){uint32_t now=millis();
if(!alive(NULL))return false;
 if(attention_checked&&(uint32_t)(now-attention_at)<10)return true;
 attention_at=now;
attention_checked=true;
  risc_resident_attention_v1 q={.struct_size=sizeof(q)};
int rc=attention.attention(attention.base.invocation,&q);
if(!alive(NULL))return false;
  if(rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_INCOMPATIBLE){if(q.struct_size!=sizeof(q)||q.supported_flags||q.pending_flags||q.monotonic_ms)return fail();
 if(rc==RISC_RESIDENT_INCOMPATIBLE){attention_available=false;
notice("Capture-safe alarm attention unavailable");
}return true;
}
  if(rc!=RISC_RESIDENT_OK||q.struct_size!=sizeof(q)||(q.supported_flags&~7u)||(q.pending_flags&~q.supported_flags))return fail();
 attention_pending=q.pending_flags;
 if(!host_alarm_supported)attention_pending&=~RISC_RESIDENT_ATTENTION_ALARM;
  if(!(q.pending_flags&6u))manual_neutral=true;
 }
 if(!attention_pending)return true;
 uint32_t manual=attention_pending&6u;
 if(manual){resident.activity=true;
int rc=attention.reject_manual(attention.base.invocation,manual);
if(!alive(NULL))return false;
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK)return fail();
 attention_pending&=~manual;
bool short_back=manual==RISC_RESIDENT_ATTENTION_CONTROLS&&manual_neutral;
manual_neutral=false;
if(short_back)back();
}
 if(!attention_pending||leaving)return true;
 if(!stop_owners())return !terminal;
 if(text_token){text_state=(risc_text_entry_state_v1_long){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
text_closing=true;
text_step();
if(text_token||terminal)return !terminal;
 }
 if(!close_ui())return !terminal;
 uint32_t flags=0;
int rc=checkpoint(RISC_RESIDENT_CHECKPOINT_ALARM,0,&flags);
if(rc==RISC_RESIDENT_BUSY)return true;
 if(flags&RISC_RESIDENT_REPLY_POLICY_REQUEST)return fail();
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)return fail();
 attention_pending=0;
idle_pending=idle_queried=false;
idle_activity();
if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF)leaving=true;
else policy_read();
if(!alive(NULL))return false;
notice("Stopped; start radio manually to start again");
explicit_refresh=true;
return true;
}
static bool alarm_stop_owners(void){
 if(!stop_owners())return false;
 if(text_token){text_state=(risc_text_entry_state_v1_long){.struct_size=sizeof(text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
text_closing=true;
text_step();
if(text_token||!alive(NULL))return false;
}
 
 if(!alarm_ui){if(!close_ui())return false;
alarm_ui=true;
dirty=explicit_refresh=true;
}
 return true;
}
static bool alarm_fallback(void){
 if(host_alarm_supported)return true;
 if(!scanner_alarm_open(&alarm_client))return false;
 if(!alarm_client.api)return true;
 bool settled=!text_token;
 if(text_token&&!text_pausing&&!text_paused&&!text_closing){
  if(text_checkpoint){risc_text_entry_checkpoint_state_v1 q={.struct_size=sizeof(q)};
int rc=text_checkpoint->checkpoint_snapshot(text_api->context,text_token,&q);
   if(!alive(NULL))return false;
   if(rc!=RISC_TEXT_ENTRY_OK||q.struct_size!=sizeof(q)||(q.flags&~15u))return fail();
   text_presenting=!!(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING);
  }else{text_step();
if(!alive(NULL))return false;
}
  settled=!text_presenting&&!text_closing;
 }
 if(ui_token){if(!snapshot())return false;
settled=!(ui_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING));
}
 alarm_status_v1 prior=alarm_client.status;
 if(!scanner_alarm_step(&alarm_client,settled,alarm_stop_owners))return false;
 if(memcmp(&prior,&alarm_client.status,sizeof(prior)))dirty=true;
 if(!alarm_client.owned&&alarm_ui){alarm_ui=false;
dirty=explicit_refresh=true;
notice("Stopped; start radio manually to start again");
}
 return true;
}
static bool handoff_step(void){
 if(!handoff_reason)return true;
 if(!stop_owners())return !terminal;
 if(!close_ui())return !terminal;
 uint32_t flags=0;
int rc=checkpoint(handoff_reason,0,&flags);
if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)return fail();
 handoff_reason=0;
idle_activity();
if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF)leaving=true;
explicit_refresh=dirty=true;
policy_read();
return !terminal;
}
static bool events(void){
 if(!ui_token)return true;
bool handled=false;
 for(unsigned i=0;i<32;i++){
  risc_scene_event_v1 e={.struct_size=sizeof(e)};
int rc=scene->next(scene->context,ui_token,&e);
if(!alive(NULL))return false;
  if(rc==RISC_SCENE_IDLE)return true;
if(rc)return fail();
  if(e.struct_size!=sizeof(e)||!e.sequence||e.sequence<=sequence)return fail();
sequence=e.sequence;
  if(e.document_revision!=document.revision||handled)continue;
resident.activity=true;
  if(e.kind==RISC_SCENE_CONTROLS_EVENT&&!e.node&&!e.action&&!e.value){if(resident.enabled&&!alarm_client.owned)handoff_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;
handled=true;
continue;
}
  if(e.kind==RISC_SCENE_SUSPEND_EVENT&&!e.node&&!e.action&&!e.value){if(!alarm_client.owned)leaving=true;
handled=true;
continue;
}
  if(alarm_ui){bool allowed=e.kind==RISC_SCENE_ACTION_EVENT&&!e.value&&(!e.node&&e.action==document.cancel_action);
   for(unsigned j=0;j<document.node_count;j++)allowed|=e.kind==RISC_SCENE_ACTION_EVENT&&!e.value&&e.node==document.nodes[j].id&&e.action&&e.action==document.nodes[j].action;
   if(allowed){action(e.action);
handled=true;
}continue;
  }
  uint32_t a=0;
if(lsp_intent(&document,&e,&a)){action(a);
handled=true;
}
  if(!alive(NULL))return false;
 }return false;
}
static void radio_step(void){
 unsigned before=radio.model.next,state=radio.model.state;
uint32_t now=millis();
if(!alive(NULL))return;
 bool changed=lss_step(&radio,now);
if(!alive(NULL))return;
 if(before!=radio.model.next)stamp(&radio.model.history[radio.model.active]);
 if(changed||state!=radio.model.state){dirty=true;
notice_text[0]=0;
}
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!rt->retain_invocation||!rt->acquire||!rt->release||!rt->yield_ms||!rt->monotonic_ms)return;
 terminal=leaving=false;
dirty=explicit_refresh=true;
alarm_ui=host_alarm_supported=false;
 ui_grant=text_grant=radio_grant=profile_grant=pref_grant=clock_grant=(risc_runtime_capability_v1){0};
 scene=NULL;
components=NULL;
lifecycle=NULL;
text_api=NULL;
text_long=NULL;
text_checkpoint=NULL;
 document=(risc_components_document_v1){0};
navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation)};
 ui_token=text_token=sequence=0;
revision=ui_flags=attention_at=attention_pending=handoff_reason=idle_at=0;
idle_ms=60000;
 attention_available=attention_checked=manual_neutral=idle_pending=idle_queried=text_idle_applied=false;
 text_closing=text_paused=text_pausing=text_presenting=pending_edit=false;
edit_field=6;
picker_return=LSP_HOME;
committed_count=0;
 memset(draft,0,sizeof(draft));
memset(entry,0,sizeof(entry));
memset(notice_text,0,sizeof(notice_text));
 lss_init(&radio,alive,retain,NULL);
view=(lora_scene_view){.model=&radio.model,.draft=draft,.notice=notice_text,.page=LSP_HOME,.radio_available=true,.text_available=true};
 if(acquire(RISC_SCENE_CAPABILITY,1,0,&ui_grant)<=0)return;
 scene=ui_grant.api;
components=risc_scene_components_get_v1(scene);
 if(!components||!scene->next||!scene->close||!scene->snapshot){release(&ui_grant);
return;
}
 lifecycle=risc_scene_lifecycle_get_v1(scene);
 scanner_alarm_init(&alarm_client,rt,alive,retain,NULL);
 if(!scanner_background_init(rt,alive,retain,NULL)||!bind_resident()||!negotiate_alarm())return;
 policy_read();
if(!alive(NULL))return;
profile_load();
if(!alive(NULL))return;
idle_activity();
 if(!lifecycle)notice("Automatic presentation updates unavailable");
 while(alive(NULL)&&!leaving){
  if(!scanner_background_capture()||!attention_step()||!alive(NULL)||leaving)break;
  if(attention_pending){rt->yield_ms(10);
continue;
}
  if(!alarm_fallback())break;
  if(alarm_client.owned){if(alarm_ui&&!text_token){(void)events();
if(alarm_client.status.state!=ALARM_STATE_CUE)(void)publish();
}if(alive(NULL))rt->yield_ms(10);
continue;
}
  if(handoff_reason){if(!handoff_step())break;
if(alive(NULL))rt->yield_ms(10);
continue;
}
  if(idle_pending&&!text_token){if(!idle_step())break;
if(alive(NULL))rt->yield_ms(10);
continue;
}
  if(pending_edit){begin_edit();
if(!alive(NULL))break;
if(pending_edit){rt->yield_ms(10);
continue;
}}
  if(text_token){
   if(text_closing||(!text_paused&&!text_pausing))text_step();
if(!alive(NULL))break;
   if(text_token&&!text_policy())break;
   if(!text_closing&&!text_paused&&!text_pausing)radio_step();
   if(alive(NULL))rt->yield_ms(10);
continue;
  }
  bool drained=events();
if(!alive(NULL)||leaving)break;
if(!drained){rt->yield_ms(10);
continue;
}
  if(pending_edit||handoff_reason)continue;
  radio_step();
if(!alive(NULL))break;
  if(!scanner_background_step(true)||!scanner_background_capture())break;
  if(ui_token&&!snapshot())break;
  if(resident.enabled&&!(ui_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))&&idle_due()){
   idle_pending=true;
idle_queried=false;
if(!idle_step())break;
  }else{if(resident.activity)idle_activity();
if(lifecycle||explicit_refresh||!ui_token)(void)publish();
}
  if(alive(NULL))rt->yield_ms(10);
 }
 if(!alive(NULL))return;
 while(alive(NULL)&&!stop_owners())rt->yield_ms(10);
 while(alive(NULL)&&!scanner_alarm_close(&alarm_client))rt->yield_ms(10);
 while(alive(NULL)&&text_token&&!text_close())rt->yield_ms(10);
 while(alive(NULL)&&ui_token&&!close_ui())rt->yield_ms(10);
 if(!alive(NULL)||!lss_unbind(&radio))return;
 if(!release(&radio_grant)||!release(&profile_grant)||!release(&pref_grant)||!release(&clock_grant))return;
 if(!scanner_background_close())return;
 (void)release(&ui_grant);
}
