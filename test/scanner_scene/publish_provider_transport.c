/* Reuse the actual scene provider's physical transport fixture in a separate TU. */
#define main scanner_unused_scene_fixture_main
#include SCANNER_SCENE_HOST_FIXTURE
#undef main
const risc_scene_api_v1 *scanner_provider_start(const char *profile){
 if(!strcmp(profile,"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
 else if(!strcmp(profile,"gray")){w=480;h=800;format=3;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,0,0,0};}
 startup();settle();int rc;while((rc=api->close(NULL,session))==RISC_SCENE_AGAIN)ms+=10;assert(rc==0);expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;return api;
}
void scanner_provider_settle(uint64_t token){session=token;settle();}
void scanner_provider_key(void){nav_pressed=RISC_NAV_HOME;}
void scanner_provider_stop(void){assert(driver->quiesce());driver->stop();}
