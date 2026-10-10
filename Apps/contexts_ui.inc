/* Contexts Library, learning and automation controller. One model on both
 * surfaces; only drawing scale and available viewport differ. */
#include "PortableApps.h"
#include "PortableWatchKeyboard.h"
enum { CU_HOME,CU_LIBRARY,CU_ITEMS,CU_ITEM,CU_NEW,CU_NAME,CU_LEARN,CU_RULES,CU_RULE,CU_CHOOSE,CU_SETTINGS,CU_WORKFLOW,CU_ADD_STEP,CU_APPS,CU_ACTIVITY,CU_SOURCES,CU_DELETE,CU_CANCEL,CU_TRIGGER,CU_TIMER };
enum { CU_ROW_BACK=1,CU_ROW_AUTO,CU_ROW_LIBRARY,CU_ROW_RULES,CU_ROW_ACTIVITY,CU_ROW_SOURCES,CU_ROW_PLACE,CU_ROW_EVENT,CU_ROW_TRIGGER,CU_ROW_NEW,CU_ROW_SAMPLE,CU_ROW_RENAME,CU_ROW_DELETE,CU_ROW_SAVE,CU_ROW_CANCEL,CU_ROW_ENABLE,CU_ROW_WHEN,CU_ROW_SETTINGS,CU_ROW_WORKFLOW,CU_ROW_HOLD,CU_ROW_PRIORITY,CU_ROW_ADD_STEP,CU_ROW_LAUNCH,CU_ROW_TIMER,CU_ROW_MESSAGE,CU_ROW_BATTERY,CU_ROW_TIME,CU_ROW_CHARGER,CU_ROW_START,CU_ROW_ARG0,CU_ROW_ARG1,CU_ROW_CONFIRM,CU_ROW_ITEMS=100,CU_ROW_RULE=200,CU_ROW_FIELD=300,CU_ROW_STEP=400,CU_ROW_APP=500,CU_ROW_SOURCE=600,CU_ROW_DURATION=700 };
typedef struct {char title[34],detail[64];unsigned action;bool on;} cu_row;
static cr_store cu_store,cu_saved;
static cu_row cu_rows[64];
static unsigned cu_count,cu_page,cu_offset,cu_item,cu_rule,cu_kind,cu_name_reason,cu_key_page,cu_return;
static bool cu_loaded,cu_edit,cu_pending,cu_learning_saved,cu_contact,cu_moved;
static int cu_down_y;
static char cu_name[33];
static contexts_learning_v1 cu_learning;
static contexts_profile_v1 cu_profiles[24];
static uint32_t cu_profile_generation,cu_last_poll;
static const char*cu_settings_names[CR_SETTINGS]={"WATCH FACE","POWER MODE","SLEEP AFTER","DEEP SLEEP","WI-FI","BLUETOOTH","BRIGHTNESS","VOLUME","NOTIFICATIONS","DO NOT DISTURB"};
static int cu_scale(void){
#ifdef PORTABLE_CONTEXTS_PAPER
 return ctx_paper?2:1;
#else
 return 1;
#endif
}
static int cu_height(void){return ctx_app->screen_height()/cu_scale();}
static unsigned cu_visible(void){return (unsigned)(cu_height()-84)/42;}
static bool cu_ready(void){
#ifdef PORTABLE_CONTEXTS_PAPER
 if(ctx_paper)return portable_paper_scroll_settled()&&portable_paper_scroll_available()&&paper_frame_ready();
#endif
 return true;
}
static void cu_page_set(unsigned page){cu_page=page;cu_offset=0;ctx_dirty=true;cu_contact=cu_moved=false;}
static void cu_note(const char*s){ctx_note(s);}
static bool cu_call(unsigned op,void*q){const contexts_fingerprint_service_v1*f=contexts_fingerprint_api(portable_contexts_service());return f&&f->fingerprint(f->base.context,op,q);}
static void cu_profiles_read(void){for(unsigned i=0;i<24;i++){cu_profiles[i]=(contexts_profile_v1){.struct_size=sizeof(cu_profiles[i]),.index=i};if(!cu_call(CONTEXTS_FP_PROFILE,&cu_profiles[i]))cu_profiles[i].kind=0;}}
static const contexts_profile_v1*cu_profile(unsigned item){if(item>=CR_ITEMS)return NULL;const cr_item*t=&cu_store.items[item];for(unsigned i=0;i<24;i++)if(cu_profiles[i].kind==(t->kind==CR_EVENT?2u:1u)&&!strcmp(cu_profiles[i].name,t->name))return &cu_profiles[i];return NULL;}
static unsigned cu_uses(unsigned item){unsigned n=0;for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]&&cu_store.rules[i].item==item)n++;return n;}
static void cu_sync_profiles(void){
 cu_profiles_read();
 for(unsigned i=0;i<24;i++)if(cu_profiles[i].kind==1||cu_profiles[i].kind==2){unsigned k=cu_profiles[i].kind==1?CR_PLACE:CR_EVENT;bool found=false;for(unsigned j=0;j<CR_ITEMS;j++)if(cu_store.items[j].kind==k&&!strcmp(cu_store.items[j].name,cu_profiles[i].name))found=true;if(!found)for(unsigned j=0;j<CR_ITEMS;j++)if(!cu_store.items[j].kind){cu_store.items[j].kind=(uint8_t)k;memcpy(cu_store.items[j].name,cu_profiles[i].name,17);break;}}
}
static bool cu_save(void){
 if(cu_pending){cu_note("RETRY SAVE OR RELOAD");}
 cu_store.generation=cu_saved.generation+1;if(!cu_store.generation)cu_store.generation=1;
 if(!portable_contexts_rules_save(&cu_store)){cu_pending=true;cu_note("SAVE UNCONFIRMED / RETRY");return false;}
 cu_saved=cu_store;cu_edit=cu_pending=false;cu_note("SAVED");return true;
}
static void cu_add(const char*t,const char*d,unsigned action,bool on){if(cu_count>=64)return;cu_row*r=&cu_rows[cu_count++];snprintf(r->title,sizeof(r->title),"%s",t?t:"");snprintf(r->detail,sizeof(r->detail),"%s",d?d:"");r->action=action;r->on=on;}
static void cu_value(unsigned field,int32_t value,char*out,size_t size){
 if(value==CR_KEEP){snprintf(out,size,"NO CHANGE");return;}
 const char*label=NULL;
 switch(field){
 case CR_FACE:label=portable_contexts_face_name((unsigned)value);break;
 case CR_POWER:label=value==0?"NORMAL":value==1?"LOW POWER":"ULTRA";break;
 case CR_WIFI:case CR_BLUETOOTH:case CR_DND:label=value?"ON":"OFF";break;
 case CR_NOTIFY:label=value==0?"OFF":value==1?"VIBRATE":value==2?"SOUND":"BOTH";break;
 case CR_SLEEP:case CR_DEEP:if(!value)label="NEVER";else{snprintf(out,size,value>=60?"%ld MIN":"%ld SEC",(long)(value>=60?value/60:value));return;}break;
 default:if(!value&&field==CR_BRIGHTNESS)label="AUTO";else{snprintf(out,size,"%ld / 10",(long)value);return;}
 }
 snprintf(out,size,"%s",label?label:"UNAVAILABLE");
}
static bool cu_field_available(unsigned f){
#ifdef PORTABLE_CONTEXTS_PAPER
 if(ctx_paper){const uint16_t bits[]={PORTABLE_CONTEXT_FACE,PORTABLE_CONTEXT_SLEEP,PORTABLE_CONTEXT_IDLE,PORTABLE_CONTEXT_DEEP,0,0,PORTABLE_CONTEXT_BRIGHTNESS,PORTABLE_CONTEXT_VOLUME,PORTABLE_CONTEXT_ALERT,PORTABLE_CONTEXT_DND};return f<CR_SETTINGS&&(!bits[f]||(portable_contexts_action_mask()&bits[f]));}
#endif
 return f<CR_SETTINGS;
}
static void cu_build(void){
 cu_count=0;char b[64];cr_rule*r=&cu_store.rules[cu_rule];cr_item*t=&cu_store.items[cu_item];
 switch(cu_page){
 case CU_HOME:
  cu_add("AUTO SWITCH",ctx_enabled?"ON / TAP TO PAUSE":"OFF / TAP TO ENABLE",CU_ROW_AUTO,ctx_enabled);
  snprintf(b,sizeof(b),"%s",ctx_fp_valid&&ctx_fp.room_slot>=0?ctx_fp.room_name:"UNKNOWN / LEARN A PLACE");cu_add("CURRENT PLACE",b,0,false);
  cu_add("CONTEXTS","SETTINGS AND ACTIONS",CU_ROW_RULES,false);cu_add("LIBRARY","PLACES / EVENTS / TRIGGERS",CU_ROW_LIBRARY,false);cu_add("ACTIVITY","RECENT ACTIVATIONS AND RESULTS",CU_ROW_ACTIVITY,false);cu_add("SOURCES","RF / AUDIO / MATCH FEATURES",CU_ROW_SOURCES,false);break;
 case CU_LIBRARY:
  cu_add("PLACES","ROOM AND LOCATION FINGERPRINTS",CU_ROW_PLACE,false);cu_add("EVENTS","SOUNDS AND RF EVENTS",CU_ROW_EVENT,false);cu_add("TRIGGERS","BATTERY / TIME / CHARGER",CU_ROW_TRIGGER,false);break;
 case CU_ITEMS:
  for(unsigned i=0;i<CR_ITEMS;i++)if(cu_store.items[i].kind&&(cu_kind==CR_PLACE?cu_store.items[i].kind==CR_PLACE:cu_kind==CR_EVENT?cu_store.items[i].kind==CR_EVENT:cu_store.items[i].kind>=CR_SIGNAL)){
   const contexts_profile_v1*p=cu_profile(i);unsigned n=p?(p->samples[0]>p->samples[1]?p->samples[0]:p->samples[1]):0;snprintf(b,sizeof(b),"%s%s%u",cu_uses(i)?"USED BY ":"NOT USED / ",cu_uses(i)?"":"SAMPLES ",cu_uses(i)?cu_uses(i):n);cu_add(cu_store.items[i].name,b,CU_ROW_ITEMS+i,false);
  }
  cu_add("+ ADD NEW",cu_kind==CR_PLACE?"LEARN THIS PLACE":cu_kind==CR_EVENT?"LEARN AN EVENT":"CHOOSE A TRIGGER",CU_ROW_NEW,false);break;
 case CU_ITEM:{
  const contexts_profile_v1*p=cu_profile(cu_item);if(t->kind<=CR_EVENT){unsigned n=p?(p->samples[0]>p->samples[1]?p->samples[0]:p->samples[1]):0;snprintf(b,sizeof(b),"%u WINDOWS / %s",n,p&&p->sources==3?"RF + AUDIO":p&&p->sources==2?"RF":p&&p->sources==1?"AUDIO":"NO MODEL");cu_add(t->name,b,0,false);cu_add("ADD ANOTHER SAMPLE","BUILD ACROSS DAYS AND TIMES",CU_ROW_SAMPLE,false);}
  else{snprintf(b,sizeof(b),t->kind==CR_BATTERY?"BATTERY AT OR BELOW %u%%":t->kind==CR_TIME?"%02u:00 TO %02u:00":"WHILE CHARGING",t->arg0,t->arg1);cu_add(t->name,b,CU_ROW_START,false);}
  cu_add("RENAME","UP TO 16 CHARACTERS",CU_ROW_RENAME,false);
  for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]&&cu_store.rules[i].item==cu_item)cu_add(cu_store.rules[i].name,"USED BY THIS CONTEXT",CU_ROW_RULE+i,false);
  cu_add("SET UP WHAT HAPPENS","CREATE A CONTEXT FOR THIS ITEM",CU_ROW_NEW,false);cu_add("DELETE","DISABLE LINKED CONTEXTS",CU_ROW_DELETE,false);break;}
 case CU_NEW:
  if(cu_kind>=CR_SIGNAL){cu_add("LOW BATTERY","CHOOSE THRESHOLD",CU_ROW_BATTERY,false);cu_add("TIME WINDOW","INCLUDING OVERNIGHT",CU_ROW_TIME,false);cu_add("ON CHARGER","WHILE CHARGING",CU_ROW_CHARGER,false);}
  else{const char*places[]={"Living room","Kitchen","Bedroom","Office","Garage"};const char*events[]={"Doorbell","Door closing","Microwave","Kettle","Alarm"};for(unsigned i=0;i<5;i++)cu_add(cu_kind==CR_PLACE?places[i]:events[i],"CHOOSE NAME",CU_ROW_ITEMS+i,false);cu_add("CUSTOM NAME","TYPE UP TO 16 CHARACTERS",CU_ROW_RENAME,false);}break;
 case CU_LEARN:
  snprintf(b,sizeof(b),cu_kind==CR_EVENT?"%u / 3 TAKES HEARD":"%u%% / 30 SECOND CAPTURE",cu_kind==CR_EVENT?cu_learning.samples:cu_learning.progress);cu_add(cu_name,b,0,false);
  cu_add(cu_learning.state==CONTEXTS_LEARN_READY?"READY TO SAVE":cu_kind==CR_EVENT?"PERFORM THE EVENT":"STAY IN THIS PLACE",cu_kind==CR_EVENT?"LEAVE QUIET BETWEEN 3 TAKES":"MOVE NATURALLY; KEEP SAMPLING",0,false);
  if(cu_learning.state==CONTEXTS_LEARN_READY||cu_learning_saved)cu_add(cu_learning_saved?"RETRY SAVE":"SAVE TO LIBRARY","ACTIONS ARE SET UP SEPARATELY",CU_ROW_SAVE,true);
  else cu_add(cu_learning.sources==3?"RF + AUDIO":cu_learning.sources==2?"RF CAPTURE":cu_learning.sources==1?"AUDIO CAPTURE":"WAITING FOR INPUT","UNKNOWN UNTIL ENOUGH EVIDENCE",0,false);
  cu_add("CANCEL","DISCARD THIS LEARNING SESSION",CU_ROW_CANCEL,false);break;
 case CU_RULES:
  for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]){snprintf(b,sizeof(b),"%s / PRIORITY %u",cu_store.rules[i].enabled?"ON":"OFF",cu_store.rules[i].priority);cu_add(cu_store.rules[i].name,b,CU_ROW_RULE+i,cu_store.rules[i].enabled);}
  cu_add("+ NEW CONTEXT","CHOOSE WHEN, THEN WHAT HAPPENS",CU_ROW_NEW,false);break;
 case CU_RULE:
  cu_add(r->name,r->enabled?"ENABLED":"DISABLED",CU_ROW_ENABLE,r->enabled);cu_add("WHEN",cu_store.items[r->item].name,CU_ROW_WHEN,false);cu_add("WHAT CHANGES","SETTINGS / NO CHANGE BY DEFAULT",CU_ROW_SETTINGS,false);snprintf(b,sizeof(b),"%u STEPS / ON ACTIVATION",r->steps);cu_add("WHAT RUNS",b,CU_ROW_WORKFLOW,false);snprintf(b,sizeof(b),"%lu SECONDS AFTER CONDITION ENDS",(unsigned long)r->hold_seconds);cu_add("HOLD",b,CU_ROW_HOLD,false);snprintf(b,sizeof(b),"%u / LOWER NUMBER WINS",r->priority);cu_add("PRIORITY",b,CU_ROW_PRIORITY,false);cu_add("RENAME","EDIT CONTEXT NAME",CU_ROW_RENAME,false);cu_add(cu_pending?"RETRY SAVE":"SAVE CONTEXT",cu_edit?"UNSAVED CHANGES":"SAVE AND RETURN",CU_ROW_SAVE,true);cu_add("DELETE","REMOVE THIS CONTEXT",CU_ROW_DELETE,false);break;
 case CU_CHOOSE:
  for(unsigned i=0;i<CR_ITEMS;i++)if(cu_store.items[i].kind)cu_add(cu_store.items[i].name,cu_store.items[i].kind==CR_PLACE?"PLACE":cu_store.items[i].kind==CR_EVENT?"EVENT":"TRIGGER",CU_ROW_ITEMS+i,false);
  if(!cu_count)cu_add("LIBRARY IS EMPTY","LEARN A PLACE OR EVENT FIRST",CU_ROW_LIBRARY,false);
  break;
 case CU_SETTINGS:
  for(unsigned i=0;i<CR_SETTINGS;i++){if(cu_field_available(i))cu_value(i,r->settings[i],b,sizeof(b));else snprintf(b,sizeof(b),"UNAVAILABLE / TAP TO CLEAR");cu_add(cu_settings_names[i],b,CU_ROW_FIELD+i,r->settings[i]!=CR_KEEP);}break;
 case CU_WORKFLOW:
  for(unsigned i=0;i<r->steps;i++){cr_step*w=&r->workflow[i];if(w->kind==CR_TIMER)snprintf(b,sizeof(b),"%lu MIN / TAP TO REMOVE",(unsigned long)(w->value/60));if(w->kind!=CR_TIMER)snprintf(b,sizeof(b),"%.32s / TAP TO REMOVE",w->text);cu_add(w->kind==CR_LAUNCH?"OPEN APP":w->kind==CR_TIMER?"START TIMER":"SHOW MESSAGE",b,CU_ROW_STEP+i,false);}if(r->steps<CR_STEPS)cu_add("+ ADD STEP","RUN ONCE ON ACTIVATION",CU_ROW_ADD_STEP,false);break;
 case CU_TIMER:{const unsigned minutes[]={5,10,15,25,45};for(unsigned i=0;i<5;i++){snprintf(b,sizeof(b),"%u MINUTES",minutes[i]);cu_add(b,"START ON ACTIVATION",CU_ROW_DURATION+i,false);}break;}
 case CU_ADD_STEP:cu_add("LAUNCH APP","CHOOSE AN INSTALLED APP",CU_ROW_LAUNCH,false);cu_add("START TIMER","CHOOSE DURATION",CU_ROW_TIMER,false);cu_add("SHOW MESSAGE","TYPE A SHORT MESSAGE",CU_ROW_MESSAGE,false);break;
 case CU_APPS:for(unsigned i=0;i<portable_catalog_count&&i<40;i++)if(portable_catalog[i].compatible)cu_add(portable_catalog[i].display_name,portable_catalog[i].file_name,CU_ROW_APP+i,false);if(!cu_count)cu_add("CATALOG UNAVAILABLE","NO APP CAN BE SELECTED",0,false);break;
 case CU_ACTIVITY:
  for(unsigned i=0;i<CR_LOGS;i++){contexts_log_v1 q={.struct_size=sizeof(q),.index=i};if(!cu_call(CONTEXTS_FP_LOG,&q))break;snprintf(b,sizeof(b),"%s / %lu SEC",q.entry.kind==CR_ENTER?"ACTIVATED":q.entry.kind==CR_LEAVE?"INACTIVE":q.entry.kind==CR_APPLIED?"SETTINGS APPLIED":q.entry.kind==CR_RAN?"ACTION RAN":"ACTION FAILED",(unsigned long)(q.entry.at_ms/1000));cu_add(q.entry.name,b,0,false);}if(!cu_count)cu_add("NO ACTIVITY YET","CONTEXT CHANGES APPEAR HERE",0,false);break;
 case CU_SOURCES:
  for(unsigned i=0;i<2;i++){const contexts_source_status_v1*v=i?&ctx_status.radio:&ctx_status.audio;const char*d=v->model_state==CONTEXTS_MODEL_UNAVAILABLE?"UNAVAILABLE ON THIS DEVICE":!(ctx_fp.sources&(1u<<i))?"OFF":ctx_fp.feature_flags[i]&4?"ON / BURST TIMING READY":ctx_fp.feature_flags[i]&2?"ON / SPECTRUM; BUILDING TIMING":"ON / WAITING FOR SAMPLES";cu_add(i?"RF":"AUDIO",d,CU_ROW_SOURCE+i,ctx_fp.sources&(1u<<i));}cu_add("MATCH FEATURES",ctx_fp.temporal_only?"REPETITION TIMING ONLY":"TIMING + SPECTRUM",CU_ROW_SOURCE+2,ctx_fp.temporal_only);break;
 case CU_DELETE:cu_add("DELETE THIS ITEM?","THIS CHANGE MUST BE SAVED",0,false);cu_add("DELETE","CONFIRM REMOVAL",CU_ROW_CONFIRM,false);cu_add("KEEP","RETURN WITHOUT DELETING",CU_ROW_CANCEL,true);break;
 case CU_CANCEL:cu_add("DISCARD CHANGES?","UNSAVED LEARNING / EDITS",0,false);cu_add("DISCARD","RETURN TO SAVED STATE",CU_ROW_CONFIRM,false);cu_add("KEEP EDITING","CONTINUE",CU_ROW_CANCEL,true);break;
 case CU_TRIGGER:
  cu_add(t->name,"TRIGGER SETTINGS",CU_ROW_RENAME,false);if(t->kind==CR_BATTERY){snprintf(b,sizeof(b),"%u%% / TAP +5",t->arg0);cu_add("BATTERY THRESHOLD",b,CU_ROW_ARG0,false);}if(t->kind==CR_TIME){snprintf(b,sizeof(b),"%02u:00 / TAP +1 HOUR",t->arg0);cu_add("START",b,CU_ROW_ARG0,false);snprintf(b,sizeof(b),"%02u:00 / TAP +1 HOUR",t->arg1);cu_add("END",b,CU_ROW_ARG1,false);}cu_add("SAVE TRIGGER","AVAILABLE FOR CONTEXTS",CU_ROW_SAVE,true);break;
 default:break;
 }
}
static const char*cu_title(void){switch(cu_page){case CU_HOME:return "CONTEXTS";case CU_LIBRARY:return "LIBRARY";case CU_ITEMS:return cu_kind==CR_PLACE?"PLACES":cu_kind==CR_EVENT?"EVENTS":"TRIGGERS";case CU_ITEM:return cu_store.items[cu_item].name;case CU_RULES:return "CONTEXTS";case CU_RULE:return "EDIT CONTEXT";case CU_SETTINGS:return "WHAT CHANGES";case CU_WORKFLOW:case CU_ADD_STEP:return "WHAT RUNS";case CU_ACTIVITY:return "ACTIVITY";case CU_SOURCES:return "SOURCES";case CU_LEARN:return "LEARNING";case CU_NAME:return "NAME";case CU_CHOOSE:return "WHEN";default:return "SET UP";}}
static void cu_text(int x,int y,int width,const char*s,bool heading,bool bright){
#ifdef PORTABLE_CONTEXTS_PAPER
 if(ctx_paper){ctxp_text(x*2,y*2,width*2,s,heading,true);return;}
#endif
 portable_nova_text(heading?0:2,x,y,width,s,bright?NOVA_CYAN:NOVA_TEXT);
}
static void cu_line(int x,int y,int width){
#ifdef PORTABLE_CONTEXTS_PAPER
 if(ctx_paper){ctx_app->fill_rect(x*2,y*2,width*2,2,true);return;}
#endif
 portable_nova_rule(x,y,width);
}
static void cu_button(int x,int y,int width,int height,const char*s,bool on){
#ifdef PORTABLE_CONTEXTS_PAPER
 if(ctx_paper){ctxp_button(x*2,y*2,width*2,height*2,s,on);return;}
#endif
 portable_nova_button(x,y,width,height,s,on);
}
static void cu_draw(void){
 if(!cu_ready())return;
#ifdef PORTABLE_CONTEXTS_PAPER
 if(ctx_paper)ctx_paper->begin();else
#endif
 portable_nova_begin();
 int h=cu_height(),top=cu_scale()==2?30:8;cu_text(12,top,216,cu_title(),true,true);cu_line(12,top+23,216);
 if(cu_page==CU_NAME){cu_text(12,top+31,216,cu_name,false,false);for(unsigned k=0;k<PWK_COUNT;k++){portable_watch_key_rect r;portable_watch_key_bounds(k,&r);char b[2]={0};const char*label=k==PWK_PAGE?"ABC":k==PWK_DELETE?"DEL":k==PWK_DONE?"DONE":b;if(k<PWK_CHARACTERS)b[0]=(char)portable_watch_key_character(cu_key_page,k);cu_button(r.x,r.y+(cu_scale()==2?50:0),r.w,r.h,label,false);}}
 else{cu_build();unsigned visible=cu_visible();if(cu_offset>=cu_count)cu_offset=cu_count?cu_count-1:0;for(unsigned i=0;i<visible&&i+cu_offset<cu_count;i++){cu_row*r=&cu_rows[i+cu_offset];int y=top+30+(int)i*42;cu_text(16,y+3,206,r->title,false,r->on);cu_text(16,y+20,206,r->detail,false,false);cu_line(12,y+39,216);}}
 cu_text(12,h-49,216,ctx_message?ctx_message:"",false,false);
 cu_button(12,h-29,38,25,"<",false);cu_button(56,h-29,128,25,"BACK",false);cu_button(190,h-29,38,25,">",false);
 ctx_app->present(false);ctx_dirty=false;
}
static bool cu_item_slot(void){for(unsigned i=0;i<CR_ITEMS;i++)if(!cu_store.items[i].kind){cu_item=i;return true;}cu_note("LIBRARY FULL / DELETE AN ITEM");return false;}
static bool cu_rule_slot(void){for(unsigned i=0;i<CR_RULES;i++)if(!cu_store.rules[i].name[0]){cu_rule=i;cr_rule_init(&cu_store.rules[i]);cu_edit=true;return true;}cu_note("CONTEXTS FULL / DELETE ONE");return false;}
static void cu_name_open(unsigned reason,const char*name){cu_name_reason=reason;snprintf(cu_name,sizeof(cu_name),"%s",name?name:"");cu_key_page=PWK_INITIAL_PAGE;cu_return=cu_page;cu_page_set(CU_NAME);}
static void cu_learn_begin(void){
 if(!ctx_enabled){if(!portable_contexts_enable(true)){cu_note("ENABLE MONITORING FAILED");return;}ctx_enabled=true;}
 cu_learning=(contexts_learning_v1){.struct_size=sizeof(cu_learning),.operation=CONTEXTS_LEARN_BEGIN,.kind=cu_kind==CR_EVENT?2:1};memcpy(cu_learning.name,cu_name,17);
 if(!cu_call(CONTEXTS_FP_LEARN,&cu_learning)){cu_note("INPUT UNAVAILABLE / CHECK SOURCES");return;}
 cu_learning_saved=false;cu_page_set(CU_LEARN);cu_note(cu_kind==CR_EVENT?"PERFORM 3 TAKES WITH QUIET BETWEEN":"CAPTURE RUNS FOR 30 SECONDS");
}
static void cu_back(void){
 if(cu_page==CU_NAME){cu_page_set(cu_return);return;}
 if(cu_page==CU_LEARN){cu_return=CU_LEARN;cu_page_set(CU_CANCEL);return;}
 if(cu_page==CU_RULE||cu_page==CU_TRIGGER){if(cu_edit||cu_pending){cu_return=cu_page;cu_page_set(CU_CANCEL);}else cu_page_set(cu_page==CU_RULE?CU_RULES:CU_ITEMS);return;}
 if(cu_page==CU_SETTINGS||cu_page==CU_WORKFLOW||cu_page==CU_CHOOSE){cu_page_set(CU_RULE);return;}
 if(cu_page==CU_ADD_STEP||cu_page==CU_APPS||cu_page==CU_TIMER){cu_page_set(CU_WORKFLOW);return;}
 if(cu_page==CU_ITEM||cu_page==CU_NEW){cu_page_set(CU_ITEMS);return;}
 if(cu_page==CU_ITEMS){cu_page_set(CU_LIBRARY);return;}
 if(cu_page==CU_DELETE||cu_page==CU_CANCEL){cu_page_set(cu_return);return;}
 if(cu_page!=CU_HOME){cu_page_set(CU_HOME);return;}
 ctx_back();
}
static void cu_name_done(void){
 unsigned limit=cu_name_reason==4?33:17;if(!cr_name(cu_name,limit)){cu_note("ENTER A NAME FIRST");return;}
 if(cu_name_reason==0){cu_learn_begin();return;}
 if(cu_name_reason==1){
  cr_item*t=&cu_store.items[cu_item];const contexts_profile_v1*p=cu_profile(cu_item);
  if(t->kind<=CR_EVENT&&p){contexts_profile_v1 q=*p;q.operation=CONTEXTS_PROFILE_RENAME;memcpy(q.new_name,cu_name,17);if(!cu_call(CONTEXTS_FP_PROFILE,&q)||!portable_contexts_models_save()){cu_note("MODEL SAVE FAILED / RETRY");return;}}
  memcpy(t->name,cu_name,17);cu_edit=true;if(cu_save()){cu_profiles_read();cu_page_set(CU_ITEM);}return;
 }
 if(cu_name_reason==2)memcpy(cu_store.rules[cu_rule].name,cu_name,17);
 if(cu_name_reason==3)memcpy(cu_store.items[cu_item].name,cu_name,17);
 if(cu_name_reason==4){cr_rule*r=&cu_store.rules[cu_rule];if(r->steps<CR_STEPS){cr_step*w=&r->workflow[r->steps++];memset(w,0,sizeof(*w));w->kind=CR_MESSAGE;memcpy(w->text,cu_name,33);}}
 cu_edit=true;cu_page_set(cu_name_reason==3?CU_TRIGGER:cu_name_reason==4?CU_WORKFLOW:CU_RULE);
}
static void cu_cycle(unsigned f){
 if(!cu_field_available(f)){cu_store.rules[cu_rule].settings[f]=CR_KEEP;cu_edit=true;cu_note("UNAVAILABLE ON THIS DEVICE");return;}
 int32_t*v=&cu_store.rules[cu_rule].settings[f];const int32_t idle[]={-1,15,30,60,300,0},deep[]={-1,600,1800,3600,14400,0};
 if(f==CR_SLEEP||f==CR_DEEP){const int32_t*a=f==CR_SLEEP?idle:deep;unsigned at=0;while(at<6&&a[at]!=*v)at++;*v=a[(at+1)%6];}
 else if(f==CR_FACE){unsigned n=portable_contexts_face_count();if(!n){cu_note("FACES UNAVAILABLE ON THIS DEVICE");return;}*v=*v+1>=(int32_t)n?-1:*v+1;}
 else{int max=f==CR_POWER?2:f==CR_NOTIFY?3:f==CR_BRIGHTNESS||f==CR_VOLUME?10:1;*v=*v>=max?-1:*v+1;if(f==CR_BRIGHTNESS&&*v==0)*v=1;}
 cu_edit=true;ctx_dirty=true;
}
static void cu_action(unsigned action){
 if(cu_pending&&action!=CU_ROW_SAVE&&action!=CU_ROW_CANCEL){cu_note("RETRY SAVE OR DISCARD");return;}
 cr_rule*r=&cu_store.rules[cu_rule];cr_item*t=&cu_store.items[cu_item];
 if(action>=CU_ROW_DURATION&&action<CU_ROW_DURATION+5){const unsigned minutes[]={5,10,15,25,45};if(r->steps<CR_STEPS){r->workflow[r->steps++]=(cr_step){.kind=CR_TIMER,.value=minutes[action-CU_ROW_DURATION]*60};cu_edit=true;}cu_page_set(CU_WORKFLOW);return;}
 if(action>=CU_ROW_SOURCE&&action<CU_ROW_SOURCE+3){unsigned i=action-CU_ROW_SOURCE;ctx_model_source=i<2?i:0;ctx_fingerprint_setting(i==2);ctx_read_status();return;}
 if(action>=CU_ROW_APP&&action<CU_ROW_APP+portable_catalog_count){unsigned i=action-CU_ROW_APP;if(r->steps<CR_STEPS){cr_step*w=&r->workflow[r->steps++];memset(w,0,sizeof(*w));w->kind=CR_LAUNCH;snprintf(w->text,sizeof(w->text),"%s",portable_catalog[i].file_name);cu_edit=true;}cu_page_set(CU_WORKFLOW);return;}
 if(action>=CU_ROW_STEP&&action<CU_ROW_STEP+CR_STEPS){unsigned i=action-CU_ROW_STEP;if(i<r->steps){for(unsigned j=i;j+1<r->steps;j++)r->workflow[j]=r->workflow[j+1];memset(&r->workflow[--r->steps],0,sizeof(cr_step));cu_edit=true;ctx_dirty=true;}return;}
 if(action>=CU_ROW_FIELD&&action<CU_ROW_FIELD+CR_SETTINGS){cu_cycle(action-CU_ROW_FIELD);return;}
 if(action>=CU_ROW_RULE&&action<CU_ROW_RULE+CR_RULES){cu_rule=action-CU_ROW_RULE;cu_edit=false;cu_page_set(CU_RULE);return;}
 if(action>=CU_ROW_ITEMS&&action<CU_ROW_ITEMS+CR_ITEMS){unsigned i=action-CU_ROW_ITEMS;
  if(cu_page==CU_NEW){snprintf(cu_name,sizeof(cu_name),"%s",cu_rows[i].title);cu_learn_begin();}
  else if(cu_page==CU_CHOOSE){r->item=(uint8_t)i;if(!r->name[0])memcpy(r->name,cu_store.items[i].name,17);cu_edit=true;cu_page_set(CU_RULE);}
  else{cu_item=i;cu_page_set(CU_ITEM);}return;
 }
 switch(action){
 case CU_ROW_AUTO:if(portable_contexts_enable(!ctx_enabled)){ctx_enabled=!ctx_enabled;cu_note(ctx_enabled?"AUTO SWITCH ON":"AUTO SWITCH OFF");}else cu_note("SAVE FAILED / RETRY");break;
 case CU_ROW_LIBRARY:cu_page_set(CU_LIBRARY);break;case CU_ROW_RULES:cu_page_set(CU_RULES);break;case CU_ROW_ACTIVITY:cu_page_set(CU_ACTIVITY);break;case CU_ROW_SOURCES:cu_page_set(CU_SOURCES);break;
 case CU_ROW_PLACE:case CU_ROW_EVENT:case CU_ROW_TRIGGER:cu_kind=action==CU_ROW_PLACE?CR_PLACE:action==CU_ROW_EVENT?CR_EVENT:CR_BATTERY;cu_page_set(CU_ITEMS);break;
 case CU_ROW_NEW:
  if(cu_page==CU_RULES){if(cu_rule_slot())cu_page_set(CU_CHOOSE);}
  else if(cu_page==CU_ITEM){if(cu_rule_slot()){r=&cu_store.rules[cu_rule];r->item=(uint8_t)cu_item;memcpy(r->name,t->name,17);cu_page_set(CU_RULE);}}
  else if(cu_item_slot())cu_page_set(CU_NEW);
  break;
 case CU_ROW_SAMPLE:cu_kind=t->kind;memcpy(cu_name,t->name,17);cu_learn_begin();break;
 case CU_ROW_RENAME:cu_name_open(cu_page==CU_NEW?0:cu_page==CU_RULE?2:cu_page==CU_TRIGGER?3:1,cu_page==CU_NEW?"":cu_page==CU_RULE?r->name:t->name);break;
 case CU_ROW_ENABLE:r->enabled=!r->enabled;cu_edit=true;ctx_dirty=true;break;
 case CU_ROW_WHEN:cu_page_set(CU_CHOOSE);break;case CU_ROW_SETTINGS:cu_page_set(CU_SETTINGS);break;case CU_ROW_WORKFLOW:cu_page_set(CU_WORKFLOW);break;
 case CU_ROW_HOLD:{const uint32_t holds[]={0,20,60,300,900};unsigned i=0;while(i<5&&holds[i]!=r->hold_seconds)i++;r->hold_seconds=holds[(i+1)%5];cu_edit=true;ctx_dirty=true;break;}
 case CU_ROW_PRIORITY:r->priority=r->priority%9+1;cu_edit=true;ctx_dirty=true;break;
 case CU_ROW_ADD_STEP:cu_page_set(CU_ADD_STEP);break;case CU_ROW_LAUNCH:cu_page_set(CU_APPS);break;
 case CU_ROW_TIMER:cu_page_set(CU_TIMER);break;
 case CU_ROW_MESSAGE:cu_name_open(4,"");break;
 case CU_ROW_BATTERY:case CU_ROW_TIME:case CU_ROW_CHARGER:
  memset(t,0,sizeof(*t));t->kind=action==CU_ROW_BATTERY?CR_BATTERY:action==CU_ROW_TIME?CR_TIME:CR_CHARGER;t->arg0=t->kind==CR_BATTERY?20:22;t->arg1=7;snprintf(t->name,sizeof(t->name),"%s",t->kind==CR_BATTERY?"Low battery":t->kind==CR_TIME?"Night time":"On charger");cu_edit=true;cu_page_set(CU_TRIGGER);break;
 case CU_ROW_START:cu_page_set(CU_TRIGGER);break;
 case CU_ROW_ARG0:t->arg0=t->kind==CR_BATTERY?(t->arg0>=50?5:t->arg0+5):(t->arg0+1)%24;cu_edit=true;ctx_dirty=true;break;
 case CU_ROW_ARG1:t->arg1=(t->arg1+1)%24;cu_edit=true;ctx_dirty=true;break;
 case CU_ROW_SAVE:
  if(cu_page==CU_LEARN){
   if(!cu_learning_saved){cu_learning.operation=CONTEXTS_LEARN_SAVE;if(!cu_call(CONTEXTS_FP_LEARN,&cu_learning)){cu_note("MORE EVIDENCE NEEDED");break;}cu_learning_saved=true;}
   if(!portable_contexts_models_save()){cu_note("MODEL SAVE FAILED / RETRY");break;}
   t->kind=(uint8_t)cu_kind;memcpy(t->name,cu_name,17);cu_edit=true;if(cu_save()){cu_sync_profiles();cu_page_set(CU_ITEM);cu_note("LEARNED / SET UP ACTIONS WHEN READY");}
  }else if(cu_save())cu_page_set(cu_page==CU_RULE?CU_RULES:CU_ITEM);break;
 case CU_ROW_DELETE:cu_return=cu_page;cu_page_set(CU_DELETE);break;
 case CU_ROW_CANCEL:if(cu_page==CU_LEARN){cu_return=CU_LEARN;cu_page_set(CU_CANCEL);}else cu_page_set(cu_return);break;
 case CU_ROW_CONFIRM:
  if(cu_page==CU_CANCEL){if(cu_return==CU_LEARN&&!cu_learning_saved){cu_learning.operation=CONTEXTS_LEARN_CANCEL;(void)cu_call(CONTEXTS_FP_LEARN,&cu_learning);}cu_store=cu_saved;cu_edit=cu_pending=false;cu_page_set(CU_HOME);
#ifdef PORTABLE_CONTEXTS_PAPER
   if(ctx_paper)ctx_paper_discarded();
#endif
  }
  else if(cu_return==CU_RULE){cr_rule_init(r);if(cu_save())cu_page_set(CU_RULES);}
  else{
   const contexts_profile_v1*p=cu_profile(cu_item);if(t->kind<=CR_EVENT&&p){contexts_profile_v1 q=*p;q.operation=CONTEXTS_PROFILE_DELETE;if(!cu_call(CONTEXTS_FP_PROFILE,&q)||!portable_contexts_models_save()){cu_note("MODEL DELETE FAILED / RETRY");break;}}
   for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]&&cu_store.rules[i].item==cu_item)cr_rule_init(&cu_store.rules[i]);
   memset(t,0,sizeof(*t));if(cu_save())cu_page_set(CU_ITEMS);
  }break;
 default:break;
 }
}
static void cu_tap(int x,int y){
 int h=cu_height();if(y>=h-29){if(x<52){if(cu_offset)cu_offset--;ctx_dirty=true;}else if(x>=188){if(cu_offset+cu_visible()<cu_count)cu_offset++;ctx_dirty=true;}else cu_back();return;}
 if(cu_page==CU_NAME){int key=portable_watch_key_hit(x,y-(cu_scale()==2?50:0));if(key<0)return;if(key==PWK_PAGE)cu_key_page=(cu_key_page+1)%PWK_PAGES;else if(key==PWK_DELETE){size_t n=strlen(cu_name);if(n)cu_name[n-1]=0;}else if(key==PWK_DONE){cu_name_done();return;}else{size_t n=strlen(cu_name);unsigned c=portable_watch_key_character(cu_key_page,(unsigned)key);if(c&&n<(cu_name_reason==4?32u:16u)){cu_name[n]=(char)c;cu_name[n+1]=0;}}ctx_dirty=true;return;}
 int top=(cu_scale()==2?30:8)+30;if(y<top)return;unsigned row=(unsigned)(y-top)/42+cu_offset;if(row<cu_count&&row<cu_offset+cu_visible())cu_action(cu_rows[row].action);
}
static bool cu_unsaved(void){return cu_edit||cu_pending||(cu_page==CU_LEARN&&!cu_learning_saved);}
#ifdef PORTABLE_CONTEXTS_PAPER
static bool cu_before_launch(const char*destination){if(!cu_unsaved())return true;ctxp_destination=destination;cu_return=cu_page;cu_page_set(CU_CANCEL);cu_note("DISCARD OR KEEP EDITING");return false;}
#endif
static void cu_run(void){
 cu_page=CU_HOME;cu_offset=cu_rule=cu_item=0;cu_edit=cu_pending=cu_learning_saved=cu_contact=cu_moved=false;cu_last_poll=cu_profile_generation=0;
 (void)cu_unsaved;cu_loaded=portable_contexts_rules_read(&cu_store);if(!cu_loaded){cr_init(&cu_store);cu_note("CONTEXT STORAGE UNAVAILABLE");}else{cu_sync_profiles();cu_note("");}cu_saved=cu_store;
 while(!ctx_exit){
#ifdef PORTABLE_CONTEXTS_PAPER
  if(ctx_paper&&(ctx_retained||portable_adapter_retained())){portable_adapter_retain();return;}
#endif
  if(ctx_retained)ctx_retain();
  uint32_t now=ctx_app->millis();
  if(cu_ready()&&(!cu_last_poll||(uint32_t)(now-cu_last_poll)>=500)){
   if(ctx_read_status()&&(cu_page==CU_HOME||cu_page==CU_SOURCES))ctx_dirty=true;
   if(cu_page==CU_LEARN&&!cu_learning_saved){unsigned prior=cu_learning.progress,state=cu_learning.state;cu_learning.operation=CONTEXTS_LEARN_POLL;(void)cu_call(CONTEXTS_FP_LEARN,&cu_learning);if(prior!=cu_learning.progress||state!=cu_learning.state)ctx_dirty=true;}
   cu_last_poll=now;
  }
  if(ctx_dirty)cu_draw();
  t5_app_input_t input={0};if(!ctx_app->poll(&input,20))break;
  if(input.exit_requested||input.buttons&T5_APP_BUTTON_BACK){cu_back();continue;}
  if(!cu_ready()){cu_contact=cu_moved=false;continue;}
  if(input.buttons&T5_APP_BUTTON_UP){if(cu_offset)cu_offset--;ctx_dirty=true;}
  if(input.buttons&T5_APP_BUTTON_DOWN){if(cu_offset+cu_visible()<cu_count)cu_offset++;ctx_dirty=true;}
  t5_app_contact_t contact={0};bool raw=ctx_app->struct_size>=offsetof(t5_app_api_v1,touch_contact)+sizeof(ctx_app->touch_contact)&&ctx_app->touch_contact&&ctx_app->touch_contact(&contact);
  if(raw){int y=contact.y/cu_scale();if(contact.down&&!cu_contact){cu_contact=true;cu_moved=false;cu_down_y=y;}else if(contact.down&&cu_contact){int delta=y-cu_down_y;if(delta>24||delta< -24){if(delta<0&&cu_offset+cu_visible()<cu_count)cu_offset++;if(delta>0&&cu_offset)cu_offset--;cu_down_y=y;cu_moved=true;ctx_dirty=true;}}else if(cu_contact){bool moved=cu_moved;cu_contact=false;cu_moved=false;if(!moved&&input.tapped)cu_tap(input.touch_x/cu_scale(),input.touch_y/cu_scale());}else if(input.tapped)cu_tap(input.touch_x/cu_scale(),input.touch_y/cu_scale());}
  else if(input.tapped)cu_tap(input.touch_x/cu_scale(),input.touch_y/cu_scale());
 }
 while(!portable_background_stop())ctx_runtime->yield_ms(20);
}
