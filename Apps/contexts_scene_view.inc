/* Declarative Contexts presentation. All geometry, fonts, scrolling and hit
 * targets belong to the shared NOVA presenter used by Lists. */
enum { CS_TAB=800,CS_KEY,CS_FINISH,CS_RETRY,CS_DISMISS_MESSAGE };
static risc_components_document_v1 cs_document;
static uint32_t cs_revision,cs_toast_token;
static uint64_t cs_last_event;
static char cs_toast[48],cs_workflow_message[33];
static void cs_copy(char*out,size_t n,const char*s){snprintf(out,n,"%s",s?s:"");}
static risc_scene_node_v1*cs_node(unsigned kind,const char*label,const char*text,unsigned action){
 unsigned i=cs_document.node_count++;risc_scene_node_v1*n=&cs_document.nodes[i];
 *n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action};
 cs_copy(n->label,sizeof(n->label),label);cs_copy(n->text,sizeof(n->text),text);cs_document.details[i].marker=6;return n;
}
static risc_component_detail_v1*cs_detail(risc_scene_node_v1*n){return &cs_document.details[n->id-1];}
static void cs_header(const char*title,const char*sub,bool back){cs_copy(cs_document.routes[0].title,RISC_SCENE_LABEL,title);cs_copy(cs_document.subtitle,RISC_SCENE_TEXT,sub);cs_document.routes[0].back_action=back?CU_ROW_BACK:0;}
static risc_scene_node_v1*cs_row(const char*label,const char*text,const char*value,unsigned action,unsigned symbol){risc_scene_node_v1*n=cs_node(RISC_COMPONENT_STAT_ROW,label,text,action);cs_detail(n)->symbol=symbol;cs_copy(cs_detail(n)->badge,sizeof(cs_detail(n)->badge),value);return n;}
static void cs_section(const char*s){cs_node(RISC_COMPONENT_SECTION,s,"",0);}
static void cs_say(const char*title,const char*s){cs_node(RISC_SCENE_TEXT_NODE,title,s,0);}
static void cs_button(const char*s,unsigned action,unsigned flags){cs_node(RISC_SCENE_ACTION,s,"",action)->flags=flags;}
static void cs_bounds(risc_scene_node_v1*n,int value,int min,int max){n->minimum=min;n->maximum=max;n->value=value;n->step=1;}
static void cs_switch(const char*s,unsigned action,bool value){cs_bounds(cs_node(RISC_COMPONENT_SWITCH,s,"",action),value,0,1);}
static void cs_step(const char*s,const char*value,unsigned action,int selected,int min,int max){cs_bounds(cs_node(RISC_COMPONENT_STEPPER,s,value,action),selected,min,max);}
static unsigned cs_changes(const cr_rule*r){unsigned n=0;for(unsigned i=0;i<CR_SETTINGS;i++)n+=r->settings[i]!=CR_KEEP;return n;}
static unsigned cs_samples(unsigned item){const contexts_profile_v1*p=cu_profile(item);return p?(p->samples[0]>p->samples[1]?p->samples[0]:p->samples[1]):0;}
static void cs_when(const cr_rule*r,char*out,size_t n){const cr_item*t=&cu_store.items[r->item];if(t->kind==CR_PLACE)snprintf(out,n,"WHILE AT %s",t->name);else if(t->kind==CR_EVENT)snprintf(out,n,"WHEN %s IS HEARD",t->name);else if(t->kind==CR_BATTERY)snprintf(out,n,"BATTERY AT OR BELOW %u%%",t->arg0);else if(t->kind==CR_TIME)snprintf(out,n,"BETWEEN %02u:00 AND %02u:00",t->arg0,t->arg1);else cs_copy(out,n,t->kind==CR_CHARGER?"WHILE CHARGING":t->name);}
static int cs_field_max(unsigned f){return f==CR_SLEEP||f==CR_DEEP?5:f==CR_FACE?(int)portable_contexts_face_count():f==CR_POWER?3:f==CR_NOTIFY?4:f==CR_BRIGHTNESS?10:f==CR_VOLUME?11:2;}
static int32_t cs_field_value(unsigned f,int index){static const int idle[]={-1,15,30,60,300,0},deep[]={-1,600,1800,3600,14400,0};return f==CR_BRIGHTNESS?(index?index:-1):f==CR_SLEEP?idle[index]:f==CR_DEEP?deep[index]:index-1;}
static int cs_field_index(unsigned f,int32_t value){for(int i=0;i<=cs_field_max(f);i++)if(cs_field_value(f,i)==value)return i;return 0;}
static void cs_item_rows(void){
 unsigned count=0;char sub[64];for(unsigned i=0;i<CR_ITEMS;i++){cr_item*t=&cu_store.items[i];if(!t->kind||(cu_kind<=CR_EVENT?t->kind!=cu_kind:t->kind<CR_SIGNAL))continue;
  unsigned samples=cs_samples(i),used=cu_uses(i);snprintf(sub,sizeof(sub),"%s / %u SAMPLES%s",samples>=3?"STRONG":samples==2?"GOOD":samples?"OK":"NEW",samples,used?" / USED":"");if(t->kind==CR_BATTERY)snprintf(sub,sizeof(sub),"BATTERY AT OR BELOW %u%%",t->arg0);if(t->kind==CR_TIME)snprintf(sub,sizeof(sub),"%02u:00 TO %02u:00",t->arg0,t->arg1);if(t->kind==CR_CHARGER)cs_copy(sub,sizeof(sub),"WHILE CHARGING");
  risc_scene_node_v1*n=cs_row(t->name,sub,"",CU_ROW_ITEMS+i,0);cs_detail(n)->marker=i%6;if(!used){n->kind=RISC_COMPONENT_ROW;cs_copy(cs_detail(n)->badge,16,"NOT USED");cs_detail(n)->tone=RISC_TONE_MUTED;}count++;
 }
 if(!count)cs_say("","NOTHING HERE YET.");
 cs_row(cu_kind==CR_PLACE?"ADD PLACE":cu_kind==CR_EVENT?"ADD EVENT":"ADD TRIGGER","","",CU_ROW_NEW,RISC_SYMBOL_PLUS);
}
static void cs_declare(void){
 memset(&cs_document,0,sizeof(cs_document));cs_document=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(cs_document),.revision=++cs_revision,.root=1,.route_count=1,.screen_key=1+cu_page*4096u+cu_item*32u+cu_rule};cs_document.routes[0].id=1;
 if(strcmp(cs_toast,ctx_message?ctx_message:"")){cs_copy(cs_toast,sizeof(cs_toast),ctx_message);++cs_toast_token;}
 cs_copy(cs_document.toast,sizeof(cs_document.toast),cs_toast);cs_document.toast_token=cs_toast_token;
 char b[72],value[32];cr_rule*r=&cu_store.rules[cu_rule];cr_item*t=&cu_store.items[cu_item];risc_scene_node_v1*n;
 if(!cu_loaded){cs_header("CONTEXTS","SAVED LIBRARY UNAVAILABLE",false);cs_say("RETRY STORAGE","Your saved rooms, events and contexts have not been replaced.");cs_button("RETRY",CS_RETRY,RISC_SCENE_PRIMARY);return;}
 cs_header(cu_title(),"",cu_page!=CU_HOME);
 if(cs_workflow_message[0]){cs_header("CONTEXT MESSAGE","",false);cs_document.flags=RISC_COMPONENTS_ALERT;cs_document.cancel_action=CS_DISMISS_MESSAGE;cs_say(cs_workflow_message,"");cs_button("DISMISS",CS_DISMISS_MESSAGE,0);return;}
 switch(cu_page){
 case CU_HOME:{
  cs_header("CONTEXTS","TEACH IT - THEN DECIDE",false);cs_switch("AUTO SWITCH",CU_ROW_AUTO,ctx_enabled);
  char active[48]="",summary[72]="";unsigned rules=0,items=0,logs=0;
  for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]){rules++;if(cs_active&(1u<<i)){if(active[0])snprintf(active+strlen(active),sizeof(active)-strlen(active)," + ");snprintf(active+strlen(active),sizeof(active)-strlen(active),"%s",cu_store.rules[i].name);}}
  for(unsigned i=0;i<CR_ITEMS;i++)items+=cu_store.items[i].kind!=0;
  if(ctx_fp_valid&&ctx_fp.room_slot>=0)snprintf(summary,sizeof(summary),"%s / %u%%",ctx_fp.room_name,ctx_fp.room_confidence);
  n=cs_node(RISC_COMPONENT_CARD,!ctx_enabled?"AUTO IS OFF":active[0]?active:"NOTHING ACTIVE",!ctx_enabled?"Nothing changes by itself.":active[0]?(summary[0]?summary:"Context settings are active."):"No context matches right now.",0);if(active[0]&&ctx_enabled)n->flags=RISC_SCENE_PRIMARY;
  snprintf(value,sizeof(value),"%u",rules);cs_row("CONTEXTS","WHAT SHOULD HAPPEN",value,CU_ROW_RULES,RISC_SYMBOL_SETTINGS);
  snprintf(value,sizeof(value),"%u",items);cs_row("LIBRARY","WHAT IT CAN RECOGNISE",value,CU_ROW_LIBRARY,RISC_SYMBOL_LIBRARY);
  contexts_log_v1 q={.struct_size=sizeof(q)};if(cu_call(CONTEXTS_FP_LOG,&q))logs=q.count;snprintf(value,sizeof(value),"%u",logs);cs_row("ACTIVITY","",value,CU_ROW_ACTIVITY,RISC_SYMBOL_ACTIVITY);
  cs_row("SOURCES","RF / AUDIO / MATCH FEATURES","",CU_ROW_SOURCES,RISC_SYMBOL_TRIGGER);break;}
 case CU_LIBRARY:case CU_ITEMS:
  cs_header("LIBRARY","PLACES - EVENTS - TRIGGERS",true);n=cs_node(RISC_COMPONENT_SEGMENTS,"","",CS_TAB);cs_bounds(n,cu_kind==CR_PLACE?0:cu_kind==CR_EVENT?1:2,0,2);cs_copy(cs_detail(n)->choices,72,"PLACES|EVENTS|TRIGGERS");cs_item_rows();break;
 case CU_ITEM:case CU_TRIGGER:{
  cs_header(t->name,t->kind==CR_PLACE?"PLACE":t->kind==CR_EVENT?"EVENT":"TRIGGER",true);
  if(t->kind<=CR_EVENT){unsigned samples=cs_samples(cu_item);n=cs_node(RISC_COMPONENT_STRENGTH,"STRENGTH",samples>=3?"STRONG":samples==2?"GOOD":samples?"OK":"NONE",0);n->value=samples>3?3:(int)samples;snprintf(value,sizeof(value),"%u",samples);cs_row("SAMPLES","",value,0,0);cs_button("+ ADD ANOTHER SAMPLE",CU_ROW_SAMPLE,0);cs_say("","MORE SAMPLES, AT DIFFERENT TIMES, MAKE IT MORE RELIABLE.");}
  else if(t->kind==CR_BATTERY){snprintf(value,sizeof(value),"%u%%",t->arg0);cs_step("WHEN BELOW",value,CU_ROW_ARG0,t->arg0/5,1,10);}
  else if(t->kind==CR_TIME){snprintf(value,sizeof(value),"%02u:00",t->arg0);cs_step("FROM",value,CU_ROW_ARG0,t->arg0,0,23);snprintf(value,sizeof(value),"%02u:00",t->arg1);cs_step("TO",value,CU_ROW_ARG1,t->arg1,0,23);}
  else cs_say("",t->kind==CR_CHARGER?"TRUE WHILE THE DEVICE IS ON A CHARGER.":"NEARBY SIGNAL RECOGNITION IS UNAVAILABLE.");
  if(cu_page==CU_TRIGGER&&cu_edit)cs_button(cu_pending?"RETRY SAVE":"SAVE TRIGGER",CU_ROW_SAVE,RISC_SCENE_PRIMARY);
  cs_section("USED BY");unsigned uses=0;for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]&&cu_store.rules[i].item==cu_item){snprintf(b,sizeof(b),"%u CHANGES",cs_changes(&cu_store.rules[i]));cs_row(cu_store.rules[i].name,b,"",CU_ROW_RULE+i,0);uses++;}if(!uses)cs_say("","NOT USED YET. IT IS LEARNED, BUT NOTHING HAPPENS.");
  cs_button("SET UP WHAT HAPPENS",CU_ROW_NEW,RISC_SCENE_PRIMARY);cs_row("RENAME","","",CU_ROW_RENAME,RISC_SYMBOL_EDIT);n=cs_row("DELETE","","",CU_ROW_DELETE,RISC_SYMBOL_DELETE);cs_detail(n)->tone=RISC_TONE_DANGER;break;}
 case CU_NEW:
  cs_header(cu_kind<=CR_EVENT?"NAME IT":"NEW TRIGGER",cu_kind==CR_PLACE?"PLACE":cu_kind==CR_EVENT?"EVENT":"WHAT SHOULD SET IT OFF?",true);cu_build();
  for(unsigned i=0;i<cu_count;i++){cu_row*row=&cu_rows[i];cs_node(cu_kind<=CR_EVENT?RISC_COMPONENT_CHIP:RISC_COMPONENT_STAT_ROW,row->action==CU_ROW_RENAME?"CUSTOM...":row->title,cu_kind<=CR_EVENT?"":row->detail,row->action);}break;
 case CU_INTRO:
  cs_header(cu_name,cu_add_sample?"ADDING ANOTHER SAMPLE":"READY WHEN YOU ARE",true);n=cs_node(RISC_COMPONENT_EMPTY,cu_kind==CR_EVENT?"THREE TAKES":"LEARN THIS PLACE",cu_kind==CR_EVENT?"Make the event three times. Leave quiet between each take.":"Stand in the room and carry on as normal. This takes about 30 seconds.",0);cs_detail(n)->symbol=cu_kind==CR_EVENT?RISC_SYMBOL_EVENT:RISC_SYMBOL_PLACE;cs_button("START",CU_ROW_START,RISC_SCENE_PRIMARY);break;
 case CU_LEARN:
  snprintf(b,sizeof(b),cu_kind==CR_EVENT?"TAKE %u OF 3":"LEARNING",cu_learning.samples<3?cu_learning.samples+1:3);cs_header(cu_name,b,false);
  n=cs_node(RISC_COMPONENT_RING,"","",0);n->maximum=100;n->value=(int)(cu_learning.progress>100?100:cu_learning.progress);snprintf(cs_detail(n)->badge,16,"%u",cu_kind==CR_EVENT?(cu_learning.samples<3?cu_learning.samples+1:3):(30u*(100u-cu_learning.progress)/100u));
  cs_say(cu_learning.state==CONTEXTS_LEARN_READY?"READY TO SAVE":cu_kind==CR_EVENT?"MAKE THE EVENT":"LEARNING... STAY HERE",cu_kind==CR_EVENT?"LEAVE QUIET BETWEEN TAKES.":"COLLECTING TEMPORAL AND SPECTRAL EVIDENCE.");
  if(cu_learning.state==CONTEXTS_LEARN_READY||cu_learning_saved)cs_button(cu_learning_saved?"RETRY SAVE":"SAVE TO LIBRARY",CU_ROW_SAVE,RISC_SCENE_PRIMARY);
  cs_button("CANCEL",CU_ROW_CANCEL,0);break;
 case CU_DONE:
  cs_header("ALL SET",cu_name,false);snprintf(b,sizeof(b),"%s LEARNED",cu_name);cs_node(RISC_COMPONENT_EMPTY,b,"",0);n=cs_node(RISC_COMPONENT_STRENGTH,"STRENGTH",cs_samples(cu_item)>=3?"STRONG":cs_samples(cu_item)==2?"GOOD":"OK",0);n->value=cs_samples(cu_item)>3?3:(int)cs_samples(cu_item);cs_say("",cu_uses(cu_item)?"ALREADY USED BY A CONTEXT.":"NOTHING HAPPENS YET. YOU CAN DECIDE NOW OR LATER.");cs_button("SET UP WHAT HAPPENS",CU_ROW_NEW,RISC_SCENE_PRIMARY);cs_button("DONE - DECIDE LATER",CS_FINISH,0);break;
 case CU_RULES:
  cs_header("CONTEXTS","WHAT SHOULD HAPPEN",true);for(unsigned i=0;i<CR_RULES;i++)if(cu_store.rules[i].name[0]){cr_rule*rule=&cu_store.rules[i];cs_when(rule,b,sizeof(b));n=cs_row(rule->name,b,rule->enabled?"":"OFF",CU_ROW_RULE+i,0);cs_detail(n)->marker=i%6;}cs_row("NEW CONTEXT","","",CU_ROW_NEW,RISC_SYMBOL_PLUS);break;
 case CU_CHOOSE:
  cs_header("WHEN...","PICK FROM YOUR LIBRARY",true);for(unsigned k=1;k<=3;k++){cs_section(k==1?"PLACES":k==2?"EVENTS":"TRIGGERS");for(unsigned i=0;i<CR_ITEMS;i++)if(cu_store.items[i].kind&&(k==3?cu_store.items[i].kind>=3:cu_store.items[i].kind==k)){n=cs_row(cu_store.items[i].name,"","",CU_ROW_ITEMS+i,0);cs_detail(n)->marker=i%6;}}cs_say("","NOT HERE? ADD IT IN THE LIBRARY FIRST.");cs_row("GO TO LIBRARY","","",CU_ROW_LIBRARY,0);break;
 case CU_RULE:{
  cs_header(r->name,"CONTEXT",true);cs_section("WHEN");cs_when(r,b,sizeof(b));cs_row(b,"","",CU_ROW_WHEN,RISC_SYMBOL_TRIGGER);cs_section("WHAT CHANGES");snprintf(value,sizeof(value),"%u CHANGES",cs_changes(r));cs_row("SETTINGS","FACE - POWER - NETWORK - SOUND",value,CU_ROW_SETTINGS,RISC_SYMBOL_SETTINGS);cs_section("WHAT RUNS");snprintf(value,sizeof(value),"%u STEPS",r->steps);cs_row("WORKFLOW","LAUNCH APPS AND MORE",value,CU_ROW_WORKFLOW,RISC_SYMBOL_WORKFLOW);cs_section("OPTIONS");
  static const unsigned holds[]={0,20,60,300,900};int at=0;for(unsigned i=0;i<5;i++)if(holds[i]==r->hold_seconds)at=(int)i;snprintf(value,sizeof(value),r->hold_seconds>=60?"%u MIN":r->hold_seconds?"%u S":"NO EXTRA",r->hold_seconds>=60?r->hold_seconds/60:r->hold_seconds);cs_step("STAYS ON FOR",value,CU_ROW_HOLD,at,0,4);snprintf(value,sizeof(value),"%u",r->priority);cs_step("PRIORITY",value,CU_ROW_PRIORITY,r->priority,1,9);cs_switch("ENABLED",CU_ROW_ENABLE,r->enabled);cs_row("RENAME","","",CU_ROW_RENAME,0);cs_button(cu_pending?"RETRY SAVE":"SAVE CONTEXT",CU_ROW_SAVE,RISC_SCENE_PRIMARY);n=cs_row("DELETE","","",CU_ROW_DELETE,0);cs_detail(n)->tone=RISC_TONE_DANGER;break;}
 case CU_SETTINGS:
  cs_header("WHAT CHANGES",r->name,true);for(unsigned f=0;f<CR_SETTINGS;f++){cu_value(f,r->settings[f],value,sizeof(value));cs_step(cu_settings_names[f],value,CU_ROW_FIELD+f,cs_field_index(f,r->settings[f]),0,cs_field_max(f));if(!cu_field_available(f))cs_document.nodes[cs_document.node_count-1].flags|=RISC_SCENE_DISABLED;}cs_say("","NO CHANGE LEAVES THAT SETTING ALONE.");break;
 case CU_WORKFLOW:
  cs_header("WORKFLOW",r->name,true);for(unsigned i=0;i<r->steps;i++){cr_step*w=&r->workflow[i];if(w->kind==CR_TIMER)snprintf(b,sizeof(b),"START TIMER %lu MIN",(unsigned long)w->value/60);else snprintf(b,sizeof(b),"%s %.32s",w->kind==CR_LAUNCH?"LAUNCH":"SHOW",w->text);cs_row(b,"RUNS WHEN IT STARTS / TAP TO REMOVE","",CU_ROW_STEP+i,w->kind==CR_TIMER?RISC_SYMBOL_CLOCK:RISC_SYMBOL_WORKFLOW);}if(!r->steps)cs_say("","NO STEPS YET.");if(r->steps<CR_STEPS)cs_row("ADD STEP","","",CU_ROW_ADD_STEP,RISC_SYMBOL_PLUS);break;
 case CU_ADD_STEP:cs_header("ADD STEP","THEN...",true);cs_row("LAUNCH AN APP","","",CU_ROW_LAUNCH,RISC_SYMBOL_WORKFLOW);cs_row("START A TIMER","","",CU_ROW_TIMER,RISC_SYMBOL_CLOCK);cs_row("SHOW A MESSAGE","","",CU_ROW_MESSAGE,RISC_SYMBOL_EVENT);break;
 case CU_SOURCES:
  cs_header("SOURCES","INDEPENDENT RF AND AUDIO",true);for(unsigned i=0;i<2;i++){bool available=(cs_source_mask&(1u<<i))!=0;cs_switch(i?"RF":"AUDIO",CU_ROW_SOURCE+i,!!(ctx_fp.sources&(1u<<i)));if(!available)cs_document.nodes[cs_document.node_count-1].flags|=RISC_SCENE_DISABLED;cs_say("",!available?"UNAVAILABLE ON THIS DEVICE.":ctx_fp.feature_flags[i]&4?"BURST TIMING READY":ctx_fp.feature_flags[i]&2?"SPECTRUM READY; BUILDING TIMING":"WAITING FOR FRESH SAMPLES.");}cs_switch("TIMING ONLY",CU_ROW_SOURCE+2,ctx_fp.temporal_only);break;
 case CU_DELETE:case CU_CANCEL:
  cs_header("CONFIRM","",false);cs_document.flags=RISC_COMPONENTS_CONFIRM;cs_document.cancel_action=CU_ROW_CANCEL;cs_say(cu_page==CU_DELETE?"DELETE THIS ITEM?":"DISCARD CHANGES?",cu_page==CU_DELETE?"Contexts using this item will stop matching.":"Unsaved changes will be discarded.");cs_button(cu_page==CU_DELETE?"KEEP":"KEEP EDITING",CU_ROW_CANCEL,0);cs_button(cu_page==CU_DELETE?"DELETE":"DISCARD",CU_ROW_CONFIRM,RISC_SCENE_DESTRUCTIVE);break;
 case CU_NAME:
  cs_header("NAME",cu_name_reason==4?"MESSAGE":"",true);n=cs_node(RISC_SCENE_KEYBOARD_NODE,"NAME",cu_name,CS_KEY);cs_bounds(n,cu_key_page,0,3);n->target=cu_name_reason==4?32:16;break;
 default:
  cs_header(cu_page==CU_APPS?"LAUNCH...":cu_page==CU_TIMER?"TIMER":"ACTIVITY",cu_page==CU_APPS?"CHOOSE AN APP":cu_page==CU_TIMER?"HOW LONG?":"RECENT",true);cu_build();for(unsigned i=0;i<cu_count;i++)cs_row(cu_rows[i].title,cu_rows[i].detail,"",cu_rows[i].action,0);break;
 }
}
static void cs_event(const risc_scene_event_v1*e){
 if(e->sequence<=cs_last_event||e->document_revision!=cs_document.revision)return;
 if(e->kind!=RISC_SCENE_ACTION_EVENT&&e->kind!=RISC_SCENE_VALUE_EVENT)return;
 unsigned a=e->action;if(a!=CU_ROW_BACK&&a!=CU_ROW_CANCEL&&a!=CS_DISMISS_MESSAGE){if(!e->node||e->node>cs_document.node_count||cs_document.nodes[e->node-1].action!=a)return;}
 if(e->node){const risc_scene_node_v1*n=&cs_document.nodes[e->node-1];if(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))return;
  bool value=n->kind==RISC_COMPONENT_SWITCH||n->kind==RISC_COMPONENT_STEPPER||n->kind==RISC_COMPONENT_SEGMENTS||n->kind==RISC_SCENE_KEYBOARD_NODE;
  if(value!=(e->kind==RISC_SCENE_VALUE_EVENT))return;
  if(value&&n->kind!=RISC_SCENE_KEYBOARD_NODE&&(e->value<n->minimum||e->value>n->maximum))return;
 }
 cs_last_event=e->sequence;ctx_dirty=true;
 if(a==CS_DISMISS_MESSAGE){cs_workflow_message[0]=0;return;}
 if(a==CS_RETRY){cu_loaded=portable_contexts_rules_read(&cu_store);if(cu_loaded){cu_sync_profiles();cu_saved=cu_store;cu_page_set(CU_HOME);}return;}
 if(!cu_loaded)return;
 if(a==CU_ROW_BACK){cu_back();return;}
 if(a==CS_TAB){if(e->value<0||e->value>2)return;cu_kind=e->value==0?CR_PLACE:e->value==1?CR_EVENT:CR_BATTERY;cu_page_set(CU_ITEMS);return;}
 if(a==CS_FINISH){cu_page_set(CU_ITEMS);return;}
 if(a==CS_KEY){unsigned key=(unsigned)e->value;size_t n=strlen(cu_name);if(key==RISC_SCENE_KEY_CANCEL){cu_back();return;}if(key==RISC_SCENE_KEY_DONE){cu_name_done();return;}if(key==RISC_SCENE_KEY_LAYER)cu_key_page=(cu_key_page+1)%4;else if(key==RISC_SCENE_KEY_BACKSPACE){if(n)cu_name[n-1]=0;}else if(key==RISC_SCENE_KEY_CLEAR)cu_name[0]=0;else {if(key==RISC_SCENE_KEY_SPACE)key=' ';if(key>=32&&key<=126&&n<(cu_name_reason==4?32u:16u)){cu_name[n]=(char)key;cu_name[n+1]=0;}}return;}
 if(cu_pending&&a!=CU_ROW_SAVE&&a!=CU_ROW_CANCEL&&a!=CU_ROW_CONFIRM){cu_note("RETRY SAVE OR DISCARD");return;}
 if(e->kind==RISC_SCENE_VALUE_EVENT){cr_rule*r=&cu_store.rules[cu_rule];cr_item*t=&cu_store.items[cu_item];int v=e->value;const risc_scene_node_v1*n=&cs_document.nodes[e->node-1];if(v<n->minimum||v>n->maximum||n->flags&RISC_SCENE_DISABLED)return;
  if(a>=CU_ROW_FIELD&&a<CU_ROW_FIELD+CR_SETTINGS){unsigned f=a-CU_ROW_FIELD;r->settings[f]=cs_field_value(f,v);cu_edit=true;return;}
  if(a==CU_ROW_PRIORITY){r->priority=(uint8_t)v;cu_edit=true;return;}
  if(a==CU_ROW_HOLD){const uint32_t hold[]={0,20,60,300,900};r->hold_seconds=hold[v];cu_edit=true;return;}
  if(a==CU_ROW_ARG0||a==CU_ROW_ARG1){if(t->kind==CR_TIME&&v==(a==CU_ROW_ARG0?t->arg1:t->arg0)){cu_note("FROM AND TO MUST DIFFER");return;}if(a==CU_ROW_ARG0)t->arg0=(uint8_t)(t->kind==CR_BATTERY?v*5:v);else t->arg1=(uint8_t)v;cu_edit=true;return;}
  if(a==CU_ROW_ENABLE){r->enabled=(uint8_t)v;cu_edit=true;return;}
  if(a==CU_ROW_AUTO&&!!v==ctx_enabled)return;
  if(a>=CU_ROW_SOURCE&&a<CU_ROW_SOURCE+3){unsigned i=a-CU_ROW_SOURCE;bool old=i==2?ctx_fp.temporal_only:!!(ctx_fp.sources&(1u<<i));if(old==!!v)return;}
 }
 cu_build();cu_action(a);
}
