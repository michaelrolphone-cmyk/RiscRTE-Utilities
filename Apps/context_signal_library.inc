/* Shared room/event training and background selection, in existing NOVA units.
 * Both signal apps operate on exactly the CFP source records used by Contexts. */
static unsigned shared_kind=CF_ROOM,shared_offset;
static bool shared_discard_confirm;
static int shared_selected=-1;
static char shared_name[17];
static unsigned shared_focus,shared_targets;
static struct {int x,y,w,h;} shared_bounds[16];
static void shared_target(int x,int y,int w,int h){
 if(shared_targets>=16)return;
 shared_bounds[shared_targets].x=x;shared_bounds[shared_targets].y=y;shared_bounds[shared_targets].w=w;shared_bounds[shared_targets].h=h;
 if(shared_targets++==shared_focus)fill(x,y+5,2,h-10,NOVA_CYAN);
}
static void shared_pill(int x,int y,int w,int h,const char*name,bool on){pill(x,y,w,h,name,on);shared_target(x,y,w,h);}
static void shared_buttons(uint32_t buttons){
 if(!shared_targets)return;
 if(buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))shared_focus=shared_focus?shared_focus-1:shared_targets-1;
 else if(buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))shared_focus=(shared_focus+1)%shared_targets;
 else if(buttons&T5_APP_BUTTON_CONFIRM){unsigned i=shared_focus%shared_targets;shared_tap(shared_bounds[i].x+shared_bounds[i].w/2,shared_bounds[i].y+shared_bounds[i].h/2);}
 if(buttons)dirty=true;
}
static bool shared_name_valid(const char*n){if(!n||!n[0]||n[0]==' ')return false;unsigned i=0;for(;i<17&&n[i];i++)if((unsigned char)n[i]<32||(unsigned char)n[i]>126)return false;return i>0&&i<17&&n[i-1]!=' ';}
static void shared_open(unsigned kind){
 shared_kind=kind;shared_offset=shared_focus=shared_targets=0;shared_selected=-1;shared_keyboard=shared_discard_confirm=false;
 cfa_shared_names();toast_message=NULL;set_page(PAGE_SHARED);
}
static void shared_keyboard_done(void){
 memcpy(shared_name,editing.name,17);shared_keyboard=false;set_page(PAGE_SHARED_EDIT);
}
static bool shared_back(void){
 if(page==PAGE_SHARED_CAPTURE){notify("SAVE OR CANCEL LEARNING");return true;}
 if(page==PAGE_SHARED_EDIT){set_page(PAGE_SHARED);return true;}
 if(page==PAGE_SHARED){set_page(PAGE_MAIN);return true;}
 return false;
}
static unsigned shared_rows(unsigned slots[CF_PROFILES]){
 unsigned n=0;for(unsigned i=0;i<CF_PROFILES;i++)if(cfa_bank.profiles[i].kind==shared_kind)slots[n++]=i;return n;
}
static void draw_shared(void){
 shared_targets=0;char b[64];
 if(page==PAGE_SHARED){
  text(0,12,8,164,shared_kind==CF_ROOM?"SHARED ROOMS":"SHARED EVENTS",NOVA_CYAN);
  shared_pill(166,3,62,34,"TOOLS",false);
  int top=44;unsigned visible=3;
  if(shared_kind==CF_ROOM){shared_pill(12,44,68,34,"RAW",signature_mode==0);shared_pill(86,44,68,34,"AUTO",signature_mode==1);shared_pill(160,44,68,34,"MANUAL",signature_mode==2);top=86;visible=2;}
  unsigned slots[CF_PROFILES],n=shared_rows(slots);if(shared_offset>=n)shared_offset=0;
  for(unsigned row=0;row<visible&&shared_offset+row<n;row++){
   const cf_profile*p=&cfa_bank.profiles[slots[shared_offset+row]];int y=top+(int)row*44;
   fill(12,y,2,40,NOVA_DIM);text(1,22,y+3,206,p->name,NOVA_WHITE);
   if(!p->present[CFA_SOURCE])snprintf(b,sizeof(b),"ADD %s SAMPLE",CFA_SOURCE?"RF":"AUDIO");
   else snprintf(b,sizeof(b),"%u WINDOWS / %s",p->feature[CFA_SOURCE].windows,(p->feature[CFA_SOURCE].identity!=cfa_identity())?"SETTINGS DIFFER":(shared_kind==CF_ROOM&&(p->feature[CFA_SOURCE].flags&CF_BACKGROUND))?"BG READY":"TRAINED");
   text(1,22,y+22,206,b,NOVA_CAP);fill(22,y+41,206,1,NOVA_LINE);shared_target(12,y,216,40);
  }
  if(!n)center(1,12,116,216,"ADD A ROOM OR EVENT BELOW",NOVA_CAP);
  shared_pill(12,194,54,36,"BACK",false);shared_pill(72,194,96,36,"+ NEW",true);shared_pill(174,194,54,36,"MORE",false);return;
 }
 if(page==PAGE_SHARED_EDIT){
  center(0,12,12,216,shared_kind==CF_ROOM?"ROOM TRAINING":"EVENT TRAINING",NOVA_CYAN);
  center(1,12,40,216,"SHARED WITH CONTEXTS",NOVA_CAP);
  round_rect(12,64,216,40,12,NOVA_DIM);text(1,22,76,196,shared_name[0]?shared_name:"TAP TO NAME",NOVA_WHITE);shared_target(12,64,216,40);
  shared_pill(12,116,216,38,shared_selected>=0?"ADD ANOTHER SAMPLE":"START TRAINING",true);
  if(shared_kind==CF_ROOM&&shared_selected>=0)shared_pill(12,160,216,34,"USE ROOM BACKGROUND",false);
  shared_pill(72,200,96,34,"BACK",false);return;
 }
 center(0,12,12,216,"TRAINING",NOVA_CYAN);center(1,12,45,216,cfa_learning.name,NOVA_WHITE);
 unsigned elapsed=(uint32_t)(app->millis()-cfa_learning_at)/1000u;
 if(shared_kind==CF_EVENT)snprintf(b,sizeof(b),"%u / 3 EVENTS CAPTURED",cfa_learning_samples);
 else snprintf(b,sizeof(b),"%u / 30 SEC / %u WINDOWS",elapsed>30?30:elapsed,cfa_learning_samples);
 center(1,12,77,216,b,NOVA_CAP);
 center(1,12,106,216,cfa_learning_saved?"SAVE UNCONFIRMED":cfa_learn_ready()?"READY TO SAVE":running?(shared_kind==CF_EVENT?"REPEAT WITH QUIET BETWEEN":"KEEP CAPTURING THIS ROOM"):"PAUSED / RESUME CAPTURE",NOVA_CAP);
 shared_pill(12,144,216,38,cfa_learning_saved?"RETRY SAVE":cfa_learn_ready()?"SAVE TO SHARED LIBRARY":running?"CAPTURING...":"RESUME",cfa_learn_ready());
 shared_pill(72,196,96,36,cfa_learning_saved?(shared_discard_confirm?"CONFIRM":"DISCARD"):"CANCEL",false);
}
static void shared_tap(int x,int y){
 if(page==PAGE_SHARED){
  if(hit(x,y,166,3,62,34)){if(shared_kind==CF_ROOM){signature_load();set_page(PAGE_SIGNATURES);}else{event_load();set_page(PAGE_EVENTS);}return;}
  if(shared_kind==CF_ROOM&&hit(x,y,12,44,216,34)){(void)cfa_filter_select(x<83?0:x<157?1:2,cfa_manual_name);return;}
  unsigned slots[CF_PROFILES],n=shared_rows(slots),visible=shared_kind==CF_ROOM?2:3;int top=shared_kind==CF_ROOM?86:44;
  if(y>=top&&y<top+(int)visible*44){unsigned row=(unsigned)(y-top)/44+shared_offset;if(row<n){shared_selected=(int)slots[row];memcpy(shared_name,cfa_bank.profiles[shared_selected].name,17);set_page(PAGE_SHARED_EDIT);}return;}
  if(hit(x,y,12,194,54,36)){set_page(PAGE_MAIN);return;}
  if(hit(x,y,174,194,54,36)){shared_offset+=visible;if(shared_offset>=n)shared_offset=0;dirty=true;return;}
  if(hit(x,y,72,194,96,36)){shared_selected=-1;shared_name[0]=0;set_page(PAGE_SHARED_EDIT);}return;
 }
 if(page==PAGE_SHARED_EDIT){
  if(hit(x,y,12,64,216,40)){memcpy(editing.name,shared_name,17);signature_keyboard=event_keyboard=false;shared_keyboard=true;keyboard_begin();return;}
  if(hit(x,y,72,200,96,34)){set_page(PAGE_SHARED);return;}
  if(shared_kind==CF_ROOM&&shared_selected>=0&&hit(x,y,12,160,216,34)){
   int old_mode=signature_mode;char old_name[17];memcpy(old_name,cfa_manual_name,17);
   memcpy(cfa_manual_name,shared_name,17);signature_mode=2;
   if(!cfa_background()){signature_mode=(unsigned)old_mode;memcpy(cfa_manual_name,old_name,17);notify("CAPTURE ROOM WITH THESE SETTINGS");return;}
   signature_mode=(unsigned)old_mode;memcpy(cfa_manual_name,old_name,17);
   if(cfa_filter_select(2,shared_name)){set_page(PAGE_MAIN);notify("ROOM BACKGROUND SUBTRACTED");}return;
  }
  if(hit(x,y,12,116,216,38)){
   if(!shared_name_valid(shared_name)){notify("ENTER A NAME FIRST");return;}
   if(cfa_failed||!cfa_loaded){cfa_retry();if(cfa_failed||!cfa_loaded){notify("SHARED MODELS UNREAD / RETRY");return;}}
   if(cf_profile_slot(&cfa_bank,shared_kind,shared_name)<0){notify("8 LABELS FULL");return;}
   if(signature_goal||event_armed||event_training.ready){notify("FINISH CURRENT SAMPLE FIRST");return;}
   if(!running)toggle();
   if(!running){notify("CAPTURE UNAVAILABLE");return;}
   cfa_begin();memset(&cfa_learning,0,sizeof(cfa_learning));memcpy(cfa_learning.name,shared_name,17);cfa_learning.kind=(uint8_t)shared_kind;
   cfa_learning_kind=shared_kind;cfa_learning_samples=0;cfa_learning_at=app->millis();cfa_learning_saved=false;set_page(PAGE_SHARED_CAPTURE);
  }return;
 }
 if(hit(x,y,12,144,216,38)){
  if(cfa_learn_ready()||cfa_learning_saved){if(cfa_learn_save())shared_open(shared_kind);}
  else if(!running)toggle();
  return;
 }
 if(hit(x,y,72,196,96,36)){
  if(cfa_learning_saved){if(!shared_discard_confirm){shared_discard_confirm=true;toast_message=NULL;dirty=true;return;}cfa_discard();if(temporal_retained())return;cfa_learning_saved=false;shared_discard_confirm=false;}
  cfa_learning_kind=cfa_learning_samples=0;set_page(PAGE_SHARED_EDIT);
 }
}
