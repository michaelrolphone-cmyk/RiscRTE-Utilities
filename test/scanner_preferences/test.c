#include <assert.h>
#include <stdio.h>
#include "scanner_preferences.h"
static risc_runtime_api_v1 runtime;
static const risc_runtime_api_v1 *identity;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?identity:NULL;}
static scanner_preferences s;
static unsigned calls,acquires,releases,submits,collects,cancels,physical_calls,retains;
static bool alive=true,atomic_lane,absent,dirty_acquire,dirty_release,malformed_api,malformed_state,corrupt_receipt,revoke;
static bool reenter;
static bool present;static uint8_t stored[64];static uint32_t size;
static uint64_t revision,token;
static int read_rc,submit_rc,receipt_rc;
static unsigned pending;
static uint8_t physical;static bool physical_fail,restore_fail,advance_on_enable;
static uint8_t copied[64],intent[192],last_intent[192];static uint32_t copied_size,intent_size,kind;
static uint64_t submitted_revision;static bool identity_change;
static void io(void){assert(!s.retained);calls++;if(revoke)identity=NULL;if(reenter){reenter=false;assert(scanner_preferences_cleanup(&s)==SCANNER_PREF_RETAINED);}}
static bool guard(void*c){(void)c;return alive;}
static void retain(void*c){(void)c;retains++;}
static int state(void*c,risc_key_value_atomic_state_v1*out){(void)c;io();*out=(risc_key_value_atomic_state_v1){sizeof(*out),atomic_lane?3:4,1,2,64,512,192,0,revision};if(malformed_state)out->namespace_id=2;return read_rc;}
static int get(void*c,uint64_t rev,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;(void)k;io();*n=0;if(read_rc)return read_rc;if(rev!=revision)return RISC_ATOMIC_STALE;if(!present)return RISC_ATOMIC_NOT_FOUND;assert(size<=cap);memcpy(b,stored,size);*n=size;return 0;}
static int submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*i,uint32_t n,uint64_t*t){
 (void)c;io();submits++;assert(count==1);*t=0;
 if(submit_rc)return submit_rc;
 if(rev!=revision)return RISC_ATOMIC_STALE;
 assert(n<=sizeof(intent));memcpy(last_intent,intent,sizeof(intent));memcpy(intent,i,n);intent_size=n;
 copied_size=m->size;if(m->size)memcpy(copied,m->value,m->size);kind=m->kind;submitted_revision=rev;*t=token=77;return 0;
}
static int collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){
 (void)c;io();collects++;assert(t==token);if(pending){pending--;return RISC_ATOMIC_PENDING;}
 r->status=receipt_rc;r->operation_id=0;r->revision=revision;
 if(!receipt_rc){revision++;r->revision=revision;r->operation_id=9;present=kind==RISC_ATOMIC_PUT;size=present?copied_size:0;if(size)memcpy(stored,copied,size);}
 if(corrupt_receipt)r->struct_size=0;
 token=0;return 0;
}
static int cancel(void*c,uint64_t t){(void)c;io();assert(t==token);cancels++;return 0;}
static risc_preference_store_v1 pref={1,sizeof(pref),NULL,state,get,submit,collect,cancel};
static int kv_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;(void)k;io();*n=0;if(read_rc)return read_rc;if(!present)return RISC_KEY_VALUE_NOT_FOUND;if(size>cap){*n=size;return RISC_KEY_VALUE_BUFFER_SMALL;}memcpy(b,stored,size);*n=size;return 0;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,NULL};
static bool set_enabled(void*c,bool enabled){(void)c;io();physical_calls++;if(advance_on_enable)revision++;if(physical_fail&&enabled)return false;if(restore_fail&&!enabled)return false;physical=enabled?1:0;return true;}
static bool status(void*c,uint8_t*p){(void)c;io();*p=physical;return true;}
static portable_bluetooth_control_v1 radio={1,sizeof(radio),NULL,NULL,NULL,set_enabled,status};
static bool acquire(const char*name,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 io();acquires++;assert(v==1);
 if(identity_change)identity=NULL;
 if(absent||(atomic_lane&&!strcmp(name,RISC_PREFERENCE_STORE_CAPABILITY))){if(dirty_acquire)g->slot=1;return false;}
 if(!strcmp(name,"bluetooth.hci")){assert(instance==16);g->api=&radio;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);g->api=&kv;}
 else {assert(instance==1);g->api=&pref;if(malformed_api)pref.collect=NULL;}
 g->slot=2;g->generation=3;return true;
}
static bool release(risc_runtime_capability_v1*g){io();releases++;assert(!token);*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};if(dirty_release)g->slot=2;return true;}
static void reset(void){
 memset(&s,0,sizeof(s));calls=acquires=releases=submits=collects=cancels=physical_calls=retains=0;
 alive=true;atomic_lane=absent=dirty_acquire=dirty_release=malformed_api=malformed_state=corrupt_receipt=revoke=identity_change=false;
 reenter=false;present=false;size=0;revision=10;token=0;read_rc=submit_rc=receipt_rc=0;pending=0;physical=0;physical_fail=restore_fail=advance_on_enable=false;
 memset(stored,0,sizeof(stored));memset(intent,0,sizeof(intent));intent_size=0;pref.collect=collect;
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};identity=&runtime;
 assert(scanner_preferences_init(&s,&runtime,guard,retain,NULL)==SCANNER_PREF_OK);
}
static uint8_t address[6]={1,2,3,4,5,6};
static void begin(void){char prior[25];assert(scanner_alias_begin(&s,1,address,prior)==0);assert(!s.preference.api);}
static int drain(void){int rc=SCANNER_PREF_PENDING;for(unsigned i=0;i<8&&rc==SCANNER_PREF_PENDING;i++)rc=scanner_preferences_step(&s);return rc;}
static void no_io(void){unsigned before=calls;assert(scanner_preferences_step(&s)==SCANNER_PREF_RETAINED);assert(scanner_preferences_cleanup(&s)==SCANNER_PREF_RETAINED);assert(calls==before);}
#define CASE(name) static void name(void)
CASE(alias_save){reset();begin();assert(!submits);assert(scanner_alias_save(&s,"  Kitchen  ")==3);assert(drain()==0);char out[25];assert(ble_alias_decode(1,address,stored,size,out)&&!strcmp(out,"Kitchen"));assert(!s.preference.api);}
CASE(alias_atomic){reset();atomic_lane=true;begin();assert(scanner_alias_save(&s,"A")==3);assert(drain()==0);assert(submits==1);}
CASE(tombstone){reset();begin();assert(scanner_alias_save(&s," ")==3);assert(drain()==0);assert(size==40&&stored[10]==0);}
CASE(corrupt_repair){reset();present=true;size=40;memset(stored,0x71,40);char p[25];assert(scanner_alias_begin(&s,1,address,p)==0&&!p[0]);assert(!submits);assert(!memcmp(s.alias.expected,stored,40));assert(scanner_alias_save(&s,"Fixed")==3);assert(drain()==0);}
CASE(same_identity){reset();begin();unsigned n=calls;char p[25];assert(scanner_alias_begin(&s,1,address,p)==0);assert(calls==n);uint8_t other[6]={0};assert(scanner_alias_begin(&s,1,other,p)==6);}
CASE(invalid_name){reset();begin();assert(scanner_alias_save(&s,"\n")==2);assert(scanner_alias_save(&s,"1234567890123456789012345")==2);assert(!submits);}
CASE(max_name){reset();begin();assert(scanner_alias_save(&s,"123456789012345678901234")==3);assert(drain()==0&&stored[10]==24);}
CASE(stale_retry){reset();begin();receipt_rc=RISC_ATOMIC_IO;assert(scanner_alias_save(&s,"A")==3);assert(drain()==2);uint8_t before[176];memcpy(before,s.alias.intent,176);revision++;assert(scanner_preferences_cleanup(&s)==0);assert(scanner_alias_save(&s,"B")==6);assert(scanner_alias_save(&s,"A")==6);assert(!memcmp(before,s.alias.intent,176));assert(submits==2&&!token);}
CASE(exact_retry){reset();begin();receipt_rc=RISC_ATOMIC_UNCERTAIN;assert(scanner_alias_save(&s,"A")==3);assert(drain()==5);uint8_t before[176];memcpy(before,s.alias.intent,176);assert(scanner_preferences_cleanup(&s)==0);receipt_rc=0;assert(scanner_alias_save(&s,"A")==3);assert(drain()==0);assert(!memcmp(before,intent,176));}
CASE(pending_case){reset();begin();pending=2;assert(scanner_alias_save(&s,"A")==3);unsigned released=releases;assert(scanner_preferences_step(&s)==3);assert(releases==released&&s.alias.token);assert(drain()==0);}
CASE(cancel_pending){reset();begin();assert(scanner_alias_save(&s,"A")==3);receipt_rc=RISC_ATOMIC_CANCELLED;assert(scanner_alias_cancel(&s)==0);assert(cancels==1&&!s.alias_active&&!s.preference.api);}
CASE(cancel_commit_race){reset();begin();assert(scanner_alias_save(&s,"A")==3);assert(scanner_alias_cancel(&s)==0);assert(present&&size==40);}
CASE(busy_submit){reset();begin();submit_rc=RISC_ATOMIC_BUSY;assert(scanner_alias_save(&s,"A")==3);assert(!s.alias.token&&s.preference.api);submit_rc=0;assert(drain()==0);}
CASE(dirty_refusal){reset();absent=dirty_acquire=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);assert(acquires==1);no_io();}
CASE(absence){reset();absent=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==1);assert(acquires==2&&!retains);}
CASE(bad_release){reset();dirty_release=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);no_io();}
CASE(bad_api){reset();malformed_api=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);no_io();}
CASE(unknown_status){reset();read_rc=555;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);no_io();}
CASE(bad_receipt){reset();begin();assert(scanner_alias_save(&s,"A")==3);corrupt_receipt=true;assert(drain()==4);no_io();}
CASE(identity_acquire){reset();identity_change=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);no_io();}
CASE(identity_read){reset();begin();revoke=true;assert(scanner_alias_save(&s,"A")==4);no_io();}
CASE(policy_default){reset();uint8_t flags=0;assert(scanner_radio_allowed(&s,&flags)==0&&flags==1);assert(!submits);}
CASE(policy_airplane){reset();present=true;size=4;uint8_t b[]={0x51,1,4,0xa1};memcpy(stored,b,4);uint8_t flags=0;assert(scanner_radio_allowed(&s,&flags)==0&&flags==4);assert(scanner_enable_bluetooth(&s)==2);assert(!submits&&!physical_calls);}
CASE(policy_corrupt){reset();present=true;size=4;uint8_t flags=0;assert(scanner_radio_allowed(&s,&flags)==2);assert(scanner_enable_bluetooth(&s)==2);assert(!submits);}
CASE(enable){reset();assert(scanner_enable_bluetooth(&s)==3);assert(!physical_calls);assert(drain()==0);assert(physical==1&&size==4&&stored[2]==3&&!s.preference.api&&!s.radio.api);}
CASE(compensate){reset();physical_fail=true;assert(scanner_enable_bluetooth(&s)==3);assert(drain()==2);assert(!present&&physical==0&&submits==2&&intent_size==192&&!memcmp(intent,"PFR0001",7));}
CASE(compensate_existing){reset();physical_fail=true;present=true;size=4;uint8_t b[]={0x51,1,1,0xa4};memcpy(stored,b,4);assert(scanner_enable_bluetooth(&s)==3);assert(drain()==2);assert(present&&!memcmp(stored,b,4));}
CASE(compensation_new_writer){reset();physical_fail=advance_on_enable=true;assert(scanner_enable_bluetooth(&s)==3);assert(drain()==4);assert(submits==1);no_io();}
CASE(radio_uncertain){reset();receipt_rc=RISC_ATOMIC_UNCERTAIN;assert(scanner_enable_bluetooth(&s)==3);assert(drain()==4);assert(!physical_calls);no_io();}
CASE(radio_retained){reset();physical=2;assert(scanner_enable_bluetooth(&s)==4);assert(!submits);no_io();}
CASE(radio_cleanup){reset();assert(scanner_enable_bluetooth(&s)==3);assert(scanner_preferences_cleanup(&s)==0);assert(physical==1);}
CASE(physical_restore_failure){reset();physical_fail=restore_fail=true;assert(scanner_enable_bluetooth(&s)==3);assert(drain()==4);no_io();}
CASE(read_preserves_editor){reset();begin();portable_preference_atomic before=s.alias;present=true;size=40;assert(ble_alias_encode(1,address,"Name",stored));char out[25];assert(scanner_alias_read(&s,1,address,out)==0&&!strcmp(out,"Name"));assert(!memcmp(&before,&s.alias,sizeof(before))&&s.alias_active&&!submits);}
CASE(read_missing){reset();char out[25]="old";assert(scanner_alias_read(&s,1,address,out)==0&&!out[0]);assert(!s.alias_active&&!submits);}
CASE(read_corrupt){reset();present=true;size=40;char out[25];assert(scanner_alias_read(&s,1,address,out)==2&&!out[0]);assert(!s.storage.api);}
CASE(read_unknown){reset();read_rc=555;char out[25];assert(scanner_alias_read(&s,1,address,out)==4);no_io();}
CASE(dirty_existing_acquire){reset();s.preference.slot=1;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);assert(!calls);no_io();}
CASE(dirty_existing_release){reset();s.storage.generation=2;assert(scanner_preferences_cleanup(&s)==4);assert(!calls);no_io();}
CASE(reentrant_acquire){reset();reenter=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);assert(calls==1);no_io();}
CASE(reentrant_state){reset();assert(scanner_preferences_open(&s)==0);reenter=true;assert(scanner_alias_begin(&s,1,address,(char[25]){0})==4);no_io();}
CASE(policy_oversized){reset();present=true;size=5;uint8_t flags=0;assert(scanner_radio_allowed(&s,&flags)==SCANNER_PREF_ERROR);assert(!s.retained&&!s.storage.api&&releases==1&&!submits);}
int main(void){(void)ble_alias_save;alias_save();alias_atomic();tombstone();corrupt_repair();same_identity();invalid_name();max_name();stale_retry();exact_retry();pending_case();cancel_pending();cancel_commit_race();busy_submit();dirty_refusal();absence();bad_release();bad_api();unknown_status();bad_receipt();identity_acquire();identity_read();policy_default();policy_airplane();policy_corrupt();enable();compensate();compensate_existing();compensation_new_writer();radio_uncertain();radio_retained();radio_cleanup();physical_restore_failure();read_preserves_editor();read_missing();read_corrupt();read_unknown();dirty_existing_acquire();dirty_existing_release();reentrant_acquire();reentrant_state();policy_oversized();puts("41 scanner preference cases passed");return 0;}
