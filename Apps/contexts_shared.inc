/* One app artifact. Provider discovery is semantic, never target/geometry based. */
#include "PortableAppCatalog.h"
#include "AlarmServiceV2.h"
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
 int found=cs_shared_acquire(ALARM_SERVICE_CAPABILITY,2,&cs_alarm_grant);
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
static bool cs_shared_launch_valid(const char *route){
 for(unsigned i=0;i<cs_catalog.count;i++){
  if(strcmp(CS_CATALOG(i).file_name,route))continue;
  portable_app_catalog_launch launch;
  bool ok=portable_catalog_launch_prepare(&cs_catalog,i,&launch);
  if(!cs_alive())return false;
  if(cs_catalog.uncertain){cs_retain();return false;}
  /* Existing rule records have a 32-byte launch identity. Never truncate or
   * reinterpret a longer modern native route as an authorized old filename. */
  return ok&&!strcmp(launch.route,route);
 }
 return false;
}
static bool cs_shared_close(void){
 if(cs_catalog.uncertain){cs_retain();return false;}
 if(!portable_catalog_close(&cs_catalog,ctx_runtime)||!cs_alive()){cs_retain();return false;}
 return true;
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
 (void)portable_catalog_refresh(&cs_catalog,ctx_runtime);
 if(!cs_alive())return false;
 if(cs_catalog.uncertain){cs_retain();return false;}
 for(unsigned i=0;i<cs_catalog.count;i++)
  if(strlen(cs_catalog.entries[i].route)>32||strcmp(cs_catalog.entries[i].route,CS_CATALOG(i).file_name))
   CS_CATALOG(i).compatible=false;
 /* Applicability is supplied by the active host, not a product build mask. */
 return cs_shared_settings();
}
