#define PORTABLE_SPECTRUM_STAGE_TRACE
#include "../../Apps/spectrum_stage_trace.h"
#include <assert.h>
static unsigned writes,logs;static int result;static char copied[577];
static int32_t write_copy(uint64_t invocation,const char *text,uint32_t n){assert(invocation==23&&n<=576);writes++;memcpy(copied,text,n);copied[n]=0;return result;}
static bool get_client(risc_diagnostic_checkpoint_client_v1 *out){assert(out->struct_size==sizeof(*out));*out=(risc_diagnostic_checkpoint_client_v1){1,sizeof(*out),23,write_copy};return true;}
static bool log_line(const char *text){assert(strlen(text)<96);logs++;return true;}
int main(void){
 risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.diagnostic=log_line,.diagnostic_checkpoint_client=get_client};
 spectrum_stage_init(&rt,"AUDIO");assert(spectrum_stage_take(false)&&spectrum_stage_take(false)&&!spectrum_stage_take(false));assert(spectrum_stage_take(true)&&spectrum_stage_take(true)&&!spectrum_stage_take(true));
 for(unsigned i=0;i<9;i++)spectrum_stage_mark("capture-before",i,256,228);
 assert(writes==9&&logs==9&&!strstr(copied,"n=3 ")&&strstr(copied,"n=4 ")&&strstr(copied,"n=9 "));
 spectrum_stage_arm();assert(spectrum_stage_take(true)&&spectrum_stage_take(false));
 result=-1;spectrum_stage_mark("submit-before",1,0,228);assert(writes==10&&logs==9);
 spectrum_stage_mark("submit-after",1,0,228);assert(writes==10&&logs==9);
 rt.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;spectrum_stage_init(&rt,"RF");spectrum_stage_mark("alloc-before",123,1,0);assert(writes==10&&logs==10);
 puts("PASS bounded copied trace ring, sampling budgets, legacy fallback and denied-write stop");
}
