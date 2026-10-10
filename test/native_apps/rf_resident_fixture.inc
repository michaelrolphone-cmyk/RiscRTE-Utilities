/* Runtime dispatch is simulated; the controller and shared adapter execute
 * unchanged with the exact resident target build flags and dependencies. */
static int32_t fx_resident_checkpoint(uint64_t invocation,const risc_resident_request_v1 *q,risc_resident_reply_v1 *r){
 fx_io();assert(invocation==2&&q->struct_size==sizeof(*q)&&r->struct_size==sizeof(*r));
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){
  /* Live RF and retry waits must be gated locally before Runtime dispatch. */
  assert(!portable_radio_capture_active());++fx_resident_polls;
  if((uint32_t)(fx_ticks-fx_policy_at)>=5000)r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;
 }
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY){assert(!running&&!owned&&!capture_requested);++fx_resident_policies;fx_policy_at=fx_ticks;}
 return RISC_RESIDENT_OK;
}
static bool fx_resident_get(risc_resident_client_v1 *out){*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=2,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=fx_resident_checkpoint};return true;}
static int32_t fx_resident_time(void*c,risc_realtime_snapshot_v1 *s){(void)c;fx_io();*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=1791584000+fx_ticks/1000,.monotonic_before_us=(uint64_t)fx_ticks*1000,.monotonic_after_us=(uint64_t)fx_ticks*1000};return RISC_REALTIME_OK;}
static const risc_realtime_api_v1 fx_resident_clock={1,sizeof(fx_resident_clock),NULL,fx_resident_time};
static const alarm_service_descriptor_v2 fx_resident_alarm={.base={2,sizeof(fx_resident_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
#define TEST_RESIDENT_IO() fx_io()
#include "resident_telemetry_fixture.h"
static bool fx_resident_retain(void){assert(!"Unexpected terminal custody");return false;}
static bool fx_resident_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 fx_io();g->slot=1;g->generation=1;
 if(!strcmp(n,"runtime.realtime")){g->api=&fx_resident_clock;fx_grants++;return true;}
 if(!strcmp(n,"telemetry.broadcast")){g->api=&resident_bt;fx_grants++;return true;}
 if(!strcmp(n,"alarm.service")){assert(v==2);g->api=&fx_resident_alarm;fx_grants++;return true;}
 if(!strcmp(n,"input.navigation")){g->api=&fx_nav_api;fx_grants++;return true;}
 return fx_acquire(n,v,id,g);
}
static const risc_runtime_api_v1 fx_runtime={.api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,.yield_ms=fx_yield,.diagnostic=fx_diag,.request_launch=fx_launch,.acquire=fx_resident_acquire,.release=fx_release,.resident_shell=fx_resident_get,.retain_invocation=fx_resident_retain};
