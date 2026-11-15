#include "../../Apps/stopwatch_scene.c"
void stopwatch_document(risc_scene_document_v1 *out){
 clock_state=(sw_clock){.elapsed_ms=1234567,.running=true};loaded=true;uncertain=pending_pause=reset_armed=false;utc_domain=true;notice[0]=0;
 static risc_key_value_v1 available;kv=&available;static risc_realtime_api_v1 available_clock;utc=&available_clock;
 document=(risc_scene_document_v1){.revision=1};declare(out);out->revision=1;
}
