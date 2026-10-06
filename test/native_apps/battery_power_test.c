/* Real app + shared production adapter; only read-only hardware is modeled. */
#include "PortablePowerStatus.h"
#define portable_app_alarm_sleep unused_alarm_sleep
#define main unused_capture_main
#define risc_runtime_get_api unused_runtime_api
#include "nova_peripherals.h"
#undef risc_runtime_get_api
#undef main
#undef portable_app_alarm_sleep
static unsigned scenario, sample_reads, launch_calls, sleep_calls, reads_at_sleep;
static bool first_failed, recovered, saw_complete, saw_charging;
static bool test_read(void *c,risc_battery_sample_v1 *s) {
    (void)c;sample_reads++;*s=(risc_battery_sample_v1){3970,42,
        PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT|PORTABLE_POWER_CHARGER_ENABLED};
    switch(scenario) {
      case 0: s->flags|=PORTABLE_POWER_INPUT_READY|RISC_BATTERY_CHARGING;break;
      case 1: s->percent=100;s->flags|=PORTABLE_POWER_INPUT_READY|PORTABLE_POWER_CHARGE_DONE;break;
      case 2: s->percent=20;break;
      case 3: s->percent=10;break;
      case 4: s->percent=0;break;
      case 5: s->percent=255;s->flags|=RISC_BATTERY_PROFILE_MISSING;break;
      case 6: s->flags|=PORTABLE_POWER_INPUT_READY|PORTABLE_POWER_THERMAL_LIMIT;break;
      case 7: s->flags&=~PORTABLE_POWER_CHARGER_ENABLED;s->flags|=PORTABLE_POWER_INPUT_READY;break;
      case 8: s->percent=255;s->flags&=~PORTABLE_POWER_BATTERY_PRESENT;s->flags|=RISC_BATTERY_PROFILE_MISSING;break;
      case 9: s->flags=0;break; /* Older provider: no invented USB/protection state. */
      case 10: s->flags|=PORTABLE_POWER_INPUT_READY;break;
      case 11: *s=(risc_battery_sample_v1){4200,100,255};return false;
      case 12:
        if(polls<40){s->flags|=PORTABLE_POWER_INPUT_READY|RISC_BATTERY_CHARGING;saw_charging=true;}
        else if(polls<100){*s=(risc_battery_sample_v1){4200,100,255};first_failed=true;return false;}
        else if(polls<140){s->flags|=PORTABLE_POWER_INPUT_READY|PORTABLE_POWER_CHARGE_DONE;saw_complete=true;}
        else {recovered=true;s->percent=39;}
        break;
      case 13: s->percent=100;break; /* Gauge100 never establishes charger done. */
      default: break;
    }
    return true;
}
static const risc_battery_gauge_api_v1 test_gauge={1,sizeof(test_gauge),NULL,test_read};
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g) {
    if(!strcmp(n,"board.battery")&&v==1){g->api=&test_gauge;grants++;return true;}
    return fake_acquire(n,v,id,g);
}
static bool test_launch(const char *s) {
    assert(!strcmp(s,"springboard.elf"));launch_calls++;
    if(scenario==15 && launch_calls==1)return false;
    return fake_launch(s);
}
static const risc_runtime_api_v1 test_runtime={1,sizeof(test_runtime),fake_health,fake_yield,fake_diag,test_launch,test_acquire,fake_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&test_runtime:NULL;}
#include NOVA_APP_SOURCE
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
                             const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
    (void)r;(void)d;(void)b;(void)a;
    assert(scenario>=16 && !frames && !subs && power_details && power_detail_page==0);
    sleep_calls++;reads_at_sleep=sample_reads;ticks+=3100;
    return scenario==17?0:scenario==18?-1:scenario==19?-2:1;
}
static void tap(unsigned at,int x,int y) {actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
int main(int argc,char**argv) {
    assert(argc==3);directory=argv[1];scenario=(unsigned)atoi(argv[2]);
    stop_poll=scenario>=16?2000:200;memset(pixels,0xa5,sizeof(pixels));
    if(scenario>=16)tap(10,50,212);
    if(scenario==14) {
        /* Open all detail pages, repeated next/previous, refresh, nested Back,
         * reopen, back, then final app return. No setting writes. */
        tap(10,50,212);tap(20,200,212);tap(30,200,212);tap(40,200,212);
        tap(50,200,212);tap(60,30,212);tap(70,120,212);tap(80,30,26);
        tap(90,50,212);tap(100,30,26);tap(110,30,26);
    } else if(scenario==15) {tap(10,30,26);tap(30,30,26);}
    assert(app_module_init()==0);app_main();
    const char *expected[]={"CHARGING","CHARGE COMPLETE","LOW BATTERY","BATTERY CRITICAL","BATTERY CRITICAL","BATTERY STATUS","THERMAL LIMIT","CHARGE DISABLED","NO BATTERY","BATTERY STATUS","USB POWER","READ ERROR","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS"};
    assert(!strcmp(power_title(),expected[scenario]));
    if(scenario==11)assert(!power_read_ok && power_sample.percent==255 && power_sample.millivolts==0 && power_sample.flags==RISC_BATTERY_PROFILE_MISSING);
    if(scenario==12)assert(first_failed&&recovered&&saw_complete&&saw_charging&&power_read_ok&&power_sample.percent==39);
    if(scenario==13)assert(!power_flag(PORTABLE_POWER_CHARGE_DONE));
    if(scenario==9)assert(!power_extended()&&!strcmp(power_yes_no(PORTABLE_POWER_INPUT_READY),"Unknown"));
    if(scenario==14)assert(!power_details && launch_calls==1 && polls<stop_poll+2);
    if(scenario==15)assert(power_navigation_failed && launch_calls==2);
    if(scenario>=16){assert(sleep_calls==1 && power_details && power_detail_page==0);if(scenario>=18)assert(sample_reads==reads_at_sleep);}

    assert(sample_reads>=1 && sample_reads<=ticks/1000+4);
    if(scenario<11 || scenario==13)assert(presents==1); /* No continuous animations. */
    for(unsigned i=0;i<32;i++)assert(!cells[i].size); /* No data mutation. */
    app_module_fini();
    if(scenario==19)assert(grants && !frames && !subs); /* Native retained state keeps its grants. */
    else assert(!grants&&!frames&&!subs);
    printf("Power production UI case%u: %u reads, %u frames, bounded retry, stride and cleanup PASS\n",scenario,sample_reads,presents);
}
