/* Separate translation unit preserves each producer's independent ABI headers. */
#define WATCH_ALARM_SLEEP_RESUME 1
#include "apps/clock/watch_alarm_sleep.h"
unsigned test_sleep_stage, test_sleep_calls;
int32_t test_sleep_detail;
static bool yes(void*c){(void)c;return true;}
static bool no_wake(void*c,bool*p){(void)c;*p=false;return true;}
static bool log_line(const char*s){(void)s;return true;}
static int32_t light(void*c,uint32_t duration,risc_light_sleep_result_v1*out){
    (void)c;(void)duration;++test_sleep_calls;out->wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO;return RISC_LIGHT_SLEEP_OK;
}
static int32_t untimed(void*c,risc_light_sleep_result_v1*out){return light(c,0,out);}
static const risc_display_power_api_v1 panel={.api_version=1,.struct_size=sizeof(panel),.prepare_sleep=yes,.resume=yes};
static const risc_power_sleep_api_v1 power={.api_version=1,.struct_size=sizeof(power),.prepare_sleep=yes,.resume=yes,.light_sleep=untimed,.light_sleep_for=light,.sleep_wake_pending=no_wake};
int test_watch_sleep(const alarm_service_v1*service){
    (void)watch_sleep_prepared;
    int result=watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_LIGHT,service,log_line);
    test_sleep_stage=watch_sleep_stage;test_sleep_detail=watch_sleep_detail;return result;
}
