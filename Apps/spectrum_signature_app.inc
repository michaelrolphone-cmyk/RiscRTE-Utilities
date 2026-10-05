/* Saved spectral samples. Existing preference and frequency-label keys retain
 * their v1 encoding. New profile slots each commit independently through @2. */
static spectrum_signature signatures[SPECTRUM_SIGNATURE_SLOTS],signature_capture;
static uint32_t signature_means[SPECTRUM_SIGNATURE_SLOTS][SPECTRUM_SIGNATURE_BANDS];
static spectrum_signature_analyzer signature_audio;
static spectrum_room_tracker room_tracker;
static uint32_t signature_seen,last_event_at,last_capture[SPECTRUM_SIGNATURE_SLOTS];
static uint64_t signature_display_sums[SPECTRUM_SIGNATURE_BANDS];
static uint32_t signature_display_frames,signature_display_mean[SPECTRUM_SIGNATURE_BANDS],signature_display_gains[SPECTRUM_SIGNATURE_BANDS];
static uint32_t filtered_amplitude[SPECTRUM_DSP_MAX_FFT/2+1];
static uint8_t signature_pending,signature_errors;
static unsigned signature_mode,signature_scroll,signature_goal,event_confidence;
static int signature_slot,signature_capture_slot,manual_room,event_slot;
static bool signature_keyboard,signature_delete_confirm;
static char signature_status[40];
static void signature_key(unsigned slot,char out[16]){snprintf(out,16,"spectrum_s%u",slot);}
static void signature_save(void){
 if(!signature_pending)return;
 if(!acquire_storage()){notify("SAMPLES UNSAVED / RETRY");return;}
 uint8_t bytes[SPECTRUM_SIGNATURE_RECORD_SIZE];
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_SLOTS;i++)if(signature_pending&(1u<<i)){
  char key[16];signature_key(i,key);
  if(!(signature_errors&(1u<<i))&&spectrum_signature_encode(&signatures[i],bytes)&&storage->put(storage->context,key,bytes,sizeof(bytes))==RISC_KEY_VALUE_OK)signature_pending&=(uint8_t)~(1u<<i);
 }
 snprintf(signature_status,sizeof(signature_status),"%s",signature_pending?"UNSAVED / RETRY OR DISCARD":"SAMPLES SAVED");dirty=true;
}
static void signature_load(void){
 if(!acquire_storage())return;
 uint8_t bytes[SPECTRUM_SIGNATURE_RECORD_SIZE];
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_SLOTS;i++)if(signature_errors&(1u<<i)){
  char key[16];signature_key(i,key);uint32_t n=0;spectrum_signature s={0};int32_t result=storage->get(storage->context,key,bytes,sizeof(bytes),&n);
  if(result==RISC_KEY_VALUE_NOT_FOUND||(result==RISC_KEY_VALUE_OK&&spectrum_signature_decode(&s,bytes,n))){signatures[i]=s;spectrum_signature_mean(&s,signature_means[i]);signature_errors&=(uint8_t)~(1u<<i);}
 }
 if(signature_errors)snprintf(signature_status,sizeof(signature_status),"SAVED SAMPLES UNREAD / RETRY");
}
static void signature_restore(void){
 memset(signatures,0,sizeof(signatures));memset(signature_means,0,sizeof(signature_means));memset(&signature_capture,0,sizeof(signature_capture));memset(last_capture,0,sizeof(last_capture));
 spectrum_signature_init(&signature_audio);spectrum_room_reset(&room_tracker);signature_seen=last_event_at=0;signature_pending=0;signature_errors=255;
 signature_mode=signature_scroll=signature_goal=event_confidence=0;signature_slot=signature_capture_slot=manual_room=event_slot=-1;signature_keyboard=signature_delete_confirm=false;signature_status[0]=0;if(storage)signature_load();
}
static int signature_filter(void){int selected=signature_mode==2?manual_room:signature_mode==1?room_tracker.selected:-1;return selected>=0&&selected<(int)SPECTRUM_SIGNATURE_SLOTS&&signatures[selected].kind==SPECTRUM_SIGNATURE_ROOM?selected:-1;}
static void signature_commit_capture(void){
 int slot=signature_capture_slot;if(slot<0||!signature_capture.frames)return;int previous_filter=signature_filter();
 bool ok=true;
 if(signatures[slot].kind)ok=spectrum_signature_combine(&signatures[slot],&signature_capture);else signatures[slot]=signature_capture;
 if(!ok){notify("SAMPLE LIMIT / USE NEW LABEL");return;}
 memcpy(signatures[slot].name,signature_capture.name,17);
 spectrum_signature_mean(&signatures[slot],signature_means[slot]);signature_pending|=(uint8_t)(1u<<slot);last_capture[slot]=signature_audio.transforms;
 signature_goal=0;signature_capture_slot=-1;memset(&signature_capture,0,sizeof(signature_capture));spectrum_room_reset(&room_tracker);if(previous_filter!=signature_filter()||previous_filter==slot){clear_analysis();refresh_analysis();}signature_save();notify(signature_pending?"SAMPLES UNSAVED / RETRY":"SAMPLE SAVED");
}
static void signature_display_reset(void){memset(signature_display_sums,0,sizeof(signature_display_sums));signature_display_frames=0;}
static void signature_prepare_filter(int selected){
 for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)signature_display_mean[i]=signature_display_frames?(uint32_t)spectrum_dsp_div_u64_u32(signature_display_sums[i]+signature_display_frames/2,signature_display_frames):signature_audio.power[i];
 spectrum_signature_gains(signature_display_mean,signature_means[selected],signature_display_gains);
}
static void signature_observe(void){
 if(signature_seen==signature_audio.transforms)return;
 signature_seen=signature_audio.transforms;
 if(signature_display_frames<512){for(unsigned i=0;i<SPECTRUM_SIGNATURE_BANDS;i++)signature_display_sums[i]+=signature_audio.power[i];++signature_display_frames;}
 if(signature_goal){if(!spectrum_signature_add(&signature_capture,signature_audio.power)){signature_goal=0;notify("SAMPLE LIMIT");}else if(signature_capture.frames>=signature_goal)signature_commit_capture();}
 int previous_filter=signature_filter();
 spectrum_room_observe(&room_tracker,signature_audio.power,signatures,signature_means);
 if(previous_filter!=signature_filter())clear_analysis();
 unsigned confidence=0;bool ambiguous=false;int found=spectrum_signature_best(signature_audio.power,signatures,signature_means,SPECTRUM_SIGNATURE_EVENT,&confidence,&ambiguous);
 if(found>=0){event_slot=found;event_confidence=confidence;last_event_at=app->millis();dirty=true;}else if(event_slot>=0&&(uint32_t)(app->millis()-last_event_at)>2000u){event_slot=-1;dirty=true;}
 if(page==PAGE_SIGNATURES||signature_goal)dirty=true;
}
/* Suspension keeps partial capture visible and explicitly resumable. No
 * synthetic frames, silent commit of a shortened room sample, or auto-resume. */
static void signature_suspend(void){signature_audio.used=0;}
static bool signature_exit_ready(void){
 signature_save();if(signature_pending||signature_goal){notify(signature_goal?"FINISH OR CANCEL SAMPLE":"RETRY SAVE OR DISCARD");set_page(PAGE_SIGNATURES);return false;}return true;
}
static void signature_open(int slot,unsigned kind){
 if(signature_goal){notify("FINISH OR CANCEL SAMPLE");return;}
 signature_slot=slot;signature_delete_confirm=false;editing=(spectrum_label){0};
 if(slot>=0){memcpy(editing.name,signatures[slot].name,17);editing.color=signatures[slot].kind;}else{editing.color=(uint8_t)kind;for(unsigned i=0;i<SPECTRUM_SIGNATURE_SLOTS;i++)if(!signatures[i].kind&&!(signature_errors&(1u<<i))){signature_slot=(int)i;break;}if(signature_slot<0){notify(signature_errors?"RETRY UNREAD SAMPLES":"8 SAMPLE LABELS FULL");return;}}
 toast_message=NULL;set_page(PAGE_SIGNATURE_EDIT);
}
static void signature_capture_begin(void){
 if(editing.color!=SPECTRUM_SIGNATURE_ROOM&&editing.color!=SPECTRUM_SIGNATURE_EVENT){notify("CHOOSE ROOM OR EVENT");set_page(PAGE_SIGNATURES);return;}
 if(signature_slot<0||signature_slot>=(int)SPECTRUM_SIGNATURE_SLOTS)return;
 unsigned slot=(unsigned)signature_slot;
 if(signature_errors&(1u<<slot)){notify("RETRY STORAGE FIRST");return;}
 if(signature_pending&(1u<<slot)){signature_save();if(signature_pending&(1u<<slot)){notify("RETRY SAVE FIRST");return;}}
 unsigned start=0,end=(unsigned)strlen(editing.name);while(start<end&&editing.name[start]==' ')++start;while(end>start&&editing.name[end-1]==' ')--end;for(unsigned i=0;i<end-start;i++)editing.name[i]=editing.name[i+start];editing.name[end-start]=0;
 if(!spectrum_signature_name_valid(editing.name)){notify("ENTER A SAMPLE NAME");return;}
 if(!signature_audio.available||(editing.color==SPECTRUM_SIGNATURE_ROOM&&!running)){notify("START MIC / WAIT FOR AUDIO");return;}
 unsigned needed=editing.color==SPECTRUM_SIGNATURE_ROOM?SPECTRUM_SIGNATURE_ROOM_FRAMES:1u;
 if(signatures[slot].frames>SPECTRUM_SIGNATURE_MAX_FRAMES-needed){notify("SAMPLE LIMIT / USE NEW LABEL");return;}
 if(last_capture[slot]==signature_audio.transforms){notify("WAIT FOR A NEW FRAME");return;}
 signature_capture=(spectrum_signature){.kind=editing.color};memcpy(signature_capture.name,editing.name,17);signature_capture_slot=(int)slot;
 signature_goal=editing.color==SPECTRUM_SIGNATURE_ROOM?SPECTRUM_SIGNATURE_ROOM_FRAMES:1;
 if(signature_goal>1)signature_audio.used=0;
 if(signature_goal==1){(void)spectrum_signature_add(&signature_capture,signature_audio.power);signature_commit_capture();}
 set_page(PAGE_SIGNATURES);
}
static void signature_delete(void){
 if(signature_slot<0)return;
 if(!signature_delete_confirm){signature_delete_confirm=true;toast_message=NULL;dirty=true;return;}
 unsigned slot=(unsigned)signature_slot;int previous_filter=signature_filter();
 if(signature_pending&(1u<<slot)){
  /* User explicitly discards only unconfirmed local edits; next read decides
   * which version actually reached the backend after an uncertain put. */
  signature_pending&=(uint8_t)~(1u<<slot);signature_errors|=(uint8_t)(1u<<slot);memset(&signatures[slot],0,sizeof(signatures[slot]));memset(signature_means[slot],0,sizeof(signature_means[slot]));signature_load();
 }else{signatures[slot]=(spectrum_signature){0};memset(signature_means[slot],0,sizeof(signature_means[slot]));signature_pending|=(uint8_t)(1u<<slot);signature_save();}
 if(manual_room==(int)slot)manual_room=-1;
 spectrum_room_reset(&room_tracker);if(previous_filter!=signature_filter()||previous_filter==(int)slot){clear_analysis();refresh_analysis();}event_slot=-1;set_page(PAGE_SIGNATURES);
}
static void draw_signatures(void){
 text(0,12,7,216,"SOUND SAMPLES",NOVA_CYAN);text(1,12,29,216,"512 HANN / 32 ms / 62.5 Hz",NOVA_CAP);
 pill(10,49,70,30,"RAW",signature_mode==0);pill(85,49,70,30,"AUTO",signature_mode==1);pill(160,49,70,30,"MANUAL",signature_mode==2);
 char status[40];int selected=signature_filter();
 if(signature_goal)snprintf(status,sizeof(status),"%s ROOM %u / %u",running?"SAMPLING":"PAUSED",signature_capture.frames,signature_goal);
 else if(signature_pending)snprintf(status,sizeof(status),"UNSAVED / TAP LABEL TO RETRY");
 else if(signature_errors)snprintf(status,sizeof(status),"UNREAD SAMPLES / RETRY");
 else if(selected>=0&&signature_mode==2)snprintf(status,sizeof(status),"MANUAL %.16s",signatures[selected].name);
 else if(selected>=0&&(room_tracker.misses||room_tracker.candidate!=selected))snprintf(status,sizeof(status),"%s %.16s",room_tracker.ambiguous?"AMBIGUOUS":"HOLD",signatures[selected].name);
 else if(selected>=0)snprintf(status,sizeof(status),"%u%% MATCH %.16s",room_tracker.confidence,signatures[selected].name);
 else if(signature_mode==1)snprintf(status,sizeof(status),"%s / %u%%",room_tracker.ambiguous?"AMBIGUOUS":"NO ROOM MATCH",room_tracker.confidence);
 else snprintf(status,sizeof(status),"%s",signature_mode==2?"SELECT A ROOM BELOW":"RAW / NO BACKGROUND FILTER");
 text(1,12,85,218,status,NOVA_CAP);
 if(signature_goal){pill(12,111,102,32,running?"STOP":"RESUME",false);pill(126,111,102,32,"CANCEL",false);center(1,12,157,216,"KEEP ROOM QUIET",NOVA_TEXT);center(1,12,177,216,"CANCEL DISCARDS THIS SAMPLE",NOVA_CAP);}
 else{
  for(unsigned row=0;row<2;row++){unsigned slot=signature_scroll+row;if(slot>=SPECTRUM_SIGNATURE_SLOTS)break;int y=107+(int)row*42;spectrum_signature *s=&signatures[slot];char label[40];snprintf(label,sizeof(label),"%u %.16s",slot+1,signature_errors&(1u<<slot)?"UNREAD":(signature_pending&(1u<<slot))&&!s->kind?"DELETE UNSAVED":s->kind?s->name:"EMPTY");text(1,14,y,151,label,NOVA_WHITE);snprintf(label,sizeof(label),s->kind?"%s / %u FRAMES":"TAP + TO CAPTURE",s->kind==1?"ROOM":"EVENT",s->frames);text(1,14,y+17,153,label,NOVA_CAP);if(s->kind==1)pill(174,y,54,31,"USE",signature_mode==2&&manual_room==(int)slot);else if(s->kind==2)text(1,177,y+8,49,event_slot==(int)slot?"HEARD":"EDIT",NOVA_CYAN);}
  pill(10,195,29,28,"<",false);pill(43,195,68,28,"ROOM +",false);pill(116,195,79,28,"EVENT +",false);pill(201,195,29,28,">",false);
 }
 text(1,12,226,63,"BACK",NOVA_CYAN);text(1,87,226,87,running?"STOP MIC":"START MIC",NOVA_CYAN);text(1,182,226,52,"RETRY",NOVA_CYAN);
}
static void draw_signature_edit(void){
 center(0,12,14,216,!editing.color?"DELETE PENDING":editing.color==1?"ROOM SAMPLE":"EVENT SAMPLE",NOVA_CYAN);
 center(1,12,45,216,editing.color==1?"AVERAGE 64 FRAMES / 2.05 sec":"CAPTURE CURRENT 32 ms FRAME",NOVA_CAP);
 round_rect(15,72,210,36,10,NOVA_DIM);text(1,24,81,193,editing.name[0]?editing.name:"TAP TO NAME",NOVA_WHITE);
 char detail[40];spectrum_signature *s=&signatures[signature_slot];snprintf(detail,sizeof(detail),"%u SAVED FRAMES",s->frames);center(1,12,120,216,detail,NOVA_CAP);
 pill(14,149,100,36,!s->kind&&(signature_pending&(1u<<signature_slot))?"RETRY SAVE":s->kind?"ADD SAMPLE":"CAPTURE",true);pill(126,149,100,36,"CANCEL",false);
 if(s->kind||(signature_pending&(1u<<signature_slot)))pill(14,195,100,30,signature_delete_confirm?"CONFIRM":signature_pending&(1u<<signature_slot)?"DISCARD":"DELETE",false);
 if(signature_pending&(1u<<signature_slot))pill(126,195,100,30,"RETRY SAVE",false);
}
static void signature_tap(int x,int y,bool *toggle_requested){
 if(page==PAGE_SIGNATURE_EDIT){if(hit(x,y,15,72,210,36)){signature_keyboard=true;keyboard_begin();}else if(hit(x,y,14,149,100,36)){if(!signatures[signature_slot].kind&&(signature_pending&(1u<<signature_slot))){signature_save();if(!(signature_pending&(1u<<signature_slot)))set_page(PAGE_SIGNATURES);}else signature_capture_begin();}else if(hit(x,y,126,149,100,36))set_page(PAGE_SIGNATURES);else if(hit(x,y,14,195,100,30)&&(signatures[signature_slot].kind||(signature_pending&(1u<<signature_slot))))signature_delete();else if(hit(x,y,126,195,100,30))signature_save();return;}
 if(hit(x,y,10,49,220,30)){signature_mode=x<82?0:x<158?1:2;clear_analysis();refresh_analysis();dirty=true;return;}
 if(y>=225){if(x<78)set_page(PAGE_CONTROLS);else if(x<177)*toggle_requested=true;else{signature_load();signature_save();notify(signature_pending?"SAMPLES UNSAVED / RETRY":signature_errors?"SAMPLES UNREAD / RETRY":"SAVED SAMPLES LOADED");}return;}
 if(signature_goal){if(hit(x,y,12,111,102,32))*toggle_requested=true;else if(hit(x,y,126,111,102,32)){signature_goal=0;signature_capture_slot=-1;memset(&signature_capture,0,sizeof(signature_capture));notify("CAPTURE CANCELLED");}return;}
 if(y>=107&&y<191){unsigned slot=signature_scroll+(unsigned)(y-107)/42;if(slot<8&&(signatures[slot].kind||(signature_pending&(1u<<slot)))){if(x>=171&&signatures[slot].kind==1){manual_room=(int)slot;signature_mode=2;clear_analysis();refresh_analysis();dirty=true;}else signature_open((int)slot,0);}return;}
 if(y>=195&&y<224){if(x<40&&signature_scroll)signature_scroll--;else if(x>=201&&signature_scroll+2<8)signature_scroll++;else if(x>=43&&x<111)signature_open(-1,1);else if(x>=116&&x<195)signature_open(-1,2);dirty=true;}
}
