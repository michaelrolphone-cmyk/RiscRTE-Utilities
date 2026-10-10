/* Original portable LoRa Messages application. The installed provider owns
 * hardware. Packets are raw broadcasts, without delivery receipts/encryption. */
#include "T5AppApi.h"
#include "utility_frame.h"
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#include "PortableRadioPolicy.h"
#include "PortableNovaUi.h"
#include "PortableWatchKeyboard.h"
#include "lora_messages_model.h"
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#include <stdio.h>
enum { PAGE_HOME,PAGE_COMPOSE,PAGE_KEYBOARD,PAGE_RF,PAGE_HISTORY,PAGE_MESSAGE,PAGE_RF_ERROR,PAGE_RADIO };
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static risc_runtime_capability_v1 grants[4];
static bool acquired[4];
static const risc_key_value_v1 *store,*preferences;
static const twatch_rtc_api_v1 *rtc;
static lm_model model;
static unsigned picker_mask,picker_return_page,page,rf_page,rf_fields,key_page,key_choice,edit_field,history_page,detail_offset,compose_offset,time_format;
static twatch_lora_config_v2 editing_profile;
static lm_message detail;
static char draft[LM_TEXT_MAX+1],entry[LM_TEXT_MAX+1];
static bool dirty;
static unsigned profile_save_state; /* 0 verified/default, 1 session-only, 2 uncertain. */
static const char *notice;
static const char *const radio_names[]={"CHOOSE RADIO","SX1262 / 433 MHz","SX1262 / 868 MHz","SX1262 / 915 MHz","SX1280 / 2.4 GHz"};
static const char *radio_name(void){return radio_names[model.choice<=4?model.choice:0];}
static const char *const field_names[]={"FREQUENCY HZ","BANDWIDTH HZ","SPREADING FACTOR","CODING RATE 4/N","POWER DBM","PREAMBLE SYMBOLS"};
static const char *field_help(unsigned field){
 static const char *const ranges[]={"Choose radio first","430000000-440000000 Hz","863000000-870000000 Hz","902000000-928000000 Hz","2400000000-2500000000 Hz"};
 if(field==0)return ranges[model.choice<=4?model.choice:0];
 if(field==1)return model.choice==4?"203125,406250,812500,1625000":"125000 / 250000 / 500000 Hz";
 if(field==4)return model.choice==4?"-18 to 13 dBm / check local limits":"-9 to 22 dBm / check local limits";
 return field==2?"5 to 12":field==3?"5 to 8":"8 to 4096";
}
static void retain(void){runtime->diagnostic("LORA cleanup-unconfirmed; invocation retained");for(;;)runtime->yield_ms(50);}
bool portable_radio_services_safe(void){return !model.uncertain;}
bool portable_radio_suspend(void){
 bool active=model.owned;
 if(!lm_stop(&model))return false;
 if(active){notice="Stopped; start again manually";dirty=true;}
 return true;
}
static void notify(const char *s){notice=s;dirty=true;}
static void release_grant(unsigned i){if(acquired[i]){if(!runtime->release(&grants[i]))retain();acquired[i]=false;grants[i]=(risc_runtime_capability_v1){0};}}
static bool acquire(unsigned i,const char *name,uint32_t version,uint64_t instance){
 if(acquired[i])return true;
 grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
 if(!runtime->acquire(name,version,instance,&grants[i]))return false;
 acquired[i]=true;return true;
}
static bool open_radio_provider(void){
 if(model.radio)return true;
 if(!acquire(0,"radio.lora",2,0)){notify("Radio unavailable; retry");return false;}
 if(!lm_selector_valid(grants[0].api)){release_grant(0);notify("Update driver for radio picker");return false;}
 model.radio=grants[0].api;return true;
}
static bool open_radio(void){
 uint8_t policy=0;
 if(!preferences && acquire(2,"storage.key-value",1,PORTABLE_TIME_FORMAT_STORE_INSTANCE))preferences=grants[2].api;
 if(!portable_radio_load(preferences,&policy)){notify("Radio policy unavailable; retry");return false;}
 if(policy&PORTABLE_RADIO_AIRPLANE){notify("Airplane mode is on");return false;}
 return open_radio_provider();
}
static void open_picker(void){
 if(!portable_radio_suspend())retain();
 picker_return_page=page;picker_mask=0;page=PAGE_RADIO;dirty=true;
 twatch_lora_profile_info_v1 info;
 if(!open_radio_provider())return;
 if(!lm_profile_info(model.radio,&info)){notify("Radio choices unavailable; retry");return;}
 picker_mask=info.supported_profiles;notice=NULL;
}
static bool save_profile(void){
 if(!store){profile_save_state=1;return false;}
 uint8_t bytes[32],check[32];uint32_t size=0;lm_profile_encode(&model.profile,model.choice,model.profile_valid,bytes);
 int32_t result=store->put(store->context,LM_PROFILE_KEY,bytes,sizeof(bytes));
 bool saved=result==RISC_KEY_VALUE_OK && store->get(store->context,LM_PROFILE_KEY,check,sizeof(check),&size)==RISC_KEY_VALUE_OK && size==sizeof(check) && !memcmp(bytes,check,sizeof(bytes));
 profile_save_state=saved?0:2;return saved;
}
static void choose_radio(unsigned choice){
 if(choice<1 || choice>4 || !(picker_mask&(1u<<(choice-1))))return;
 if(!lm_select(&model,choice)){if(model.uncertain)retain();notify("Radio choice rejected; retry");return;}
 bool saved=save_profile();page=picker_return_page;
 notify(!store?"Radio set for this session only":!saved?"Radio set; save unconfirmed":
        lm_profile_matches(&model.profile,choice)?"Radio saved / RF retained":"Radio saved / set RF next");
}
static void load_preferences(void){
 if(acquire(1,"storage.key-value",1,LM_STORE_INSTANCE)){
  const risc_key_value_v1 *v=grants[1].api;
  if(v && v->api_version==1 && v->struct_size>=sizeof(*v) && v->get && v->put){
   store=v;uint8_t bytes[32];uint32_t n=0;int32_t status=v->get(v->context,LM_PROFILE_KEY,bytes,sizeof(bytes),&n);
   if(status==RISC_KEY_VALUE_OK && lm_profile_decode(&model.profile,&model.choice,&model.profile_valid,bytes,n)){
    if(!model.choice)notice="Choose radio; saved RF retained";
   }
   else if(status!=RISC_KEY_VALUE_NOT_FOUND)notice="Saved RF invalid; configure again";
  }
 }
 if(acquire(2,"storage.key-value",1,PORTABLE_TIME_FORMAT_STORE_INSTANCE))preferences=grants[2].api;
 (void)portable_time_format_load(preferences,&time_format);
 if(acquire(3,"rtc.clock",2,0)){
  const twatch_rtc_api_v1 *v=grants[3].api;
  if(v && v->api_version==2 && v->struct_size>=sizeof(*v) && v->read)rtc=v;
 }
}
static void stamp(lm_message *message){
 twatch_rtc_time_v1 raw,t;
 if(message && rtc && rtc->read(rtc->context,&raw) && portable_time_forward(&raw,&t)){
  message->year=t.year;message->month=t.month;message->day=t.day;message->hour=t.hour;message->minute=t.minute;message->clock_valid=true;
 }
}
static void time_text(const lm_message *v,char out[32]){
 if(!v->clock_valid){snprintf(out,32,"Time unset");return;}
 if(time_format==PORTABLE_TIME_FORMAT_24)snprintf(out,32,"%02u/%02u %02u:%02u",v->month,v->day,v->hour,v->minute);
 else snprintf(out,32,"%02u/%02u %u:%02u %s",v->month,v->day,v->hour%12?v->hour%12:12,v->minute,v->hour<12?"AM":"PM");
}
static const char *message_state(const lm_message *v){return v->status==LM_IN?"RECEIVED":v->status==LM_OUT_SENT?"TRANSMITTED":v->status==LM_OUT_UNKNOWN?"SEND UNCONFIRMED":"SENDING";}
static const char *state_text(void){
 switch(model.state){
 case LM_LISTENING:return "LISTENING / RAW PACKETS";
 case LM_SENDING:return "SENDING / PLEASE WAIT";
 case LM_TRANSMITTED:return "TRANSMITTED / NO RECEIPT";
 case LM_SEND_UNKNOWN:return "SEND UNCONFIRMED";
 case LM_RADIO_ERROR:return "RADIO ERROR / RETRY";
 case LM_CLEANUP_ERROR:return "RADIO CLEANUP UNCONFIRMED";
 default:return !model.choice?"RADIO OFF / CHOOSE RADIO":!model.profile_valid?"RADIO OFF / SET RF FIRST":
        !lm_profile_matches(&model.profile,model.choice)?"RF DOES NOT MATCH RADIO":"RADIO OFF / READY";
 }
}
static bool hit(int x,int y,int l,int t,int w,int h){return portable_nova_hit(x,y,l,t,w,h);}
static void text(int y,const char *s,uint32_t color){portable_nova_text(1,12,y,216,s,color);}
static void header(const char *title){
 portable_nova_begin();portable_nova_text(1,10,12,38,"BACK",NOVA_CYAN);portable_nova_text(NOVA_FACE_ORBITRON_10,52,15,184,title,NOVA_WHITE);portable_nova_rule(12,38,216);
}
static int64_t field_value(unsigned i){switch(i){case 0:return editing_profile.frequency_hz;case 1:return editing_profile.bandwidth_hz;case 2:return editing_profile.sf;case 3:return editing_profile.coding_rate;case 4:return editing_profile.power_dbm;default:return editing_profile.preamble;}}
static void draw_keyboard(void){
 header(edit_field<6?field_names[edit_field]:"COMPOSE MESSAGE");size_t n=strlen(entry);const char *tail=n>26?entry+n-26:entry;text(47,tail,NOVA_WHITE);
 for(unsigned k=0;k<PWK_COUNT;k++){
  portable_watch_key_rect r;(void)portable_watch_key_bounds(k,&r);char label[2]={0};const char *s;
  if(k<PWK_CHARACTERS){label[0]=(char)portable_watch_key_character(key_page,k);s=label[0]==' '?"SP":label;}
  else s=k==PWK_PAGE?"PAGE":k==PWK_DELETE?"DELETE":"DONE";
  portable_nova_round(r.x,r.y,r.w,r.h,3,k==key_choice?NOVA_DIM:NOVA_LINE);portable_nova_center(1,r.x,r.y+3,r.w,s,NOVA_CYAN);
 }
 char counter[64];snprintf(counter,sizeof(counter),"%u / %u    %s",(unsigned)n,edit_field<6?11:LM_TEXT_MAX,edit_field<6?"Exact value":"ASCII / raw broadcast");text(214,notice?notice:counter,NOVA_CAP);
}
static void draw(void){
 if(!utility_frame_begin(app))return;
 char line[80],clock[32];
 if(page==PAGE_KEYBOARD){draw_keyboard();app->present(false);dirty=false;return;}
 header(page==PAGE_HOME?"LORA MESSAGES":page==PAGE_COMPOSE?"REVIEW MESSAGE":(page==PAGE_RF || page==PAGE_RF_ERROR)?(profile_save_state==2?"RF SAVE UNCERTAIN":profile_save_state==1?"RF SESSION ONLY":"RF SETTINGS"):page==PAGE_HISTORY?"SESSION HISTORY":page==PAGE_RADIO?"RADIO HARDWARE":"MESSAGE");
 if(page==PAGE_HOME){
  text(43,state_text(),NOVA_CYAN);text(61,notice?notice:profile_save_state==2?"Radio settings save unconfirmed":profile_save_state==1?"Radio settings: session only":radio_name(),NOVA_CAP);
  portable_nova_button(12,82,216,44,"COMPOSE",false);
  snprintf(line,sizeof(line),"HISTORY  %u / %u",model.count,LM_HISTORY_MAX);portable_nova_button(12,132,216,44,line,false);
  portable_nova_button(12,182,104,44,model.owned?"STOP":"LISTEN",model.listening);portable_nova_button(124,182,104,44,"SET RF",false);
 }else if(page==PAGE_COMPOSE){
  if(!draft[0])text(48,"Tap here to write a message",NOVA_TEXT);
  else for(unsigned row=0;row<5;row++){unsigned start=compose_offset+row*18;size_t length=strlen(draft);if(start>=length)break;unsigned n=(unsigned)length-start;if(n>18)n=18;memcpy(line,draft+start,n);line[n]=0;text(48+(int)row*19,line,NOVA_TEXT);}
  text(146,state_text(),NOVA_CYAN);text(166,notice?notice:model.state==LM_SEND_UNKNOWN?"Retry may send a duplicate":"Tap text to page / no receipt",NOVA_CAP);
  portable_nova_button(12,190,104,44,"EDIT",false);portable_nova_button(124,190,104,44,model.state==LM_SENDING?"CANCEL":"SEND",false);
 }else if(page==PAGE_RF){
  /* A separate 56px header target opens the radio picker without changing RF. */
  portable_nova_fill(176,0,64,37,0);portable_nova_text(1,181,12,56,"RADIO",NOVA_CYAN);
  for(unsigned row=0;row<3;row++){unsigned i=rf_page*3+row;char value[32];if(rf_fields&(1u<<i))snprintf(value,sizeof(value),"%lld",(long long)field_value(i));else snprintf(value,sizeof(value),"NOT SET");portable_nova_row(12,42+(int)row*48,216,44,field_names[i],value,false);}
  portable_nova_button(12,188,104,44,rf_page?"PREVIOUS":"NEXT",false);portable_nova_button(124,188,104,44,"APPLY",false);
 }else if(page==PAGE_HISTORY){
  if(!model.count){text(58,"No packets this session",NOVA_TEXT);text(82,"History clears when app exits",NOVA_CAP);}
  for(unsigned row=0;row<3;row++){unsigned i=history_page*3+row;lm_message *v=lm_at(&model,i);if(!v)break;time_text(v,clock);int y=44+(int)row*46;portable_nova_text(1,12,y,216,v->text,NOVA_TEXT);snprintf(line,sizeof(line),"%s  %s",v->status==LM_IN?"IN":v->status==LM_OUT_SENT?"TX":v->status==LM_OUT_UNKNOWN?"?":"...",clock);portable_nova_text(1,12,y+19,216,line,NOVA_CAP);portable_nova_rule(12,y+41,216);}
  portable_nova_button(12,188,104,44,"NEWER",false);portable_nova_button(124,188,104,44,"OLDER",false);
 }else if(page==PAGE_RADIO){
  for(unsigned choice=1;choice<=4;choice++){
   bool available=!!(picker_mask&(1u<<(choice-1)));int y=40+(int)(choice-1)*44;
   portable_nova_row(12,y,216,44,radio_names[choice],!available?"UNAVAILABLE":model.choice==choice?(profile_save_state==2?"SELECTED / UNSAVED":profile_save_state==1?"SELECTED / SESSION":"SELECTED"):"SELECT",available && model.choice==choice);
  }
  text(220,notice?notice:"Match physical radio and antenna",NOVA_CAP);
 }else if(page==PAGE_RF_ERROR){
  portable_nova_wrap(1,12,56,216,24,4,"Set all six RF values. Match radio, antenna, peers and local power limits. Radio stays off.",NOVA_TEXT);
  portable_nova_button(12,188,216,44,"BACK TO RF SETTINGS",false);
 }else{
  time_text(&detail,clock);text(44,clock,NOVA_CAP);text(63,message_state(&detail),NOVA_CYAN);
  /* Page by 72 source bytes so every byte can be reviewed on the small screen.
   * Each line is at most 18 monospaced-safe printable characters. */
  for(unsigned row=0;row<4;row++){unsigned start=detail_offset+row*18;if(start>=detail.length)break;unsigned n=detail.length-start;if(n>18)n=18;memcpy(line,detail.text+start,n);line[n]=0;text(86+(int)row*19,line,NOVA_TEXT);}
  if(detail.status==LM_IN)snprintf(line,sizeof(line),"%uB  RSSI %d  SNR %s%u.%02u",detail.length,detail.rssi,detail.snr<0?"-":"",(unsigned)(detail.snr<0?-detail.snr:detail.snr)/4,(unsigned)(detail.snr<0?-detail.snr:detail.snr)%4*25);
  else snprintf(line,sizeof(line),"%u bytes / delivery not known",detail.length);
  text(166,detail.binary?"Binary bytes shown as dots":line,NOVA_CAP);
  portable_nova_button(12,188,104,44,"PREVIOUS",false);portable_nova_button(124,188,104,44,"NEXT",false);
 }
 app->present(false);dirty=false;
}
static void edit(unsigned field){
 edit_field=field;entry[0]=0;
 if(field<6){if(rf_fields&(1u<<field))snprintf(entry,sizeof(entry),"%lld",(long long)field_value(field));}
 else memcpy(entry,draft,sizeof(draft));
 notice=NULL;key_page=field<6?0:PWK_INITIAL_PAGE;key_choice=0;page=PAGE_KEYBOARD;dirty=true;
}
static bool apply_field(void){
 int64_t n;if(!lm_number(entry,edit_field==4,&n))return false;
 switch(edit_field){
 case 0:if(n<0 || n>UINT32_MAX || !lm_frequency_matches((uint32_t)n,model.choice))return false;editing_profile.frequency_hz=(uint32_t)n;break;
 case 1:if(model.choice==4?(n!=203125 && n!=406250 && n!=812500 && n!=1625000):(n!=125000 && n!=250000 && n!=500000))return false;editing_profile.bandwidth_hz=(uint32_t)n;break;
 case 2:if(n<5 || n>12)return false;editing_profile.sf=(uint8_t)n;break;
 case 3:if(n<5 || n>8)return false;editing_profile.coding_rate=(uint8_t)n;break;
 case 4:if(model.choice==4?(n<-18 || n>13):(n<-9 || n>22))return false;editing_profile.power_dbm=(int8_t)n;break;
 case 5:if(n<8 || n>4096)return false;editing_profile.preamble=(uint16_t)n;break;
 default:return false;
 }
 rf_fields|=1u<<edit_field;return true;
}
static void key(unsigned k){
 size_t n=strlen(entry);unsigned limit=edit_field<6?11:LM_TEXT_MAX;
 if(k<PWK_CHARACTERS){unsigned ch=portable_watch_key_character(key_page,k);if(ch && n<limit){entry[n]=(char)ch;entry[n+1]=0;}}
 else if(k==PWK_PAGE)key_page=(key_page+1)%PWK_PAGES;
 else if(k==PWK_DELETE){if(n)entry[n-1]=0;}
 else if(k==PWK_DONE){
  if(edit_field<6){if(!apply_field()){notify(field_help(edit_field));return;}page=PAGE_RF;}
  else{memcpy(draft,entry,sizeof(draft));compose_offset=0;page=PAGE_COMPOSE;}
  notice=NULL;
 }
 dirty=true;
}
static void apply_profile(void){
 if(rf_fields!=63 || !lm_profile_matches(&editing_profile,model.choice)){notify("Set six valid matching RF values");page=PAGE_RF_ERROR;return;}
 if(!portable_radio_suspend())retain();
 model.profile=editing_profile;model.profile_valid=true;page=PAGE_HOME;
 if(!store){profile_save_state=1;notify("RF set for this session only");return;}
 if(!save_profile())notify("RF active; save unconfirmed");
 else notify("RF saved / radio remains off");
}
static void send_message(void){
 if(model.state==LM_SENDING){if(!portable_radio_suspend())retain();notify("Send canceled / delivery unknown");return;}
 if(!model.choice){open_picker();return;}
 if(!model.profile_valid || !lm_profile_matches(&model.profile,model.choice)){notify("Set matching RF before sending");page=PAGE_HOME;return;}
 if(!open_radio())return;
 unsigned before=model.next;bool ok=lm_send(&model,draft,app->millis());
 if(model.uncertain)retain();
 if(model.next!=before)stamp(&model.history[model.active]);
 notify(ok?"Sending; no automatic retry":model.state==LM_SEND_UNKNOWN?"Unconfirmed; retry may duplicate":"Empty text or RF/provider rejected");
}
static bool back(void){
 dirty=true;
 if(page==PAGE_HOME){
#ifdef LORA_RETURN_APP
  if(!portable_radio_suspend())retain();
  if(!runtime->request_launch || !runtime->request_launch(LORA_RETURN_APP)){notify("Return unavailable; retry");return true;}
#endif
  return false;
 }
 if(page==PAGE_KEYBOARD){page=edit_field<6?PAGE_RF:PAGE_COMPOSE;return true;}
 if(page==PAGE_RADIO){page=picker_return_page;notice=NULL;return true;}
 if(page==PAGE_MESSAGE){page=PAGE_HISTORY;return true;}
 if(page==PAGE_RF_ERROR){page=PAGE_RF;notice=NULL;return true;}
 page=PAGE_HOME;return true;
}
static bool input(t5_app_input_t *in){
 if(in->exit_requested)return false;
 if(in->buttons&T5_APP_BUTTON_BACK)return back();
 int x=in->touch_x-(app->screen_width()-240)/2,y=in->touch_y-(app->screen_height()-240)/2;
 if(in->tapped && hit(x,y,0,0,48,40))return back();
 if(page==PAGE_RF && in->tapped && hit(x,y,176,0,64,40)){open_picker();return true;}
 if(page==PAGE_RADIO){if(in->tapped && hit(x,y,12,40,216,176))choose_radio(1+(unsigned)(y-40)/44);return true;}
 if(page==PAGE_KEYBOARD){
  if(in->buttons&T5_APP_BUTTON_LEFT)key_choice=(key_choice+PWK_COUNT-1)%PWK_COUNT;
  if(in->buttons&T5_APP_BUTTON_RIGHT)key_choice=(key_choice+1)%PWK_COUNT;
  if(in->buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT))dirty=true;
  int k=in->tapped?portable_watch_key_hit(x,y):-1;
  if(k>=0)key((unsigned)k);else if(in->buttons&T5_APP_BUTTON_CONFIRM)key(key_choice);
  return true;
 }
 if(!in->tapped){if(page==PAGE_COMPOSE && (in->buttons&T5_APP_BUTTON_CONFIRM))send_message();return true;}
 if(page==PAGE_HOME){
  if(hit(x,y,12,82,216,44)){page=PAGE_COMPOSE;dirty=true;}
  else if(hit(x,y,12,132,216,44)){page=PAGE_HISTORY;history_page=0;dirty=true;}
  else if(hit(x,y,12,182,104,44)){
   if(model.owned){if(!portable_radio_suspend())retain();notify("Radio stopped");}
   else if(!model.choice)open_picker();
   else if(!model.profile_valid || !lm_profile_matches(&model.profile,model.choice))notify("Set matching RF before listening");
   else if(open_radio()){bool ok=lm_listen(&model,app->millis());if(model.uncertain)retain();notify(ok?"No encryption or receipts":"RF/provider rejected; check setup");}
  }else if(hit(x,y,124,182,104,44)){
   if(!model.choice)open_picker();
   else{if(!portable_radio_suspend())retain();
    editing_profile=model.profile;rf_fields=model.profile_valid?63:0;rf_page=0;page=PAGE_RF;dirty=true;}
  }
 }else if(page==PAGE_COMPOSE){
  if(hit(x,y,12,190,104,44))edit(6);
  else if(hit(x,y,12,44,216,95)){if(!draft[0])edit(6);else{compose_offset=compose_offset+90<strlen(draft)?compose_offset+90:0;dirty=true;}}
  else if(hit(x,y,124,190,104,44))send_message();
 }else if(page==PAGE_RF){
  if(hit(x,y,12,42,216,140)){unsigned row=(unsigned)(y-42)/48;if(row<3)edit(rf_page*3+row);}
  else if(hit(x,y,12,188,104,44)){rf_page^=1;dirty=true;}
  else if(hit(x,y,124,188,104,44))apply_profile();
 }else if(page==PAGE_HISTORY){
  if(hit(x,y,12,44,216,138)){unsigned i=history_page*3+(unsigned)(y-44)/46;lm_message *v=lm_at(&model,i);if(v){detail=*v;detail_offset=0;page=PAGE_MESSAGE;dirty=true;}}
  else if(hit(x,y,12,188,104,44)){if(history_page)history_page--;dirty=true;}
  else if(hit(x,y,124,188,104,44)){if((history_page+1)*3<model.count)history_page++;dirty=true;}
 }else if(page==PAGE_RF_ERROR){
  if(hit(x,y,12,188,216,44)){page=PAGE_RF;notice=NULL;dirty=true;}
 }else if(page==PAGE_MESSAGE){
  if(hit(x,y,12,188,104,44)){if(detail_offset>=72)detail_offset-=72;dirty=true;}
  else if(hit(x,y,124,188,104,44)){if(detail_offset+72<detail.length)detail_offset+=72;dirty=true;}
 }
 return true;
}
void app_main(void){
 utility_frame_reset();
 app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);
 if(!app || app->abi_version!=1 || app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label) || !app->poll || !app->present || !app->millis || !app->screen_width || !app->screen_height || !app->set_back_exits_app ||
    !runtime || runtime->api_version!=1 || runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !runtime->acquire || !runtime->release || !runtime->diagnostic || !runtime->yield_ms)return;
 if(app->screen_width()<240 || app->screen_height()<240)return;
 memset(&model,0,sizeof(model));memset(grants,0,sizeof(grants));memset(acquired,0,sizeof(acquired));memset(draft,0,sizeof(draft));store=preferences=NULL;rtc=NULL;notice=NULL;profile_save_state=0;page=PAGE_HOME;dirty=true;
 app->set_back_exits_app(false);load_preferences();
 for(;;){
  if(dirty)draw();
  t5_app_input_t in={0};
  if(!app->poll(&in,30)){
#ifdef PORTABLE_ALARM_CLIENT
   if(portable_app_sleep_retained())return;
#endif
   break;
  }
  if(!input(&in))break;
  unsigned state=model.state,next=model.next;bool changed=lm_step(&model,app->millis());
  if(model.uncertain)retain();
  if(next!=model.next)stamp(&model.history[model.active]);
  if(changed || state!=model.state){dirty=true;if(state!=model.state)notice=NULL;}
 }
 if(!portable_radio_suspend())retain();
 for(unsigned i=4;i>0;i--)release_grant(i-1);
 model.radio=NULL;store=preferences=NULL;rtc=NULL;
}
