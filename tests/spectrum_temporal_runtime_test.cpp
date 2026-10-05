/* Production Spectrum codec/client against the real Runtime file backend.
 * Namespace authority and ELF lifetime are covered by Runtime's own suite. */
#define _Static_assert static_assert
#include "../Apps/spectrum_temporal_files.h"
#include "runtime/storage/AppDataFiles.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

using RiscStorage::AppDataFiles;
enum Fault { NONE, READ, WRITE, RENAME_BEFORE, RENAME_AFTER, CLOSE };
static Fault fault=NONE;
static unsigned renames=0,provider_calls=0,close_skip=0;
static bool retained_stage_close=false;
static uint32_t ticks=0;
static bool hit(Fault f){if(fault!=f)return false;fault=NONE;errno=f==WRITE?ENOSPC:EIO;return true;}
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __wrap_read(int f,void*b,size_t n){return hit(READ)?-1:__real_read(f,b,n);}
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" ssize_t __wrap_write(int f,const void*b,size_t n){return hit(WRITE)?-1:__real_write(f,b,n);}
extern "C" int __real_rename(const char*,const char*);
extern "C" int __wrap_rename(const char*a,const char*b){++renames;if(hit(RENAME_BEFORE))return -1;int r=__real_rename(a,b);return hit(RENAME_AFTER)?-1:r;}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int f){
 bool fail=false;
 if(fault==CLOSE){if(close_skip)--close_skip;else{char proc[64],path[256];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",f);ssize_t n=readlink(proc,path,sizeof(path)-1);assert(n>0);path[n]=0;retained_stage_close=std::string(path).find("/n00000002/.pending")!=std::string::npos;fail=true;}}
 int r=__real_close(f);return fail&&hit(CLOSE)?-1:r;
}
static uint32_t now(void*){return ticks;}
static bool cooperate(void*){++ticks;return true;}
static risc_app_data_v1 bind(AppDataFiles&store){return {1,sizeof(risc_app_data_v1),&store,
 [](void*c,const char*n,uint32_t*s,uint64_t*r){++provider_calls;return static_cast<AppDataFiles*>(c)->stat(2,n,s,r);},
 [](void*c,const char*n,uint64_t r,void*b,uint32_t z,uint32_t*s,uint64_t*a){++provider_calls;return static_cast<AppDataFiles*>(c)->read(2,n,r,b,z,s,a);},
 [](void*c,const char*n,uint64_t r,const void*b,uint32_t z){++provider_calls;return static_cast<AppDataFiles*>(c)->replace(2,n,r,b,z);}};}
static int32_t put(AppDataFiles&store,unsigned ns,const char*name,const void*bytes,uint32_t size){uint32_t old=0;uint64_t revision=0;int32_t result=store.stat(ns,name,&old,&revision);if(result&&result!=RISC_APP_DATA_NOT_FOUND)return result;return store.replace(ns,name,revision,bytes,size);}
static st_files client,reloaded;
static st_library library,loaded,before;
static uint8_t bytes[ST_BANK_MAX];
static st_example example(){st_segmenter segment;st_segment_reset(&segment);st_frame frame={};frame.level_db=-2000;frame.peak_hz=1000;frame.flags=ST_ACTIVE;st_set_nibble(&frame,40,15);for(unsigned i=0;i<64;i++)st_segment_observe(&segment,&frame);assert(segment.ready&&segment.event.count==64&&st_example_valid(&segment.event));return segment.event;}
static void fill_bank(unsigned bank){assert(st_files_begin(&client,&library,bank)==0);st_example e=example();for(unsigned i=bank*4;i<bank*4+4;i++){st_label*l=&library.labels[i];l->present=true;l->shift_limit=2;l->next_id=7;snprintf(l->name,sizeof(l->name),"Event %u",i);for(unsigned j=0;j<6;j++){l->examples[j]=e;l->examples[j].id=j+1;l->examples[j].kind=j==5?ST_NEGATIVE:ST_POSITIVE;}}assert(st_files_freeze(&client,&library)&&client.wanted_size==ST_BANK_MAX);}
static void rename_label(const char*name){assert(st_files_begin(&client,&library,0)==0);strcpy(library.labels[0].name,name);assert(st_files_freeze(&client,&library));}
static void load_both(st_files&c,st_library&l){assert(st_files_load(&c,&l,0,false)==0);assert(st_files_load(&c,&l,1,false)==0);}
int main(int argc,char**argv){
 assert(argc==2);AppDataFiles store({nullptr,now,cooperate,malloc,free});auto api=bind(store);st_files_init(&client,&api);
 assert(st_files_load(&client,&library,0,false)==RISC_APP_DATA_UNAVAILABLE&&!client.ready);
 assert(store.configure(argv[1]));load_both(client,library);assert(client.ready==3&&!client.exists);
 fill_bank(0);assert(st_files_save(&client)==0);fill_bank(1);assert(st_files_save(&client)==0);
 st_files_init(&reloaded,&api);load_both(reloaded,loaded);assert(!memcmp(&library,&loaded,sizeof(library)));
 uint32_t size=0;uint64_t revision=0;assert(store.stat(2,st_file_name(0),&size,&revision)==0&&size==ST_BANK_MAX);
 rename_label("Other app CAS");assert(put(store,1,st_file_name(0),"timecard",8)==0);
 uint64_t got=0;assert(store.read(2,st_file_name(0),revision,bytes,sizeof(bytes),&size,&got)==RISC_APP_DATA_STALE);
 assert(st_files_save(&client)==0);load_both(reloaded,loaded);assert(!memcmp(&library,&loaded,sizeof(library)));
 /* Fill the namespace to its exact committed-byte quota, then grow a bank.
  * The failure preserves both the old bank and the complete intended edit. */
 st_example saved=library.labels[0].examples[0];assert(st_files_begin(&client,&library,0)==0);memset(&library.labels[0].examples[0],0,sizeof(saved));assert(st_files_freeze(&client,&library));assert(st_files_save(&client)==0);
 const uint32_t slack=RISC_APP_DATA_NAMESPACE_MAX-(ST_BANK_MAX*2-64*38);std::vector<uint8_t> filler(slack,1);assert(put(store,2,"quota-test",filler.data(),filler.size())==0);
 assert(st_files_begin(&client,&library,0)==0);library.labels[0].examples[0]=saved;assert(st_files_freeze(&client,&library));assert(st_files_save(&client)==RISC_APP_DATA_NO_SPACE&&client.pending==0);assert(st_files_load(&reloaded,&loaded,0,false)==0&&!loaded.labels[0].examples[0].id);
 assert(put(store,2,"quota-test",nullptr,0)==0);assert(st_files_save(&client)==0);assert(st_files_load(&reloaded,&loaded,0,false)==0&&loaded.labels[0].examples[0].id==saved.id);
 rename_label("Write full");fault=WRITE;assert(st_files_save(&client)==RISC_APP_DATA_NO_SPACE&&client.pending==0);assert(st_files_save(&client)==0);
 rename_label("Unknown before");fault=RENAME_BEFORE;assert(st_files_save(&client)==RISC_APP_DATA_COMMIT_UNKNOWN&&client.pending==0);assert(st_files_save(&client)==0);
 rename_label("Unknown after");fault=RENAME_AFTER;assert(st_files_save(&client)==RISC_APP_DATA_COMMIT_UNKNOWN&&client.pending==0);unsigned prior=renames;assert(st_files_save(&client)==0&&renames==prior);
 /* The client rereads rather than reusing a token from a previous mount. */
 AppDataFiles restarted({nullptr,now,cooperate,malloc,free});assert(restarted.configure(argv[1]));auto restarted_api=bind(restarted);st_files_init(&reloaded,&restarted_api);load_both(reloaded,loaded);assert(!memcmp(&library,&loaded,sizeof(library)));
 before=loaded;fault=READ;assert(st_files_load(&reloaded,&loaded,0,false)==RISC_APP_DATA_IO&&!memcmp(&before,&loaded,sizeof(loaded)));assert(st_files_load(&reloaded,&loaded,0,false)==0);
 /* A real competing replacement cannot silently overwrite the pending edit;
  * explicit discard reloads that committed version rather than the stale base. */
 before=loaded;assert(st_files_begin(&reloaded,&loaded,0)==0);strcpy(loaded.labels[0].name,"Pending edit");assert(st_files_freeze(&reloaded,&loaded));strcpy(before.labels[0].name,"External edit");++before.generation[0];size=(uint32_t)st_bank_encode(&before,0,bytes,sizeof(bytes));assert(put(restarted,2,st_file_name(0),bytes,size)==0);prior=renames;assert(st_files_save(&reloaded)==ST_FILES_CONFLICT&&renames==prior&&reloaded.pending==0&&!strcmp(loaded.labels[0].name,"Pending edit"));assert(st_files_load(&reloaded,&loaded,0,true)==0&&reloaded.pending<0&&!strcmp(loaded.labels[0].name,"External edit"));library=loaded;
 size=(uint32_t)st_bank_encode(&library,0,bytes,sizeof(bytes));assert(size==ST_BANK_MAX);bytes[5]^=1;assert(put(restarted,2,st_file_name(0),bytes,size)==0);before=loaded;assert(st_files_load(&reloaded,&loaded,0,false)==ST_FILES_CORRUPT&&!memcmp(&before,&loaded,sizeof(loaded)));assert(st_files_begin(&reloaded,&loaded,0)==ST_FILES_CORRUPT);bytes[5]^=1;assert(put(restarted,2,st_file_name(0),bytes,size)==0);assert(st_files_load(&reloaded,&loaded,0,false)==0);
 /* A successful physical close followed by a reported close fault models
  * ambiguous descriptor ownership. Both production layers latch retention. */
 assert(st_files_begin(&reloaded,&loaded,0)==0);strcpy(loaded.labels[0].name,"Retained");assert(st_files_freeze(&reloaded,&loaded));fault=CLOSE;close_skip=1;assert(st_files_save(&reloaded)==RISC_APP_DATA_RETAINED&&reloaded.retained&&restarted.retained()&&retained_stage_close);prior=provider_calls;assert(st_files_save(&reloaded)==RISC_APP_DATA_RETAINED);assert(st_files_load(&reloaded,&loaded,0,true)==RISC_APP_DATA_RETAINED&&provider_calls==prior);
 puts("Spectrum / real Runtime AppDataFiles: two maximum banks, namespace isolation/global CAS, quota and injected ENOSPC retry, rename-before/after reconciliation, host backend restart, conflict/discard, read/corruption reservation and retained stage-close/no-further-I/O PASS");
}
