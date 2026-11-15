#define ALARM_NATIVE_MIXED_POINTS
#define ALARM_SERVICE_CLOCK_EDIT_V2
#define POINTS_SERVICE_CONTEXT
#define main historical_mixed_fixture
#include "native_utc_alarm_service_test.c"
#undef main
static const alarm_clock_edit_descriptor_v2 *editor(void){const alarm_clock_edit_descriptor_v2 *e=alarm_clock_edit_descriptor(client);assert(e);return e;}
static void seed(uint32_t value){epoch=INT64_C(946684800)+value-(int64_t)(ms/1000);}
static void setup_mixed(void){epoch=INT64_C(946684800)+date(2026,10,5,10,0,0);boot(true);save(schedule(12,0,3));until(ALARM_STATE_READY);}
static alarm_clock_edit_permit_v2 begin_edit(uint32_t requested){alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};int r=editor()->begin(NULL,requested,&p);if(r)fprintf(stderr,"begin=%d phase=%u error=%d\n",r,phase,error);assert(r==ALARM_OK);return p;}
static void finish_edit(alarm_clock_edit_permit_v2 *p){seed(p->requested);int r=editor()->finish(NULL,p);if(r!=ALARM_PENDING)fprintf(stderr,"finish=%d phase=%u error=%d\n",r,phase,error);assert(r==ALARM_PENDING);until(ALARM_STATE_READY);}
static void restart(void){provider=NULL;started=in_call=custody_retained=false;haptic_uncertain=audio_uncertain=false;kv=NULL;clock_api=NULL;realtime=NULL;haptic=NULL;audio=NULL;boot(false);}
static void interrupted(void){
 for(unsigned after_seed=0;after_seed<2;after_seed++){
  setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);
  points_service_context context={.struct_size=sizeof(context)};char utc[40]="UTC";
  assert(points_service_read_context(client,&points_configured,utc,0,&context)==ALARM_PENDING);
  if(after_seed)seed(p.requested);
  restart();until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&edit_recovery);
  assert(points_service_read_context(client,&points_configured,utc,0,&context)==ALARM_RTC);
  assert(editor()->finish(NULL,&p)==ALARM_STALE);
  p=begin_edit(old-86400);finish_edit(&p);assert(plan().deadline==date(2026,10,4,12,0,0));
 }
}
static void legacy_import(void){
 setup_mixed();uint8_t legacy[316]={0};memcpy(legacy,"ACE2",4);alarm_write32(legacy+4,1);alarm_write32(legacy+8,2);alarm_write32(legacy+12,7);alarm_write32(legacy+16,seconds-86400);alarm_write32(legacy+20,seconds);
 alarm_write32(legacy+312,clock_participant_checksum(legacy,312));memcpy(blobs[9],legacy,316);sizes[9]=316;
 boot(false);until(ALARM_STATE_READY);assert(!put_count&&!memcmp(blobs[9],legacy,316));
 assert(edit_points_floor(&points_configured)==points_configured.created);
 alarm_clock_edit_permit_v2 p=begin_edit(seconds-86400);assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 assert(sizes[9]==316&&!memcmp(blobs[9],"ACE2",4)&&edit_points_floor(&points_configured)==points_configured.created);
 p=begin_edit(seconds-86400);finish_edit(&p);assert(!memcmp(blobs[9],"ACM2",4));
}
static void io_and_zone(void){
 setup_mixed();uint32_t old=seconds;put_result=RISC_BOUND_KEY_VALUE_IO;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);put_result=0;
 p=begin_edit(old);zone("America/Denver");seed(p.requested);assert(editor()->finish(NULL,&p)==ALARM_RETAINED);
 in_call=true;points_service_context context={.struct_size=sizeof(context)};char utc[40]="UTC";
 assert(points_service_read_context(client,&points_configured,utc,0,&context)==ALARM_RETAINED);in_call=false;
 restart();until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
 p=begin_edit(old);finish_edit(&p);
}
static void same_revision_recovery(void){
 for(unsigned family=0;family<3;family++){
  setup_mixed();
  if(family<2){alarm_config c={.revision=1,.created=seconds,.deadline=seconds+3600,.duration=family?3600:0,.kind=(uint8_t)(family+1),.enabled=1};alarm_config_encode(&c,blobs[family]);sizes[family]=32;client->refresh(NULL);until(ALARM_STATE_READY);}
  epoch-=86400;client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
  if(family<2){alarm_config c;assert(alarm_config_decode(&c,blobs[family],32,(uint8_t)(family+1)));c.deadline++;alarm_config_encode(&c,blobs[family]);}
  else{points_config c=points_configured;c.points[0].minute++;save(c);}
  unsigned writes=put_count;alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};
  assert(editor()->begin(NULL,seconds,&p)==ALARM_STORAGE);assert(put_count==writes&&!sizes[9]);
 }
 epoch=INT64_C(946684800)+date(2026,10,5,10,0,0);boot(true);until(ALARM_STATE_READY);assert(!points_configured_saved);
 epoch-=86400;client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
 save(points_configured);alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};unsigned writes=put_count;
 assert(editor()->begin(NULL,seconds,&p)==ALARM_STORAGE&&put_count==writes&&!sizes[9]);
}
static void points_write_outcomes(void){
 const int outcomes[]={RISC_BOUND_KEY_VALUE_BUSY,RISC_BOUND_KEY_VALUE_INVALID,RISC_BOUND_KEY_VALUE_IO,RISC_BOUND_KEY_VALUE_CONTEXT,-1234};
 for(unsigned n=0;n<sizeof(outcomes)/sizeof(outcomes[0]);n++)for(unsigned committed=0;committed<2;committed++){
  restart();epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);save(schedule(12,0,3));
  for(unsigned t=0;t<200&&phase!=WRITE_OCC;t++)tick();
  assert(phase==WRITE_OCC&&selected==2);
  put_result=outcomes[n];put_commits=committed&&put_result==RISC_BOUND_KEY_VALUE_IO;
  unsigned reads_before=get_count,writes_before=put_count;tick();assert(put_count==writes_before+1&&get_count==reads_before);
  if(put_result==RISC_BOUND_KEY_VALUE_CONTEXT||put_result==-1234){assert(custody_retained&&error==ALARM_RETAINED);forbid=true;unsigned calls=get_count+put_count+reads+clocks;client->step(NULL);assert(calls==get_count+put_count+reads+clocks);forbid=false;}
  else if(put_result==RISC_BOUND_KEY_VALUE_IO){assert(phase==VERIFY_OCC);tick();if(committed){assert(points_occ.generation&&!custody_retained);}else assert(error==ALARM_STORAGE&&!custody_retained);}
  else{assert(error==ALARM_STORAGE&&!custody_retained);unsigned calls=get_count+put_count;client->step(NULL);assert(calls==get_count+put_count);put_result=0;put_commits=true;client->refresh(NULL);playing();assert(points_occ.generation==1);}
 }
 restart();
}
static void backward_occurrence(void){
 setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);
 alarm_sleep_v1 ticket=plan();assert(ticket.deadline==date(2026,10,4,12,0,0));
 ms+=(uint64_t)(ticket.deadline-seconds)*1000;assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();
 assert(selected==2&&points_occ.edge==POINTS_EDGE_START&&points_occ.deadline==ticket.deadline);ack();
 uint8_t history[64];memcpy(history,blobs[6],64);boot(false);until(ALARM_STATE_READY);assert(!memcmp(history,blobs[6],64));
 p=begin_edit(date(2026,10,4,10,0,0));finish_edit(&p);assert(!memcmp(history,blobs[6],64));
 assert(plan().deadline==date(2026,10,4,12,27,0));
 /* The old start is acknowledged; setting time back does not resurrect it. */
 ticket=plan();ms+=(uint64_t)(ticket.deadline-seconds)*1000;assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();assert(points_occ.edge==POINTS_EDGE_WARNING);ack();
}
static void recovery_cannot_authenticate_corrupt_history(void){
 setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);
 alarm_sleep_v1 ticket=plan();ms+=(uint64_t)(ticket.deadline-seconds)*1000;
 assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();points_ledger corrupt=points_occ;
 corrupt.deadline++;corrupt.recovery_until++;assert(points_ledger_valid(&corrupt));points_ledger_encode(&corrupt,blobs[6]);
 restart();valid=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
 unsigned writes=put_count;uint8_t before[EDIT_BYTES];unsigned size=sizes[9];memcpy(before,blobs[9],size);
 p=(alarm_clock_edit_permit_v2){.struct_size=sizeof(p)};
 assert(editor()->begin(NULL,ticket.deadline+10,&p)==ALARM_STORAGE);
 assert(put_count==writes&&!edit_permit_valid&&sizes[9]==size&&!memcmp(before,blobs[9],size));
}
static void pending_history_floor_replacement(void){
 for(unsigned last=0;last<2;last++)for(unsigned forward=0;forward<2;forward++){
  setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);
  unsigned edge;
  for(;;){alarm_sleep_v1 ticket=plan();ms+=(uint64_t)(ticket.deadline-seconds)*1000;
   assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();edge=points_occ.edge;
   if(!last||edge==POINTS_EDGE_END)break;
   ack();
  }
  points_ledger pending=points_occ;uint32_t requested=pending.deadline+(forward?10:0);
  restart();valid=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
  p=begin_edit(requested);valid=true;seed(requested);assert(editor()->finish(NULL,&p)==ALARM_PENDING);playing();
  assert(points_occ.state==ALARM_OCC_PENDING&&points_occ.deadline==pending.deadline&&points_occ.edge==edge&&points_occ.generation>pending.generation);
  ack();uint8_t history[64];memcpy(history,blobs[6],64);restart();until(ALARM_STATE_READY);assert(!memcmp(history,blobs[6],64));
 }
}
static void interrupted_history_floor_replacement(void){
 for(unsigned last=0;last<2;last++)for(unsigned after_seed=0;after_seed<2;after_seed++){
  setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);
  unsigned edge;
  do{alarm_sleep_v1 ticket=plan();ms+=(uint64_t)(ticket.deadline-seconds)*1000;
   assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();edge=points_occ.edge;ack();
  }while(last&&edge!=POINTS_EDGE_END);
  uint8_t history[64];memcpy(history,blobs[6],64);uint32_t requested=seconds+3600;
  p=begin_edit(requested);if(after_seed)seed(requested);restart();until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&edit_recovery);
  assert(!memcmp(history,blobs[6],64));points_service_context context={.struct_size=sizeof(context)};char utc[40]="UTC";
  assert(points_service_read_context(client,&points_configured,utc,0,&context)==ALARM_RTC);
  assert(editor()->finish(NULL,&p)==ALARM_STALE);
  p=begin_edit(requested);finish_edit(&p);assert(!memcmp(history,blobs[6],64));restart();until(ALARM_STATE_READY);assert(!memcmp(history,blobs[6],64));
 }
}
static void acknowledged_history_floor_replacement(void){
 for(unsigned last=0;last<2;last++)for(unsigned forward=0;forward<2;forward++){
  setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);
  unsigned edge=POINTS_EDGE_START;
  do{alarm_sleep_v1 ticket=plan();ms+=(uint64_t)(ticket.deadline-seconds)*1000;
   assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();edge=points_occ.edge;ack();
  }while(last&&edge!=POINTS_EDGE_END);
  uint8_t history[64];memcpy(history,blobs[6],64);uint32_t requested=seconds+(forward?3600:0);
  p=begin_edit(requested);finish_edit(&p);assert(edit_points_floor(&points_configured)==requested);
  assert(!memcmp(history,blobs[6],64));restart();until(ALARM_STATE_READY);assert(!memcmp(history,blobs[6],64));
  /* Retained identity alone can reconstruct old history; it cannot authorize
   * another deadline, a missing row, changed config or changed provenance. */
  points_config historical,changed=points_configured;points_ledger ledger=points_occ;
  assert(edit_points_recorded_config(&points_configured,&ledger,&historical));
  changed.points[0].minute++;assert(!edit_points_recorded_config(&changed,&ledger,&historical));
  points_configured_saved=false;assert(!edit_points_recorded_config(&points_configured,&ledger,&historical));points_configured_saved=true;
  ledger.deadline++;ledger.recovery_until++;assert(points_ledger_valid(&ledger));assert(!edit_points_recorded_config(&points_configured,&ledger,&historical));
  ledger.generation++;points_ledger_encode(&ledger,blobs[6]);restart();until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE);
 }
}
static void verified_zone_identity(void){

 setup_mixed();uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);
 alarm_sleep_v1 ticket=plan();ms+=(uint64_t)(ticket.deadline-seconds)*1000;
 assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();ack();
 /* Snapshot the completed history in a new verified authorization. */
p=begin_edit(seconds);finish_edit(&p);uint32_t floor=p.requested;
 uint8_t history[64],authorization[EDIT_BYTES];memcpy(history,blobs[6],64);unsigned auth_size=sizes[9];memcpy(authorization,blobs[9],auth_size);
 const char denver[40]="America/Denver",utc[40]="UTC";unsigned zi=(unsigned)portable_timezone_find(denver,40);
zone(denver);client->refresh(NULL);until(ALARM_STATE_READY);
 assert(edit_points_floor(&points_configured)==floor);
 points_service_context out;memset(&out,0xa5,sizeof(out));out.struct_size=sizeof(out);points_service_context unchanged=out;
 assert(points_service_read_context(client,&points_configured,utc,0,&out)==ALARM_STALE&&!memcmp(&out,&unchanged,sizeof(out)));
 assert(points_service_read_context(client,&points_configured,denver,zi,&out)==ALARM_OK&&out.effective_created==floor&&out.timezone_index==zi);
 assert(!memcmp(history,blobs[6],64)&&sizes[9]==auth_size&&!memcmp(authorization,blobs[9],auth_size));
restart();until(ALARM_STATE_READY);
 assert(points_service_read_context(client,&points_configured,denver,zi,&out)==ALARM_OK&&out.effective_created==floor);
 assert(!memcmp(history,blobs[6],64));
 /* The new zone changes future UTC projection, not the acknowledged local
  * day/slot/edge history. No second start is due at Denver noon. */
 ticket=plan();assert(ticket.deadline==date(2026,10,4,18,27,0));
 ms+=(uint64_t)(ticket.deadline-seconds)*1000;assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();
 assert(points_occ.edge==POINTS_EDGE_WARNING);ack();
 /* A zone switch cannot evade durable history checks by resurrecting the
  * prior UTC ACK with a newer generation. Retain all existing source guards. */
 points_ledger rolled;assert(points_ledger_decode(&rolled,history,64));rolled.generation=points_occ.generation+1;rolled.state=ALARM_OCC_PENDING;
 points_ledger_encode(&rolled,blobs[6]);restart();until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE);
}
static void clean_refusals(void){
 setup_mixed();uint32_t old=seconds;unsigned writes=put_count;alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};
 get_result=RISC_BOUND_KEY_VALUE_BUSY;assert(editor()->begin(NULL,old-86400,&p)==ALARM_BUSY);assert(put_count==writes&&!sizes[9]&&!edit_permit_valid&&!custody_retained);get_result=0;
 put_result=RISC_BOUND_KEY_VALUE_BUSY;put_commits=false;assert(editor()->begin(NULL,old-86400,&p)==ALARM_BUSY);assert(!sizes[9]&&!edit_permit_valid&&!custody_retained);put_result=0;put_commits=true;
 p=begin_edit(old-86400);assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 /* A configuration race before seed invalidates the reconciliation; it does
  * not borrow authorization for a newly observed writer identity. */
 points_config newer=points_configured;newer.revision++;save(newer);writes=put_count;
 p=(alarm_clock_edit_permit_v2){.struct_size=sizeof(p)};assert(editor()->begin(NULL,old-86400,&p)==ALARM_STALE&&put_count==writes&&!edit_permit_valid);
 until(ALARM_STATE_READY);p=begin_edit(old-86400);finish_edit(&p);
}
#ifndef POINTS_MIXED_TEST_ENTRY
#define POINTS_MIXED_TEST_ENTRY main
#endif
int POINTS_MIXED_TEST_ENTRY(void){
 recovery_cannot_authenticate_corrupt_history();pending_history_floor_replacement();interrupted_history_floor_replacement();acknowledged_history_floor_replacement();verified_zone_identity();clean_refusals();backward_occurrence();points_write_outcomes();same_revision_recovery();interrupted();legacy_import();io_and_zone();
 setup_mixed();uint8_t cfg[64];memcpy(cfg,blobs[5],64);uint32_t old=seconds;
 alarm_sleep_v1 ticket=plan();alarm_clock_edit_permit_v2 p=begin_edit(old-86400);
 assert(!memcmp(blobs[9],"ACM2",4));assert(client->step(NULL)==ALARM_BUSY);
 finish_edit(&p);assert(!memcmp(cfg,blobs[5],64));assert(edit_points_floor(&points_configured)==old-86400);
 assert(alarm_service_resume(client,&ticket)==ALARM_STALE);
 assert(plan().deadline==date(2026,10,4,12,0,0));
 points_service_context context={.struct_size=sizeof(context)};char utc[40]="UTC";
 assert(points_service_read_context(client,&points_configured,utc,0,&context)==ALARM_OK&&context.effective_created==old-86400);
 boot(false);until(ALARM_STATE_READY);assert(plan().deadline==date(2026,10,4,12,0,0));
 p=begin_edit(old-172800);assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);assert(edit_points_floor(&points_configured)==old-86400);
 p=begin_edit(old);finish_edit(&p);assert(edit_points_floor(&points_configured)==points_configured.created);
 /* A post-seed writer race cannot confirm or roll back another identity. */
 p=begin_edit(old-86400);seed(p.requested);points_config changed=points_configured;changed.revision++;save(changed);
 assert(editor()->finish(NULL,&p)==ALARM_RETAINED&&custody_retained);
 unsigned calls=get_count+put_count+reads+clocks;forbid=true;assert(editor()->cancel(NULL,&p)==ALARM_RETAINED);assert(calls==get_count+put_count+reads+clocks);
 puts("Mixed clock edit: durable floor, sleep, restart, cancel, replacement and post-seed race passed");return 0;
}
