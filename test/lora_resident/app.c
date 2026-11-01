#include "../../Apps/lora_scene.c"
extern void lora_app_event(unsigned,const void*);
__attribute__((visibility("default"))) int app_module_init(void){lora_app_event(1,(const void*)app_main);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){lora_app_event(2,(const void*)app_main);}
