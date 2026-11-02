/* One app artifact. Provider discovery is semantic, never target/geometry based. */
#include "PortableAppCatalog.h"
#include "AlarmServiceFacadeV2.h"
#include "RiscResidentContextsV1.h"
static const char *const cs_face_names[]={
 "NOVA","ANALOG","RADAR","HEX","TERMINAL","MINIMAL","BINARY","CHRONO",
 "AURORA","HORIZON","GIANT","MOON","TOPO","ORRERY","FLIP","MATRIX",
 "GRID","RIBBON","YEAR","24H","AGENDA","DATE","DOTS","PROGRESS","NEXT",
 "RING","LADDER","DAYLINE","TRIPLE","STATUS","BOARD","WORKDAY","UP NEXT","CONTEXTS"
};
static portable_app_catalog cs_catalog;
static uint16_t cs_setting_mask,cs_setting_options[CR_SETTINGS];
#define CS_CATALOG_COUNT cs_catalog.count
#define CS_CATALOG(i) cs_catalog.entries[i].manifest
/* Failed/partial acquisition is never treated as hardware absence. */
static int cs_shared_acquire(const char *name,uint32_t version,risc_runtime_capability_v1 *g){
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=ctx_runtime->acquire(name,version,0,g);
 if(!cs_alive())return -1;
 if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):(g->api||g->slot||g->generation))){cs_retain();return -1;}
 return ok?1:0;
}
static bool cs_shared_alarm(void){
 int found=cs_shared_acquire(ALARM_SERVICE_FACADE_CAPABILITY,ALARM_SERVICE_FACADE_API,&cs_alarm_grant);
 if(found<0)return false;
 if(found){
  if(!alarm_service_descriptor(cs_alarm_grant.api)){cs_retain();return false;}
  return true;
 }
 found=cs_shared_acquire(ALARM_SERVICE_CAPABILITY,1,&cs_alarm_grant);
 if(found<0)return false;
 if(found){
  const alarm_service_v1*a=cs_alarm_grant.api;
  if(a->api_version!=1||a->struct_size<sizeof(*a)||!a->status||!a->refresh){cs_retain();return false;}
 }
 return true;
}
static bool cs_catalog_result(int32_t rc){
 if(!cs_alive())return false;
 if(!portable_catalog_result(&cs_catalog,rc)&&cs_catalog.uncertain){cs_retain();return false;}
 return rc==RISC_APP_CATALOG_OK;
}
static bool cs_shared_catalog(void){
 int found=cs_shared_acquire(RISC_APP_CATALOG_CAPABILITY,1,&cs_catalog.grant);
 if(found<=0)return found==0;
 const risc_app_catalog_v1 *api=cs_catalog.grant.api;
 if(api->api_version!=1||api->struct_size<sizeof(*api)||!api->snapshot||!api->get){cs_retain();return false;}
 cs_catalog.api=api;cs_catalog.initialized=true;
 risc_app_catalog_snapshot_v1 snapshot={.struct_size=sizeof(snapshot)};
 if(!cs_catalog_result(api->snapshot(api->context,&snapshot)))return cs_alive();
 if(snapshot.struct_size<sizeof(snapshot)||!snapshot.generation||snapshot.count>RISC_APP_CATALOG_MAX)return true;
 cs_catalog.staging=malloc((size_t)snapshot.count*sizeof(*cs_catalog.staging));
 if(snapshot.count&&!cs_catalog.staging)return true;
 for(uint32_t i=0;i<snapshot.count;i++){
  risc_app_catalog_entry_v1 entry={.struct_size=sizeof(entry)};
  if(!cs_catalog_result(api->get(api->context,snapshot.generation,i,&entry)))goto invalid;
  /* The existing persisted workflow ABI carries at most32 route bytes.
   * Reject invalid metadata before the legacy manifest bridge can truncate. */
  if(!memchr(entry.id,0,sizeof(entry.id))||strlen(entry.id)>=sizeof(((t5_app_manifest_t*)0)->file_name)-3u||
     !portable_catalog_entry(&entry,&cs_catalog.staging[i]))goto invalid;
  for(uint32_t j=0;j<i;j++)if(!strcmp(cs_catalog.staging[j].route,cs_catalog.staging[i].route)||
    !strcmp(cs_catalog.staging[j].manifest.file_name,cs_catalog.staging[i].manifest.file_name))goto invalid;
 }
 risc_app_catalog_snapshot_v1 after={.struct_size=sizeof(after)};
 if(!cs_catalog_result(api->snapshot(api->context,&after)))goto invalid;
 if(after.struct_size<sizeof(after)||after.count!=snapshot.count||after.generation!=snapshot.generation)goto invalid;
 cs_catalog.entries=cs_catalog.staging;cs_catalog.staging=NULL;cs_catalog.count=snapshot.count;cs_catalog.generation=snapshot.generation;
 return true;
invalid:
 if(!cs_alive()||cs_catalog.uncertain)return false;
 portable_catalog_clear(&cs_catalog);return true;
}
static bool cs_shared_launch_valid(const char *route){
 for(unsigned i=0;i<cs_catalog.count;i++){
  if(!CS_CATALOG(i).compatible||strcmp(CS_CATALOG(i).file_name,route))continue;
  risc_app_catalog_entry_v1 current={.struct_size=sizeof(current)};
  int rc=cs_catalog.api->get(cs_catalog.api->context,cs_catalog.generation,i,&current);
  if(!cs_catalog_result(rc)){if(cs_alive())portable_catalog_clear(&cs_catalog);return false;}
  return current.struct_size>=sizeof(current)&&current.flags==RISC_APP_CATALOG_AVAILABLE&&
   portable_catalog_path(current.file_name,sizeof(current.file_name))&&!strcmp(current.file_name,route);
 }
 return false;
}
static bool cs_shared_close(void){
 if(cs_catalog.uncertain){cs_retain();return false;}
 if(!cs_release(&cs_catalog.grant))return false;
 portable_catalog_clear(&cs_catalog);cs_catalog=(portable_app_catalog){0};return true;
}
static int cs_shared_query(uint32_t reason){
 risc_resident_request_v1 request={sizeof(request),reason,0,0};
 risc_resident_reply_v1 reply={sizeof(reply),0};
 int rc=cs_resident.client.checkpoint(cs_resident.client.invocation,&request,&reply);
 if(!cs_alive())return -1;
 if(rc==RISC_RESIDENT_RETAINED||reply.struct_size!=sizeof(reply)||reply.flags){cs_retain();return -1;}
 if(rc==RISC_RESIDENT_OK)return 1;
 if(rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_DENIED||rc==RISC_RESIDENT_INVALID)return 0;
 cs_retain();return -1;
}
static bool cs_shared_settings(void){
 /* Explicit closed-scene preparation is distinct from the pure queries. */
 if(cs_resident.enabled&&cs_shared_query(RISC_CONTEXTS_SETTINGS_PREPARE)<0)return false;
 uint16_t mask=0;memset(cs_setting_options,0,sizeof(cs_setting_options));
 if(cs_resident.enabled)for(unsigned field=0;field<CR_SETTINGS;field++){
  int available=cs_shared_query(RISC_CONTEXTS_SETTING_QUERY(field));
  if(available<0)return false;
  if(available)mask|=(uint16_t)(1u<<field);
  if(available&&(field==CR_POWER||field==CR_NOTIFY)){
   for(unsigned value=0;value<(field==CR_POWER?3u:4u);value++){
    int supported=cs_shared_query(RISC_CONTEXTS_SETTING_OPTION_QUERY(field,value));
    if(supported<0)return false;
    if(supported)cs_setting_options[field]|=(uint16_t)(1u<<value);
   }
   if(!cs_setting_options[field])mask&=(uint16_t)~(1u<<field);
  }
 }
 cs_setting_mask=mask;return true;
}
static bool cs_shared_setting_value(unsigned field,int32_t value){
 if(value==CR_KEEP)return true;
 if(!(cs_setting_mask&(1u<<field)))return false;
 if(field==CR_POWER||field==CR_NOTIFY)
  return value>=0&&value<16&&(cs_setting_options[field]&(1u<<value));
 return true;
}
static int cs_shared_option_max(unsigned field){
 unsigned count=0;
 for(unsigned i=0;i<16;i++)if(cs_setting_options[field]&(1u<<i))count++;
 return (int)count;
}
static int32_t cs_shared_option_value(unsigned field,int index){
 if(index<=0)return CR_KEEP;
 for(unsigned i=0;i<16;i++)if(cs_setting_options[field]&(1u<<i))if(!--index)return (int32_t)i;
 return CR_KEEP;
}
static bool cs_shared_prepare(void){
 cs_catalog=(portable_app_catalog){0};cs_setting_mask=0;
 if(!cs_shared_alarm())return false;
 if(!cs_shared_catalog())return false;
 if(cs_catalog.uncertain){cs_retain();return false;}
 for(unsigned i=0;i<cs_catalog.count;i++)
  if(strlen(cs_catalog.entries[i].route)>32||strcmp(cs_catalog.entries[i].route,CS_CATALOG(i).file_name))
   CS_CATALOG(i).compatible=false;
 if(!cs_atomic_detect())return false;
 /* Applicability is supplied by the active host, not a product build mask. */
 return cs_shared_settings();
}
