#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "AudioInputV1.h"
static const risc_runtime_api_v1 *runtime;
static bool live;static unsigned retains,acquires,releases,operations,boundaries;static int result;static bool bad_release;
static bool spectrum_domain_alive(void){return live;}
static void spectrum_domain_retain(void){if(live){retains++;live=false;}}
static bool spectrum_domain_storage_boundary(void){boundaries++;return live;}
#include "spectrum_scene_io.h"
#include "spectrum_scene_data_scope.h"
static int32_t stat_data(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;(void)n;assert(live);operations++;*s=4;*r=3;return result;}
static int32_t read_data(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*s,uint64_t*r){(void)c;(void)n;assert(live&&expected==3&&cap>=4);operations++;memcpy(b,"DATA",4);*s=4;*r=3;return result;}
static int32_t replace_data(void*c,const char*n,uint64_t expected,const void*b,uint32_t s){(void)c;(void)n;assert(live&&expected==3&&s==4&&!memcmp(b,"NEXT",4));operations++;return result;}
static risc_app_data_v1 data={1,sizeof(data),NULL,stat_data,read_data,replace_data};
static bool acquire(const char*n,uint32_t api,uint64_t instance,risc_runtime_capability_v1*g){assert(live&&api==1&&instance==2&&!strcmp(n,RISC_APP_DATA_CAPABILITY));acquires++;*g=(risc_runtime_capability_v1){sizeof(*g),1,2,&data};return true;}
static bool release(risc_runtime_capability_v1*g){assert(live&&g->api==&data);releases++;*g=(risc_runtime_capability_v1){.struct_size=bad_release?1:sizeof(*g)};return true;}
static risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;(void)k;(void)b;(void)cap;assert(live);operations++;*n=0;return result;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;assert(live);operations++;return result;}
static risc_key_value_v1 kv={2,sizeof(kv),NULL,get,put};
static void reset(void){runtime=&rt;live=true;retains=acquires=releases=operations=boundaries=0;result=0;bad_release=false;}
int main(void){(void)spectrum_guard_microphone;unsigned count=0;rf_app_data_scope scope;uint32_t size;uint64_t revision;char bytes[4];
 reset();const risc_app_data_v1*p=rf_data_bind(&scope,&rt,2);assert(!p->stat(p->context,"record",&size,&revision)&&size==4&&revision==3);assert(!p->read(p->context,"record",revision,bytes,4,&size,&revision)&&!memcmp(bytes,"DATA",4));assert(!p->replace(p->context,"record",revision,"NEXT",4));assert(acquires==3&&releases==3&&operations==3&&boundaries==3&&!retains);count++;
 for(unsigned i=0;i<3;i++){reset();p=rf_data_bind(&scope,&rt,2);result=i==0?RISC_APP_DATA_COMMIT_UNKNOWN:i==1?RISC_APP_DATA_STALE:RISC_APP_DATA_IO;assert(p->replace(p->context,"record",3,"NEXT",4)==result);assert(live&&!retains&&acquires==1&&releases==1);count++;}
 for(unsigned i=0;i<3;i++){reset();p=rf_data_bind(&scope,&rt,2);result=i==0?RISC_APP_DATA_CONTEXT:i==1?RISC_APP_DATA_RETAINED:-99;assert(p->stat(p->context,"record",&size,&revision)==RISC_APP_DATA_RETAINED);assert(!live&&retains==1&&!releases&&operations==1);assert(p->stat(p->context,"record",&size,&revision)==RISC_APP_DATA_RETAINED&&operations==1&&acquires==1);count++;}
 reset();p=rf_data_bind(&scope,&rt,2);scope.grant=(risc_runtime_capability_v1){sizeof(scope.grant),1,2,&data};assert(p->stat(p->context,"record",&size,&revision)==RISC_APP_DATA_RETAINED&&!acquires&&!operations&&!releases&&retains==1);count++;
 reset();p=rf_data_bind(&scope,&rt,2);bad_release=true;assert(p->stat(p->context,"record",&size,&revision)==RISC_APP_DATA_RETAINED&&retains==1&&releases==1);count++;
 for(unsigned i=0;i<2;i++){reset();result=i? -99:RISC_KEY_VALUE_CONTEXT;const risc_key_value_v1*q=spectrum_guard_storage(&kv);assert(q->get(q->context,"key",bytes,4,&size)==RISC_KEY_VALUE_CONTEXT&&retains==1&&operations==1);assert(q->put(q->context,"key",bytes,4)==RISC_KEY_VALUE_CONTEXT&&operations==1);count++;}
 reset();risc_runtime_capability_v1 g={.struct_size=1};assert(!spectrum_release(&g)&&retains==1&&!releases);count++;
 printf("Spectrum scoped storage %u cases PASS\n",count);return 0;}
