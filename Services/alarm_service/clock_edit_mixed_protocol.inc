/* Serialized mixed editor: exact participant snapshots before seed and at the
 * finish/cancel boundary. Any post-seed identity uncertainty retains custody. */
static uint8_t edit_next[EDIT_BYTES],edit_payload[6][128];
static int32_t edit_return(int32_t r){if(custody_retained)r=ALARM_RETAINED;in_call=false;update_view();return r;}
/* A clean BUSY read did not observe new state. Other failed reads invalidate
 * the settled snapshot; RTC-only recovery still owns explicit repair. */
static int32_t edit_read_failure(int32_t r){return edit_return(r==ALARM_BUSY?r:fail(r));}
static bool edit_live_permit(const alarm_clock_edit_permit_v2*p){return edit_permit_valid&&p&&p->struct_size==sizeof(*p)&&!memcmp(p,&edit_permit,sizeof(*p));}
static bool edit_unchanged_time(uint32_t current,uint32_t old,uint64_t started_ms,uint64_t at){
 if(at<started_ms||at-started_ms>5000)return false;
 uint32_t expected=old+(uint32_t)(at-started_ms)/1000;
 return current>=old&&current+1>=expected&&current<=expected+1;
}
static bool edit_exact_alarm(unsigned i){uint8_t a[64],b[64];edit_config_bytes(&config[i],a);edit_occ_bytes(&occurrences[i],a+32);edit_config_bytes(&staging[i],b);edit_occ_bytes(&staged_occ[i],b+32);return !memcmp(a,b,64);}
static int32_t edit_read_participants(void){
 for(unsigned i=0;i<2;i++)if(!read_config(i)||!read_occurrence(i))return edit_read_error();
 if(!read_points_config()||!read_points_occ())return edit_read_error();
 return ALARM_OK;
}
static bool edit_points_exact(void){uint8_t a[128],b[128];edit_points_bytes(&points_configured,&points_occ,a,a+64);edit_points_bytes(&points_staging,&points_staged_occ,b,b+64);return points_staging_saved==points_configured_saved&&!memcmp(a,b,128);}
static void edit_make_rows(clock_participant_record*r,unsigned group,uint32_t requested,bool prior){
 for(unsigned i=0;i<3;i++){
  unsigned n=group*3+i;uint8_t*b=edit_payload[n];uint32_t floor,provenance,zone=0;
  if(i<2){edit_config_bytes(&config[i],b);edit_occ_bytes(&occurrences[i],b+32);floor=prior?edit_floor(i,&config[i]):(requested<config[i].created?requested:config[i].created);provenance=config[i].revision?CLOCK_CONFIG_SAVED:CLOCK_CONFIG_ABSENT;}
  else{edit_points_bytes(&points_configured,&points_occ,b,b+64);floor=prior?edit_points_floor(&points_configured):(requested<points_configured.created?requested:points_configured.created);provenance=points_configured_saved?CLOCK_CONFIG_SAVED:CLOCK_CONFIG_VIRTUAL;zone=timezone_index;}
  r->rows[r->count++]=(clock_participant_row){group,i+1,1,floor,zone,provenance,b,b+(i<2?32:64)};
 }
}
static int32_t edit_after_snapshot(void){
 /* Verify our durable INTENT still exists before reading selected identities. */
 uint32_t size=0;int32_t r=storage_get(kv->context,ALARM_CLOCK_EDIT_KEY,edit_next,sizeof(edit_next),&size);
 if(r!=RISC_BOUND_KEY_VALUE_OK||size!=edit_size||memcmp(edit_next,edit_record,size))return ALARM_RETAINED;
 r=edit_read_participants();if(r!=ALARM_OK)return ALARM_RETAINED;
 unsigned zone;portable_timezone_rule rule;r=read_zone(&zone,&rule);
 if(r!=ALARM_OK||zone!=timezone_index||!edit_exact_alarm(0)||!edit_exact_alarm(1)||!edit_points_exact())return ALARM_RETAINED;
 return ALARM_OK;
}
static int32_t edit_begin(void*c,uint32_t requested,alarm_clock_edit_permit_v2*out){
 (void)c;if(custody_retained)return ALARM_RETAINED;
 if(!started||!out||out->struct_size!=sizeof(*out)||requested>ALARM_RTC_MAX)return ALARM_INVALID;
 if(in_call||edit_permit_valid)return ALARM_BUSY;
 sleep_ticket_valid=sleep_waiting=false;
 bool recovery=error==ALARM_RTC&&phase==BLOCKED;
 if(active||dismiss||persistence_pending||foreground_failed||haptic_uncertain||audio_uncertain)return ALARM_BUSY;
 if(error&&!recovery)return error;
 if(phase!=IDLE&&!recovery)return ALARM_PENDING;
 in_call=true;uint32_t current;int32_t result=read_utc(&current);
 if(custody_retained)return edit_return(ALARM_RETAINED);
 if(result!=ALARM_OK){if(!recovery)return edit_read_failure(result);current=edit_loaded?edit_view.previous:previous_seconds;}
 uint64_t at=now_ms();if(custody_retained)return edit_return(ALARM_RETAINED);
 if(!recovery){
  if(!anchored||at<previous_ms||at-previous_ms>UINT32_MAX||current<previous_seconds)return edit_return(fail(ALARM_RTC));
  uint32_t elapsed=(uint32_t)(at-previous_ms)/1000,delta=current-previous_seconds;
  if((uint64_t)delta>elapsed+UINT64_C(2)||(uint64_t)elapsed>delta+UINT64_C(2))return edit_return(fail(ALARM_RTC));
 }
 result=edit_read_participants();if(result!=ALARM_OK)return edit_read_failure(result);
 unsigned zone;portable_timezone_rule rule;result=read_zone(&zone,&rule);
 if(result!=ALARM_OK)return edit_read_failure(custody_retained?ALARM_RETAINED:mixed_storage_result==RISC_BOUND_KEY_VALUE_BUSY?ALARM_BUSY:result);
 bool same=zone==timezone_index&&edit_exact_alarm(0)&&edit_exact_alarm(1)&&edit_points_exact();
 if(!same&&!recovery){begin_reconcile();return edit_return(ALARM_STALE);}
 if(recovery){
  for(unsigned i=0;i<2;i++){
   uint8_t a[32],b[32];edit_config_bytes(&config[i],a);edit_config_bytes(&staging[i],b);
   if(staging[i].revision<config[i].revision||(staging[i].revision==config[i].revision&&memcmp(a,b,32)))return edit_return(fail(ALARM_STORAGE));
   edit_occ_bytes(&occurrences[i],a);
   if(!edit_alarm_history(a,&staged_occ[i]))return edit_return(fail(ALARM_STORAGE));
  }
  uint8_t a[64],b[64];points_config_encode(&points_configured,a);points_config_encode(&points_staging,b);
  if(points_staging.revision<points_configured.revision||(points_staging.revision==points_configured.revision&&(memcmp(a,b,64)||points_staging_saved!=points_configured_saved))||points_staged_occ.generation<points_occ.generation)return edit_return(fail(ALARM_STORAGE));
  if(points_occ.generation&&points_staged_occ.generation&&!clock_points_history_successor(&points_occ,&points_staged_occ))return edit_return(fail(ALARM_STORAGE));
 }
 result=edit_load();if(result!=ALARM_OK)return edit_read_failure(result);
 if(!edit_history_valid(staging,staged_occ)||!edit_points_history_valid(&points_staging,&points_staged_occ,points_staging_saved))return edit_return(fail(ALARM_STORAGE));
 if(recovery){memcpy(config,staging,sizeof(config));memcpy(occurrences,staged_occ,sizeof(occurrences));points_configured=points_staging;points_occ=points_staged_occ;points_configured_saved=points_staging_saved;timezone_index=zone;timezone_rule=rule;}
 /* RTC-only recovery may precede ordinary reconciliation. Validate a recorded
  * occurrence against the OLD authorization before snapshotting it into a new
  * row, so this edit cannot manufacture its own historical proof. */
 if(points_occ.revision==points_configured.revision&&points_occ.state){
  points_event event={0};uint8_t mode=points_configured.points[points_occ.slot].mode;
  if((mode&&points_occ.mode!=mode)||!recorded_point(&points_configured,&points_occ,&event)||event.deadline!=points_occ.deadline)return edit_return(fail(ALARM_STORAGE));
 }
 uint32_t generation=edit_loaded?edit_view.generation:0;if(generation==UINT32_MAX)return edit_return(ALARM_EXHAUSTED);
 /* Preserve exact prior bytes and identities for cancellation, incrementing
  * only authorization generation and integrity as the original contract does. */
 if(edit_loaded){memcpy(edit_prior_record,edit_record,edit_size);edit_prior_size=edit_size;
  unsigned offset=!memcmp(edit_prior_record,"ACE2",4)?12:16;alarm_write32(edit_prior_record+offset,generation+1);
  alarm_write32(edit_prior_record+edit_prior_size-4,clock_participant_checksum(edit_prior_record,edit_prior_size-4));
 }else{clock_participant_record empty={.generation=generation+1};if(!clock_participant_encode_scratch(edit_prior_record,sizeof(edit_prior_record),&edit_prior_size,&empty,edit_types,3,ALARM_RTC_MAX,edit_encode_scratch,sizeof(edit_encode_scratch)))return edit_return(ALARM_STORAGE);}
 clock_participant_record next={.phase=ALARM_CLOCK_EDIT_INTENT,.generation=generation+1,.requested=requested,.previous=current,
  .prior_phase=edit_loaded?edit_view.phase:0,.prior_requested=edit_loaded?edit_view.requested:0,.prior_previous=edit_loaded?edit_view.previous:0};
 edit_make_rows(&next,0,requested,false);if(next.prior_phase)edit_make_rows(&next,1,next.prior_requested,true);
 size_t next_size;if(!clock_participant_encode_scratch(edit_next,sizeof(edit_next),&next_size,&next,edit_types,3,ALARM_RTC_MAX,edit_encode_scratch,sizeof(edit_encode_scratch)))return edit_return(ALARM_STORAGE);
 result=edit_store(edit_next,next_size,true);if(result!=ALARM_OK)return edit_return(result);
 edit_permit=(alarm_clock_edit_permit_v2){sizeof(edit_permit),generation+1,generation+1,requested,current,view.snapshot,{0,0}};
 edit_started_ms=at;edit_permit_valid=true;edit_recovery=false;*out=edit_permit;return edit_return(ALARM_OK);
}
static int32_t edit_finish(void*c,const alarm_clock_edit_permit_v2*p){
 (void)c;if(custody_retained)return ALARM_RETAINED;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
 if(!edit_live_permit(p))return ALARM_STALE;
 in_call=true;edit_permit_valid=false;sleep_ticket_valid=false;
 uint32_t current;int32_t r=read_utc(&current);if(custody_retained)return edit_return(ALARM_RETAINED);
 uint64_t at=now_ms();if(custody_retained)return edit_return(ALARM_RETAINED);
 if(r!=ALARM_OK||at<edit_started_ms||at-edit_started_ms>5000||current<p->requested||current-p->requested>2){edit_recovery=true;return edit_return(fail(ALARM_RTC));}
 if(edit_after_snapshot()!=ALARM_OK){custody_retained=true;edit_recovery=true;return edit_return(fail(ALARM_RETAINED));}
 clock_participant_record next=edit_view;next.phase=ALARM_CLOCK_EDIT_VERIFIED;next.prior_phase=next.prior_requested=next.prior_previous=0;next.count=3;
 size_t size;if(!clock_participant_encode_scratch(edit_next,sizeof(edit_next),&size,&next,edit_types,3,ALARM_RTC_MAX,edit_encode_scratch,sizeof(edit_encode_scratch))){custody_retained=true;return edit_return(ALARM_RETAINED);}
 r=edit_store(edit_next,size,false);if(r!=ALARM_OK)return edit_return(fail(r));
 edit_recovery=false;seconds=previous_seconds=current;sample_ms=previous_ms=at;anchored=true;begin_reconcile();return edit_return(ALARM_PENDING);
}
static int32_t edit_cancel(void*c,const alarm_clock_edit_permit_v2*p){
 (void)c;if(custody_retained)return ALARM_RETAINED;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
 if(!edit_live_permit(p))return ALARM_STALE;
 in_call=true;edit_permit_valid=false;sleep_ticket_valid=false;
 uint32_t current;int32_t r=read_utc(&current);if(custody_retained)return edit_return(ALARM_RETAINED);
 uint64_t at=now_ms();if(custody_retained)return edit_return(ALARM_RETAINED);
 if(r!=ALARM_OK||!edit_unchanged_time(current,p->previous,edit_started_ms,at)){edit_recovery=true;return edit_return(fail(ALARM_RTC));}
 if(edit_after_snapshot()!=ALARM_OK){custody_retained=true;edit_recovery=true;return edit_return(ALARM_RETAINED);}
 r=edit_store(edit_prior_record,edit_prior_size,false);if(r!=ALARM_OK)return edit_return(fail(r));
 edit_recovery=edit_view.phase==ALARM_CLOCK_EDIT_INTENT;seconds=previous_seconds=current;sample_ms=previous_ms=at;anchored=true;begin_reconcile();return edit_return(ALARM_PENDING);
}
static int32_t edit_status(void*c,alarm_clock_edit_status_v2*out){
 (void)c;if(custody_retained)return ALARM_RETAINED;if(!started||!out||out->struct_size!=sizeof(*out))return ALARM_INVALID;if(in_call)return ALARM_BUSY;
 *out=(alarm_clock_edit_status_v2){sizeof(*out),edit_loaded?edit_view.phase:0,edit_loaded?edit_view.generation:0,edit_loaded?edit_view.requested:0,edit_loaded?edit_view.previous:0,{edit_floor(0,&config[0]),edit_floor(1,&config[1])}};return ALARM_OK;
}
