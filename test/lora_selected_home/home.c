#include "AlarmRecords.h"
#include HOME_SOURCE
extern void lora_home_event(unsigned);
extern void lora_home_observer(bool (*)(void));
extern void lora_alarm_observer(bool (*)(void));
static bool observing_alarm(void){return clock_alarm_modal;}
static bool observing_controls(void){return clock_resident_dispatching&&pqa_visible(&clock_quick.ui);}
__attribute__((visibility("default"))) int app_module_init(void){lora_home_event(1);lora_home_observer(observing_controls);lora_alarm_observer(observing_alarm);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){lora_home_event(2);lora_home_observer(NULL);lora_alarm_observer(NULL);}
__attribute__((destructor)) static void unloaded(void){lora_home_event(3);}
