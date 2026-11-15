/* Production native Stopwatch/controller/client boundary; Runtime providers are
 * controlled doubles. No RTC, alarm or toolbar is used by this profile. */
#define PORTABLE_STOPWATCH_NATIVE_UTC
#define PORTABLE_STOPWATCH_NATIVE_STANDALONE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_APP_LAUNCH_GUARD
#include "../../Apps/stopwatch.c"
#include <assert.h>
#include <stdio.h>
static bool retained;
static unsigned reads,acquisitions,releases;
static int read_result;static uint32_t valid=RISC_REALTIME_VALID;static bool release_failed;
bool portable_adapter_retained(void){return retained;}
void portable_adapter_retain(void){retained=true;}
static int32_t sample(void*c,risc_realtime_snapshot_v1*s){assert(c&&!retained);reads++;if(read_result)return read_result;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=valid,.epoch_seconds=valid?1791115200:0};return 0;}
static const risc_realtime_api_v1 table={1,sizeof(table),&retained,sample};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!retained&&!strcmp(n,"runtime.realtime")&&v==1&&!id);acquisitions++;g->api=&table;g->slot=1;g->generation=1;return true;}
static bool release(risc_runtime_capability_v1*g){assert(!retained);releases++;if(release_failed)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_runtime_api_v1 rt;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&rt:NULL;}
const t5_app_api_v1*t5_app_get_api(uint32_t v){(void)v;return NULL;}
int main(int argc,char**argv){(void)argc;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};runtime=&rt;
 if(!strcmp(argv[1],"unset"))valid=RISC_REALTIME_UNSET;
 if(!strcmp(argv[1],"io"))read_result=RISC_REALTIME_IO;
 if(!strcmp(argv[1],"context"))read_result=RISC_REALTIME_CONTEXT;
 if(!strcmp(argv[1],"release"))release_failed=true;
 uint32_t seconds=777;bool ok=utc_now(&seconds);
 assert(ok==(!read_result&&valid==RISC_REALTIME_VALID&&!release_failed));assert(reads==1&&acquisitions==1);
 assert(retained==(read_result==RISC_REALTIME_CONTEXT||release_failed));
 assert(releases==(read_result==RISC_REALTIME_CONTEXT?0u:1u));
 if(ok)assert(seconds==1791115200u-946684800u);else assert(seconds==777);
 puts("PASS native standalone Stopwatch: real UTC reader, unset/error/retained release, zero RTC/alarm authority");
}
