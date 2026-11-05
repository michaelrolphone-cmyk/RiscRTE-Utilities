/* Exercise the actual hash-checked, materialized custody wrapper, not a copy. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRealtimeV1.h"
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_CHECKED_STORAGE_RUNTIME
#define PORTABLE_ALARM_TERMINAL_RETENTION
#define PORTABLE_RF_SHARED_CANVAS
#define PORTABLE_PERF_KV_READS 0
#define portable_perf_count(x) ((void)(x))
static const risc_runtime_api_v1 *native_custody_runtime;
static bool native_custody_retained,failed;
static void *gauge;
static risc_display_surface_v1 surface,native_custody_surface;
static void portable_stage_log(const risc_runtime_api_v1 *r,const char *a,const char *b){(void)r;(void)a;(void)b;}
static bool portable_paper_frame_drain(void){return true;}
static unsigned retained,acquires,releases,health_calls;
static int fault;
static bool changed;
static risc_runtime_capability_v1 *last_grant;
void portable_adapter_retain(void);
static int token;
static bool keep(void){retained++;return true;}
static bool health(risc_runtime_health_v1 *out){
 assert(out&&out->struct_size==sizeof(*out));health_calls++;
 if(fault==8)changed=true;
 if(fault==15)portable_adapter_retain();
 if(fault==16)last_grant->slot=7;
 return fault!=7;
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 (void)name;(void)version;(void)instance;acquires++;last_grant=g;
 switch(fault){
 case 1:g->api=&token;break;
 case 2:g->slot=1;break;
 case 3:g->generation=1;break;
 case 4:g->struct_size--;break;
 case 5:g->struct_size++;break;
 case 6:changed=true;break;
 case 14:*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};break;
 case 10:g->api=&token;g->slot=3;g->generation=4;return true;
 default:break;
 }
 return false;
}
static bool release(risc_runtime_capability_v1 *g){
 releases++;if(fault==11)return false;
 if(fault==12)return true;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),
 .health=health,.acquire=acquire,.release=release,.retain_invocation=keep};
static const risc_runtime_api_v1 other={.api_version=1,.struct_size=sizeof(other)};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?(changed?&other:&runtime):NULL;}
#include "native_custody_adapter.inc"
static void reset(int mode){
 fault=mode;runtime.health=mode==9?NULL:health;changed=false;retained=acquires=releases=health_calls=0;
 native_custody_retained=failed=false;native_custody_runtime=&runtime;
}
static void refusal(const char *name,uint32_t version,uint64_t instance,int mode,bool expect_retained){
 reset(mode);risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 assert(!custody_acquire(name,version,instance,&g));
 assert(acquires==1&&retained==(unsigned)expect_retained&&native_custody_retained==expect_retained);
 assert(!releases);
 if(expect_retained){
  unsigned before=acquires;
  assert(!custody_acquire(name,version,instance,&g)&&acquires==before);
  assert(!custody_release(&g)&&!releases);
 }
}
int main(void){
 assert(settings_runtime.api_version==0); /* Focused test invokes the wrapper directly. */
 assert(sizeof(sdr_optional_requests)/sizeof(sdr_optional_requests[0])==3);
 assert(sdr_optional_acquire_request("display.output",1,0));
 assert(sdr_optional_acquire_request("ui.canvas-input",1,0));
 assert(sdr_optional_acquire_request("alarm.service.facade",2,0));
 for(unsigned i=0;i<sizeof(sdr_optional_requests)/sizeof(sdr_optional_requests[0]);i++){
  const sdr_optional_request *r=&sdr_optional_requests[i];
  refusal(r->name,r->version,r->instance,0,false);assert(health_calls==1);
  for(int mode=1;mode<=9;mode++)refusal(r->name,r->version,r->instance,mode,true);
  refusal(r->name,r->version,r->instance,15,true);
  refusal(r->name,r->version,r->instance,16,true);
  for(int dirty=1;dirty<=5;dirty++){
   reset(14);risc_runtime_capability_v1 pre={.struct_size=sizeof(pre)};
   if(dirty==1)pre.api=&token;
   if(dirty==2)pre.slot=1;
   if(dirty==3)pre.generation=1;
   if(dirty==4)pre.struct_size--;
   if(dirty==5)pre.struct_size++;
   assert(!custody_acquire(r->name,r->version,r->instance,&pre)&&retained==1);
   assert(!health_calls&&!releases);
  }
  refusal(r->name,r->version+1,r->instance,0,true);
  refusal(r->name,r->version,r->instance+1,0,true);
  reset(10);risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
  assert(custody_acquire(r->name,r->version,r->instance,&g)&&g.api&&!retained);
  assert(custody_release(&g)&&!g.api&&!g.slot&&!g.generation&&!retained&&releases==1);
  for(int mode=11;mode<=12;mode++){
   reset(10);g=(risc_runtime_capability_v1){.struct_size=sizeof(g)};
   assert(custody_acquire(r->name,r->version,r->instance,&g));
   fault=mode;assert(!custody_release(&g)&&retained==1&&g.api&&g.slot&&g.generation);
  }
 }
 const char *unchanged[]={"ui.canvas.raster","telemetry.broadcast","net.wifi","bluetooth.hci",
                         "alarm.service","display.output.extra","ui.canvas-input.extra","unknown"};
 for(unsigned i=0;i<sizeof(unchanged)/sizeof(unchanged[0]);i++)refusal(unchanged[i],1,0,0,true);
 puts("PASS: generated custody exact optional absence, dirty grants, authority loss and release failures");
 return 0;
}
