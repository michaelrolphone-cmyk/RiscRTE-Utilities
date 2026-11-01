#include "../../Apps/lora_scene.c"
extern void lora_stamp_observer(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
extern void lora_app_event(unsigned,const void*);
__attribute__((visibility("default"))) int app_module_init(void){lora_app_event(1,(const void*)app_main);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){for(unsigned i=0;i<radio.model.count;i++){const lm_message*m=lm_at(&radio.model,i);lora_stamp_observer(m->clock_valid,m->year,m->month,m->day,m->hour,m->minute);}lora_app_event(2,(const void*)app_main);}
