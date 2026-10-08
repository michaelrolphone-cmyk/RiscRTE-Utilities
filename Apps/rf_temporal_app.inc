/* Temporal examples are an independent app-data collection. Legacy KV room
 * and single-frame profiles remain intact; missing/new files never replace them. */
static uint32_t event_neural_crc[2];
static bool event_neural_checkpoint,event_neural_cache_blocked,event_neural_used;
static risc_runtime_capability_v1 event_data_grant;
#ifdef PORTABLE_BLE_BROADCAST
#include "PortableBroadcastAppData.h"
static portable_broadcast_app_data event_broadcast_data;
#endif
static bool event_data_acquired,event_initialized,event_armed,event_wait_quiet,event_known_hold,event_previous_active;
static uint64_t event_pair_power[128];
static uint32_t event_previous_power[128];
static unsigned event_pair_count,event_quiet,event_wait_count,event_capture_kind,event_list_scroll,event_example_scroll,event_edit_shift,event_missed;
static bool event_pair_active;
static int event_edit_slot,event_delete_slot,event_delete_example;
static uint32_t event_armed_at,event_result_at;
static const char *event_message;
_Static_assert(sizeof(event_library)+sizeof(event_files)+sizeof(event_segment)+sizeof(event_training)+sizeof(event_match)<340000,"bounded temporal state and three file snapshots");
static const char *event_neural_file(void){return "rf-neural.rnn";}
static int32_t event_neural_read(uint32_t *size,uint64_t *revision){
 *size=0;*revision=0;const risc_app_data_v1 *api=event_files.api;
 if(!api||temporal_retained())return RISC_APP_DATA_UNAVAILABLE;
 int32_t result=api->stat(api->context,event_neural_file(),size,revision);
 if(result==RISC_APP_DATA_RETAINED){event_files.retained=true;return result;}
 if(result!=RISC_APP_DATA_OK)return result;
 if(*size!=RN_RECORD_SIZE||!*revision)return RT_FILES_CORRUPT;
 uint32_t actual=0;uint64_t current=0;result=api->read(api->context,event_neural_file(),*revision,event_files.readback,sizeof(event_files.readback),&actual,&current);
 if(result==RISC_APP_DATA_RETAINED){event_files.retained=true;return result;}
 if(result)return result;
 if(actual!=*size||!current||!rn_record_valid(event_files.readback,actual))return RT_FILES_CORRUPT;
 *revision=current;return RISC_APP_DATA_OK;
}
/* Called only with radio ownership released by existing storage paths.
 * Pending edits disable the model immediately and do not overwrite snapshots. */
static void event_neural_refresh(void){
 event_neural_checkpoint=event_neural_cache_blocked=event_neural_used=false;
 uint8_t available=event_files.pending<0?(uint8_t)((event_files.ready&1?15u:0u)|(event_files.ready&2?240u:0u)):0;
 rn_reset(&event_neural,&event_library,available);
 if(event_neural.state==RN_NEEDS_EXAMPLES||event_files.ready!=3)return;
 for(unsigned bank=0;bank<2;bank++){
  size_t size=rt_bank_encode(&event_library,bank,event_files.readback,sizeof(event_files.readback));
  if(!size){memset(&event_neural,0,sizeof(event_neural));return;}
  event_neural_crc[bank]=rf_signature_u32(event_files.readback+size-4);
 }
 uint32_t size;uint64_t revision;int32_t result=event_neural_read(&size,&revision);
 if(!result)(void)rn_record_load(&event_neural,&event_library,event_neural_crc,event_files.readback,size);
 else if(result!=RISC_APP_DATA_NOT_FOUND)event_neural_cache_blocked=true;
}
static void event_neural_save(void){
 if(!event_neural_checkpoint||event_neural_cache_blocked||event_files.pending>=0)return;
 pause_capture();if(temporal_retained())return;
 uint32_t size;uint64_t revision;int32_t result=event_neural_read(&size,&revision);
 if(temporal_retained())return;
 if(result==RISC_APP_DATA_NOT_FOUND)revision=0;
 else if(result){event_neural_cache_blocked=true;portable_rf_capture_resume();return;}
 if(!rn_record_encode(&event_neural,&event_library,event_neural_crc,event_files.wanted,sizeof(event_files.wanted))){event_neural_checkpoint=false;portable_rf_capture_resume();return;}
 if(result==RISC_APP_DATA_OK&&!memcmp(event_files.readback,event_files.wanted,RN_RECORD_SIZE))result=RISC_APP_DATA_OK;
 else result=event_files.api->replace(event_files.api->context,event_neural_file(),revision,event_files.wanted,RN_RECORD_SIZE);
 if(result==RISC_APP_DATA_RETAINED){event_files.retained=true;return;}
 event_neural_checkpoint=false;event_neural_cache_blocked=result!=RISC_APP_DATA_OK;
 portable_rf_capture_resume();dirty=true;
}
static void event_neural_tick(void){
 if(!event_initialized||event_files.ready!=3||event_files.pending>=0||temporal_retained()||event_armed||event_training.ready||signature_goal||event_match.running||event_segment.collecting||event_segment.ready||(running&&(!ambient.ready||(ambient.foreground&&rf_background_db(rf_signature_total(foreground_power))>=prefs.threshold_db*100)||input_waiting)))return;
 unsigned before=event_neural.state;rn_tick(&event_neural,&event_library);
 if(event_neural.state!=before){dirty=true;if(event_neural.state==RN_ACTIVE)event_neural_checkpoint=true;}
 if(event_neural_checkpoint)event_neural_save();
}
static const char *event_error(int32_t code){switch(code){case RISC_APP_DATA_OK:return "SAVED";case RISC_APP_DATA_UNAVAILABLE:return "APP DATA UNAVAILABLE";case RISC_APP_DATA_NO_SPACE:return "STORAGE FULL / KEEP DRAFT";case RISC_APP_DATA_COMMIT_UNKNOWN:return "SAVE UNKNOWN / RETRY";case RISC_APP_DATA_STALE:return "STORAGE CHANGED / RETRY";case RT_FILES_CONFLICT:return "COLLECTION CHANGED / DISCARD?";case RT_FILES_CORRUPT:return "COLLECTION UNREAD / RETRY";case RISC_APP_DATA_RETAINED:return "STORAGE CLEANUP UNCONFIRMED";default:return "STORAGE ERROR / RETRY";}}
static bool event_acquire(void){
 if(event_files.api)return true;
 if(event_data_acquired)return false;
 event_data_grant=(risc_runtime_capability_v1){.struct_size=sizeof(event_data_grant)};
 if(!runtime->acquire("storage.app-data",1,RF_APP_DATA_INSTANCE,&event_data_grant))return false;
 event_data_acquired=true;
 const risc_app_data_v1 *api=event_data_grant.api;if(!api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->stat||!api->read||!api->replace)return false;
#ifdef PORTABLE_BLE_BROADCAST
 event_files.api=portable_broadcast_data_bind(&event_broadcast_data,api,runtime);
#else
 event_files.api=api;
#endif
 return event_files.api!=NULL;
}
static void event_load(void){
 if(event_files.retained)return;
 if(!event_acquire()){event_message="APP DATA UNAVAILABLE";return;}
 for(unsigned bank=0;bank<2;bank++)if(!(event_files.ready&(1u<<bank))&&event_files.pending!=(int)bank){int32_t result=rt_files_load(&event_files,&event_library,bank,false);if(result)event_message=event_error(result);if(event_files.retained)return;}
 if(!rf_identity_valid(&event_library.identity))event_library.identity=prefs.identity;
 if(event_files.ready==3&&event_files.pending<0)event_message=NULL;
 event_neural_refresh();
}
static void event_restore(void){

 memset(&event_library,0,sizeof(event_library));rt_files_init(&event_files,NULL);rt_segment_reset(&event_segment);rt_segment_reset(&event_training);memset(&event_match,0,sizeof(event_match));event_match.selected=-1;
 event_data_grant=(risc_runtime_capability_v1){0};event_data_acquired=false;event_initialized=true;event_armed=event_wait_quiet=event_known_hold=event_previous_active=false;event_pair_count=event_quiet=event_wait_count=event_missed=event_list_scroll=event_example_scroll=0;event_pair_active=false;event_edit_slot=event_delete_slot=event_delete_example=-1;event_capture_kind=RT_POSITIVE;event_edit_shift=1;event_armed_at=event_result_at=0;event_message=NULL;event_keyboard=false;memset(event_pair_power,0,sizeof(event_pair_power));memset(event_previous_power,0,sizeof(event_previous_power));event_load();
}
static bool temporal_retained(void){return event_initialized&&event_files.retained;}
static bool temporal_exit_blocked(void){return event_initialized&&(event_files.pending>=0||event_armed||event_training.ready);}
static void temporal_suspend(void){
 if(!event_initialized)return;
 if(event_armed){rt_segment_stop(&event_training);event_armed=false;event_message=event_training.ready?"INTERRUPTED WINDOW / REVIEW":"STOPPED / NO ACTIVITY SAVED";}
 event_match.running=false;rt_segment_rearm(&event_segment);event_pair_count=0;event_pair_active=false;memset(event_pair_power,0,sizeof(event_pair_power));
}
static void temporal_reset_context(void){

 event_known_hold=false;event_previous_active=false;event_pair_count=0;event_pair_active=false;event_quiet=0;memset(event_pair_power,0,sizeof(event_pair_power));memset(event_previous_power,0,sizeof(event_previous_power));rt_segment_reset(&event_segment);event_match.running=false;
}
static void temporal_start(void){rf_background_reset(&ambient);temporal_reset_context();}
static void event_retry(void){
 pause_capture();if(temporal_retained())return;
 if(event_files.pending>=0){int32_t result=rt_files_save(&event_files);event_message=event_error(result);if(!temporal_retained())event_neural_refresh();if(!temporal_retained())portable_rf_capture_resume();if(!temporal_retained())notify(result?event_message:running?"SAVED / LIVE":"SAVED / RF OFF");}
 else{event_load();if(!temporal_retained())portable_rf_capture_resume();if(!temporal_retained())notify(event_message?event_message:"EVENT COLLECTIONS LOADED");}
 dirty=true;
}
static void event_discard(void){
 if(event_files.pending<0)return;
 pause_capture();unsigned bank=(unsigned)event_files.pending;int32_t result=rt_files_load(&event_files,&event_library,bank,true);event_message=event_error(result);if(!temporal_retained())event_neural_refresh();if(!temporal_retained())portable_rf_capture_resume();if(!temporal_retained())notify(result?event_message:"UNSAVED EDIT DISCARDED");event_delete_slot=event_delete_example=-1;dirty=true;
}
static bool event_begin_edit(unsigned slot){
 if(slot>=8||event_files.pending>=0||event_armed||event_training.ready||signature_goal){notify("SAVE OR DISCARD CURRENT EDIT");return false;}
 pause_capture();int32_t result=rt_files_begin(&event_files,&event_library,slot/4);if(result){if(!temporal_retained())portable_rf_capture_resume();notify(event_error(result));return false;}memset(&event_neural,0,sizeof(event_neural));event_neural_checkpoint=event_neural_used=false;event_match.running=false;event_match.selected=-1;return true;
}
static void event_finish_edit(void){
 if(!rt_files_freeze(&event_files,&event_library)){event_neural_refresh();if(!temporal_retained()){portable_rf_capture_resume();notify("INVALID EVENT EDIT");}return;}int32_t result=rt_files_save(&event_files);event_message=event_error(result);if(!temporal_retained())event_neural_refresh();if(!temporal_retained())portable_rf_capture_resume();if(!temporal_retained()){notify(result?event_message:running?"SAVED / LIVE":"SAVED / RF OFF");if(event_files.pending>=0)set_page(PAGE_EVENTS);}dirty=true;
}
static void event_open(int slot){
 if(temporal_exit_blocked()){notify("SAVE OR DISCARD CURRENT EDIT");return;}
 bool naming=slot<0;
 if(slot<0){for(unsigned i=0;i<8;i++)if((event_files.ready&(1u<<(i/4)))&&!event_library.labels[i].present){slot=(int)i;break;}if(slot<0){notify(event_files.ready!=3?"RETRY EVENT STORAGE":"8 EVENT LABELS FULL");return;}}
 event_edit_slot=slot;editing=(rf_label){0};const rt_label*l=&event_library.labels[slot];if(l->present)memcpy(editing.name,l->name,17);event_edit_shift=l->present?l->shift_limit:1;event_delete_slot=event_delete_example=-1;event_example_scroll=0;event_keyboard=true;toast_message=NULL;set_page(PAGE_EVENT_LABEL);
 if(naming)keyboard_begin();
}
static void event_save_name(void){
 if(event_edit_slot<0)return;
 unsigned start=0,end=(unsigned)strlen(editing.name);while(start<end&&editing.name[start]==' ')start++;while(end>start&&editing.name[end-1]==' ')end--;for(unsigned i=0;i<end-start;i++)editing.name[i]=editing.name[start+i];editing.name[end-start]=0;
 if(!rf_signature_name_valid(editing.name)){notify("ENTER AN EVENT NAME");return;}
 if(!event_begin_edit((unsigned)event_edit_slot))return;
 rt_label*l=&event_library.labels[event_edit_slot];l->present=true;l->shift_limit=(uint8_t)event_edit_shift;if(!l->next_id)l->next_id=1;memcpy(l->name,editing.name,17);event_finish_edit();
}
static void event_arm(unsigned kind){
 if(event_edit_slot<0||kind<RT_POSITIVE||kind>RT_NEGATIVE)return;
 if(!rf_identity_equal(&event_library.identity,&prefs.identity)){notify("RECEIVER MISMATCH / RESTORE RF");return;}
 if(event_files.pending>=0||event_training.ready||signature_goal){notify("SAVE OR DISCARD CURRENT EDIT");return;}
 if(event_armed){set_page(PAGE_EVENT_CAPTURE);return;}
 if(!event_library.labels[event_edit_slot].present||event_edit_shift!=event_library.labels[event_edit_slot].shift_limit||strcmp(editing.name,event_library.labels[event_edit_slot].name)){
  event_save_name();if(temporal_retained())return;
  if(event_files.pending>=0){toast_message=NULL;set_page(PAGE_EVENTS);return;}
  if(!event_library.labels[event_edit_slot].present||event_edit_shift!=event_library.labels[event_edit_slot].shift_limit||strcmp(editing.name,event_library.labels[event_edit_slot].name))return;
 }
 rt_label*l=&event_library.labels[event_edit_slot];if(rt_example_count(l,0)>=6||l->next_id==UINT32_MAX){notify("6 EXAMPLES FULL / DELETE ONE");return;}
 if(!running){toggle();}if(!running){notify("RF FAILED / TAP RECORD AGAIN");return;}
 rt_segment_reset(&event_training);event_capture_kind=kind;event_armed=true;event_wait_quiet=true;event_wait_count=0;event_armed_at=app->millis();event_message="WAIT FOR QUIET / THEN ACT";event_delete_slot=event_delete_example=-1;toast_message=NULL;set_page(PAGE_EVENT_CAPTURE);
}
static void event_cancel_capture(void){event_armed=false;rt_segment_reset(&event_training);event_message="CAPTURE CANCELLED";set_page(PAGE_EVENT_LABEL);}
static void event_save_capture(void){
 if(!event_training.ready||event_edit_slot<0)return;
 rt_label*l=&event_library.labels[event_edit_slot];unsigned index=0;for(;index<6&&l->examples[index].id;index++);if(index==6||l->next_id==UINT32_MAX){notify("6 EXAMPLES FULL / KEEP WINDOW");return;}
 pause_capture();int32_t result=rt_files_begin(&event_files,&event_library,(unsigned)event_edit_slot/4);if(result){if(!temporal_retained())portable_rf_capture_resume();notify(event_error(result));return;}
 rt_example example=event_training.event;example.kind=(uint8_t)event_capture_kind;example.id=l->next_id++;l->examples[index]=example;rt_segment_reset(&event_training);event_armed=false;event_finish_edit();if(!temporal_retained())set_page(event_files.pending>=0?PAGE_EVENTS:PAGE_EVENT_LABEL);
}
static void event_delete_label(unsigned slot){
 if(event_delete_slot!=(int)slot||event_delete_example!=-2){event_delete_slot=(int)slot;event_delete_example=-2;toast_message=NULL;dirty=true;return;}
 if(!event_begin_edit(slot))return;
 memset(&event_library.labels[slot],0,sizeof(rt_label));event_finish_edit();event_delete_slot=event_delete_example=-1;
}
static void event_delete_saved(unsigned index){
 if(event_edit_slot<0||index>=6||!event_library.labels[event_edit_slot].examples[index].id)return;
 if(event_delete_slot!=event_edit_slot||event_delete_example!=(int)index){event_delete_slot=event_edit_slot;event_delete_example=(int)index;toast_message=NULL;dirty=true;return;}
 if(!event_begin_edit((unsigned)event_edit_slot))return;
 memset(&event_library.labels[event_edit_slot].examples[index],0,sizeof(rt_example));event_finish_edit();event_delete_slot=event_delete_example=-1;
}
static void temporal_observe(void){
 ambient.freeze_upward=event_armed||event_training.ready||event_match.running||event_known_hold;
 rf_background_observe(&ambient,signature_audio.power);
 uint64_t active_power=rf_background_salient(&ambient,foreground_power);
 detect_frequency_labels();
 rt_frame frame=rt_frame_make(foreground_power,event_previous_power,ambient.foreground&&rf_background_db(active_power)>=prefs.threshold_db*100,burst_at,burst_gap);memcpy(event_previous_power,foreground_power,sizeof(event_previous_power));
 if(!ambient.ready){rt_segment_reset(&event_segment);if(event_armed){event_armed_at=app->millis();dirty=true;}return;}
 if(!(frame.flags&RT_ACTIVE)){if(event_quiet<4)event_quiet++;if(event_quiet>=4)event_known_hold=false;}else event_quiet=0;
 if(event_segment.ready&&event_match.running&&(frame.flags&RT_ACTIVE)&&!event_previous_active)event_missed++;
 event_previous_active=!!(frame.flags&RT_ACTIVE);
 if(!event_segment.ready)rt_segment_observe(&event_segment,&frame);
 if(event_armed){
  if(event_wait_quiet){event_training.pre[event_training.next]=frame;event_training.next=(event_training.next+1u)%RT_PRE;if(event_training.used<RT_PRE)event_training.used++;if(frame.flags&RT_ACTIVE)event_wait_count=0;else if(++event_wait_count>=RT_PRE){event_wait_quiet=false;event_armed_at=app->millis();event_message="TRIGGER THE EVENT NOW";}}
  else{rt_segment_observe(&event_training,&frame);if(event_training.ready){event_armed=false;event_message=event_training.event.flags&RT_CLIPPED?"WINDOW LIMIT / REVIEW":"EXAMPLE READY / REVIEW";dirty=true;}}
  if(event_armed&&!event_training.collecting&&(uint32_t)(app->millis()-event_armed_at)>=8000u){event_armed=false;event_message="NO NEW ACTIVITY / RECORD AGAIN";set_page(PAGE_EVENT_LABEL);notify(event_message);}
 }
 if(event_segment.ready&&!event_match.running&&rf_identity_equal(&event_library.identity,&prefs.identity)){rt_match_begin(&event_match,&event_segment.event);event_match.allowed_labels=(uint8_t)((event_files.ready&1?15u:0u)|(event_files.ready&2?240u:0u));rt_segment_rearm(&event_segment);}
 if(page==PAGE_EVENTS||page==PAGE_EVENT_CAPTURE)dirty=true;
}
static void temporal_tick(void){
 if(event_match.running){rt_match_tick(&event_match,&event_library,64);if(event_match.complete){event_neural_used=rn_apply(&event_neural,&event_match);event_result_at=app->millis();event_known_hold=event_match.reason==RT_RESULT_MATCH;dirty=true;}}
 if(event_match.complete&&(uint32_t)(app->millis()-event_result_at)>4000u){event_match.complete=false;dirty=true;}
}
static bool temporal_exit_ready(void){
 if(!temporal_exit_blocked())return true;
 event_message=event_files.pending>=0?"RETRY SAVE OR DISCARD":"SAVE OR CANCEL THE WINDOW";notify(event_message);set_page(event_training.ready||event_armed?PAGE_EVENT_CAPTURE:PAGE_EVENTS);return false;
}
static void draw_events(void){
 text(0,12,7,216,"LEARN RF EVENTS",NOVA_CYAN);char caption[64];
 if(event_files.pending>=0)snprintf(caption,sizeof(caption),"%s",event_error(event_files.errors[event_files.pending]));else if(event_files.ready!=3)snprintf(caption,sizeof(caption),"STORAGE UNAVAILABLE / RETRY");else if(!rf_identity_equal(&event_library.identity,&prefs.identity))snprintf(caption,sizeof(caption),"RESTORE EVENT RF IN RECEIVER");else if(!ambient.ready)snprintf(caption,sizeof(caption),"%s",running?"LEARNING / 64 OBSERVATIONS":"START RF TO DETECT");else if(event_match.running)snprintf(caption,sizeof(caption),"MATCHING / %u EXAMPLES",event_match.compared);else if(event_match.complete&&event_match.selected>=0)snprintf(caption,sizeof(caption),"%.16s %u%%",event_library.labels[event_match.selected].name,event_match.score/10);else if(event_match.complete)snprintf(caption,sizeof(caption),"%s",event_match.reason==RT_RESULT_NEGATIVE?"CONFUSING NONMATCH":event_match.reason==RT_RESULT_AMBIGUOUS?"AMBIGUOUS / NO LABEL":"UNKNOWN EVENT");else snprintf(caption,sizeof(caption),"BACKGROUND RELATIVE / READY");text(1,12,33,216,caption,NOVA_CAP);
 for(unsigned row=0;row<2;row++){unsigned slot=event_list_scroll+row;rt_label*l=&event_library.labels[slot];int y=64+(int)row*49;snprintf(caption,sizeof(caption),"%u %.16s",slot+1,l->present?l->name:"EMPTY");text(1,14,y,151,caption,NOVA_WHITE);snprintf(caption,sizeof(caption),"%u MATCH / %u OTHER",rt_example_count(l,1),rt_example_count(l,2));text(1,14,y+20,156,caption,NOVA_CAP);if(l->present)pill(183,y,45,32,event_delete_slot==(int)slot&&event_delete_example==-2?"SURE":"X",false);}
 if(event_missed){snprintf(caption,sizeof(caption),"BUSY / %u ONSETS MISSED",event_missed);text(1,12,163,216,caption,0xffd24au);}
 else{if(event_neural.state==RN_TRAINING)snprintf(caption,sizeof(caption),"NEURAL LEARNING %u%%",event_neural.epoch*100u/RN_EPOCHS);
  else if(event_neural.state==RN_PREPARING)snprintf(caption,sizeof(caption),"NEURAL / PREPARING EXAMPLES");
  else if(event_neural.state==RN_CHECKING)snprintf(caption,sizeof(caption),"NEURAL / HELD-OUT CHECK");
  else if(event_neural.has_active&&event_neural_cache_blocked)snprintf(caption,sizeof(caption),"NEURAL READY / RAM ONLY");
  else if(event_neural.has_active)snprintf(caption,sizeof(caption),"NEURAL READY / CHECK %u OF %u",event_neural.candidate_correct,event_neural.checks);
  else if(event_neural.state==RN_BASELINE)snprintf(caption,sizeof(caption),"%s",event_neural.calibrated!=event_neural.eligible?"NEURAL / TEST CONFUSING EVENTS":"TEMPORAL / NO NEURAL GAIN");
  else snprintf(caption,sizeof(caption),"NEURAL NEEDS 3 MATCH + 2 OTHER");
  text(1,12,163,216,caption,NOVA_CAP);}
 if(event_files.pending>=0){pill(12,188,103,30,"RETRY SAVE",false);pill(125,188,103,30,event_delete_example==-3?"CONFIRM":"DISCARD",false);}else{pill(12,188,34,30,"<",false);pill(57,188,125,30,"NEW EVENT",true);pill(194,188,34,30,">",false);}
 text(1,12,225,65,"BACK",NOVA_CYAN);text(1,88,225,91,running?"STOP RF":"START RF",NOVA_CYAN);text(1,181,225,55,"RETRY",NOVA_CYAN);
}
static void draw_event_label(void){
 text(0,12,7,216,"TEACH AN EVENT",NOVA_CYAN);round_rect(12,38,155,32,9,NOVA_DIM);text(1,21,46,139,editing.name[0]?editing.name:"TAP TO NAME",NOVA_WHITE);pill(174,38,54,32,"SAVE",false);
 const rt_label*l=&event_library.labels[event_edit_slot];char caption[64];if(running&&!ambient.ready)snprintf(caption,sizeof(caption),"WARMING %u / 64",ambient.frames);else snprintf(caption,sizeof(caption),"%u MATCH / %u OTHER / 6",rt_example_count(l,1),rt_example_count(l,2));text(1,12,80,154,caption,NOVA_CAP);pill(174,76,54,24,running?"STOP":"RF",false);
 pill(12,104,104,34,"RECORD",true);pill(124,104,104,34,"NONMATCH",false);text(1,12,151,60,"SHIFT",NOVA_CAP);pill(76,145,29,32,"-",false);snprintf(caption,sizeof(caption),event_edit_shift?"+/- %u.%u bands":"OFF",event_edit_shift,0u);center(1,108,152,84,caption,NOVA_WHITE);pill(199,145,29,32,"+",false);
 if(event_match.selected==event_edit_slot){int shift=event_match.shift[event_edit_slot];snprintf(caption,sizeof(caption),"SHIFT %+d bands %u / %u MHz",shift,rf_coord_hz(&event_library.identity,event_match.observed_coord)/1000000u,rf_coord_hz(&event_library.identity,event_match.reference_coord[event_edit_slot])/1000000u);text(1,12,180,216,caption,NOVA_CAP);}
 else text(1,12,180,216,"NEURAL: TEACH AT LEAST 2 LABELS",NOVA_CAP);
 pill(12,205,104,29,"EXAMPLES",false);pill(124,205,104,29,"BACK",false);
}
static void draw_event_capture(void){
 text(0,12,7,216,event_capture_kind==RT_NEGATIVE?"EVENT TO EXCLUDE":"RECORD RF EVENT",NOVA_CYAN);center(1,12,35,216,event_library.labels[event_edit_slot].name,NOVA_WHITE);char caption[64];
 if(!event_armed&&!event_training.ready){center(1,12,85,216,"CAPTURE STOPPED",NOVA_CAP);center(1,12,117,216,"NO EXAMPLE SAVED",NOVA_CAP);pill(68,183,104,36,"BACK",false);return;}
 if(event_training.ready){const rt_example*e=&event_training.event;center(1,12,63,216,e->flags&RT_CONFIRMED_END?"WHOLE EVENT / END CONFIRMED":e->flags&RT_CLIPPED?"END NOT VERIFIED / REVIEW":"READY TO SAVE",e->flags&RT_CLIPPED?0xffd24au:NOVA_CYAN);snprintf(caption,sizeof(caption),"SPAN %u ms / %u ONSETS",e->duration_ms,e->impacts);center(1,12,90,216,caption,NOVA_TEXT);snprintf(caption,sizeof(caption),"RISE %u / FALL %u ms",e->attack_ms,e->decay_ms);center(1,12,112,216,caption,NOVA_CAP);snprintf(caption,sizeof(caption),"PEAK %u MHz / %u BURSTS",rf_coord_hz(&event_library.identity,e->peak_coord)/1000000u,e->count);center(1,12,134,216,caption,NOVA_CAP);if(e->flags&RT_CLIPPED)pill(12,151,216,25,e->flags&RT_CONFIRMED_END?"UNDO WHOLE EVENT":"USE AS WHOLE EVENT",!!(e->flags&RT_CONFIRMED_END));int buttons=e->flags&RT_CLIPPED?188:175;pill(12,buttons,104,36,rt_partial(e)?"SAVE WINDOW":"SAVE",true);pill(124,buttons,104,36,"CANCEL",false);}
 else{center(1,12,71,216,!ambient.ready?"KEEP QUIET / PREPARING":event_wait_quiet?"QUIET OBSERVATIONS":event_training.collecting?"RECORDING RF OBSERVATIONS":"TRIGGER THE EVENT NOW",NOVA_CYAN);if(!ambient.ready)snprintf(caption,sizeof(caption),"BACKGROUND %u%%",ambient.frames*100u/RF_BG_WARMUP);else snprintf(caption,sizeof(caption),"%u / 64 OBSERVATIONS",event_training.event.count);center(1,12,104,216,caption,NOVA_WHITE);center(1,12,129,216,event_capture_kind==RT_NEGATIVE?"TRIGGER A NONMATCH":"LET THE EVENT END",NOVA_CAP);center(1,12,151,216,"BURSTS / GAPS UNOBSERVED",NOVA_CAP);pill(12,185,104,36,"STOP",false);pill(124,185,104,36,"CANCEL",false);}
}
static void draw_event_examples(void){
 text(0,12,7,216,"SAVED EXAMPLES",NOVA_CYAN);text(1,12,32,216,event_library.labels[event_edit_slot].name,NOVA_WHITE);
 for(unsigned row=0;row<2;row++){unsigned ex=event_example_scroll+row;const rt_example*e=&event_library.labels[event_edit_slot].examples[ex];int y=62+(int)row*58;char caption[64];snprintf(caption,sizeof(caption),e->id?"%u %s / %u ms":"%u EMPTY",ex+1,e->kind==1?"POSITIVE":"NONMATCH",e->duration_ms);text(1,12,y,166,caption,NOVA_WHITE);if(e->id){snprintf(caption,sizeof(caption),"%s%u MHz / %u ONSETS",e->flags&RT_CONFIRMED_END?"END SET / ":e->flags&RT_CLIPPED?"CLIP / ":"",rf_coord_hz(&event_library.identity,e->peak_coord)/1000000u,e->impacts);text(1,12,y+22,172,caption,NOVA_CAP);pill(184,y,44,34,event_delete_example==(int)ex?"SURE":"X",false);}}
 pill(12,189,40,33,"<",false);pill(68,189,104,33,"BACK",false);pill(188,189,40,33,">",false);
}
static void event_tap(int x,int y,bool *toggle_requested){
 if(page==PAGE_EVENTS){
  if(y>=223){if(x<76){toast_message=NULL;set_page(PAGE_CONTROLS);}else if(x<179)*toggle_requested=true;else event_retry();return;}
  if(event_files.pending>=0&&y>=185&&y<220){if(x<120)event_retry();else if(event_delete_example==-3)event_discard();else{event_delete_example=-3;toast_message=NULL;dirty=true;}return;}
  if(y>=64&&y<164){unsigned slot=event_list_scroll+(unsigned)(y-64)/49;if(slot<8&&event_library.labels[slot].present){if(x>=180)event_delete_label(slot);else event_open((int)slot);}return;}
  if(y>=185&&y<221){if(x<53&&event_list_scroll)event_list_scroll--;else if(x>=188&&event_list_scroll<6)event_list_scroll++;else if(x>=57&&x<183)event_open(-1);dirty=true;}return;
 }
 if(page==PAGE_EVENT_LABEL){if(hit(x,y,12,38,155,32)){event_keyboard=true;keyboard_begin();}else if(hit(x,y,174,38,54,32))event_save_name();else if(hit(x,y,174,76,54,24))*toggle_requested=true;else if(hit(x,y,12,104,104,34))event_arm(1);else if(hit(x,y,124,104,104,34))event_arm(2);else if(hit(x,y,76,145,29,32)){if(event_edit_shift)event_edit_shift--;dirty=true;}else if(hit(x,y,199,145,29,32)){if(event_edit_shift<2)event_edit_shift++;dirty=true;}else if(hit(x,y,12,205,104,29)){event_example_scroll=0;set_page(PAGE_EVENT_EXAMPLES);}else if(hit(x,y,124,205,104,29)){toast_message=NULL;set_page(PAGE_EVENTS);}return;}
 if(page==PAGE_EVENT_CAPTURE){if(!event_armed&&!event_training.ready){if(hit(x,y,68,183,104,36))set_page(PAGE_EVENT_LABEL);return;}if(event_training.ready){bool clipped=!!(event_training.event.flags&RT_CLIPPED);int buttons=clipped?188:175;if(clipped&&hit(x,y,12,151,216,25)){event_training.event.flags^=RT_CONFIRMED_END;dirty=true;}else if(hit(x,y,12,buttons,104,36))event_save_capture();else if(hit(x,y,124,buttons,104,36))event_cancel_capture();}else if(hit(x,y,12,185,104,36)){rt_segment_stop(&event_training);event_armed=false;if(!event_training.ready){event_message="NO ONSET / NOTHING SAVED";set_page(PAGE_EVENT_LABEL);notify(event_message);}dirty=true;}else if(hit(x,y,124,185,104,36))event_cancel_capture();return;}
 if(page==PAGE_EVENT_EXAMPLES){if(y>=62&&y<178&&x>=180){unsigned ex=event_example_scroll+(unsigned)(y-62)/58;event_delete_saved(ex);}else if(y>=187&&y<224){if(x<55&&event_example_scroll)event_example_scroll--;else if(x>=185&&event_example_scroll<4)event_example_scroll++;else if(x>=68&&x<173)set_page(PAGE_EVENT_LABEL);dirty=true;}}
}
