#include "../../Apps/waterfall.c"
#include <assert.h>
#include <stdlib.h>
extern const contexts_service_v1 *fixture_service(void);
extern void fixture_adapter_init(const risc_runtime_api_v1*);
static const contexts_service_v1 *service_api;
static unsigned retains,shared_refusals,shared_acquires,data_stats;
static bool appdata_live;
static uint32_t fixture_millis(void){return 1000;}
static uint8_t disk[3][CF_STORE_MAX],before[3][CF_STORE_MAX];static uint32_t sizes[3];static uint64_t revision=17;static unsigned writes;
static int file(const char*n){if(!strcmp(n,"rf-events-a.rft"))return 0;if(!strcmp(n,"rf-events-b.rft"))return 1;if(!strcmp(n,"context-fingerprints.cfp"))return 2;return -1;}
static int32_t stat_data(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;assert(appdata_live);*s=0;*r=0;data_stats++;int f=file(n);if(f<0)return RISC_APP_DATA_NOT_FOUND;*s=sizes[f];*r=revision;return 0;}
static int32_t read_data(void*c,const char*n,uint64_t r,void*b,uint32_t z,uint32_t*s,uint64_t*a){(void)c;assert(appdata_live);*s=0;*a=0;int f=file(n);if(f<0)return RISC_APP_DATA_NOT_FOUND;if(r!=revision)return RISC_APP_DATA_STALE;if(z<sizes[f])return RISC_APP_DATA_BUFFER_SMALL;memcpy(b,disk[f],sizes[f]);*s=sizes[f];*a=revision;return 0;}
static int32_t replace_data(void*c,const char*n,uint64_t r,const void*b,uint32_t z){(void)c;assert(appdata_live&&file(n)==2&&r==revision&&z<=CF_STORE_MAX);memcpy(disk[2],b,z);sizes[2]=z;revision++;writes++;return 0;}
static const risc_app_data_v1 data_api={1,sizeof(data_api),(void*)3,stat_data,read_data,replace_data};
/* Mirrors Runtime.cpp:547-551: one appDataContext_ per invocation, shared
 * between storage.app-data and storage.shared-data. No service behavior mocked. */
/* The provisioned RTC may have an unset date, but acquisition succeeds. */
static bool fixture_rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;memset(t,0,sizeof(*t));return true;}
static const twatch_rtc_api_v1 fixture_rtc={.api_version=2,.struct_size=sizeof(fixture_rtc),.read=fixture_rtc_read};
static bool acquire_raw(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 assert(g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation);
 printf("ACQUIRE %s api=%u instance=%llu live_appdata=%u\n",n,v,(unsigned long long)i,appdata_live);
 if(!strcmp(n,"storage.app-data")||!strcmp(n,"storage.shared-data")){
  if(appdata_live){assert(!strcmp(n,"storage.shared-data"));shared_refusals++;return false;}
  appdata_live=true;g->api=&data_api;if(!strcmp(n,"storage.shared-data"))shared_acquires++;
 }else if(!strcmp(n,"contexts.service")){g->api=service_api;}
 else if(!strcmp(n,"runtime.realtime"))return false;
 else if(!strcmp(n,"rtc.clock")){assert(v==2&&i==0);g->api=&fixture_rtc;}
 else {fprintf(stderr,"unexpected acquire: %s\n",n);abort();}
 g->slot=1;g->generation=1;return true;
}
static bool release_raw(risc_runtime_capability_v1*g){assert(g->api);if(g->api==&data_api)appdata_live=false;g->api=0;g->slot=g->generation=0;return true;}
static bool retain_raw(void){retains++;puts("RETAIN INVOCATION");return true;}
static void yield_raw(uint32_t n){(void)n;}
static bool diagnostic_raw(const char*s){puts(s);return true;}
static const risc_runtime_api_v1 raw={.api_version=1,.struct_size=sizeof(raw),.acquire=acquire_raw,.release=release_raw,.retain_invocation=retain_raw,.yield_ms=yield_raw,.diagnostic=diagnostic_raw};
int main(int argc,char**argv){
 setbuf(stdout,NULL);
 bool checkpoint_first=argc>1&&!strcmp(argv[1],"checkpoint-first");
 service_api=fixture_service();fixture_adapter_init(&raw);
 static const t5_app_api_v1 fixture_app={.millis=fixture_millis};app=&fixture_app;runtime=waterfall_runtime=&raw;
 rf_bulk=&rf_resident_bulk;prefs=rf_preferences_default();
 static cf_bank seed,decoded;seed.generation=1;cf_profile*p=&seed.profiles[6];p->kind=CF_ROOM;p->present[CF_RF]=1;p->weight[CF_RF]=1;p->feature[CF_RF].identity=99;strcpy(p->name,"SavedRoom");sizes[2]=cf_encode_source(&seed,CF_RF,disk[2],sizeof(disk[2]));assert(sizes[2]);
 static rt_library events;events.identity=prefs.identity;events.generation[0]=events.generation[1]=1;events.labels[0].present=true;events.labels[0].next_id=1;events.labels[0].shift_limit=1;strcpy(events.labels[0].name,"SavedEvent");sizes[0]=rt_bank_encode(&events,0,disk[0],sizeof(disk[0]));sizes[1]=rt_bank_encode(&events,1,disk[1],sizeof(disk[1]));assert(sizes[0]&&sizes[1]);memcpy(before,disk,sizeof(disk));
 /* Foreground's contexts_controls_load stops unopened contexts, never opens it. */
 assert(portable_contexts_stop());assert(!retains);
 if(checkpoint_first){assert(portable_contexts_models_save());assert(!retains&&!appdata_live);}
 event_restore();assert(!appdata_live&&!retains);
 puts("ENTER cfa_restore");cfa_restore();puts("RETURN cfa_restore");
 printf("STARTUP retains=%u refusals=%u shared=%u failed=%u loaded=%u live=%u\n",retains,shared_refusals,shared_acquires,cfa_failed,cfa_loaded,appdata_live);
 assert(!retains&&!shared_refusals&&shared_acquires==2&&!cfa_failed&&cfa_loaded&&!appdata_live);
 assert(!writes&&!memcmp(before,disk,sizeof(disk))&&!strcmp(event_library.labels[0].name,"SavedEvent")&&!strcmp(cfa_bank.profiles[6].name,"SavedRoom"));
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(service_api);assert(fp);
 contexts_profile_v1 edit={.struct_size=sizeof(edit),.index=0,.operation=CONTEXTS_PROFILE_RENAME,.new_name="RenamedRoom"};assert(fp->fingerprint(fp->base.context,CONTEXTS_FP_PROFILE,&edit));
 contexts_fingerprint_status_v1 state={.struct_size=sizeof(state)};assert(fp->fingerprint(fp->base.context,CONTEXTS_FP_STATUS,&state)&&state.dirty_sources==CONTEXTS_RADIO);
 /* The outer RF fence must checkpoint dirty Contexts before borrowing its native app-data grant. */
 event_files.ready=0;event_load();assert(!retains&&!appdata_live&&event_files.ready==3&&writes==1&&shared_acquires==3&&!shared_refusals);
 assert(fp->fingerprint(fp->base.context,CONTEXTS_FP_STATUS,&state)&&state.dirty_sources==0);
 assert(cf_decode_source(&decoded,CF_RF,disk[2],sizes[2])&&!strcmp(decoded.profiles[6].name,"RenamedRoom"));
 assert(!memcmp(before[0],disk[0],sizeof(disk[0]))&&!memcmp(before[1],disk[1],sizeof(disk[1]))&&!strcmp(event_library.labels[0].name,"SavedEvent"));
 assert(portable_contexts_models_save()&&!retains&&!appdata_live&&writes==1);
 puts("PASS dirty Contexts checkpoint before RF reacquire: persisted slot/name, clean release, no retention, saved event bytes unchanged");
 return 0;
}
