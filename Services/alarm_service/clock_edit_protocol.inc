/* Explicit serialized editor authority. All API2 holders can see the tail;
 * selected Settings is the only product client that dispatches these methods. */
static int32_t edit_return(int32_t r){if(custody_retained)r=ALARM_RETAINED;in_call=false;update_view();return r;}
static bool edit_live_permit(const alarm_clock_edit_permit_v2*p){return edit_permit_valid&&p&&p->struct_size==sizeof(*p)&&!memcmp(p,&edit_permit,sizeof(*p));}
static bool edit_unchanged_time(uint32_t current,uint32_t old,uint64_t started_ms,uint64_t at){
 if(at<started_ms||at-started_ms>5000)return false;
 uint32_t expected=old+(uint32_t)(at-started_ms)/1000;
 return current>=old&&current+1>=expected&&current<=expected+1;
}
static int32_t edit_begin(void*c,uint32_t requested,alarm_clock_edit_permit_v2*out){
 (void)c;if(custody_retained)return ALARM_RETAINED;
 if(!started||!out||out->struct_size!=sizeof(*out)||requested>ALARM_RTC_MAX)return ALARM_INVALID;
 if(in_call||edit_permit_valid)return ALARM_BUSY;
 sleep_ticket_valid=sleep_waiting=false;
 /* An explicit editor operation, unlike ordinary refresh/step, may repair a
  * typed RTC-only block. No other service error or output custody is cleared. */
 bool recovery=error==ALARM_RTC&&phase==BLOCKED;
 if(active||dismiss||persistence_pending||foreground_failed||haptic_uncertain||audio_uncertain)return ALARM_BUSY;
 if(error&&!recovery)return error;
 if(phase!=IDLE&&!recovery)return ALARM_PENDING;
 in_call=true;
 uint32_t current;int32_t result=read_utc(&current);
 if(custody_retained)return edit_return(ALARM_RETAINED);
 if(result!=ALARM_OK){if(!recovery)return edit_return(result);current=edit_loaded?edit_word(edit_record,20):previous_seconds;}
 uint64_t at=now_ms();if(custody_retained)return edit_return(ALARM_RETAINED);
 if(!recovery){
  if(!anchored||at<previous_ms||at-previous_ms>UINT32_MAX||current<previous_seconds)return edit_return(fail(ALARM_RTC));
  /* The checked bound is part of the time-domain contract. Divide the bounded
   *32-bit duration, not a64-bit value that requires an unavailable target helper. */
  uint32_t elapsed=(uint32_t)(at-previous_ms)/1000,delta=current-previous_seconds;
  if((uint64_t)delta>elapsed+UINT64_C(2)||(uint64_t)elapsed>delta+UINT64_C(2))return edit_return(fail(ALARM_RTC));
 }
 /* Read exact current identities before authorizing them. No application KV
  * privilege is borrowed and no configuration or occurrence is rewritten. */
 for(unsigned i=0;i<2;i++){
  if(!read_config(i)||!read_occurrence(i))return edit_return(fail(ALARM_STORAGE));
  if(custody_retained)return edit_return(ALARM_RETAINED);
  uint8_t a[32],b[32];edit_config_bytes(&config[i],a);edit_config_bytes(&staging[i],b);
  if(memcmp(a,b,32)){
   if(!recovery){begin_reconcile();return edit_return(ALARM_STALE);}
   if(staging[i].revision<=config[i].revision)return edit_return(fail(ALARM_STORAGE));
  }
  edit_occ_bytes(&occurrences[i],a);edit_occ_bytes(&staged_occ[i],b);
  if(memcmp(a,b,32)){
   if(!recovery){begin_reconcile();return edit_return(ALARM_STALE);}
   if(staged_occ[i].generation<=occurrences[i].generation)return edit_return(fail(ALARM_STORAGE));
  }
 }
 if(recovery){memcpy(config,staging,sizeof(config));memcpy(occurrences,staged_occ,sizeof(occurrences));}
 result=edit_load();if(result!=ALARM_OK)return edit_return(fail(result));
 if(!edit_history_valid(config,occurrences))return edit_return(fail(ALARM_STORAGE));
 uint32_t generation=edit_loaded?edit_word(edit_record,12):0;if(generation==UINT32_MAX)return edit_return(ALARM_EXHAUSTED);
 uint8_t next[EDIT_BYTES]={0};memcpy(next,"ACE2",4);alarm_write32(next+4,1);alarm_write32(next+8,ALARM_CLOCK_EDIT_INTENT);alarm_write32(next+12,generation+1);
 alarm_write32(next+16,requested);alarm_write32(next+20,current);
 alarm_write32(next+24,edit_loaded?edit_word(edit_record,8):ALARM_CLOCK_EDIT_NONE);
 alarm_write32(next+28,edit_loaded?edit_word(edit_record,16):0);alarm_write32(next+32,edit_loaded?edit_word(edit_record,20):0);
 for(unsigned i=0;i<2;i++)if(config[i].revision){
  uint8_t*n=edit_row(next,0,i),*old=edit_row(next,1,i);
  edit_config_bytes(&config[i],n);edit_occ_bytes(&occurrences[i],n+32);alarm_write32(n+64,requested<config[i].created?requested:config[i].created);
  /* Preserve the effective prior authorization without granting one to a new
   * config identity. Normal created-time lower bound is safe as prior state. */
  bool prior=false;
  if(edit_loaded&&edit_word(edit_record,8)!=ALARM_CLOCK_EDIT_NONE){unsigned groups=edit_word(edit_record,8)==ALARM_CLOCK_EDIT_INTENT?2:1;
   for(unsigned g=0;g<groups;g++)if(edit_row_matches(edit_crow(edit_record,g,i),&config[i]))prior=true;}
  if(prior){memcpy(old,n,64);alarm_write32(old+64,edit_floor(i,&config[i]));}
 }
 uint8_t prior[EDIT_BYTES]={0};if(edit_loaded)memcpy(prior,edit_record,EDIT_BYTES);else{memcpy(prior,"ACE2",4);alarm_write32(prior+4,1);}
 alarm_write32(prior+12,generation+1);edit_seal(prior);
 edit_seal(next);result=edit_store(next,true);if(result!=ALARM_OK)return edit_return(result);
 memcpy(edit_prior_record,prior,EDIT_BYTES);
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
 uint8_t next[EDIT_BYTES];memcpy(next,edit_record,EDIT_BYTES);alarm_write32(next+8,ALARM_CLOCK_EDIT_VERIFIED);
 /* Requested whole-second seed is independently verified above; its exact
  * requested floor, not a cumulative minimum across edits, is persisted. */
 memset(next+24,0,16);memset(edit_row(next,1,0),0,2*EDIT_ROW_BYTES);edit_seal(next);
 r=edit_store(next,false);if(r!=ALARM_OK)return edit_return(fail(r));
 edit_recovery=false;seconds=previous_seconds=current;sample_ms=previous_ms=at;anchored=true;begin_reconcile();return edit_return(ALARM_PENDING);
}
static int32_t edit_cancel(void*c,const alarm_clock_edit_permit_v2*p){
 (void)c;if(custody_retained)return ALARM_RETAINED;if(!started)return ALARM_INVALID;if(in_call)return ALARM_BUSY;
 if(!edit_live_permit(p))return ALARM_STALE;
 in_call=true;edit_permit_valid=false;sleep_ticket_valid=false;
 uint32_t current;int32_t r=read_utc(&current);if(custody_retained)return edit_return(ALARM_RETAINED);
 uint64_t at=now_ms();if(custody_retained)return edit_return(ALARM_RETAINED);
 if(r!=ALARM_OK||!edit_unchanged_time(current,p->previous,edit_started_ms,at)){edit_recovery=true;return edit_return(fail(ALARM_RTC));}
 /* The live one-use permit owns the exact prior record snapshot. A reset
  * destroys this snapshot and permit, so interrupted intent requires new Save. */
 uint8_t next[EDIT_BYTES];memcpy(next,edit_prior_record,EDIT_BYTES);
 r=edit_store(next,false);if(r!=ALARM_OK)return edit_return(fail(r));
 edit_recovery=edit_word(next,8)==ALARM_CLOCK_EDIT_INTENT;seconds=previous_seconds=current;sample_ms=previous_ms=at;anchored=true;begin_reconcile();return edit_return(ALARM_PENDING);
}
static int32_t edit_status(void*c,alarm_clock_edit_status_v2*out){
 (void)c;if(custody_retained)return ALARM_RETAINED;if(!started||!out||out->struct_size!=sizeof(*out))return ALARM_INVALID;if(in_call)return ALARM_BUSY;
 *out=(alarm_clock_edit_status_v2){sizeof(*out),edit_loaded?edit_word(edit_record,8):ALARM_CLOCK_EDIT_NONE,edit_loaded?edit_word(edit_record,12):0,edit_loaded?edit_word(edit_record,16):0,edit_loaded?edit_word(edit_record,20):0,{edit_floor(0,&config[0]),edit_floor(1,&config[1])}};return ALARM_OK;
}
