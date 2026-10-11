/* Hardware-only doubles. Production adapter, controllers and Light helper are
 * independently compiled from the same paths and defines as the target ELFs. */
#include "RiscDisplayOutputPowerV1.h"
#include "RiscTouchPowerV1.h"
#include "RiscStorageVolumeV1.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
#include "X4PowerV1.h"
static unsigned light_entries,alarm_resumes,storage_prepares,storage_commits,storage_resumes;
static unsigned touch_prepares,touch_resumes,panel_prepares,panel_resumes;
static bool bluetooth_on;
static unsigned brightness=60;
static bool idle_brightness(void*c,uint16_t level,uint16_t duration){(void)c;(void)duration;assert(!retained_flag);brightness=level;return true;}
static bool idle_seed(void*c,risc_display_frame_v1 frame){(void)c;(void)frame;return true;}
static int32_t idle_panel_prepare(void*c,uint32_t budget){(void)c;assert(!retained_flag&&!frames&&!subs&&budget);panel_prepares++;return idle_scenario==3?RISC_DISPLAY_POWER_RETAINED:RISC_DISPLAY_POWER_OK;}
static int32_t idle_panel_resume(void*c,uint32_t budget){(void)c;(void)budget;assert(!retained_flag);panel_resumes++;return RISC_DISPLAY_POWER_OK;}
static int32_t idle_touch_prepare(void*c,uint32_t budget){(void)c;assert(!retained_flag&&!subs&&budget);touch_prepares++;return RISC_TOUCH_POWER_OK;}
static int32_t idle_touch_resume(void*c,uint32_t budget){(void)c;(void)budget;assert(!retained_flag);touch_resumes++;return RISC_TOUCH_POWER_OK;}
static bool idle_store_prepare(void*c){(void)c;assert(!retained_flag&&!frames&&!subs);storage_prepares++;return true;}
static bool idle_store_commit(void*c){(void)c;assert(!retained_flag&&storage_prepares);storage_commits++;return true;}
static bool idle_terminal(void*c){(void)c;assert(!"Automatic idle must never enter terminal storage shutdown");return false;}
static int32_t idle_store_resume(void*c){(void)c;assert(!retained_flag);storage_resumes++;return RISC_STORAGE_SLEEP_READY;}
static bool idle_key(void*c,bool*down){(void)c;assert(!retained_flag);*down=false;return true;}
static int32_t idle_light(void*c,uint32_t duration,risc_light_sleep_result_v1*out){(void)c;assert(!retained_flag&&duration&&!frames&&!subs&&!bluetooth_on&&brightness==0&&storage_commits&&panel_prepares&&touch_prepares);light_entries++;ticks+=3000;*out=(risc_light_sleep_result_v1){.struct_size=sizeof(*out)};return RISC_LIGHT_SLEEP_OK;}
static int32_t idle_alarm_resume(void*c,const alarm_sleep_v1*ticket){(void)c;assert(!retained_flag&&ticket->struct_size==sizeof(*ticket)&&storage_resumes&&panel_resumes&&touch_resumes);alarm_resumes++;return ALARM_OK;}
static bool idle_wifi_off(void*c){(void)c;assert(!retained_flag);return true;}
static wifi_link_t idle_wifi_status(void*c){(void)c;assert(!retained_flag);return WIFI_LINK_DOWN;}
static bool idle_ble_set(void*c,bool value){(void)c;assert(!retained_flag);bluetooth_on=value;return true;}
static bool idle_ble_status(void*c,uint8_t*out){(void)c;assert(!retained_flag);*out=bluetooth_on?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;}
static bool idle_battery(void*c,risc_battery_sample_v1*out){assert(!retained_flag);bool ok=fake_battery(c,out);if(idle_scenario==1||idle_scenario==5){out->percent=9;out->flags=0;}return ok;}
static void idle_yield(uint32_t ms){assert(!retained_flag);fake_yield(ms);if(!aged&&polls>=40){aged=true;ticks+=(idle_scenario==1||idle_scenario==5)?20001:60001;}}
static const wifi_api_v1 idle_wifi={.api_version=1,.struct_size=sizeof(idle_wifi),.status=idle_wifi_status,.disconnect_checked=idle_wifi_off};
static const portable_bluetooth_control_v1 idle_ble={.api_version=1,.struct_size=sizeof(idle_ble),.set_enabled=idle_ble_set,.status=idle_ble_status};
static const x4_power_v1 idle_power={1,sizeof(idle_power),NULL,idle_key,idle_light};
static risc_display_output_api_v1_power idle_display;
static risc_touch_power_api_v1 idle_touch;
static const risc_battery_gauge_api_v1 idle_gauge={1,sizeof(idle_gauge),NULL,idle_battery};
static risc_storage_volume_api_v1_sleep idle_storage={.terminal={.power={.volume={.base={.api_version=1,.struct_size=sizeof(idle_storage)}}},.extension_tag=RISC_STORAGE_POWER_COMMIT_TAG,.extension_version=1,.commit_power_down=idle_terminal},.sleep_tag=RISC_STORAGE_SLEEP_TAG,.sleep_version=1,.prepare_sleep=idle_store_prepare,.commit_sleep=idle_store_commit,.resume_sleep=idle_store_resume};
static bool idle_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 assert(version==1||strcmp(name,"x4.power"));
 if(!strcmp(name,"x4.power")){assert(instance==17);g->api=&idle_power;}
 else if(!strcmp(name,"storage.volume")){assert(instance==9);g->api=&idle_storage;}
 else if(!strcmp(name,"net.wifi")){assert(instance==15);g->api=&idle_wifi;}
 else if(!strcmp(name,"bluetooth.hci")){assert(instance==16);g->api=&idle_ble;}
 else if(!strcmp(name,"display.output")&&instance==3)g->api=&idle_display;
 else if(!strcmp(name,"input.touch.raw")&&instance==4)g->api=&idle_touch;
 else return false;
 return true;
}
static void idle_setup(void){
 idle_display.history.base=display_api;idle_display.history.base.struct_size=sizeof(idle_display);idle_display.history.base.set_brightness=idle_brightness;
 idle_display.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;idle_display.history.extension_version=1;idle_display.history.seed_previous=idle_seed;
 idle_display.power_tag=RISC_DISPLAY_POWER_TAG;idle_display.power_version=1;idle_display.prepare=idle_panel_prepare;idle_display.resume=idle_panel_resume;
 idle_touch.base=touch_api;idle_touch.base.struct_size=sizeof(idle_touch);idle_touch.power_tag=RISC_TOUCH_POWER_TAG;idle_touch.power_version=1;idle_touch.prepare=idle_touch_prepare;idle_touch.resume=idle_touch_resume;
 test_alarm.features=ALARM_DESCRIPTOR_RESUME_SLEEP;test_alarm.resume_sleep=idle_alarm_resume;
}
