/* Temporal examples are an independent app-data collection. Legacy KV room
 * and single-frame profiles remain intact; missing/new files never replace them. */
static st_library event_library;
static st_files event_files;
static st_segmenter event_segment,event_training;
static st_matcher event_match;
static sn_trainer event_neural;
static uint32_t event_neural_crc[2];
static bool event_neural_checkpoint,event_neural_cache_blocked,event_neural_used;
#include "spectrum_scene_data_scope.h"
/* Reuse the RF app/shared-data transaction fence for Audio namespace 2. */
static rf_app_data_scope event_data_scope;
static bool event_initialized,event_armed,event_wait_quiet,event_known_hold,event_previous_active;
static uint64_t event_pair_power[128];
static uint32_t event_previous_power[128];
static unsigned event_pair_count,event_quiet,event_wait_count,event_capture_kind,event_list_scroll,event_example_scroll,event_edit_shift,event_missed;
static bool event_pair_active;
static int event_edit_slot,event_delete_slot,event_delete_example;
static uint32_t event_armed_at,event_result_at;
static const char *event_message;
_Static_assert(sizeof(event_library)+sizeof(event_files)+sizeof(event_segment)+sizeof(event_training)+sizeof(event_match)<315000,"bounded temporal state and three file snapshots");
static const char *event_neural_file(void){return "spectrum-neural.snn";}
static int32_t event_neural_read(uint32_t *size,uint64_t *revision){
 *size=0;*revision=0;const risc_app_data_v1 *api=event_files.api;
 if(!api||temporal_retained())return RISC_APP_DATA_UNAVAILABLE;
 int32_t result=api->stat(api->context,event_neural_file(),size,revision);
 if(result==RISC_APP_DATA_RETAINED){event_files.retained=true;return result;}
 if(result!=RISC_APP_DATA_OK)return result;
 if(*size!=SN_RECORD_SIZE||!*revision)return ST_FILES_CORRUPT;
 uint32_t actual=0;uint64_t current=0;result=api->read(api->context,event_neural_file(),*revision,event_files.readback,sizeof(event_files.readback),&actual,&current);
 if(result==RISC_APP_DATA_RETAINED){event_files.retained=true;return result;}
 if(result)return result;
 if(actual!=*size||!current||!sn_record_valid(event_files.readback,actual))return ST_FILES_CORRUPT;
 *revision=current;return RISC_APP_DATA_OK;
}
/* Called only with microphone ownership released by existing storage paths.
 * Pending edits disable the model immediately and do not overwrite snapshots. */
static void event_neural_refresh(void){
 event_neural_checkpoint=event_neural_cache_blocked=event_neural_used=false;
 uint8_t available=event_files.pending<0?(uint8_t)((event_files.ready&1?15u:0u)|(event_files.ready&2?240u:0u)):0;
 sn_reset(&event_neural,&event_library,available);
 if(event_neural.state==SN_NEEDS_EXAMPLES||event_files.ready!=3)return;
 for(unsigned bank=0;bank<2;bank++){
  size_t size=st_bank_encode(&event_library,bank,event_files.readback,sizeof(event_files.readback));
  if(!size){memset(&event_neural,0,sizeof(event_neural));return;}
  event_neural_crc[bank]=spectrum_signature_u32(event_files.readback+size-4);
 }
 uint32_t size;uint64_t revision;int32_t result=event_neural_read(&size,&revision);
 if(!result)(void)sn_record_load(&event_neural,&event_library,event_neural_crc,event_files.readback,size);
 else if(result!=RISC_APP_DATA_NOT_FOUND)event_neural_cache_blocked=true;
}
static void event_neural_save(void){
 if(!event_neural_checkpoint||event_neural_cache_blocked||event_files.pending>=0)return;
 pause_capture();if(temporal_retained())return;
 uint32_t size;uint64_t revision;int32_t result=event_neural_read(&size,&revision);
 if(temporal_retained())return;
 if(result==RISC_APP_DATA_NOT_FOUND)revision=0;
 else if(result){event_neural_cache_blocked=true;portable_audio_capture_resume();return;}
 if(!sn_record_encode(&event_neural,&event_library,event_neural_crc,event_files.wanted,sizeof(event_files.wanted))){event_neural_checkpoint=false;portable_audio_capture_resume();return;}
 if(result==RISC_APP_DATA_OK&&!memcmp(event_files.readback,event_files.wanted,SN_RECORD_SIZE))result=RISC_APP_DATA_OK;
 else result=event_files.api->replace(event_files.api->context,event_neural_file(),revision,event_files.wanted,SN_RECORD_SIZE);
 if(result==RISC_APP_DATA_RETAINED){event_files.retained=true;return;}
 event_neural_checkpoint=false;event_neural_cache_blocked=result!=RISC_APP_DATA_OK;
 portable_audio_capture_resume();dirty=true;
}
static void event_neural_tick(void){
 if(!event_initialized||event_files.ready!=3||event_files.pending>=0||temporal_retained()||event_armed||event_training.ready||signature_goal||event_match.running||event_segment.collecting||event_segment.ready||(running&&(!ambient.ready||(ambient.foreground&&spectrum_background_db(spectrum_signature_total(foreground_power))>=prefs.threshold_db*100)||input_waiting)))return;
 unsigned before=event_neural.state;sn_tick(&event_neural,&event_library);
 if(event_neural.state!=before){dirty=true;if(event_neural.state==SN_ACTIVE)event_neural_checkpoint=true;}
 if(event_neural_checkpoint)event_neural_save();
}
static const char *event_error(int32_t code){switch(code){case RISC_APP_DATA_OK:return "SAVED";case RISC_APP_DATA_UNAVAILABLE:return "APP DATA UNAVAILABLE";case RISC_APP_DATA_NO_SPACE:return "STORAGE FULL / KEEP DRAFT";case RISC_APP_DATA_COMMIT_UNKNOWN:return "SAVE UNKNOWN / RETRY";case RISC_APP_DATA_STALE:return "STORAGE CHANGED / RETRY";case ST_FILES_CONFLICT:return "COLLECTION CHANGED / DISCARD?";case ST_FILES_CORRUPT:return "COLLECTION UNREAD / RETRY";case RISC_APP_DATA_RETAINED:return "STORAGE CLEANUP UNCONFIRMED";default:return "STORAGE ERROR / RETRY";}}
static bool event_acquire(void){
 if(event_files.api)return true;
 /* Preserve eager admission: an unavailable/invalid store must not appear
  * readable to the fingerprint loader. Release the probe before publishing
  * this stable app-owned table; every later callback borrows its own lease. */
 const risc_app_data_v1 *api=rf_data_bind(&event_data_scope,runtime,2u),*source=NULL;
 int32_t result=rf_data_begin(&event_data_scope,&source);
 if(!result)result=rf_data_finish(&event_data_scope,RISC_APP_DATA_OK);
 if(result)return false;
 event_files.api=api;return true;
}
static void event_load(void){
 if(event_files.retained)return;
 if(!event_acquire()){event_message="APP DATA UNAVAILABLE";return;}
 for(unsigned bank=0;bank<2;bank++)if(!(event_files.ready&(1u<<bank))&&event_files.pending!=(int)bank){int32_t result=st_files_load(&event_files,&event_library,bank,false);if(result)event_message=event_error(result);if(event_files.retained)return;}
 if(event_files.ready==3&&event_files.pending<0)event_message=NULL;
 event_neural_refresh();
}
static void event_restore(void){
 spectrum_speech_reset(&speech);
 memset(&event_library,0,sizeof(event_library));st_files_init(&event_files,NULL);st_segment_reset(&event_segment);st_segment_reset(&event_training);memset(&event_match,0,sizeof(event_match));event_match.selected=-1;
 event_data_scope=(rf_app_data_scope){0};event_initialized=true;event_armed=event_wait_quiet=event_known_hold=event_previous_active=false;event_pair_count=event_quiet=event_wait_count=event_missed=event_list_scroll=event_example_scroll=0;event_pair_active=false;event_edit_slot=event_delete_slot=event_delete_example=-1;event_capture_kind=ST_POSITIVE;event_edit_shift=1;event_armed_at=event_result_at=0;event_message=NULL;event_keyboard=false;memset(event_pair_power,0,sizeof(event_pair_power));memset(event_previous_power,0,sizeof(event_previous_power));event_load();
}
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#include "PortableNativeCustody.h"
#endif
static bool temporal_retained(void){
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(portable_adapter_retained())return true;
#endif
 return event_initialized&&(event_files.retained||event_data_scope.retained);
}
static bool temporal_exit_blocked(void){return event_initialized&&(event_files.pending>=0||event_armed||event_training.ready);}
static void temporal_suspend(void){
 if(!event_initialized)return;
 if(event_armed){st_segment_stop(&event_training);event_armed=false;event_message=event_training.ready?"INTERRUPTED WINDOW / REVIEW":"STOPPED / NO SOUND SAVED";}
 event_match.running=false;st_segment_rearm(&event_segment);event_pair_count=0;event_pair_active=false;memset(event_pair_power,0,sizeof(event_pair_power));
}
static void temporal_reset_context(void){
 spectrum_speech_reset(&speech);
 event_known_hold=false;event_previous_active=false;event_pair_count=0;event_pair_active=false;event_quiet=0;memset(event_pair_power,0,sizeof(event_pair_power));memset(event_previous_power,0,sizeof(event_previous_power));st_segment_reset(&event_segment);event_match.running=false;
}
static void temporal_start(void){spectrum_background_reset(&ambient);temporal_reset_context();}
static void event_retry(void){
 pause_capture();if(temporal_retained())return;
 if(event_files.pending>=0){int32_t result=st_files_save(&event_files);event_message=event_error(result);if(!temporal_retained())event_neural_refresh();if(!temporal_retained())portable_audio_capture_resume();if(!temporal_retained())notify(result?event_message:running?"SAVED / LIVE":"SAVED / MIC OFF");}
 else{event_load();if(!temporal_retained())portable_audio_capture_resume();if(!temporal_retained())notify(event_message?event_message:"EVENT COLLECTIONS LOADED");}
 if(!temporal_retained()&&event_files.pending<0)cfa_retry();
 dirty=true;
}
static void event_discard(void){
 if(event_files.pending<0&&!cfa_unsaved())return;
 pause_capture();if(temporal_retained())return;
 int32_t result=0;
 if(event_files.pending>=0){unsigned bank=(unsigned)event_files.pending;result=st_files_load(&event_files,&event_library,bank,true);event_message=event_error(result);if(!temporal_retained())event_neural_refresh();}
 if(!temporal_retained()&&!result)cfa_discard();
 if(!temporal_retained())portable_audio_capture_resume();
 if(!temporal_retained())notify(result?event_message:"UNSAVED EDIT DISCARDED");
 event_delete_slot=event_delete_example=-1;dirty=true;
}
static bool event_begin_edit(unsigned slot){
 if(slot>=8||event_files.pending>=0||event_armed||event_training.ready||signature_goal){notify("SAVE OR DISCARD CURRENT EDIT");return false;}
 pause_capture();int32_t result=st_files_begin(&event_files,&event_library,slot/4);if(result){if(!temporal_retained())portable_audio_capture_resume();notify(event_error(result));return false;}memset(&event_neural,0,sizeof(event_neural));event_neural_checkpoint=event_neural_used=false;event_match.running=false;event_match.selected=-1;return true;
}
static void event_finish_edit(void){
 if(!st_files_freeze(&event_files,&event_library)){event_neural_refresh();if(!temporal_retained()){portable_audio_capture_resume();notify("INVALID EVENT EDIT");}return;}int32_t result=st_files_save(&event_files);event_message=event_error(result);if(!temporal_retained())event_neural_refresh();if(!temporal_retained())portable_audio_capture_resume();if(!temporal_retained()){notify(result?event_message:running?"SAVED / LIVE":"SAVED / MIC OFF");if(event_files.pending>=0)set_page(PAGE_EVENTS);}dirty=true;
}
static void event_open(int slot){
 if(temporal_exit_blocked()){notify("SAVE OR DISCARD CURRENT EDIT");return;}
 bool naming=slot<0;
 if(slot<0){for(unsigned i=0;i<8;i++)if((event_files.ready&(1u<<(i/4)))&&!event_library.labels[i].present){slot=(int)i;break;}if(slot<0){notify(event_files.ready!=3?"RETRY EVENT STORAGE":"8 EVENT LABELS FULL");return;}}
 event_edit_slot=slot;editing=(spectrum_label){0};const st_label*l=&event_library.labels[slot];if(l->present)memcpy(editing.name,l->name,17);event_edit_shift=l->present?l->shift_limit:1;event_delete_slot=event_delete_example=-1;event_example_scroll=0;event_keyboard=true;toast_message=NULL;set_page(PAGE_EVENT_LABEL);
 if(naming)keyboard_begin();
}
static void event_save_name(void){
 if(event_edit_slot<0)return;
 unsigned start=0,end=(unsigned)strlen(editing.name);while(start<end&&editing.name[start]==' ')start++;while(end>start&&editing.name[end-1]==' ')end--;for(unsigned i=0;i<end-start;i++)editing.name[i]=editing.name[start+i];editing.name[end-start]=0;
 if(!spectrum_signature_name_valid(editing.name)){notify("ENTER AN EVENT NAME");return;}
 if(!event_begin_edit((unsigned)event_edit_slot))return;
 st_label*l=&event_library.labels[event_edit_slot];l->present=true;l->shift_limit=(uint8_t)event_edit_shift;if(!l->next_id)l->next_id=1;cfa_rename_event((unsigned)event_edit_slot,editing.name);memcpy(l->name,editing.name,17);event_finish_edit();if(!temporal_retained()&&event_files.pending<0)cfa_save();
}
static void event_arm(unsigned kind){
 if(event_edit_slot<0||kind<ST_POSITIVE||kind>ST_NEGATIVE)return;
 if(event_files.pending>=0||event_training.ready||signature_goal){notify("SAVE OR DISCARD CURRENT EDIT");return;}
 if(event_armed){set_page(PAGE_EVENT_CAPTURE);return;}
 if(!event_library.labels[event_edit_slot].present||event_edit_shift!=event_library.labels[event_edit_slot].shift_limit||strcmp(editing.name,event_library.labels[event_edit_slot].name)){
  event_save_name();if(temporal_retained())return;
  if(event_files.pending>=0){toast_message=NULL;set_page(PAGE_EVENTS);return;}
  if(!event_library.labels[event_edit_slot].present||event_edit_shift!=event_library.labels[event_edit_slot].shift_limit||strcmp(editing.name,event_library.labels[event_edit_slot].name))return;
 }
 st_label*l=&event_library.labels[event_edit_slot];if(st_example_count(l,0)>=6||l->next_id==UINT32_MAX){notify("6 EXAMPLES FULL / DELETE ONE");return;}
 if(!running){toggle();}if(!running){notify("MIC FAILED / TAP RECORD AGAIN");return;}
 st_segment_reset(&event_training);event_capture_kind=kind;cfa_arm();event_armed=true;event_wait_quiet=true;event_wait_count=0;event_armed_at=app->millis();event_message="WAIT FOR QUIET / THEN ACT";event_delete_slot=event_delete_example=-1;toast_message=NULL;set_page(PAGE_EVENT_CAPTURE);
}
static void event_cancel_capture(void){event_armed=false;st_segment_reset(&event_training);event_message="CAPTURE CANCELLED";set_page(PAGE_EVENT_LABEL);}
static void event_save_capture(void){
 if(!event_training.ready||event_edit_slot<0)return;
 cfa_event_boundary(true,event_training.collecting,event_training.ready);
 if(!cfa_save_event((unsigned)event_edit_slot,event_library.labels[event_edit_slot].name,event_capture_kind==ST_POSITIVE)){notify("TEMPORAL SAMPLE NOT READY / RECORD AGAIN");return;}
 st_label*l=&event_library.labels[event_edit_slot];unsigned index=0;for(;index<6&&l->examples[index].id;index++);if(index==6||l->next_id==UINT32_MAX){notify("6 EXAMPLES FULL / KEEP WINDOW");return;}
 pause_capture();int32_t result=st_files_begin(&event_files,&event_library,(unsigned)event_edit_slot/4);if(result){if(!temporal_retained())portable_audio_capture_resume();notify(event_error(result));return;}
 st_example example=event_training.event;example.kind=(uint8_t)event_capture_kind;example.id=l->next_id++;l->examples[index]=example;st_segment_reset(&event_training);event_armed=false;event_finish_edit();if(!temporal_retained()&&event_files.pending<0)cfa_save();if(!temporal_retained())set_page(event_files.pending>=0?PAGE_EVENTS:PAGE_EVENT_LABEL);
}
static void event_delete_label(unsigned slot){
 if(event_delete_slot!=(int)slot||event_delete_example!=-2){event_delete_slot=(int)slot;event_delete_example=-2;toast_message=NULL;dirty=true;return;}
 if(!event_begin_edit(slot))return;
 cfa_forget(8+slot);cfa_forget(16+slot);memset(&event_library.labels[slot],0,sizeof(st_label));event_finish_edit();if(!temporal_retained()&&event_files.pending<0)cfa_save();event_delete_slot=event_delete_example=-1;
}
static void event_delete_saved(unsigned index){
 if(event_edit_slot<0||index>=6||!event_library.labels[event_edit_slot].examples[index].id)return;
 if(event_delete_slot!=event_edit_slot||event_delete_example!=(int)index){event_delete_slot=event_edit_slot;event_delete_example=(int)index;toast_message=NULL;dirty=true;return;}
 if(!event_begin_edit((unsigned)event_edit_slot))return;
 cfa_forget((event_library.labels[event_edit_slot].examples[index].kind==1?8u:16u)+(unsigned)event_edit_slot);
 memset(&event_library.labels[event_edit_slot].examples[index],0,sizeof(st_example));event_finish_edit();if(!temporal_retained()&&event_files.pending<0)cfa_save();event_delete_slot=event_delete_example=-1;
}
static void temporal_observe(void){
 ambient.freeze_upward=event_armed||event_training.ready||event_match.running||event_known_hold;
 spectrum_background_observe(&ambient,signature_audio.power);
 uint64_t active_power=spectrum_background_salient(&ambient,foreground_power);
 bool voice_before=speech.active;unsigned voice_confidence=speech.confidence;
 spectrum_speech_observe(&speech,signature_audio.power,foreground_power,ambient.ready,prefs.threshold_db);
 if(speech.active!=voice_before||speech.confidence!=voice_confidence)dirty=true;
 detect_frequency_labels();
 for(unsigned i=0;i<128;i++)event_pair_power[i]+=foreground_power[i];
 event_pair_active|=ambient.foreground&&spectrum_background_db(active_power)>=prefs.threshold_db*100;if(++event_pair_count<2)return;
 uint32_t power[128];for(unsigned i=0;i<128;i++){power[i]=(uint32_t)(event_pair_power[i]/2u);event_pair_power[i]=0;}event_pair_count=0;
 st_frame frame=st_frame_make(power,event_previous_power,event_pair_active);memcpy(event_previous_power,power,sizeof(power));event_pair_active=false;
 if(!ambient.ready){st_segment_reset(&event_segment);if(event_armed){event_armed_at=app->millis();dirty=true;}return;}
 if(!(frame.flags&ST_ACTIVE)){if(event_quiet<4)event_quiet++;if(event_quiet>=4)event_known_hold=false;}else event_quiet=0;
 if(event_segment.ready&&event_match.running&&(frame.flags&ST_ACTIVE)&&!event_previous_active)event_missed++;
 event_previous_active=!!(frame.flags&ST_ACTIVE);
 if(!event_segment.ready)st_segment_observe(&event_segment,&frame);
 if(event_armed){
  if(event_wait_quiet){event_training.pre[event_training.next]=frame;event_training.next=(event_training.next+1u)%ST_PRE;if(event_training.used<ST_PRE)event_training.used++;if(frame.flags&ST_ACTIVE)event_wait_count=0;else if(++event_wait_count>=ST_PRE){event_wait_quiet=false;event_armed_at=app->millis();event_message="MAKE THE SOUND NOW";}}
  else{st_segment_observe(&event_training,&frame);if(event_training.ready){event_armed=false;event_message=event_training.event.flags&ST_CLIPPED?"WINDOW LIMIT / REVIEW":"EXAMPLE READY / REVIEW";dirty=true;}}
  if(event_armed&&!event_training.collecting&&(uint32_t)(app->millis()-event_armed_at)>=8000u){event_armed=false;event_message="NO NEW SOUND / RECORD AGAIN";set_page(PAGE_EVENT_LABEL);notify(event_message);}
 }
 if(event_armed||event_training.ready)cfa_event_boundary(true,event_training.collecting,event_training.ready);
 else cfa_event_boundary(false,event_segment.collecting,event_segment.ready);
 if(event_segment.ready&&!event_match.running){st_match_begin(&event_match,&event_segment.event);event_match.allowed_labels=(uint8_t)((event_files.ready&1?15u:0u)|(event_files.ready&2?240u:0u));st_segment_rearm(&event_segment);}
 if(page==PAGE_EVENTS||page==PAGE_EVENT_CAPTURE)dirty=true;
}
static void temporal_tick(void){
 if(event_match.running){st_match_tick(&event_match,&event_library,64);if(event_match.complete){event_neural_used=sn_apply(&event_neural,&event_match);event_result_at=app->millis();event_known_hold=event_match.reason==ST_RESULT_MATCH;dirty=true;}}
 if(event_match.complete&&(uint32_t)(app->millis()-event_result_at)>4000u){event_match.complete=false;dirty=true;}
}
static bool temporal_exit_ready(void){
 cfa_save();if(!temporal_exit_blocked()&&!cfa_unsaved())return true;
 event_message=(event_files.pending>=0||cfa_unsaved())?"RETRY SAVE OR DISCARD":"SAVE OR CANCEL THE WINDOW";notify(event_message);set_page(event_training.ready||event_armed?PAGE_EVENT_CAPTURE:PAGE_EVENTS);return false;
}
