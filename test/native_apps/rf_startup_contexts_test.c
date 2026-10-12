#include "../../Apps/waterfall.c"
#include <assert.h>
#include <stdlib.h>
extern const contexts_service_v1 *fixture_service(void);
extern void fixture_adapter_init(const risc_runtime_api_v1*);
static const contexts_service_v1 *service_api;
static unsigned retains,shared_refusals,shared_acquires,data_stats;
static bool appdata_live,active_app,fail_fingerprint_release,release_failed;
static unsigned clocks_after_retained;
static uint32_t fixture_millis(void){if(release_failed)clocks_after_retained++;return 1000;}
static int32_t stat_data(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;*s=0;*r=0;data_stats++;if(active_app&&fail_fingerprint_release&&!strcmp(n,"context-fingerprints.cfp"))release_failed=true;printf("STAT %s\n",n);return RISC_APP_DATA_NOT_FOUND;}
static int32_t read_data(void*c,const char*n,uint64_t r,void*b,uint32_t z,uint32_t*s,uint64_t*a){(void)c;(void)n;(void)r;(void)b;(void)z;*s=0;*a=0;return RISC_APP_DATA_NOT_FOUND;}
static int32_t replace_data(void*c,const char*n,uint64_t r,const void*b,uint32_t z){(void)c;(void)n;(void)r;(void)b;(void)z;assert(0);return RISC_APP_DATA_INVALID;}
static const risc_app_data_v1 data_api={1,sizeof(data_api),(void*)3,stat_data,read_data,replace_data};
/* Mirrors Runtime.cpp:547-551: one appDataContext_ per invocation, shared
 * between storage.app-data and storage.shared-data. No service behavior mocked. */
static bool acquire_raw(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 assert(g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation);
 printf("ACQUIRE %s api=%u instance=%llu live_appdata=%u\n",n,v,(unsigned long long)i,appdata_live);
 if(!strcmp(n,"storage.app-data")||!strcmp(n,"storage.shared-data")){
  if(appdata_live){assert(!strcmp(n,"storage.shared-data"));shared_refusals++;return false;}
  appdata_live=true;active_app=!strcmp(n,"storage.app-data");g->api=&data_api;if(!strcmp(n,"storage.shared-data"))shared_acquires++;
 }else if(!strcmp(n,"contexts.service")){g->api=service_api;}
 else if(!strcmp(n,"runtime.realtime"))return false;
 else {fprintf(stderr,"unexpected acquire: %s\n",n);abort();}
 g->slot=1;g->generation=1;return true;
}
static bool release_raw(risc_runtime_capability_v1*g){assert(g->api);if(release_failed&&g->api==&data_api)return false;if(g->api==&data_api)appdata_live=false;g->api=0;g->slot=g->generation=0;return true;}
static bool retain_raw(void){retains++;puts("RETAIN INVOCATION");return true;}
static void yield_raw(uint32_t n){(void)n;}
static bool diagnostic_raw(const char*s){puts(s);return true;}
static const risc_runtime_api_v1 raw={.api_version=1,.struct_size=sizeof(raw),.acquire=acquire_raw,.release=release_raw,.retain_invocation=retain_raw,.yield_ms=yield_raw,.diagnostic=diagnostic_raw};
int main(int argc,char**argv){
 bool checkpoint_first=argc>1&&!strcmp(argv[1],"checkpoint-first");
 fail_fingerprint_release=argc>1&&!strcmp(argv[1],"retained-load");
 service_api=fixture_service();fixture_adapter_init(&raw);
 static const t5_app_api_v1 fixture_app={.millis=fixture_millis};app=&fixture_app;runtime=waterfall_runtime=&raw;
 rf_bulk=&rf_resident_bulk;prefs=rf_preferences_default();
 /* Foreground's contexts_controls_load stops unopened contexts, never opens it. */
 assert(portable_contexts_stop());assert(!retains);
 if(checkpoint_first){assert(portable_contexts_models_save());assert(!retains&&!appdata_live);}
 event_restore();assert(!appdata_live&&!retains);
 puts("ENTER cfa_restore");cfa_restore();puts("RETURN cfa_restore");
 if(fail_fingerprint_release){assert(event_files.retained&&appdata_live&&event_data_scope.retained&&cfa_failed&&clocks_after_retained==0);puts("PASS: retained fingerprint cleanup performs no later timing/service call");return 0;}
 assert(!retains&&!shared_refusals&&shared_acquires==2&&!cfa_failed&&cfa_loaded&&!appdata_live);
 /* A later checkpoint must also remain possible; startup reordering alone
  * would leave the native app-data grant blocking these operations. */
 assert(portable_contexts_models_save());assert(!retains&&!appdata_live);
 event_files.ready=0;event_load();assert(!retains&&!appdata_live&&event_files.ready==3);
 assert(portable_contexts_models_save());assert(!retains&&!appdata_live);
 puts("PASS: startup and repeated RF/Contexts storage alternate without retention");
 printf("counts retained=%u shared_refusals=%u shared_acquires=%u stats=%u\n",retains,shared_refusals,shared_acquires,data_stats);
 return 0;
}
