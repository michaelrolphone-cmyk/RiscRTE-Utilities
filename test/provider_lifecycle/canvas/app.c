#include "RiscRuntimeV1.h"
#include "RiscCanvasV1.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
extern void canvas_runtime_complete(void);
extern unsigned canvas_runtime_case(void);
extern bool canvas_runtime_done(void);
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);
 if(canvas_runtime_done())return;
 const unsigned scenario=canvas_runtime_case();
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};assert(r->acquire("ui.canvas",1,0,&grant));
 const risc_canvas_api_v1*a=risc_canvas_get_v1(grant.api,sizeof(risc_canvas_api_v1));assert(a);
 risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_MONO1,RISC_CANVAS_RESPONSIVE,0,0};uint64_t session=0;assert(!a->open(a->context,&request,&session));
 for(unsigned iteration=0;iteration<3;iteration++){
  risc_canvas_frame_v1 frame={.struct_size=sizeof(frame)};assert(!a->begin_frame(a->context,session,&frame));
  if(scenario==1){assert(!a->cancel_frame(a->context,session,frame.lease));assert(!a->close(a->context,session));assert(r->release(&grant));canvas_runtime_complete();return;}
  memset(frame.pixels,iteration%2?0:255,frame.byte_count);uint64_t receipt=0;assert(!a->submit(a->context,session,frame.lease,&receipt));
  if(scenario==3){assert(!r->release(&grant));if(risc_runtime_get_api(1)==r)assert(r->retain_invocation());return;}
  if(scenario==4){assert(a->quiesce(a->context,session)==RISC_CANVAS_RETAINED);assert(r->retain_invocation());return;}
  if(scenario==2)assert(r->exit_requested());
  assert(a->quiesce(a->context,session)==RISC_CANVAS_AGAIN);
  risc_canvas_receipt_v1 status={.struct_size=sizeof(status)};
  unsigned polls=0;
  do{if(++polls>1000){fprintf(stderr,"canvas presentation did not settle after 1000 fixture ticks\n");assert(0);}
   r->yield_ms(1);assert(!a->poll(a->context,session,receipt,&status));}while(status.state==RISC_CANVAS_QUEUED||status.state==RISC_CANVAS_PRESENTING);
  assert(status.state==RISC_CANVAS_COMPLETE);assert(!a->forget_receipt(a->context,session,receipt));assert(!a->quiesce(a->context,session));assert(!a->resume(a->context,session));
  if(scenario==2)break;
 }
 assert(!a->close(a->context,session));assert(r->release(&grant));canvas_runtime_complete();
}
