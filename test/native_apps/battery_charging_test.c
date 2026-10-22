/* Charging-specific cross-state coverage. Production app/adapter are unmodified. */
#include "PortablePowerStatus.h"
#define portable_app_alarm_sleep unused_alarm_sleep
#define main unused_capture_main
#define risc_runtime_get_api unused_runtime_api
#include "nova_peripherals.h"
#undef risc_runtime_get_api
#undef main
#undef portable_app_alarm_sleep
static unsigned scenario, sample_reads, launch_calls, detail_pages;
static bool saw_charging, saw_error, saw_complete, saw_usb;
static bool test_read(void *c, risc_battery_sample_v1 *s);
static const risc_battery_gauge_api_v1 test_gauge={1,sizeof(test_gauge),NULL,test_read};
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g) {
    if(!strcmp(n,"board.battery")&&v==1){g->api=&test_gauge;grants++;return true;}
    return fake_acquire(n,v,id,g);
}
static bool test_launch(const char *s) {
    assert(!strcmp(s,"springboard.elf"));launch_calls++;
    if(scenario==13 && launch_calls==1)return false;
    return fake_launch(s);
}
static const risc_runtime_api_v1 test_runtime={
    .api_version=1,.struct_size=sizeof(test_runtime),.health=fake_health,
    .yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=test_launch,
    .acquire=test_acquire,.release=fake_release
};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&test_runtime:NULL;}
#include NOVA_APP_SOURCE
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
                            const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
    (void)r;(void)d;(void)b;(void)a;assert(!"Unexpected sleep in short charging test");return 0;
}
static bool test_read(void *c,risc_battery_sample_v1 *s) {
    (void)c;sample_reads++;
    if(power_details)detail_pages|=1u<<power_detail_page;
    *s=(risc_battery_sample_v1){3970,42,
      PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT|
      PORTABLE_POWER_CHARGER_ENABLED|PORTABLE_POWER_INPUT_READY};
    switch(scenario) {
      case 0: case 11: case 13:s->flags|=RISC_BATTERY_CHARGING;break;
      case 1:s->percent=97;s->flags|=PORTABLE_POWER_CHARGE_DONE;break;
      case 2:s->percent=100;break;
      case 3:s->percent=255;s->flags|=RISC_BATTERY_CHARGING|RISC_BATTERY_PROFILE_MISSING;break;
      case 4:s->percent=255;s->flags|=PORTABLE_POWER_CHARGE_DONE|RISC_BATTERY_PROFILE_MISSING;break;
      case 5:s->flags&=~PORTABLE_POWER_CHARGER_ENABLED;break;
      case 6:s->flags|=PORTABLE_POWER_THERMAL_LIMIT;break;
      case 7:s->percent=255;s->flags&=~PORTABLE_POWER_BATTERY_PRESENT;s->flags|=RISC_BATTERY_PROFILE_MISSING;break;
      case 8:s->flags=RISC_BATTERY_CHARGING;break;
      case 9:*s=(risc_battery_sample_v1){4200,100,255};return false;
      case 10:
        if(script_step()<40){s->flags|=RISC_BATTERY_CHARGING;saw_charging=true;}
        else if(script_step()<80){*s=(risc_battery_sample_v1){4200,100,255};saw_error=true;return false;}
        else if(script_step()<120){s->percent=97;s->flags|=PORTABLE_POWER_CHARGE_DONE;saw_complete=true;}
        else {saw_usb=true;}
        break;
      case 12:s->flags=PORTABLE_POWER_CHARGE_DONE|PORTABLE_POWER_THERMAL_LIMIT|PORTABLE_POWER_INPUT_READY;break;
      default:assert(!"Unknown scenario");
    }
    return true;
}
static void tap(unsigned at,int x,int y) {actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
int main(int argc,char **argv) {
    assert(argc==3);directory=argv[1];scenario=(unsigned)atoi(argv[2]);assert(scenario<14);
    stop_poll=240;memset(pixels,0xa5,sizeof(pixels));
    if(scenario==11){
      /* Exercise each Details page and refresh while charging remains true. */
      tap(10,50,212);tap(20,120,212);
      tap(30,200,212);tap(40,120,212);
      tap(50,200,212);tap(60,120,212);
      tap(70,200,212);tap(80,120,212);
      tap(90,200,212);tap(100,30,212);tap(110,30,26);
      tap(120,160,212);tap(130,50,212);tap(140,30,26);tap(150,30,26);
    }else if(scenario==13){tap(10,30,26);tap(30,30,26);}
    assert(app_module_init()==0);app_main();
    static const char *titles[]={"CHARGING","CHARGE COMPLETE","USB POWER","CHARGING","CHARGE COMPLETE","CHARGE DISABLED","THERMAL LIMIT","NO BATTERY","CHARGING","READ ERROR","USB POWER","CHARGING","BATTERY STATUS","CHARGING"};
    assert(!strcmp(power_title(),titles[scenario]));
    if(scenario==0||scenario==3||scenario==8||scenario==11)assert(!strcmp(power_caption(),"PMIC reports charging"));
    if(scenario==1||scenario==4){assert(!strcmp(power_caption(),"Charger reports done"));assert(power_color()==POWER_GREEN);}
    if(scenario==2)assert(!power_flag(PORTABLE_POWER_CHARGE_DONE));
    if(scenario==3||scenario==4||scenario==7||scenario==9)assert(!power_percent_known());
    if(scenario==6){assert(!strcmp(power_caption(),"PMIC limiting charge"));assert(power_color()==POWER_AMBER);}
    if(scenario==8||scenario==12){assert(!power_extended());assert(!strcmp(power_yes_no(PORTABLE_POWER_INPUT_READY),"Unknown"));assert(!strcmp(power_yes_no(PORTABLE_POWER_CHARGE_DONE),"Unknown"));}
    if(scenario==9)assert(!power_read_ok&&power_sample.percent==255&&power_sample.millivolts==0&&power_sample.flags==RISC_BATTERY_PROFILE_MISSING);
    if(scenario==10)assert(saw_charging&&saw_error&&saw_complete&&saw_usb&&power_read_ok&&!power_flag(PORTABLE_POWER_CHARGE_DONE));
    if(scenario==11)assert(detail_pages==15&&!power_details&&launch_calls==1);
    if(scenario==13)assert(power_navigation_failed&&launch_calls==2&&!strcmp(power_caption(),"Back failed. Try again."));
    assert(sample_reads>=1&&sample_reads<=ticks/1000+10);
    if(scenario<10||scenario==12)assert(presents==1);
    for(unsigned i=0;i<32;i++)assert(!cells[i].size);
    app_module_fini();assert(!grants&&!frames&&!subs);
    printf("Charging case %u: %s; %u reads, %u frames; no writes, stride and cleanup PASS\n",scenario,titles[scenario],sample_reads,presents);
    return 0;
}
