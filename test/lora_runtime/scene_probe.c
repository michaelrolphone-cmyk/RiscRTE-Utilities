/* Read-only test observation; all scene operations use production code. */
#include SCENE_SOURCE
__attribute__((visibility("default"))) void lora_scene_probe(risc_components_document_v1*out,uint32_t*focus){*out=logical_model.doc;*focus=logical_model.navigation_path.depth?logical_model.navigation_path.focus[logical_model.navigation_path.depth-1]:0;}
extern void lora_probe_register(void(*)(risc_components_document_v1*,uint32_t*));
__attribute__((constructor)) static void probe_register(void){lora_probe_register(lora_scene_probe);}
extern void lora_scene_trace(unsigned);
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void*f,void*c){(void)c;if(f==(void*)scene_configure)lora_scene_trace(1);if(f==(void*)update_document)lora_scene_trace(2);}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*f,void*c){(void)c;if(f==(void*)scene_snapshot)lora_scene_trace(3);}
