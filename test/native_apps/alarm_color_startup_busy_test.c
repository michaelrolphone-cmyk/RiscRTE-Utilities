#define PORTABLE_RTC_UTC8_DENVER
#define ALARM_VOLUME_CONTROL
#define ALARM_DND_CONTROL
#define ALARM_CLOCK_CHANGE_CONTROL
#ifndef TEST_BASELINE
#define ALARM_STARTUP_BUSY_RETRY
#endif
#define main prior_points_fixture_main
#include "points_service_test.c"
#undef main
static int32_t injected;
static unsigned attempts;
static int32_t busy_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n) {
    ++attempts;if(injected){*n=0;return injected;}return get_blob(c,k,b,cap,n);
}
static const risc_bound_key_value_v1 busy_bound={1,sizeof(busy_bound),NULL,busy_get,put_blob};
static void fresh(void){boot(true);kv=&busy_bound;injected=0;attempts=0;rtc_base=civil(2026,10,14,2,13);}
int main(void){
 const phase_t phases[]={LOAD_ALARM,LOAD_TIMER,LOAD_MODE,LOAD_VOLUME,LOAD_DND,LOAD_ALARM_OCC,LOAD_TIMER_OCC,LOAD_POINTS_CFG,LOAD_POINTS_OCC};
 for(unsigned i=0;i<sizeof(phases)/sizeof(*phases);i++){
  fresh();for(unsigned n=0;phase!=phases[i]&&n<40;n++)pump(1);assert(phase==phases[i]&&!anchored);
  injected=RISC_BOUND_KEY_VALUE_BUSY;
#ifdef TEST_BASELINE
  pump(1);assert(phase==BLOCKED&&error==ALARM_STORAGE);assert(!put_calls&&!opens&&!effects);
#else
  unsigned before=attempts;for(unsigned n=0;n<12;n++){assert(client->step(NULL)==ALARM_PENDING);assert(phase==phases[i]&&!error&&!anchored);}
  assert(attempts==before+12&&!put_calls&&!opens&&!effects);
  injected=0;for(unsigned n=0;n<80&&phase!=IDLE;n++)pump(1);if(phase!=IDLE||error)fprintf(stderr,"load=%u phase=%d error=%d active=%d selected=%u\n",i,phase,error,active,selected);assert(phase==IDLE&&!error);
#endif
 }
#ifndef TEST_BASELINE
 fresh();injected=RISC_BOUND_KEY_VALUE_BUSY;assert(client->step(NULL)==ALARM_PENDING);injected=RISC_BOUND_KEY_VALUE_IO;pump(1);assert(phase==BLOCKED&&error==ALARM_STORAGE);injected=0;pump(10);assert(phase==BLOCKED);assert(client->refresh(NULL)==ALARM_PENDING);for(unsigned n=0;n<80&&phase!=IDLE;n++)pump(1);assert(phase==IDLE);
#endif
 assert(driver_api->quiesce());puts("Color alarm: nine startup read phases, bounded BUSY deferral, genuine IO block and explicit Retry PASS");return 0;
}
