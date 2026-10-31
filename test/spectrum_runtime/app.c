#include "observer.h"
#define app_main spectrum_production_main
#include "../../Apps/audio_spectrum_scene.c"
#undef app_main
extern void spectrum_test_mapping(unsigned,spectrum_test_observer);
extern unsigned spectrum_test_entry(void);
extern bool spectrum_test_resident(void);
static void observe(spectrum_test_view*v){*v=(spectrum_test_view){.document=ss_document,.navigation=ss_nav,.scene=ss_scene_session,.canvas=ss_canvas_session,.text=ss_text_session,.flags=ss_flags,.width=ss_layout.width,.height=ss_layout.height,.scale=ss_layout.scale,.transforms=spectrum.transforms,.page=page,.running=running,.cursor=cursor_visible};}
__attribute__((constructor)) static void mapped(void){spectrum_test_mapping(1,observe);}
__attribute__((destructor)) static void unmapped(void){spectrum_test_mapping(2,observe);}
__attribute__((visibility("default"))) int app_main(void){unsigned entry=spectrum_test_entry();int result=spectrum_production_main();const risc_runtime_api_v1*r=risc_runtime_get_api(1);if(!spectrum_test_resident()&&entry==1&&r&&r->request_launch)(void)r->request_launch("default.elf");return result;}
