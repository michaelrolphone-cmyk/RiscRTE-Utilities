/* App-local capability-pixel presentation for the existing Spectrum editors.
 * The tap targets are built from the current model on every input event.
 * Painting may format/read presentation state, but never changes the model.
 * Target construction never reads providers (including the clock), nor the
 * last rendered frame. Controller coordinates below select existing actions;
 * they do not scale or otherwise constrain native display geometry. */
static spectrum_native_rect nsub_col(spectrum_native_rect r,unsigned i,unsigned n){
 return spectrum_native_column(r,i,n,native_layout.gap);
}
static spectrum_native_rect nsub_span(spectrum_native_rect r,unsigned first,unsigned rows,unsigned total){
 spectrum_native_rect a=nrow(r,first,total),b=nrow(r,first+rows-1,total);a.h=b.y+b.h-a.y;return a;
}
static spectrum_native_rect nsub_detail(spectrum_native_rect r){
 spectrum_native_rect right=nsub_col(r,3,4);r.w=right.x-r.x-native_layout.gap;return r;
}
static void nsub_heading(bool paint,const char *s){ntext(paint,native_layout.header,0,s,NOVA_CYAN);}
static void nsub_lines(bool paint,spectrum_native_rect r,const char *a,const char *b,uint32_t color){
 ntext(paint,nrow(r,0,2),1,a,color);ntext(paint,nrow(r,1,2),1,b,NOVA_CAP);
}
static void nsub_footer(bool paint,int back_x,int back_y){
 npill(paint,nsub_col(native_layout.footer,0,3),"BACK",false,back_x,back_y);
 npill(paint,nsub_col(native_layout.footer,1,3),running?"STOP MIC":"START MIC",false,120,230);
 npill(paint,nsub_col(native_layout.footer,2,3),"RETRY",false,205,230);
}
static void nsub_focus(bool paint,spectrum_native_rect r,unsigned index){
 if(paint&&shared_focus==index&&r.w>6&&r.h>6)fill(r.x+2,r.y+3,2,r.h-6,NOVA_CYAN);
}
static void nsub_filter(bool paint,spectrum_native_rect r,bool shared){
 const char *names[3]={"RAW","AUTO","MANUAL"};
 const int legacy_x[3]={45,120,195};
 for(unsigned i=0;i<3;i++){spectrum_native_rect c=nsub_col(r,i,3);npill(paint,c,names[i],signature_mode==i,legacy_x[i],shared?60:64);if(shared)nsub_focus(paint,c,i+1);}
}
static void nsub_signatures(bool paint){
 spectrum_native_rect area=native_layout.content;char a[80]={0},b[80]={0};
 ntext(paint,nrow(native_layout.header,0,2),0,"SOUND SAMPLES",NOVA_CYAN);
 ntext(paint,nrow(native_layout.header,1,2),1,"512 HANN / 32 ms / 62.5 Hz",NOVA_CAP);
 unsigned units=signature_goal?5:7;
 nsub_filter(paint,nrow(area,0,units),false);
 if(paint){
  int selected=signature_filter();
  if(signature_goal)snprintf(a,sizeof(a),"%s ROOM %u / %u",running?"SAMPLING":"PAUSED",signature_capture.frames,signature_goal);
  else if(signature_pending)snprintf(a,sizeof(a),"UNSAVED / TAP LABEL TO RETRY");
  else if(signature_errors)snprintf(a,sizeof(a),"UNREAD SAMPLES / RETRY");
  else if(selected>=0&&signature_mode==2)snprintf(a,sizeof(a),"MANUAL %.16s",signatures[selected].name);
  else if(selected>=0&&(room_tracker.misses||room_tracker.candidate!=selected))snprintf(a,sizeof(a),"%s %.16s",room_tracker.ambiguous?"AMBIGUOUS":"HOLD",signatures[selected].name);
  else if(selected>=0)snprintf(a,sizeof(a),"%u%% MATCH %.16s",room_tracker.confidence,signatures[selected].name);
  else if(signature_mode==1)snprintf(a,sizeof(a),"%s / %u%%",room_tracker.ambiguous?"AMBIGUOUS":"NO ROOM MATCH",room_tracker.confidence);
  else snprintf(a,sizeof(a),"%s",signature_mode==2?"SELECT A ROOM BELOW":"RAW / NO BACKGROUND FILTER");
 }
 ntext(paint,nrow(area,1,units),1,a,NOVA_CAP);
 if(signature_goal){
  spectrum_native_rect r=nrow(area,2,5);
  npill(paint,nsub_col(r,0,2),running?"STOP":"RESUME",false,63,127);
  npill(paint,nsub_col(r,1,2),"CANCEL",false,177,127);
  ntext(paint,nrow(area,3,5),1,"KEEP ROOM QUIET",NOVA_TEXT);
  ntext(paint,nrow(area,4,5),1,"CANCEL DISCARDS THIS SAMPLE",NOVA_CAP);
 }else{
  for(unsigned row=0;row<2;row++){
   unsigned slot=signature_scroll+row;if(slot>=SPECTRUM_SIGNATURE_SLOTS)break;
   const spectrum_signature *s=&signatures[slot];spectrum_native_rect r=nsub_span(area,2+row*2,2,7),details=nsub_detail(r);
   bool present=s->kind||(signature_pending&(1u<<slot));
   if(paint){
    snprintf(a,sizeof(a),"%u %.16s",slot+1,signature_errors&(1u<<slot)?"UNREAD":(signature_pending&(1u<<slot))&&!s->kind?"DELETE UNSAVED":s->kind?s->name:"EMPTY");
    if(s->kind)snprintf(b,sizeof(b),"%s / %u FRAMES",s->kind==SPECTRUM_SIGNATURE_ROOM?"ROOM":"EVENT",s->frames);
    else snprintf(b,sizeof(b),"TAP + TO CAPTURE");
   }
   nsub_lines(paint,details,a,b,NOVA_WHITE);
   if(present)native_target(details,70,120+(int)row*42);
   if(s->kind==SPECTRUM_SIGNATURE_ROOM)npill(paint,nsub_col(r,3,4),"USE",signature_mode==2&&manual_room==(int)slot,200,120+(int)row*42);
   else if(s->kind==SPECTRUM_SIGNATURE_EVENT)npill(paint,nsub_col(r,3,4),event_slot==(int)slot?"HEARD":"EDIT",false,200,120+(int)row*42);
  }
  spectrum_native_rect r=nrow(area,6,7);
  if(signature_scroll)npill(paint,nsub_col(r,0,4),"<",false,24,208);
  else ntext(paint,nsub_col(r,0,4),1,"<",NOVA_DIM);
  npill(paint,nsub_col(r,1,4),"ROOM +",false,77,208);
  npill(paint,nsub_col(r,2,4),"EVENT +",false,150,208);
  if(signature_scroll+2<SPECTRUM_SIGNATURE_SLOTS)npill(paint,nsub_col(r,3,4),">",false,215,208);
  else ntext(paint,nsub_col(r,3,4),1,">",NOVA_DIM);
 }
 nsub_footer(paint,40,230);
}
static void nsub_signature_edit(bool paint){
 spectrum_native_rect area=native_layout.content;char b[64]={0};
 bool valid=signature_slot>=0&&signature_slot<(int)SPECTRUM_SIGNATURE_SLOTS;
 const spectrum_signature *s=valid?&signatures[signature_slot]:NULL;
 bool pending=valid&&(signature_pending&(1u<<(unsigned)signature_slot));
 nsub_heading(paint,!editing.color?"DELETE PENDING":editing.color==SPECTRUM_SIGNATURE_ROOM?"ROOM SAMPLE":"EVENT SAMPLE");
 ntext(paint,nrow(area,0,4),1,"100 FRAMES / 3.20 SEC",NOVA_CAP);
 spectrum_native_rect name=nrow(area,1,4);
 if(paint)round_rect(name.x,name.y,name.w,name.h,10,NOVA_DIM);
 ntext(paint,name,1,editing.name[0]?editing.name:"TAP TO NAME",NOVA_WHITE);
 if(valid)native_target(name,120,90);
 if(paint&&s)snprintf(b,sizeof(b),"%u SAVED FRAMES",s->frames);
 ntext(paint,nrow(area,2,4),1,b,NOVA_CAP);
 spectrum_native_rect actions=nrow(area,3,4);
 if(s&&(s->kind||pending))npill(paint,nsub_col(actions,0,2),signature_delete_confirm?"CONFIRM":pending?"DISCARD":"DELETE",false,64,210);
 if(pending)npill(paint,nsub_col(actions,1,2),"RETRY SAVE",false,176,210);
 if(s)npill(paint,nsub_col(native_layout.footer,0,2),!s->kind&&pending?"RETRY SAVE":s->kind?"ADD SAMPLE":"CAPTURE",true,64,167);
 npill(paint,nsub_col(native_layout.footer,1,2),"CANCEL",false,176,167);
}
static void nsub_events(bool paint){
 spectrum_native_rect area=native_layout.content;char a[96]={0},b[64]={0};
 nsub_heading(paint,"LEARN SOUNDS");
 if(paint){
  if(cfa_unsaved())snprintf(a,sizeof(a),"FINGERPRINT UNSAVED / RETRY");
  else if(event_files.pending>=0)snprintf(a,sizeof(a),"%s",event_error(event_files.errors[event_files.pending]));
  else if(event_files.ready!=3)snprintf(a,sizeof(a),"STORAGE UNAVAILABLE / RETRY");
  else if(!ambient.ready)snprintf(a,sizeof(a),"%s",running?"LEARNING AMBIENT / 2.1 SEC":"START MIC TO DETECT");
  else if(event_match.running)snprintf(a,sizeof(a),"MATCHING / %u EXAMPLES",event_match.compared);
  else if(event_match.complete&&event_match.selected>=0&&event_match.selected<8)snprintf(a,sizeof(a),"%.16s %u%%",cfa_name(2,event_library.labels[event_match.selected].name),event_match.score/10);
  else if(event_match.complete)snprintf(a,sizeof(a),"%s",event_match.reason==ST_RESULT_NEGATIVE?"CONFUSING NONMATCH":event_match.reason==ST_RESULT_AMBIGUOUS?"AMBIGUOUS / NO LABEL":"UNKNOWN EVENT");
  else snprintf(a,sizeof(a),"BACKGROUND RELATIVE / READY");
 }
 ntext(paint,nrow(area,0,7),1,a,NOVA_CAP);
 for(unsigned row=0;row<2;row++){
  unsigned slot=event_list_scroll+row;if(slot>=8)break;
  const st_label*l=&event_library.labels[slot];spectrum_native_rect r=nsub_span(area,1+row*2,2,7),details=nsub_detail(r);
  if(paint){snprintf(a,sizeof(a),"%u %.16s",slot+1,l->present?l->name:"EMPTY");snprintf(b,sizeof(b),"%u SOUND / %u OTHER",st_example_count(l,1),st_example_count(l,2));}
  nsub_lines(paint,details,a,b,NOVA_WHITE);
  if(l->present){native_target(details,80,80+(int)row*49);npill(paint,nsub_col(r,3,4),event_delete_slot==(int)slot&&event_delete_example==-2?"SURE":"DELETE",false,205,80+(int)row*49);}
 }
 if(paint){
  if(event_missed)snprintf(a,sizeof(a),"BUSY / %u ONSETS MISSED",event_missed);
  else if(cfa_has_events())snprintf(a,sizeof(a),"BURST TIMING + FFT / READY");
  else if(event_neural.state==SN_TRAINING)snprintf(a,sizeof(a),"NEURAL LEARNING %u%%",event_neural.epoch*100u/SN_EPOCHS);
  else if(event_neural.state==SN_PREPARING)snprintf(a,sizeof(a),"NEURAL / PREPARING EXAMPLES");
  else if(event_neural.state==SN_CHECKING)snprintf(a,sizeof(a),"NEURAL / HELD-OUT CHECK");
  else if(event_neural.has_active&&event_neural_cache_blocked)snprintf(a,sizeof(a),"NEURAL READY / RAM ONLY");
  else if(event_neural.has_active)snprintf(a,sizeof(a),"NEURAL READY / CHECK %u OF %u",event_neural.candidate_correct,event_neural.checks);
  else if(event_neural.state==SN_BASELINE)snprintf(a,sizeof(a),"%s",event_neural.calibrated!=event_neural.eligible?"NEURAL / TEST CONFUSING SOUNDS":"TEMPORAL / NO NEURAL GAIN");
  else snprintf(a,sizeof(a),"NEURAL NEEDS 3 SOUND + 2 OTHER");
 }
 ntext(paint,nrow(area,5,7),1,a,event_missed?0xffd24au:NOVA_CAP);
 spectrum_native_rect r=nrow(area,6,7);
 if(event_files.pending>=0||cfa_unsaved()){
  npill(paint,nsub_col(r,0,2),"RETRY SAVE",false,65,203);
  npill(paint,nsub_col(r,1,2),event_delete_example==-3?"CONFIRM":"DISCARD",false,175,203);
 }else{
  if(event_list_scroll)npill(paint,nsub_col(r,0,3),"<",false,29,203);else ntext(paint,nsub_col(r,0,3),1,"<",NOVA_DIM);
  npill(paint,nsub_col(r,1,3),"NEW EVENT",true,120,203);
  if(event_list_scroll<6)npill(paint,nsub_col(r,2,3),">",false,210,203);else ntext(paint,nsub_col(r,2,3),1,">",NOVA_DIM);
 }
 nsub_footer(paint,40,230);
}
static void nsub_event_label(bool paint){
 spectrum_native_rect area=native_layout.content,r=nrow(area,0,5),name=nsub_detail(r);char b[96]={0};
 bool valid=event_edit_slot>=0&&event_edit_slot<8;
 nsub_heading(paint,"TEACH A SOUND");
 if(paint)round_rect(name.x,name.y,name.w,name.h,9,NOVA_DIM);
 ntext(paint,name,1,editing.name[0]?editing.name:"TAP TO NAME",NOVA_WHITE);
 if(valid){native_target(name,80,54);npill(paint,nsub_col(r,3,4),"SAVE",false,200,54);}
 r=nrow(area,1,5);
 if(paint&&valid){const st_label*l=&event_library.labels[event_edit_slot];if(running&&!ambient.ready)snprintf(b,sizeof(b),"WARMING %u / 64",ambient.frames);else snprintf(b,sizeof(b),"%u SOUND / %u OTHER / 6",st_example_count(l,1),st_example_count(l,2));}
 ntext(paint,nsub_detail(r),1,b,NOVA_CAP);npill(paint,nsub_col(r,3,4),running?"STOP":"MIC",false,200,88);
 r=nrow(area,2,5);
 if(valid){npill(paint,nsub_col(r,0,2),"RECORD SOUND",true,64,121);npill(paint,nsub_col(r,1,2),"NOT THIS SOUND",false,176,121);}
 r=nrow(area,3,5);spectrum_native_rect value=nsub_col(r,1,4),last=nsub_col(r,2,4);value.w=last.x+last.w-value.x;
 if(event_edit_shift)npill(paint,nsub_col(r,0,4),"-",false,90,161);else ntext(paint,nsub_col(r,0,4),1,"-",NOVA_DIM);
 if(paint){if(event_edit_shift)snprintf(b,sizeof(b),"SHIFT +/- %u.%u st",event_edit_shift*15/10,event_edit_shift*15%10);else snprintf(b,sizeof(b),"SHIFT OFF");}
 ntext(paint,value,1,b,NOVA_WHITE);
 if(event_edit_shift<2)npill(paint,nsub_col(r,3,4),"+",false,213,161);else ntext(paint,nsub_col(r,3,4),1,"+",NOVA_DIM);
 if(paint){if(valid&&event_match.selected==event_edit_slot){int shift=event_match.shift[event_edit_slot]*15;snprintf(b,sizeof(b),"LAST %+d.%d st / %u / %u Hz",shift/10,abs_int(shift%10),event_match.observed_hz,event_match.reference_hz[event_edit_slot]);}else snprintf(b,sizeof(b),"NEURAL: TEACH AT LEAST 2 LABELS");}
 ntext(paint,nrow(area,4,5),1,b,NOVA_CAP);
 if(valid)npill(paint,nsub_col(native_layout.footer,0,2),"EXAMPLES",false,64,219);
 npill(paint,nsub_col(native_layout.footer,1,2),"BACK",false,176,219);
}
static void nsub_event_capture(bool paint){
 spectrum_native_rect area=native_layout.content;char b[96]={0};
 bool valid=event_edit_slot>=0&&event_edit_slot<8;
 nsub_heading(paint,event_capture_kind==ST_NEGATIVE?"SOUND TO EXCLUDE":"RECORD YOUR SOUND");
 ntext(paint,nrow(area,0,6),1,valid?event_library.labels[event_edit_slot].name:"NO EVENT SELECTED",NOVA_WHITE);
 if(!event_armed&&!event_training.ready){
  ntext(paint,nrow(area,2,6),1,"CAPTURE STOPPED",NOVA_CAP);ntext(paint,nrow(area,3,6),1,"NO EXAMPLE SAVED",NOVA_CAP);
  npill(paint,native_layout.footer,"BACK",false,120,201);return;
 }
 if(event_training.ready){
  const st_example *e=&event_training.event;bool clipped=!!(e->flags&ST_CLIPPED);int buttons=clipped?206:193;
  ntext(paint,nrow(area,1,6),1,e->flags&ST_CONFIRMED_END?"WHOLE EVENT / END CONFIRMED":clipped?"END NOT VERIFIED / REVIEW":"READY TO SAVE",clipped?0xffd24au:NOVA_CYAN);
  if(paint)snprintf(b,sizeof(b),"%u ms / %u IMPACTS",e->duration_ms,e->impacts);
  ntext(paint,nrow(area,2,6),1,b,NOVA_TEXT);
  if(paint)snprintf(b,sizeof(b),"ATTACK %u / DECAY %u ms",e->attack_ms,e->decay_ms);
  ntext(paint,nrow(area,3,6),1,b,NOVA_CAP);
  if(paint)snprintf(b,sizeof(b),"PEAK %u Hz / %u COLUMNS",e->peak_hz,e->count);
  ntext(paint,nrow(area,4,6),1,b,NOVA_CAP);
  if(clipped)npill(paint,nrow(area,5,6),e->flags&ST_CONFIRMED_END?"UNDO WHOLE EVENT":"USE AS WHOLE EVENT",!!(e->flags&ST_CONFIRMED_END),120,163);
  if(valid)npill(paint,nsub_col(native_layout.footer,0,2),st_partial(e)?"SAVE WINDOW":"SAVE",true,64,buttons);
  npill(paint,nsub_col(native_layout.footer,1,2),"CANCEL",false,176,buttons);
 }else{
  ntext(paint,nrow(area,1,6),1,!ambient.ready?"KEEP QUIET / PREPARING":event_wait_quiet?"A MOMENT OF QUIET":event_training.collecting?"RECORDING YOUR SOUND":"MAKE THE SOUND NOW",NOVA_CYAN);
  if(paint){if(!ambient.ready)snprintf(b,sizeof(b),"BACKGROUND %u%%",ambient.frames*100u/SPECTRUM_BG_WARMUP);else snprintf(b,sizeof(b),"%u.%02u / 4.10 SEC",event_training.event.count*64u/1000u,event_training.event.count*64u%1000u/10u);}
  ntext(paint,nrow(area,2,6),1,b,NOVA_WHITE);
  ntext(paint,nrow(area,3,6),1,event_capture_kind==ST_NEGATIVE?"MAKE A SOUND TO EXCLUDE":"PAUSE AFTER THE SOUND",NOVA_CAP);
  ntext(paint,nrow(area,4,6),1,"THEN REVIEW AND SAVE",NOVA_CAP);
  npill(paint,nsub_col(native_layout.footer,0,2),"STOP",false,64,203);
  npill(paint,nsub_col(native_layout.footer,1,2),"CANCEL",false,176,203);
 }
}
static void nsub_event_examples(bool paint){
 spectrum_native_rect area=native_layout.content;char a[80]={0},b[80]={0};bool valid=event_edit_slot>=0&&event_edit_slot<8;
 nsub_heading(paint,"SAVED EXAMPLES");ntext(paint,nrow(area,0,3),1,valid?event_library.labels[event_edit_slot].name:"NO EVENT SELECTED",NOVA_WHITE);
 if(valid)for(unsigned row=0;row<2;row++){
  unsigned ex=event_example_scroll+row;if(ex>=6)break;const st_example *e=&event_library.labels[event_edit_slot].examples[ex];
  spectrum_native_rect r=nrow(area,row+1,3);
  if(paint){
   if(e->id){snprintf(a,sizeof(a),"%u %s / %u ms",ex+1,e->kind==1?"POSITIVE":"NONMATCH",e->duration_ms);snprintf(b,sizeof(b),"%s%u Hz / %u HITS",e->flags&ST_CONFIRMED_END?"END SET / ":e->flags&ST_CLIPPED?"CLIP / ":"",e->peak_hz,e->impacts);}
   else{snprintf(a,sizeof(a),"%u EMPTY",ex+1);b[0]=0;}
  }
  nsub_lines(paint,nsub_detail(r),a,b,NOVA_WHITE);
  if(e->id)npill(paint,nsub_col(r,3,4),event_delete_slot==event_edit_slot&&event_delete_example==(int)ex?"SURE":"DELETE",false,206,79+(int)row*58);
 }
 if(event_example_scroll)npill(paint,nsub_col(native_layout.footer,0,3),"<",false,32,205);else ntext(paint,nsub_col(native_layout.footer,0,3),1,"<",NOVA_DIM);
 npill(paint,nsub_col(native_layout.footer,1,3),"BACK",false,120,205);
 if(event_example_scroll<4)npill(paint,nsub_col(native_layout.footer,2,3),">",false,208,205);else ntext(paint,nsub_col(native_layout.footer,2,3),1,">",NOVA_DIM);
}
static void nsub_shared(bool paint){
 spectrum_native_rect area=native_layout.content;char b[96]={0};
 if(page==PAGE_SHARED){
  bool rooms=shared_kind==CF_ROOM;spectrum_native_rect title=nsub_detail(native_layout.header);
  ntext(paint,title,0,rooms?"SHARED ROOMS":"SHARED EVENTS",NOVA_CYAN);npill(paint,nsub_col(native_layout.header,3,4),"TOOLS",false,197,20);nsub_focus(paint,nsub_col(native_layout.header,3,4),0);
  unsigned slots[CF_PROFILES],n=shared_rows(slots),visible=rooms?2:3;
  if(rooms)nsub_filter(paint,nrow(area,0,4),true);
  /* A stale offset must not be reset during paint or input mapping. MORE
   * remains reachable and the unchanged controller resets it safely. */
  for(unsigned row=0;row<visible&&shared_offset<n&&row<n-shared_offset;row++){
   const cf_profile *p=&cfa_bank.profiles[slots[shared_offset+row]];spectrum_native_rect r=nrow(area,row+(rooms?1:0),4);
   if(paint){if(!p->present[CFA_SOURCE])snprintf(b,sizeof(b),"ADD AUDIO SAMPLE");else snprintf(b,sizeof(b),"%u WINDOWS / %s",p->feature[CFA_SOURCE].windows,p->feature[CFA_SOURCE].identity!=cfa_identity()?"SETTINGS DIFFER":rooms&&(p->feature[CFA_SOURCE].flags&CF_BACKGROUND)?"BG READY":"TRAINED");}
   nsub_lines(paint,r,p->name,b,NOVA_WHITE);native_target(r,120,(rooms?106:64)+(int)row*44);nsub_focus(paint,r,row+(rooms?4:1));
  }
  if(paint){
   if(cfa_failed||!cfa_loaded)snprintf(b,sizeof(b),"SHARED MODELS UNREAD / RETRY TRAINING");
   else if(cfa_unsaved())snprintf(b,sizeof(b),"SHARED CHANGES UNSAVED / TOOLS TO RETRY");
   else if(!n)snprintf(b,sizeof(b),"ADD A ROOM OR EVENT BELOW");
   else if(shared_offset>=n)snprintf(b,sizeof(b),"LIST CHANGED / TAP MORE");
   else snprintf(b,sizeof(b),"SHARED WITH CONTEXTS");
  }
  ntext(paint,nrow(area,3,4),1,b,NOVA_CAP);
  npill(paint,nsub_col(native_layout.footer,0,3),"BACK",false,39,212);npill(paint,nsub_col(native_layout.footer,1,3),"+ NEW",true,120,212);
  npill(paint,nsub_col(native_layout.footer,2,3),"MORE",false,201,212);
  unsigned shown=shared_offset<n?spectrum_native_min((int)visible,(int)(n-shared_offset)):0;
  for(unsigned i=0;i<3;i++)nsub_focus(paint,nsub_col(native_layout.footer,i,3),(rooms?4:1)+shown+i);
  return;
 }
 if(page==PAGE_SHARED_EDIT){
  nsub_heading(paint,shared_kind==CF_ROOM?"ROOM TRAINING":"EVENT TRAINING");
  ntext(paint,nrow(area,0,4),1,"SHARED WITH CONTEXTS",NOVA_CAP);
  spectrum_native_rect name=nrow(area,1,4);if(paint)round_rect(name.x,name.y,name.w,name.h,12,NOVA_DIM);
  ntext(paint,name,1,shared_name[0]?shared_name:"TAP TO NAME",NOVA_WHITE);native_target(name,120,84);nsub_focus(paint,name,0);
  npill(paint,nrow(area,2,4),shared_selected>=0?"ADD ANOTHER SAMPLE":"START TRAINING",true,120,135);nsub_focus(paint,nrow(area,2,4),1);
  if(shared_kind==CF_ROOM&&shared_selected>=0){npill(paint,nrow(area,3,4),"USE ROOM BACKGROUND",false,120,177);nsub_focus(paint,nrow(area,3,4),2);}
  npill(paint,native_layout.footer,"BACK",false,120,217);nsub_focus(paint,native_layout.footer,shared_kind==CF_ROOM&&shared_selected>=0?3:2);return;
 }
 nsub_heading(paint,"TRAINING");ntext(paint,nrow(area,0,4),1,cfa_learning.name,NOVA_WHITE);
 /* Read the clock only for painting. Readiness never changes geometry or
  * the legacy action: shared_tap revalidates it at action time. */
 bool ready=false;
 if(paint){
  unsigned elapsed=(uint32_t)(app->millis()-cfa_learning_at)/1000u;
  ready=cfa_learning_samples>=3&&(cfa_learning_kind==CF_EVENT||elapsed>=30u);
  if(shared_kind==CF_EVENT)snprintf(b,sizeof(b),"%u / 3 EVENTS CAPTURED",cfa_learning_samples);
  else snprintf(b,sizeof(b),"%u / 30 SEC / %u WINDOWS",elapsed>30?30:elapsed,cfa_learning_samples);
 }
 ntext(paint,nrow(area,1,4),1,b,NOVA_CAP);
 ntext(paint,nrow(area,2,4),1,cfa_learning_saved?"SAVE UNCONFIRMED":ready?"READY TO SAVE":running?(shared_kind==CF_EVENT?"REPEAT WITH QUIET BETWEEN":"KEEP CAPTURING THIS ROOM"):"PAUSED / RESUME CAPTURE",NOVA_CAP);
 npill(paint,nrow(area,3,4),cfa_learning_saved?"RETRY SAVE":ready?"SAVE TO SHARED LIBRARY":running?"CAPTURING...":"RESUME",ready,120,163);nsub_focus(paint,nrow(area,3,4),0);
 npill(paint,native_layout.footer,cfa_learning_saved?(shared_discard_confirm?"CONFIRM":"DISCARD"):"CANCEL",false,120,214);nsub_focus(paint,native_layout.footer,1);
}
static bool spectrum_native_subpage(bool paint){
 switch(page){
 case PAGE_SIGNATURES:nsub_signatures(paint);return true;
 case PAGE_SIGNATURE_EDIT:nsub_signature_edit(paint);return true;
 case PAGE_EVENTS:nsub_events(paint);return true;
 case PAGE_EVENT_LABEL:nsub_event_label(paint);return true;
 case PAGE_EVENT_CAPTURE:nsub_event_capture(paint);return true;
 case PAGE_EVENT_EXAMPLES:nsub_event_examples(paint);return true;
 case PAGE_SHARED:case PAGE_SHARED_EDIT:case PAGE_SHARED_CAPTURE:nsub_shared(paint);return true;
 default:return false;
 }
}
