#include "PointsServiceContext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static points_service_context provided;
static int32_t outcome;
static unsigned called;
static int32_t basic(void *c){(void)c;return ALARM_OK;}
static int32_t status(void *c,alarm_status_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t ack(void *c,const alarm_token_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t sleep_prepare(void *c,alarm_sleep_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t resume(void *c,const alarm_sleep_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t begin(void *c,uint32_t t,alarm_clock_edit_permit_v2 *p){(void)c;(void)t;(void)p;return ALARM_OK;}
static int32_t finish(void *c,const alarm_clock_edit_permit_v2 *p){(void)c;(void)p;return ALARM_OK;}
static int32_t edit_status(void *c,alarm_clock_edit_status_v2 *p){(void)c;(void)p;return ALARM_OK;}
static int32_t context(void *c,points_service_context *p){assert(c==&called);called++;*p=provided;return outcome;}
static alarm_service_descriptor_v2 descriptor(uint32_t size){
 alarm_service_descriptor_v2 d={{2,size,&called,status,basic,basic,ack,sleep_prepare,basic},ALARM_SERVICE_DESCRIPTOR_TAG,1,0,ALARM_DESCRIPTOR_RESUME_SLEEP|ALARM_DESCRIPTOR_CLOCK_EDIT,resume};return d;
}
static points_config config;
static const char zone[40]="UTC";
static void rejected(const alarm_service_v1 *api,int expected){
 points_service_context out;memset(&out,0xa5,sizeof(out));out.struct_size=sizeof(out);points_service_context before=out;
 assert(points_service_read_context(api,&config,zone,0,&out)==expected);assert(!memcmp(&out,&before,sizeof(out)));
}
int main(void){
 config=points_default_config();config.created=100;
 provided=(points_service_context){.struct_size=sizeof(provided),.timezone_id="UTC",.sampled_utc=200,.effective_created=80,.snapshot=1};
 points_config_encode(&config,provided.config);
 points_clock_edit_context_service_v2 mixed={{descriptor(sizeof(mixed)),ALARM_CLOCK_EDIT_TAG,1,begin,finish,finish,edit_status},{POINTS_SERVICE_CONTEXT_TAG,1,context}};
 points_service_context out={.struct_size=sizeof(out)};
 assert(points_service_read_context(&mixed.base.service.base,&config,zone,0,&out)==ALARM_OK);assert(out.effective_created==80);
 const points_service_context good=provided;
 for(int rc=ALARM_RETAINED;rc<=ALARM_PENDING;rc++)if(rc){outcome=rc;rejected(&mixed.base.service.base,rc);}
 outcome=0;
 provided.config[20]^=1;rejected(&mixed.base.service.base,ALARM_STALE);provided=good;
 provided.timezone_id[1]='X';rejected(&mixed.base.service.base,ALARM_STALE);provided=good;
 provided.timezone_index=1;rejected(&mixed.base.service.base,ALARM_STALE);provided=good;
 provided.struct_size--;rejected(&mixed.base.service.base,ALARM_INVALID);provided=good;
 provided.snapshot=0;rejected(&mixed.base.service.base,ALARM_INVALID);provided=good;
 provided.effective_created=config.created+1;rejected(&mixed.base.service.base,ALARM_INVALID);provided=good;
 provided.sampled_utc=provided.effective_created-1;rejected(&mixed.base.service.base,ALARM_INVALID);provided=good;
 provided.sampled_utc=ALARM_RTC_MAX+1;rejected(&mixed.base.service.base,ALARM_INVALID);provided=good;
 unsigned before=called;
 for(size_t n=sizeof(alarm_service_v1);n<sizeof(mixed);n++){
  alarm_service_v1 *short_table=malloc(n);assert(short_table);memcpy(short_table,&mixed,n);short_table->struct_size=(uint32_t)n;
  rejected(short_table,ALARM_INVALID);free(short_table);
 }
 assert(called==before);
 points_clock_edit_context_service_v2 bad=mixed;bad.points.version++;rejected(&bad.base.service.base,ALARM_INVALID);
 bad=mixed;bad.points.tag++;rejected(&bad.base.service.base,ALARM_INVALID);
 bad=mixed;bad.points.context=NULL;rejected(&bad.base.service.base,ALARM_INVALID);
 bad=mixed;bad.base.tag++;rejected(&bad.base.service.base,ALARM_INVALID);
 assert(called==before);
 char invalid_zone[40]="UTC";invalid_zone[8]='x';
 assert(points_service_read_context(&mixed.base.service.base,&config,invalid_zone,0,&out)==ALARM_INVALID);
 assert(points_service_read_context(&mixed.base.service.base,&config,zone,0,NULL)==ALARM_INVALID);
 puts("Points service context identity and outcomes passed");return 0;
}
