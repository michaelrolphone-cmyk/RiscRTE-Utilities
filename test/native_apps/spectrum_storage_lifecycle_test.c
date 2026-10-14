/* Actual Audio Spectrum app; model only Runtime's one native app/shared-data
 * slot and the shared checkpoint admission sequence. No physical device I/O. */
#define PORTABLE_CONTEXTS_CLIENT
#define main spectrum_legacy_test_main
#include "audio_spectrum_test.c"
#undef main
static bool storage_live,storage_terminal,fail_storage_release,dirty_refusal;
static unsigned storage_acquires,storage_releases,storage_calls,storage_refusals,checkpoints,storage_serial;
static uint64_t storage_instance,disk_revision;
static unsigned char disk_bytes[32];
static uint32_t disk_size;
static int32_t injected_result;
static risc_runtime_capability_v1 *retained_grant;
static risc_runtime_capability_v1 retained_snapshot;
static risc_app_data_v1 native_data;
static void storage_io(void *context){assert(!storage_terminal&&storage_live&&(uintptr_t)context==storage_serial);storage_calls++;}
static int32_t storage_result(void){if(injected_result==RISC_APP_DATA_RETAINED)storage_terminal=true;return injected_result;}
static bool selected(const char *name){return storage_instance==2&&!strcmp(name,"context-fingerprints.cfp");}
static int32_t native_stat(void*c,const char*n,uint32_t*s,uint64_t*v){
 storage_io(c);*s=0;*v=0;if(injected_result)return storage_result();
 if(!selected(n)||!disk_size)return RISC_APP_DATA_NOT_FOUND;
 *s=disk_size;*v=disk_revision;return RISC_APP_DATA_OK;
}
static int32_t native_read(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*s,uint64_t*v){
 storage_io(c);*s=0;*v=0;if(injected_result)return storage_result();
 if(!selected(n)||!disk_size)return RISC_APP_DATA_NOT_FOUND;
 if(expected!=disk_revision)return RISC_APP_DATA_STALE;
 if(cap<disk_size)return RISC_APP_DATA_INVALID;
 memcpy(b,disk_bytes,disk_size);*s=disk_size;*v=disk_revision;return RISC_APP_DATA_OK;
}
static int32_t native_replace(void*c,const char*n,uint64_t expected,const void*b,uint32_t size){
 storage_io(c);if(injected_result)return storage_result();
 assert(selected(n)&&size<=sizeof(disk_bytes));
 if(expected!=(disk_size?disk_revision:0))return RISC_APP_DATA_STALE;
 memcpy(disk_bytes,b,size);disk_size=size;disk_revision++;return RISC_APP_DATA_OK;
}
static bool storage_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 assert(!storage_terminal);
 if(!strcmp(n,"rtc.clock"))return false;
 if(strcmp(n,RISC_APP_DATA_CAPABILITY)&&strcmp(n,RISC_SHARED_DATA_CAPABILITY))return acquire(n,version,instance,g);
 assert(version==1&&g->struct_size==sizeof(*g));
 if(storage_live){storage_refusals++;storage_terminal=true;fputs("single-slot storage refusal: checked adapter retains invocation\n",stderr);return false;}
 assert(instance>=1&&instance<=3);storage_live=true;storage_instance=instance;storage_serial++;
 native_data=(risc_app_data_v1){1,sizeof(native_data),(void*)(uintptr_t)storage_serial,native_stat,native_read,native_replace};
 g->api=&native_data;g->slot=1;g->generation=storage_serial;retained_grant=g;retained_snapshot=*g;
 storage_acquires++;
 if(dirty_refusal){storage_terminal=true;return false;}
 return true;
}
static bool storage_release(risc_runtime_capability_v1*g){
 assert(!storage_terminal);
 if(g->api!=&native_data)return release(g);
 assert(storage_live&&g->generation==storage_serial);
 if(fail_storage_release){storage_terminal=true;retained_grant=g;retained_snapshot=*g;return false;}
 storage_live=false;storage_releases++;return true;
}
static uint32_t storage_millis(void){assert(!storage_terminal);return millis();}
/* cfa_restore invokes this real public hook. Model its shared-data admission;
 * fingerprint parsing and service internals are covered by their existing suite. */
bool portable_contexts_models_save(void){
 assert(!storage_terminal);checkpoints++;
 for(uint64_t instance=1;instance<=3;instance++){
  risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
  if(!rt.acquire(RISC_SHARED_DATA_CAPABILITY,1,instance,&g))return false;
  const risc_app_data_v1*f=g.api;uint32_t s=0;uint64_t v=0;
  int32_t rc=f->stat(f->context,"context-fingerprints.cfp",&s,&v);
  assert(rc==RISC_APP_DATA_OK||rc==RISC_APP_DATA_NOT_FOUND);
  if(!rt.release(&g))return false;
 }
 return true;
}
const contexts_service_v1 *portable_contexts_service(void){return NULL;}
bool portable_contexts_stop(void){return true;}
bool portable_contexts_enable(bool on){(void)on;return true;}
bool portable_contexts_training(bool on){(void)on;return true;}
uint16_t portable_contexts_action_mask(void){return PORTABLE_CONTEXT_ACTIONS;}
unsigned portable_contexts_face_count(void){return 0;}
const char *portable_contexts_face_name(unsigned i){(void)i;return "";}
bool portable_contexts_rules_read(cr_store*s){memset(s,0,sizeof(*s));return true;}
bool portable_contexts_rules_save(const cr_store*s){(void)s;return true;}
static void reset_storage(void){
 reset();storage_live=storage_terminal=fail_storage_release=dirty_refusal=false;
 storage_acquires=storage_releases=storage_calls=storage_refusals=checkpoints=storage_serial=0;
 storage_instance=disk_revision=disk_size=0;injected_result=0;retained_grant=NULL;
 memset(disk_bytes,0,sizeof(disk_bytes));rt.acquire=storage_acquire;rt.release=storage_release;api.millis=storage_millis;
}
static void startup_ok(void){assert(checkpoints==1&&event_files.ready==3&&cfa_loaded&&!cfa_failed&&!storage_live&&!storage_refusals&&!temporal_retained());}
static void late_read_write(void){
 startup_ok();const risc_app_data_v1*f=event_files.api;const char*n="context-fingerprints.cfp";
 const unsigned char wanted[]={1,2,3,4};unsigned char actual[8]={0};uint32_t size=0;uint64_t revision=0,current=0;
 assert(f->replace(f->context,n,0,wanted,sizeof(wanted))==RISC_APP_DATA_OK&&!storage_live);
 assert(f->stat(f->context,n,&size,&revision)==RISC_APP_DATA_OK&&size==sizeof(wanted)&&revision&&!storage_live);
 assert(portable_contexts_models_save()&&!storage_live);
 assert(f->read(f->context,n,revision,actual,sizeof(actual),&size,&current)==RISC_APP_DATA_OK&&current==revision&&!memcmp(actual,wanted,size)&&!storage_live);
 assert(f->replace(f->context,n,revision,wanted,sizeof(wanted))==RISC_APP_DATA_OK&&!storage_live);
 assert(f->read(f->context,n,revision,actual,sizeof(actual),&size,&current)==RISC_APP_DATA_STALE&&!storage_live);
 injected_result=RISC_APP_DATA_NO_SPACE;
 assert(f->replace(f->context,n,disk_revision,wanted,sizeof(wanted))==RISC_APP_DATA_NO_SPACE&&!storage_live);
 injected_result=0;assert(portable_contexts_models_save()&&!storage_live);
 assert(storage_acquires==storage_releases&&storage_calls+1==storage_releases&&!storage_refusals);
}
static unsigned failure_mode;
static void retained_operation(void){
 startup_ok();const risc_app_data_v1*f=event_files.api;unsigned before=storage_releases;uint32_t size=77;uint64_t revision=88;unsigned char bytes[4]={0};int32_t result;
 if(failure_mode<3)injected_result=RISC_APP_DATA_RETAINED;
 if(failure_mode==3)fail_storage_release=true;
 if(failure_mode==4)dirty_refusal=true;
 if(failure_mode==1)result=f->read(f->context,"context-fingerprints.cfp",1,bytes,sizeof(bytes),&size,&revision);
 else if(failure_mode==2)result=f->replace(f->context,"context-fingerprints.cfp",0,bytes,sizeof(bytes));
 else result=f->stat(f->context,"context-fingerprints.cfp",&size,&revision);
 assert(result==RISC_APP_DATA_RETAINED&&temporal_retained()&&storage_releases==before);
 assert(retained_grant&&!memcmp(retained_grant,&retained_snapshot,sizeof(retained_snapshot)));
 unsigned calls=storage_calls,acqs=storage_acquires;
 assert(f->stat(f->context,"context-fingerprints.cfp",&size,&revision)==RISC_APP_DATA_RETAINED);
 assert(!size&&!revision&&calls==storage_calls&&acqs==storage_acquires&&before==storage_releases);
}
int main(int argc,char**argv){
 (void)argv;reset_storage();check(startup_ok);back();run();
 assert(!storage_live&&!storage_terminal&&!storage_refusals&&storage_acquires==storage_releases);
 if(argc>1)return 0; /* The unpatched-source reproduction only runs startup. */
 reset_storage();check(late_read_write);back();run();assert(!storage_live&&!storage_terminal);
 for(failure_mode=0;failure_mode<5;failure_mode++){
  reset_storage();check(retained_operation);run();assert(storage_terminal&&storage_live&&temporal_retained());
  assert(retained_grant&&!memcmp(retained_grant,&retained_snapshot,sizeof(retained_snapshot)));
 }
 reset_storage();injected_result=RISC_APP_DATA_RETAINED;run();
 assert(temporal_retained()&&checkpoints==0&&!polls&&!frames&&storage_releases==1&&storage_acquires==2&&storage_calls==1);
 puts("PASS: actual Spectrum startup, shared interleaving, later write/read, stale revision, no-space, retained stat/read/replace, refused release, dirty acquisition, startup terminal fence");return 0;
}
