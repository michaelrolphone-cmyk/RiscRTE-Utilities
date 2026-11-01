/* Production host adapter and policy; no substituted resident callbacks. */
#include HOST_SOURCE
#include <assert.h>
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
extern void lora_host_event(unsigned,int);
__attribute__((visibility("default"))) void app_main(void){
 lora_host_event(1,0);
 const t5_app_api_v1*a=t5_app_get_api(1);assert(a);a->set_back_exits_app(false);
 a->fill_rect(0,0,240,240,true);a->present(true);assert(portable_paper_frame_drain());
 int result=portable_resident_run_foreground("default.elf");lora_host_event(2,result);
}
