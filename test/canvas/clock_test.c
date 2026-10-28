/* Own the production now() fence and failure behavior, not a reimplementation. */
#include <assert.h>
#include "../../lib/CanvasApp/canvas.c"
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?rt:NULL;}
static unsigned cheap_calls,health_calls;
static bool cheap_ok=true,health_ok=true;
static uint32_t timestamp;
static bool cheap(uint32_t *out){cheap_calls++;*out=timestamp;return cheap_ok;}
static bool health_clock(risc_runtime_health_v1 *out){health_calls++;out->uptime_ms=timestamp;return health_ok;}
int main(void){
 risc_runtime_api_v1 api={.api_version=1,.struct_size=sizeof(api),.health=health_clock,.monotonic_ms=cheap};rt=&api;
 timestamp=0;assert(now()==0&&!exiting&&cheap_calls==1&&!health_calls);
 timestamp=UINT32_MAX;assert(now()==UINT32_MAX);timestamp=0;assert(now()==0);
 clock_last=19;cheap_ok=false;timestamp=99;assert(now()==19&&exiting&&!health_calls);
 unsigned calls=cheap_calls;retained=true;exiting=false;assert(now()==19&&cheap_calls==calls&&!health_calls&&!exiting);
 retained=false;rt=NULL;assert(now()==19&&cheap_calls==calls&&!health_calls&&!exiting);rt=&api;
 api.monotonic_ms=NULL;timestamp=0;assert(now()==0&&health_calls==1);
 health_ok=false;clock_last=21;assert(now()==21&&exiting&&health_calls==2);
 health_ok=true;exiting=false;api.monotonic_ms=cheap;api.struct_size=offsetof(risc_runtime_api_v1,health)+sizeof(api.health);
 timestamp=UINT32_MAX;assert(now()==UINT32_MAX&&!exiting&&health_calls==3&&cheap_calls==calls);
 puts("Canvas owning now: zero/wrap, prefix/null fallback, no retry, last-time failure and custody PASS");
}
