/* Real app + shared production adapter; only read-only hardware is modeled. */
#include "PortablePowerStatus.h"
#define portable_app_alarm_sleep unused_alarm_sleep
#define main unused_capture_main
#define risc_runtime_get_api unused_runtime_api
#include "battery_native_peripherals.h"
#undef risc_runtime_get_api
#undef main
#undef portable_app_alarm_sleep
static unsigned scenario, sample_reads, launch_calls, sleep_calls, reads_at_sleep;
static bool first_failed, recovered, saw_complete, saw_charging;
static bool exit_signaled;
static bool test_exit_requested(void){
 if((scenario==29 || scenario==32) &&
#ifdef PORTABLE_ASYNC_RGB565
    pending_token &&
#endif
    (scenario==29 || script_step()>=4))exit_signaled=true;
 return exit_signaled;
}
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
        if(script_step()<40){s->flags|=PORTABLE_POWER_INPUT_READY|RISC_BATTERY_CHARGING;saw_charging=true;}
        else if(script_step()<100){*s=(risc_battery_sample_v1){4200,100,255};first_failed=true;return false;}
        else if(script_step()<140){s->flags|=PORTABLE_POWER_INPUT_READY|PORTABLE_POWER_CHARGE_DONE;saw_complete=true;}
        else {recovered=true;s->percent=39;}
        break;
      case 13: s->percent=100;break; /* Gauge100 never establishes charger done. */
      default: break;
    }
    return true;
}
static const risc_battery_gauge_api_v1 test_gauge={1,sizeof(test_gauge),NULL,test_read};
#ifdef PORTABLE_BLE_BROADCAST
#include "TelemetryBroadcastV1.h"
static telemetry_broadcast_status_v1 visual_broadcast={.struct_size=sizeof(visual_broadcast),.state=TELEMETRY_BROADCAST_LIVE,.fields=3,.enabled=true,.settings_valid=true};
static bool visual_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;(void)allow;(void)p;return true;}
static bool visual_pause(void*c){(void)c;return true;}
static bool visual_status(void*c,telemetry_broadcast_status_v1*s){(void)c;*s=visual_broadcast;return true;}
static int32_t visual_enumerate(void*c,uint32_t i,risc_telemetry_field_v1*s){(void)c;(void)i;(void)s;return 0;}
static int32_t visual_read(void*c,uint32_t i,int32_t*s){(void)c;(void)i;(void)s;return 0;}
static const telemetry_broadcast_v1 visual_api={1,sizeof(visual_api),NULL,visual_step,visual_pause,visual_status,visual_enumerate,visual_read};
#endif
static bool test_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g) {
    if(!strcmp(n,"board.battery")&&v==1){g->api=&test_gauge;grants++;return true;}
    #ifdef PORTABLE_BLE_BROADCAST
    if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)&&v==1){g->api=&visual_api;grants++;return true;}
#endif
    return fake_acquire(n,v,id,g);
}
static bool test_launch(const char *s) {
    assert(!strcmp(s,"springboard.elf"));launch_calls++;fprintf(stderr,"launch step=%u pending=%llu observed=%u completed=%u\n",script_step(),(unsigned long long)pending_token,present_pending_seen,present_completions);
    if(scenario==15 && launch_calls==1)return false;
    return fake_launch(s);
}
static bool test_retain(void){assert(!"Unexpected retained invocation in UI fixture");return false;}
static const risc_runtime_api_v1 test_runtime={.api_version=1,.struct_size=sizeof(test_runtime),.health=fake_health,.yield_ms=fake_yield,.diagnostic=fake_diag,.request_launch=test_launch,.acquire=test_acquire,.release=fake_release,.retain_invocation=test_retain,.exit_requested=test_exit_requested};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&test_runtime:NULL;}
#include NOVA_APP_SOURCE
static unsigned text_calls,shortened_text;
void __real_portable_nova_native_text(unsigned,int,int,int,const char*,uint32_t);
void __wrap_portable_nova_native_text(unsigned face,int x,int y,int width,const char *s,uint32_t color){
 int glyph_extent=face==4?31:face==1?14:12;
 assert(x>=0&&y>=0&&x+width<=(int)test_width&&y+glyph_extent<=(int)test_height);
 assert(portable_nova_measure(face,s)<=width);text_calls++;if(strstr(s,"..."))shortened_text++;
 __real_portable_nova_native_text(face,x,y,width,s,color);
}

int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
                             const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
    (void)r;(void)d;(void)b;(void)a;
    assert(scenario>=16 && !frames && !subs && power_details && power_detail_page==0);
    sleep_calls++;reads_at_sleep=sample_reads;ticks+=3100;
    return scenario==17?0:scenario==18?-1:scenario==19?-2:1;
}
static void tap(unsigned at,int x,int y) {
 battery_native_layout l;assert(battery_native_layout_make(&l,test_width,test_height,x==50?2:3));
 battery_native_rect r=y<50?l.back:l.footer[x==50||x==30?0:x==200?2:1];
 x=r.x+r.w/2;y=r.y+r.h/2;
 actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
static void raw_tap(unsigned at,int x,int y){actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
static void native_cases(void){
 battery_native_layout two,three;assert(battery_native_layout_make(&two,test_width,test_height,2));assert(battery_native_layout_make(&three,test_width,test_height,3));
 battery_native_rect r=two.footer[0];
 if(scenario==20){raw_tap(10,r.x-1,r.y);raw_tap(20,r.x,r.y-1);raw_tap(30,r.x+r.w,r.y);raw_tap(40,r.x,r.y+r.h);}
 else if(scenario==21){
  raw_tap(10,r.x,r.y);r=three.footer[2];raw_tap(20,r.x+r.w-1,r.y+r.h-1);
  r=three.footer[0];raw_tap(30,r.x,r.y);r=two.back;raw_tap(40,r.x+r.w-1,r.y+r.h-1);raw_tap(50,r.x,r.y);
 }else if(scenario>=22){
  raw_tap(10,r.x+r.w-1,r.y+r.h-1);r=three.footer[2];
  for(unsigned i=22;i<scenario;i++)raw_tap(20+(i-22)*10,r.x,r.y);
 }
}
static void reset_run(void){
 assert(!grants&&!frames&&!subs&&!pending_token);
 ticks=polls=grants=frames=subs=presents=0;
 sample_reads=launch_calls=sleep_calls=reads_at_sleep=text_calls=shortened_text=0;
 first_failed=recovered=saw_complete=saw_charging=exit_signaled=launch_with_pending=false;
 present_pending_seen=present_completions=present_pending_polls=action_count=0;
 present_delay=scenario>=29?250:3;
}
static void run_case(void) {
    stop_poll=scenario>=16&&scenario<20?3000:200;memset(pixels,0xa5,sizeof(pixels));
#ifdef PORTABLE_BLE_BROADCAST
    if(scenario>=100){
     assert(app_module_init()==0);power_app=t5_app_get_api(1);utility_frame_reset();
     power_sample=(risc_battery_sample_v1){3970,42,PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT|PORTABLE_POWER_CHARGER_ENABLED|PORTABLE_POWER_INPUT_READY|RISC_BATTERY_CHARGING};
     power_read_ok=true;
     if(scenario==101){power_details=true;power_detail_page=0;}
     if(scenario==102||scenario==104){power_broadcast=true;power_broadcast_state=visual_broadcast;}
     if(scenario==103){power_read_ok=false;power_sample=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};}
     if(scenario==104){power_broadcast_error=true;power_broadcast_state.omitted_fields=1;}
     if(scenario==105)power_navigation_failed=true;
     if(scenario==106){power_details=true;power_detail_page=3;}
     if(scenario==107){power_sample.percent=0;power_sample.flags&=~RISC_BATTERY_CHARGING;}
     if(scenario==108){power_sample.percent=100;power_sample.flags&=~RISC_BATTERY_CHARGING;}
     if(scenario==109){power_sample.percent=255;power_sample.flags|=RISC_BATTERY_PROFILE_MISSING;power_sample.flags&=~RISC_BATTERY_CHARGING;}
     power_render();assert(presents==1&&text_calls);app_module_fini();assert(!grants&&!frames&&!subs&&!pending_token);return;
    }
#endif

    if(scenario>=16&&scenario<20)tap(10,50,212);
    if(scenario==14) {
        /* Open all detail pages, repeated next/previous, refresh, nested Back,
         * reopen, back, then final app return. No setting writes. */
        tap(10,50,212);tap(20,200,212);tap(30,200,212);tap(40,200,212);
        tap(50,200,212);tap(60,30,212);tap(70,120,212);tap(80,30,26);
        tap(90,50,212);tap(100,30,26);tap(110,30,26);
    } else if(scenario==15) {tap(10,30,26);tap(30,30,26);}
    if(scenario>=20&&scenario<28)native_cases();
    if(scenario==30)tap(2,30,26);
    if(scenario==31){tap(2,50,212);tap(4,30,26);tap(6,30,26);}
    if(scenario==32)tap(2,50,212);
    assert(app_module_init()==0);app_main();
    const char *expected[]={"CHARGING","CHARGE COMPLETE","LOW BATTERY","BATTERY CRITICAL","BATTERY CRITICAL","BATTERY STATUS","THERMAL LIMIT","CHARGE DISABLED","NO BATTERY","BATTERY STATUS","USB POWER","READ ERROR","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS","BATTERY STATUS"};
    assert(!strcmp(power_title(),expected[scenario<20?scenario:14]));
    if(scenario==11)assert(!power_read_ok && power_sample.percent==255 && power_sample.millivolts==0 && power_sample.flags==RISC_BATTERY_PROFILE_MISSING);
    if(scenario==12)assert(first_failed&&recovered&&saw_complete&&saw_charging&&power_read_ok&&power_sample.percent==39);
    if(scenario==13)assert(!power_flag(PORTABLE_POWER_CHARGE_DONE));
    if(scenario==9)assert(!power_extended()&&!strcmp(power_yes_no(PORTABLE_POWER_INPUT_READY),"Unknown"));
    if(scenario==14)assert(!power_details && launch_calls==1 && script_step()<stop_poll+2);
    if(scenario==15)assert(power_navigation_failed && launch_calls==2);
    if(scenario>=16&&scenario<20){fprintf(stderr,"sleep=%u details=%u page=%u ticks=%u polls=%u\n",sleep_calls,power_details,power_detail_page,ticks,polls);assert(sleep_calls==1 && power_details && power_detail_page==0);if(scenario>=18)assert(sample_reads==reads_at_sleep);}

    if(scenario==20)assert(!power_details&&presents==1);
    if(scenario==21)assert(!power_details&&launch_calls==1);
    if(scenario>=22&&scenario<28)assert(power_details&&power_detail_page==(scenario-22)%power_detail_count());
    if(scenario>=29&&scenario<=32){
     assert(!power_details&&launch_calls==1);
     if(scenario==29||scenario==32)assert(exit_signaled);
#ifdef PORTABLE_ASYNC_RGB565
     assert(launch_with_pending&&pending_token);
#endif
    }
    assert(sample_reads>=1 && sample_reads<=ticks/1000+4);
    if(scenario<11 || scenario==13)assert(presents==1);
    assert(text_calls); /* No continuous animations. */
    for(unsigned i=0;i<32;i++)assert(!cells[i].size); /* No data mutation. */
    app_module_fini();
    if(scenario==19)assert(grants && !frames && !subs); /* Native retained state keeps its grants. */
    else assert(!grants&&!frames&&!subs&&!pending_token);
#ifdef PORTABLE_ASYNC_RGB565
    assert(present_pending_seen>=present_delay&&present_completions==presents&&!pending_token);
    printf("Async RGB565: %u pending observations, %u terminal frames, immutable provider-owned pixels PASS\n",present_pending_seen,present_completions);
#endif
    printf("Native Battery UI case%u: %u reads, %u frames, bounded retry, stride and cleanup PASS\n",scenario,sample_reads,presents);printf("Native %ux%u: %u text calls, %u explicit shortened strings\n",test_width,test_height,text_calls,shortened_text);
}

int main(int argc,char**argv){
 assert(argc==5);directory=argv[1];scenario=(unsigned)atoi(argv[2]);test_width=(unsigned)atoi(argv[3]);test_height=(unsigned)atoi(argv[4]);assert(test_width<=800&&test_height<=800);
 if(scenario==28){
  for(unsigned entry=0;entry<3;entry++){scenario=21;reset_run();run_case();}
  puts("Native Battery repeated module entry: 3 complete init/app/nested-Back/fini cycles PASS");
 }else{reset_run();run_case();}
 return 0;
}
