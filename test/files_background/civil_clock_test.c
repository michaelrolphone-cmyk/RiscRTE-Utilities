/* Reuse canonical custody doubles and test provider-owned RTC interpretation. */
#define main original_controller_tests
#include "controller_test.c"
#undef main
static int civil_rc;static bool civil_dirty,civil_replace,raw_dirty,raw_ok,civil_present;
static unsigned civil_reads,raw_reads;static int64_t civil_epoch;
static risc_civil_clock_policy_v1 civil_policy;
static bool raw_read(void*c,risc_rtc_calendar_v2*out){
 (void)c;touch();raw_reads++;
 if(raw_ok||raw_dirty)*out=(risc_rtc_calendar_v2){2026,raw_ok?11:0,4,3,17,30,0};
 return raw_ok;
}
static int32_t civil_read_unused(void*c,risc_civil_time_v1*out){(void)c;(void)out;assert(0);return -1;}
static int32_t project_unused(void*c,int64_t epoch,uint32_t ns,risc_civil_calendar_v1*out){(void)c;(void)epoch;(void)ns;(void)out;assert(0);return -1;}
static int32_t interpret(void*c,const risc_civil_calendar_v1*in,int64_t*out){
 (void)c;touch();civil_reads++;assert(in->year==2026&&in->hour==17&&in->minute==30&&!in->zone[0]);
 assert(releases>=1); /* RTC is closed before the policy callback. */
 if(civil_replace)current=&other;
 if(!civil_rc||civil_dirty)*out=civil_epoch;
 return civil_rc;
}
static bool with_civil(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(strcmp(n,RISC_CIVIL_CLOCK_CAPABILITY))return acquire(n,v,id,g);
 touch();assert(v==1&&!id);
 if(!civil_present)return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.api=&civil_policy,.slot=81,.generation=1};return true;
}
static void setup(void){
 reset(false);assert(init());rt.acquire=with_civil;rtc_present=true;
 rtc_api.base.struct_size=sizeof(rtc_api.base);rtc_api.base.read=raw_read;
 civil_policy=(risc_civil_clock_policy_v1){.api_version=1,.struct_size=sizeof(civil_policy),.read=civil_read_unused,.policy_version=1,.policy_tag=RISC_CIVIL_CLOCK_POLICY_TAG,.project=project_unused,.interpret_rtc=interpret};
 civil_rc=0;civil_dirty=civil_replace=raw_dirty=false;raw_ok=civil_present=true;
 civil_epoch=1800000000;civil_reads=raw_reads=0;
}
int main(void){uint64_t utc;unsigned before;
 setup();assert(files_background_clock(&rt,&utc)&&utc==1800000000&&civil_reads==1&&!realtime_acquires);
 setup();civil_rc=-1;clock_present=true;assert(files_background_clock(&rt,&utc)&&!utc&&!realtime_acquires);
 setup();civil_rc=-9;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);
 setup();civil_rc=-1;civil_dirty=true;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);
 setup();civil_replace=true;assert(!files_background_clock(&rt,&utc)&&retains);
 setup();civil_policy.policy_tag=0;assert(!files_background_clock(&rt,&utc)&&retains&&!civil_reads&&!realtime_acquires);
 setup();civil_policy.struct_size=offsetof(risc_civil_clock_policy_v1,policy_tag)+sizeof(uint32_t);assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);
 setup();civil_policy.interpret_rtc=NULL;assert(!files_background_clock(&rt,&utc)&&retains&&!realtime_acquires);
 setup();civil_present=false;clock_present=true;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&realtime_acquires==1);
 setup();civil_policy.struct_size=sizeof(risc_civil_clock_api_v1);clock_present=true;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&!civil_reads&&realtime_acquires==1);
 setup();raw_ok=false;clock_present=true;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&!civil_reads);
 /* Delivered legacy RTC fills a calendar and then reports invalid time.
  * Ignore those fields; release RTC and use only an available fallback. */
 setup();raw_ok=false;raw_dirty=true;assert(files_background_clock(&rt,&utc)&&!utc&&!retains&&!civil_reads&&realtime_acquires==1);
 setup();raw_ok=false;raw_dirty=true;clock_present=true;assert(files_background_clock(&rt,&utc)&&utc==1800000000&&!retains&&!civil_reads&&realtime_acquires==1);
 setup();rtc_api.base.struct_size=sizeof(rtc_api);rtc_ok=false;assert(files_background_clock(&rt,&utc)&&!utc&&!raw_reads&&!civil_reads&&!realtime_acquires);
 setup();rtc_api.base.struct_size=sizeof(rtc_api);rtc_api.utc_version=9;assert(!files_background_clock(&rt,&utc)&&retains&&!raw_reads&&!civil_reads&&!realtime_acquires);
 before=calls;assert(!files_background_clock(&rt,&utc)&&calls==before);
 puts("Background civil RTC interpretation: 15 authority, absence, error and custody cases PASS");return 0;
}
