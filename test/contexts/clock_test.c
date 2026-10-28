/* Own actual training start and service-tick deadline consumers. */
#define UTILITY_CONTEXTS_DIRECT_TEST
#include "app_test.c"
static unsigned cheap_calls,clock_health_calls;
static bool cheap_ok=true;
static uint32_t timestamp;
static bool cheap(uint32_t*out){cheap_calls++;*out=timestamp;return cheap_ok;}
static bool clock_health(risc_runtime_health_v1*out){clock_health_calls++;out->uptime_ms=timestamp;return true;}
static void init_clock(risc_runtime_api_v1 *a){
 *a=app_runtime;a->monotonic_ms=cheap;a->health=clock_health;ctx_runtime=a;
 cs_client=(portable_contexts_client){0};cs_client.runtime=a;cs_client.api=&service.base;
 ctx_service=&service.base;cs_client.save_failed=true;cs_client.settings_valid=true;cs_terminal=false;native_alive=true;
 cu_page=CU_LEARN;cu_learning_saved=false;cu_kind=CR_PLACE;cu_learning=(contexts_learning_v1){0};
 ready=false;cheap_ok=true;timestamp=0;cheap_calls=clock_health_calls=0;
}
int main(void){
 risc_runtime_api_v1 a;init_clock(&a);
 assert(portable_contexts_training(true)&&cs_learning_at==0&&cheap_calls==1&&!clock_health_calls);
 timestamp=UINT32_MAX;assert(portable_contexts_training(true)&&cs_learning_at==UINT32_MAX);
 timestamp=0;assert(cs_service_tick()&&cu_learning.state!=CONTEXTS_LEARN_FAILED);
 /* Both the shared policy deadline and app learning deadline use the canonical
  * reader. A failed current clock must never fall back to successful health. */
 unsigned h=clock_health_calls,c=cheap_calls;cheap_ok=false;timestamp=123;
 assert(cs_service_tick()&&cu_learning.state==CONTEXTS_LEARN_FAILED&&!cs_client.foreground_learning);
 assert(cheap_calls==c+2&&clock_health_calls==h);
 cs_learning_at=42;h=clock_health_calls;assert(!portable_contexts_training(true)&&cs_learning_at==42&&clock_health_calls==h);
 init_clock(&a);a.monotonic_ms=NULL;timestamp=0;assert(portable_contexts_training(true)&&cs_learning_at==0&&clock_health_calls==1&&!cheap_calls);
 timestamp=UINT32_MAX-100;assert(portable_contexts_training(true));timestamp=100;
 assert(cs_service_tick()&&cu_learning.state!=CONTEXTS_LEARN_FAILED);
 timestamp=cs_learning_at+45000u;assert(cs_service_tick()&&cu_learning.state==CONTEXTS_LEARN_FAILED);
 init_clock(&a);a.struct_size=RISC_RUNTIME_MONOTONIC_V1_SIZE-1;timestamp=77;
 assert(portable_contexts_training(true)&&cs_learning_at==77&&clock_health_calls==1&&!cheap_calls);
 puts("Contexts owning training/deadline: cheap authority, old/null fallback, zero/wrap and failure behavior PASS");
}
