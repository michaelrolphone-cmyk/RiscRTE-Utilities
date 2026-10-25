#include <setjmp.h>
static jmp_buf cut_env;static int cut_stage=-1;
static void clock_edit_test_cut(unsigned stage){if((int)stage==cut_stage)longjmp(cut_env,1);}
#define ALARM_SERVICE_CLOCK_EDIT_V2
#define main legacy_service_tests
#include "native_schedules_alarm_service_test.c"
#undef main
static const alarm_clock_edit_descriptor_v2*editor(void){const alarm_clock_edit_descriptor_v2*e=alarm_clock_edit_descriptor(client);assert(e);return e;}
static alarm_clock_edit_permit_v2 begin_edit(uint32_t value){alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};int r=editor()->begin(NULL,value,&p);assert(r==ALARM_OK);assert(p.requested==value&&p.generation&&p.nonce);return p;}
static void seed_to(uint32_t value){epoch=INT64_C(946684800)+value-(int64_t)(ms/1000);}
static unsigned ledger_phase(void){alarm_clock_edit_status_v2 s={.struct_size=sizeof(s)};assert(editor()->edit_status(NULL,&s)==0);return s.phase;}
static void finish_edit(alarm_clock_edit_permit_v2*p){seed_to(p->requested);assert(editor()->finish(NULL,p)==ALARM_PENDING);assert(editor()->finish(NULL,p)==ALARM_STALE);}
static void configuration_bytes(uint8_t b[2][32]){memcpy(b[0],blobs[0],32);memcpy(b[1],blobs[1],32);}
static void unchanged_config(uint8_t b[2][32]){assert(!memcmp(b[0],blobs[0],32)&&!memcmp(b[1],blobs[1],32));}
static void edits(void){
 const uint32_t dates[]={date(2027,1,2,3,4,5),date(2025,12,30,23,59,58),date(2026,1,1,0,0,0),date(2026,10,5,11,59,59)};
 for(unsigned d=0;d<4;d++)for(unsigned kind=0;kind<=2;kind++){
  setup();if(kind)schedule(kind,3600);until(ALARM_STATE_READY);uint8_t cfg[2][32];configuration_bytes(cfg);
  alarm_sleep_v1 old=plan();alarm_clock_edit_permit_v2 p=begin_edit(dates[d]);assert(client->step(NULL)==ALARM_BUSY);assert(client->refresh(NULL)==ALARM_BUSY);assert(alarm_service_resume(client,&old)==ALARM_BUSY);
  assert(ledger_phase()==ALARM_CLOCK_EDIT_INTENT);finish_edit(&p);until(ALARM_STATE_READY);assert(ledger_phase()==ALARM_CLOCK_EDIT_VERIFIED);unchanged_config(cfg);assert(alarm_service_resume(client,&old)==ALARM_STALE);
  /* Fresh service retains authorized domain and never repeats a consumed occurrence. */
  uint8_t occurrence[2][32];memcpy(occurrence[0],blobs[3],32);memcpy(occurrence[1],blobs[4],32);boot(false);until(ALARM_STATE_READY);unchanged_config(cfg);assert(!memcmp(occurrence[0],blobs[3],32)&&!memcmp(occurrence[1],blobs[4],32));
  /* An unannounced awake jump still faults, even inside prior authorized range. */
  epoch+=100;client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
 }
}
static void pending_and_cancel(void){
 setup();schedule(2,3600);until(ALARM_STATE_READY);uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);assert(ledger_phase()==ALARM_CLOCK_EDIT_INTENT);
 assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);assert(ledger_phase()==ALARM_CLOCK_EDIT_NONE);assert(editor()->cancel(NULL,&p)==ALARM_STALE);
 /* Reset after begin, before seed: explicit recovery, no automatic schedule delivery. */
 p=begin_edit(old-86400);boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&ledger_phase()==ALARM_CLOCK_EDIT_INTENT&&!active);assert(editor()->finish(NULL,&p)==ALARM_STALE);
 p=begin_edit(old-86400);finish_edit(&p);until(ALARM_STATE_READY);assert(seconds==old-86400);
 /* Reset after seed, before finish: same guarded recovery, durable intent only. */
 p=begin_edit(old-172800);seed_to(p.requested);boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&ledger_phase()==ALARM_CLOCK_EDIT_INTENT&&!active);
 p=begin_edit(old-172800);finish_edit(&p);until(ALARM_STATE_READY);
 /* A later forward edit replaces, rather than accumulates, prior lower floor. */
 p=begin_edit(old);finish_edit(&p);until(ALARM_STATE_READY);seed_to(old-172800);boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
}
static void explicit_rtc_recovery(void){
 for(unsigned back=0;back<2;back++)for(unsigned kind=0;kind<3;kind++){
  setup();if(kind)schedule(kind,3600);until(ALARM_STATE_READY);uint32_t old=seconds;uint8_t cfg[2][32];configuration_bytes(cfg);
  epoch+=back?-86400:86400;client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!active);
  alarm_clock_edit_permit_v2 p=begin_edit(old);finish_edit(&p);until(ALARM_STATE_READY);unchanged_config(cfg);assert(seconds==old);
 }
 /* Explicit Save can repair an invalid native clock without first guessing
  * time. It does not claim a clean cancel is possible from invalid time. */
 setup();schedule(2,3600);valid=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
 alarm_clock_edit_permit_v2 p=begin_edit(date(2026,1,1,0,0,0));valid=true;finish_edit(&p);until(ALARM_STATE_READY);
 setup();until(ALARM_STATE_READY);error=ALARM_STORAGE;phase=BLOCKED;p=(alarm_clock_edit_permit_v2){.struct_size=sizeof(p)};
 unsigned before=get_count+put_count+reads+clocks;assert(editor()->begin(NULL,100,&p)==ALARM_STORAGE);assert(before==get_count+put_count+reads+clocks);
}
static void identities(void){
 for(unsigned kind=1;kind<=2;kind++){
  setup();schedule(kind,5);until(ALARM_STATE_READY);ms+=5000;playing();alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};assert(editor()->begin(NULL,seconds-100,&p)==ALARM_BUSY);ack();
  uint8_t occ[32];memcpy(occ,blobs[kind==1?3:4],32);uint32_t old=seconds;p=begin_edit(old-86400);finish_edit(&p);until(ALARM_STATE_READY);assert(!memcmp(occ,blobs[kind==1?3:4],32));
  seed_to(old+2); /* unannounced jump rejected */ client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);assert(!memcmp(occ,blobs[kind==1?3:4],32));
 }
 setup();schedule(2,100);until(ALARM_STATE_READY);uint32_t old=seconds;alarm_clock_edit_permit_v2 p=begin_edit(old-86400);finish_edit(&p);until(ALARM_STATE_READY);
 /* New config identity cannot inherit the older config's clock authorization. */
 alarm_config replacement={2,old+200,old,200,2,1};alarm_config_encode(&replacement,blobs[1]);client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
}
static void pending_occurrence_recovery(void){
 for(unsigned expire=0;expire<2;expire++){
  setup();schedule(2,10);until(ALARM_STATE_READY);ms=10000;playing();uint32_t deadline=desired.deadline,generation=desired.generation;
  /* Owed PENDING record, then fresh invalid native time before evaluation. */
  boot(false);valid=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!active);valid=true;
  alarm_clock_edit_permit_v2 p=begin_edit(deadline-86400);finish_edit(&p);until(ALARM_STATE_READY);
  alarm_sleep_v1 ticket=plan();assert(ticket.deadline==deadline);ms+=(uint64_t)(deadline-seconds)*1000;
  assert(alarm_service_resume(client,&ticket)==ALARM_OK);playing();assert(desired.generation==generation+1);
  if(expire){ms+=ALARM_INVOCATION_MS;until(ALARM_STATE_READY);}else ack();
  alarm_occurrence done;assert(alarm_occurrence_decode(&done,blobs[4],sizes[4],2));assert(done.generation==generation+1&&done.state==(expire?ALARM_OCC_EXPIRED:ALARM_OCC_ACKED));
  boot(false);until(ALARM_STATE_READY);assert(!active);assert(plan().deadline==0);
  /* Completed identity cannot be changed to the other completed state. */
  p=begin_edit(seconds-1);finish_edit(&p);until(ALARM_STATE_READY);done.state=expire?ALARM_OCC_ACKED:ALARM_OCC_EXPIRED;alarm_occurrence_encode(&done,blobs[4]);boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE);
 }
}
static void verified_io(void){
 setup();until(ALARM_STATE_READY);put_result=RISC_BOUND_KEY_VALUE_IO;alarm_clock_edit_permit_v2 p=begin_edit(seconds+100);finish_edit(&p);until(ALARM_STATE_READY);
 p=begin_edit(seconds+100);assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);assert(!custody_retained);
}
static void rejects(void){
 setup();until(ALARM_STATE_READY);alarm_clock_edit_permit_v2 p=begin_edit(seconds+100),bad=p;bad.requested++;assert(editor()->finish(NULL,&bad)==ALARM_STALE);bad=p;bad.reserved[0]=1;assert(editor()->cancel(NULL,&bad)==ALARM_STALE);assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 /* A changed clock cannot authorize restoring prior ledger via cancel. */
 p=begin_edit(seconds+100);seed_to(p.requested);assert(editor()->cancel(NULL,&p)==ALARM_RTC);assert(ledger_phase()==ALARM_CLOCK_EDIT_INTENT&&!edit_permit_valid);p=begin_edit(seconds+100);finish_edit(&p);until(ALARM_STATE_READY);
 /* Corrupt durable record is not treated as absent. */
 blobs[9][30]^=1;boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE);
}
static void fresh_instance(void){provider=NULL;started=in_call=custody_retained=false;haptic_uncertain=audio_uncertain=false;kv=NULL;clock_api=NULL;realtime=NULL;haptic=NULL;audio=NULL;boot(false);}
static void cuts(unsigned operation,unsigned boundary){
 setup();schedule(2,3600);until(ALARM_STATE_READY);uint32_t old=seconds;uint8_t cfg[2][32];configuration_bytes(cfg);
 alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};
 if(operation)p=begin_edit(old-86400);
 if(operation==1)seed_to(p.requested);
 cut_stage=(int)boundary;
 if(!setjmp(cut_env)){
  if(!operation)(void)editor()->begin(NULL,old-86400,&p);
  else if(operation==1)(void)editor()->finish(NULL,&p);
  else (void)editor()->cancel(NULL,&p);
  assert(!"cut must run");
 }
 cut_stage=-1;fresh_instance();
 unsigned persisted=sizes[9]?alarm_read32(blobs[9]+8):ALARM_CLOCK_EDIT_NONE;
 until(persisted==ALARM_CLOCK_EDIT_INTENT?ALARM_STATE_BLOCKED:ALARM_STATE_READY);
 assert((persisted!=ALARM_CLOCK_EDIT_INTENT)||error==ALARM_RTC);unchanged_config(cfg);
 assert(editor()->finish(NULL,&p)==ALARM_STALE);
}
static void record_and_permit_negatives(void){
 for(unsigned offset=0;offset<EDIT_BYTES;offset++){
  setup();schedule(1,3600);until(ALARM_STATE_READY);alarm_clock_edit_permit_v2 p=begin_edit(seconds-100);finish_edit(&p);until(ALARM_STATE_READY);
  blobs[9][offset]^=1;boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE);
 }
 setup();schedule(2,3600);until(ALARM_STATE_READY);alarm_clock_edit_permit_v2 p=begin_edit(seconds-100);uint8_t pending[EDIT_BYTES];memcpy(pending,blobs[9],EDIT_BYTES);finish_edit(&p);until(ALARM_STATE_READY);
 /* Same-generation replacement is a rollback, not a fresh authorized edit. */
 memcpy(blobs[9],pending,EDIT_BYTES);client->refresh(NULL);until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE);
 setup();until(ALARM_STATE_READY);p=begin_edit(seconds+10);
 for(unsigned n=0;n<sizeof(p);n++){alarm_clock_edit_permit_v2 bad=p;((uint8_t*)&bad)[n]^=1;assert(editor()->finish(NULL,&bad)==ALARM_STALE);assert(editor()->cancel(NULL,&bad)==ALARM_STALE);}
 assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_READY);
 /* Cancel a recovery Save restores exactly the earlier unverified intent,
  * not a flattened fabricated VERIFIED authorization. */
 p=begin_edit(seconds-10);boot(false);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);uint8_t previous[EDIT_BYTES];memcpy(previous,blobs[9],EDIT_BYTES);
 p=begin_edit(seconds-20);assert(editor()->cancel(NULL,&p)==ALARM_PENDING);until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&ledger_phase()==ALARM_CLOCK_EDIT_INTENT);
 assert(!memcmp(previous,blobs[9],12)&&!memcmp(previous+16,blobs[9]+16,EDIT_BYTES-20));
}
static void terminal(unsigned mode){
 setup();schedule(1,3600);until(ALARM_STATE_READY);alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};
 if(mode<3){put_result=mode==0?99:mode==1?-99:RISC_BOUND_KEY_VALUE_CONTEXT;assert(editor()->begin(NULL,seconds+100,&p)==ALARM_RETAINED);}
 else {p=begin_edit(seconds+100);if(mode<6||mode==9)seed_to(p.requested);
  if(mode==3||mode==6)put_result=99;else if(mode==4||mode==7)get_result=RISC_BOUND_KEY_VALUE_IO;else if(mode==9)read_result=99;else put_commits=false;
  assert((mode<6||mode==9?editor()->finish(NULL,&p):editor()->cancel(NULL,&p))==ALARM_RETAINED);}

 unsigned before=get_count+put_count+reads+clocks;assert(client->step(NULL)==ALARM_RETAINED);assert(editor()->cancel(NULL,&p)==ALARM_RETAINED);assert(!provider->quiesce());assert(before==get_count+put_count+reads+clocks);
 /* New process/invocation may inspect committed bytes; the retained one did
  * no such callback. The on-media phase, not guessed previous return, wins. */
 fresh_instance();unsigned saved=sizes[9]?alarm_read32(blobs[9]+8):ALARM_CLOCK_EDIT_NONE;
 until(saved==ALARM_CLOCK_EDIT_INTENT?ALARM_STATE_BLOCKED:ALARM_STATE_READY);
 if(saved==ALARM_CLOCK_EDIT_INTENT)assert(error==ALARM_RTC&&!active);
}
int main(void){edits();pending_and_cancel();explicit_rtc_recovery();identities();pending_occurrence_recovery();rejects();verified_io();record_and_permit_negatives();for(unsigned op=0;op<3;op++)for(unsigned boundary=0;boundary<3;boundary++)cuts(op,boundary);for(unsigned i=0;i<10;i++){pid_t pid=fork();assert(pid>=0);if(!pid){/* Fresh invocation, not release of a retained service. */provider=NULL;started=false;custody_retained=false;haptic_uncertain=audio_uncertain=false;terminal(i);_exit(0);}int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}puts("PASS native editor service: exact durable authorization, forward/backward edits, reset cuts, no duplicate ACK, stale permits/tickets, retained unknown outcomes; no physical claim");return 0;}
