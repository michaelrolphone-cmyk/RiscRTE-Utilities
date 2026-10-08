/* NOVA power screens. The PMIC owns charging; this app never changes charger
 * policy or clears IRQs. Its opt-in BLE page controls only broadcast preference. */
#include "T5AppApi.h"
#include "PortableNovaUi.h"
#include "PortablePowerStatus.h"
#include "RiscRuntimeV1.h"
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_BLE_BROADCAST
#include "PortableBroadcastClient.h"
static bool power_broadcast, power_broadcast_error, power_broadcast_requested;
static telemetry_broadcast_status_v1 power_broadcast_state;
#endif
#ifdef PORTABLE_RETURN_APP
#error Power details require nested Back via BATTERY_RETURN_APP
#endif
static const t5_app_api_v1 *power_app;
static risc_battery_sample_v1 power_sample;
static bool power_read_ok, power_details, power_navigation_failed;
static uint32_t power_sample_at;
static unsigned power_detail_page;
#define POWER_GREEN 0x3dff9au
#define POWER_AMBER 0xffb020u
#define POWER_RED 0xff4d5eu
static bool power_extended(void) {
    return power_read_ok && (power_sample.flags & PORTABLE_POWER_STATUS_VALID);
}
static bool power_flag(unsigned flag) { return power_extended() && (power_sample.flags & flag); }
static bool power_percent_known(void) {
    return power_read_ok && power_sample.percent <= 100 && !(power_sample.flags & RISC_BATTERY_PROFILE_MISSING);
}
static const char *power_title(void) {
    if (!power_read_ok) return "READ ERROR";
    if (power_extended() && !power_flag(PORTABLE_POWER_BATTERY_PRESENT)) return "NO BATTERY";
    if (power_flag(PORTABLE_POWER_THERMAL_LIMIT)) return "THERMAL LIMIT";
    if (power_flag(PORTABLE_POWER_CHARGE_DONE)) return "CHARGE COMPLETE";
    if (power_sample.flags & RISC_BATTERY_CHARGING) return "CHARGING";
    if (power_flag(PORTABLE_POWER_INPUT_READY))
        return power_flag(PORTABLE_POWER_CHARGER_ENABLED) ? "USB POWER" : "CHARGE DISABLED";
    if (power_percent_known() && power_sample.percent <= 10) return "BATTERY CRITICAL";
    if (power_percent_known() && power_sample.percent <= 20) return "LOW BATTERY";
    return "BATTERY STATUS";
}
static uint32_t power_color(void) {
    if (!power_read_ok || (power_percent_known() && power_sample.percent <= 10 &&
        !(power_sample.flags & RISC_BATTERY_CHARGING) && !power_flag(PORTABLE_POWER_INPUT_READY))) return POWER_RED;
    if (power_flag(PORTABLE_POWER_THERMAL_LIMIT) ||
        (power_percent_known() && power_sample.percent <= 20 && !(power_sample.flags & RISC_BATTERY_CHARGING))) return POWER_AMBER;
    return power_flag(PORTABLE_POWER_CHARGE_DONE) ? POWER_GREEN : NOVA_CYAN;
}
static const char *power_caption(void) {
    if (!power_read_ok) return "Retrying every second";
    if (power_navigation_failed) return "Back failed. Try again.";
    if (power_extended() && !power_flag(PORTABLE_POWER_BATTERY_PRESENT)) return "Presence not detected";
    if (power_flag(PORTABLE_POWER_THERMAL_LIMIT)) return "PMIC limiting charge";
    if (power_flag(PORTABLE_POWER_CHARGE_DONE)) return "Charger reports done";
    if (power_sample.flags & RISC_BATTERY_CHARGING) return "PMIC reports charging";
    if (power_flag(PORTABLE_POWER_INPUT_READY)) return "Not reporting charge";
    if (power_percent_known() && power_sample.percent <= 20) return "Connect USB to charge";
    return power_extended() ? "USB not available" : "Basic telemetry only";
}
/* Integer-only ring. One bounded scan per changed snapshot, never animation.
 * Clockwise progress uses a fixed 101-point unit-circle lookup, avoiding
 * target libm imports. Ring and controls keep 8+ pixels from every edge. */
static const int8_t power_circle[101][2] = {
    {0,-100},
    {6,-100},
    {13,-99},
    {19,-98},
    {25,-97},
    {31,-95},
    {37,-93},
    {43,-90},
    {48,-88},
    {54,-84},
    {59,-81},
    {64,-77},
    {68,-73},
    {73,-68},
    {77,-64},
    {81,-59},
    {84,-54},
    {88,-48},
    {90,-43},
    {93,-37},
    {95,-31},
    {97,-25},
    {98,-19},
    {99,-13},
    {100,-6},
    {100,0},
    {100,6},
    {99,13},
    {98,19},
    {97,25},
    {95,31},
    {93,37},
    {90,43},
    {88,48},
    {84,54},
    {81,59},
    {77,64},
    {73,68},
    {68,73},
    {64,77},
    {59,81},
    {54,84},
    {48,88},
    {43,90},
    {37,93},
    {31,95},
    {25,97},
    {19,98},
    {13,99},
    {6,100},
    {0,100},
    {-6,100},
    {-13,99},
    {-19,98},
    {-25,97},
    {-31,95},
    {-37,93},
    {-43,90},
    {-48,88},
    {-54,84},
    {-59,81},
    {-64,77},
    {-68,73},
    {-73,68},
    {-77,64},
    {-81,59},
    {-84,54},
    {-88,48},
    {-90,43},
    {-93,37},
    {-95,31},
    {-97,25},
    {-98,19},
    {-99,13},
    {-100,6},
    {-100,0},
    {-100,-6},
    {-99,-13},
    {-98,-19},
    {-97,-25},
    {-95,-31},
    {-93,-37},
    {-90,-43},
    {-88,-48},
    {-84,-54},
    {-81,-59},
    {-77,-64},
    {-73,-68},
    {-68,-73},
    {-64,-77},
    {-59,-81},
    {-54,-84},
    {-48,-88},
    {-43,-90},
    {-37,-93},
    {-31,-95},
    {-25,-97},
    {-19,-98},
    {-13,-99},
    {-6,-100},
    {0,-100},
};
static void power_ring(unsigned percent, bool known, uint32_t color) {
    const int cx=120,cy=111,r=65;
    /* Subtle full circular track with 100 unambiguous progress segments. */
    for (unsigned i=0;i<100;i++) {
        int x=cx+power_circle[i][0]*r/100,y=cy+power_circle[i][1]*r/100;
        portable_nova_round(x-3,y-3,6,6,3,known && i<percent?color:NOVA_LINE);
    }
}
static void power_overview(void) {
    const uint32_t color=power_color();char value[16];
    portable_nova_begin();
    portable_nova_button(8,4,44,44,"<",false);
    portable_nova_center(0,56,15,176,"POWER",NOVA_CAP);
    power_ring(power_sample.percent,power_percent_known(),color);
    if (power_percent_known()) snprintf(value,sizeof(value),"%u",(unsigned)power_sample.percent);
    else strcpy(value,"--");
    /* Large font contains digits, not %. Draw the real small percent glyph
     * beside the centered number instead of silently substituting a space. */
    int digits=portable_nova_measure(4,value);
    int left=120-(digits+(power_percent_known()?14:0))/2;
    portable_nova_text(4,left,82,digits+1,value,power_read_ok?NOVA_WHITE:POWER_RED);
    if(power_percent_known())portable_nova_text(1,left+digits+3,98,14,"%",NOVA_WHITE);
    portable_nova_center(2,48,119,144,power_percent_known()?"BATTERY ESTIMATE":"ESTIMATE UNAVAILABLE",NOVA_CAP);
    portable_nova_center(2,24,139,192,power_title(),color);
    portable_nova_center(2,16,173,208,power_caption(),NOVA_CAP);
    portable_nova_button(12,192,104,44,"Details",false);
#ifdef PORTABLE_BLE_BROADCAST
    portable_nova_button(124,192,104,44,"BLE",false);
#else
    portable_nova_button(124,192,104,44,power_read_ok?"Refresh":"Retry",false);
#endif
    power_app->present(false);
}
static const char *power_yes_no(unsigned flag) {
    return !power_extended()?"Unknown":power_flag(flag)?"Yes":"No";
}
static void power_detail_view(void) {
    char value[40];portable_nova_begin();portable_nova_header("POWER DETAILS");
    if (power_detail_page==0) {
        if (power_read_ok) snprintf(value,sizeof(value),"%u mV",(unsigned)power_sample.millivolts);
        else strcpy(value,"Unavailable");
        portable_nova_row(16,56,208,54,"Battery voltage",value,false);
        portable_nova_row(16,116,208,54,"USB available",power_yes_no(PORTABLE_POWER_INPUT_READY),false);
    } else if (power_detail_page==1) {
        portable_nova_row(16,56,208,54,"Charger enabled",power_yes_no(PORTABLE_POWER_CHARGER_ENABLED),false);
        portable_nova_row(16,116,208,54,"Charge complete",power_yes_no(PORTABLE_POWER_CHARGE_DONE),false);
    } else if (power_detail_page==2) {
        portable_nova_row(16,56,208,54,"Battery present",power_yes_no(PORTABLE_POWER_BATTERY_PRESENT),false);
        portable_nova_row(16,116,208,54,"PMIC thermal limit",power_yes_no(PORTABLE_POWER_THERMAL_LIMIT),false);
    } else {
        portable_nova_center(1,16,60,208,"Charging is autonomous",NOVA_WHITE);
        portable_nova_wrap(2,20,88,200,17,3,"Sleep does not stop the PMIC charger. USB and protection conditions still apply.",NOVA_TEXT);
        portable_nova_wrap(2,20,142,200,17,2,"No cell temperature or time-to-full measurement.",NOVA_CAP);
    }
    snprintf(value,sizeof(value),"%u / 4%s",power_detail_page+1,power_read_ok?"":"  -  Read error");
    portable_nova_center(2,16,174,208,value,power_read_ok?NOVA_CAP:POWER_RED);
    portable_nova_button(12,192,52,44,"Prev",false);
    portable_nova_button(72,192,96,44,power_read_ok?"Refresh":"Retry",false);
    portable_nova_button(176,192,52,44,"Next",false);
    power_app->present(false);
}
#ifdef PORTABLE_BLE_BROADCAST
static const char *power_broadcast_label(void) {
    switch(power_broadcast_state.state) {
      case TELEMETRY_BROADCAST_OFF:return "Disabled";
      case TELEMETRY_BROADCAST_BLE_OFF:return "Bluetooth is off";
      case TELEMETRY_BROADCAST_PAUSED:return "Paused";
      case TELEMETRY_BROADCAST_STARTING:return "Starting";
      case TELEMETRY_BROADCAST_LIVE:return "Broadcasting";
      case TELEMETRY_BROADCAST_RETRY:return "Waiting / retrying";
      case TELEMETRY_BROADCAST_RETAINED:return "Cleanup pending";
      default:return "Settings unavailable";
    }
}
static void power_broadcast_view(void) {
    portable_nova_begin();portable_nova_header("BLE BROADCAST");
    portable_nova_row(12,52,216,50,"Sensor status",power_broadcast_label(),false);
    portable_nova_wrap(2,16,108,208,17,2,"Available sensor values only. No estimated temperature.",NOVA_TEXT);
    portable_nova_wrap(2,16,147,208,17,2,"Open to nearby receivers without pairing.",NOVA_CAP);
    if(power_broadcast_state.omitted_fields)portable_nova_center(2,12,177,216,"Some fields exceed BLE limit",POWER_AMBER);
    if(power_broadcast_error)portable_nova_center(2,12,177,216,"Not saved. Try again.",POWER_RED);
    portable_nova_button(36,192,168,44,power_broadcast_error?"Retry":power_broadcast_state.enabled?"Disable":"Enable",false);
    power_app->present(false);
}
#endif
static void power_render(void) {
#ifdef PORTABLE_BLE_BROADCAST
    if(power_broadcast){power_broadcast_view();return;}
#endif
    if(power_details)power_detail_view();else power_overview();
}
static bool power_refresh(void) {
    risc_battery_sample_v1 next={0,255,RISC_BATTERY_PROFILE_MISSING};
    bool ok=portable_power_read(&next);
    bool changed=ok!=power_read_ok || memcmp(&next,&power_sample,sizeof(next));
    power_read_ok=ok;power_sample=next;power_sample_at=power_app->millis();
#ifdef PORTABLE_BLE_BROADCAST
    telemetry_broadcast_status_v1 state={.struct_size=sizeof(state)};
    if(!portable_broadcast_status(&state))state.state=TELEMETRY_BROADCAST_SETTINGS_ERROR;
    changed|=memcmp(&state,&power_broadcast_state,sizeof(state))!=0;
    power_broadcast_state=state;
#endif
    return changed;
}
static bool power_back(void) {
#ifdef PORTABLE_BLE_BROADCAST
    if(power_broadcast){power_broadcast=false;power_render();return false;}
#endif
    if (power_details) { power_details=false;power_render();return false; }
#ifdef BATTERY_RETURN_APP
    const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
    if (!rt || !rt->request_launch || !rt->request_launch(BATTERY_RETURN_APP)) {
        power_navigation_failed=true;power_render();return false;
    }
#endif
    return true;
}
void app_main(void) {
    power_app=t5_app_get_api(1);
    if(!power_app || !power_app->poll || !power_app->present || !power_app->millis ||
       !power_app->screen_width || !power_app->screen_height ||
       power_app->screen_width()!=240 || power_app->screen_height()!=240)return;
    power_sample=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};
    power_read_ok=power_details=power_navigation_failed=false;power_detail_page=0;
#ifdef PORTABLE_BLE_BROADCAST
    power_broadcast=power_broadcast_error=false;memset(&power_broadcast_state,0,sizeof(power_broadcast_state));
#endif
    (void)power_refresh();power_render();
    for (;;) {
        t5_app_input_t input={0};if(!power_app->poll(&input,50))return;
        /* A queued quick-action launch is terminal, not an in-app Back. */
        if(input.exit_requested && !(input.buttons&T5_APP_BUTTON_BACK))return;
        if(input.buttons&T5_APP_BUTTON_BACK) {if(power_back())return;continue;}
        bool render=false;
        if((uint32_t)(power_app->millis()-power_sample_at)>=1000)render=power_refresh();
        if(power_details && (input.buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP))) {power_detail_page=(power_detail_page+3)%4;render=true;}
        if(power_details && (input.buttons&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_DOWN))) {power_detail_page=(power_detail_page+1)%4;render=true;}
        bool refresh=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
#ifdef PORTABLE_BLE_BROADCAST
        if(power_broadcast && (refresh || (input.tapped && portable_nova_hit(input.touch_x,input.touch_y,36,192,168,44)))) {
            if(!power_broadcast_error)power_broadcast_requested=!power_broadcast_state.enabled;
            power_broadcast_error=!portable_broadcast_enable(power_broadcast_requested);
            (void)power_refresh();power_render();continue;
        }
#endif
        if(input.tapped) {
            if(portable_nova_hit(input.touch_x,input.touch_y,8,4,44,44)) {if(power_back())return;continue;}
#ifdef PORTABLE_BLE_BROADCAST
            if(power_broadcast)continue;
            if(!power_details && portable_nova_hit(input.touch_x,input.touch_y,124,192,104,44)) {power_broadcast=true;power_render();continue;}
#endif
            if(!power_details && portable_nova_hit(input.touch_x,input.touch_y,12,192,104,44)) {power_details=true;power_detail_page=0;render=true;}
            else if(!power_details && portable_nova_hit(input.touch_x,input.touch_y,124,192,104,44)) refresh=true;
            else if(power_details && portable_nova_hit(input.touch_x,input.touch_y,12,192,52,44)) {power_detail_page=(power_detail_page+3)%4;render=true;}
            else if(power_details && portable_nova_hit(input.touch_x,input.touch_y,176,192,52,44)) {power_detail_page=(power_detail_page+1)%4;render=true;}
            else if(power_details && portable_nova_hit(input.touch_x,input.touch_y,72,192,96,44)) refresh=true;
        }
        if(refresh) {(void)power_refresh();render=true;}
        if(render)power_render();
    }
}
