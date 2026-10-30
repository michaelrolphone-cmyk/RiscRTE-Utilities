#include "PointsServiceProjection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned projected;
static int32_t basic(void *c){(void)c;return ALARM_OK;}
static int32_t status(void *c,alarm_status_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t ack(void *c,const alarm_token_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t sleep_prepare(void *c,alarm_sleep_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t resume(void *c,const alarm_sleep_v1 *s){(void)c;(void)s;return ALARM_OK;}
static int32_t begin(void *c,uint32_t t,alarm_clock_edit_permit_v2 *p){(void)c;(void)t;(void)p;return ALARM_OK;}
static int32_t finish(void *c,const alarm_clock_edit_permit_v2 *p){(void)c;(void)p;return ALARM_OK;}
static int32_t edit_status(void *c,alarm_clock_edit_status_v2 *p){(void)c;(void)p;return ALARM_OK;}
static int32_t project(void *c,points_catalog_projection *p){assert(c==&projected);projected++;p->catalog_revision=17;return ALARM_OK;}
static alarm_service_descriptor_v2 descriptor(uint32_t size,uint32_t features){
 alarm_service_descriptor_v2 d={{2,size,&projected,status,basic,basic,ack,sleep_prepare,basic},ALARM_SERVICE_DESCRIPTOR_TAG,1,0,features,resume};return d;
}
static void rejected(const alarm_service_v1 *api){
 points_catalog_projection p={.struct_size=sizeof(p)};unsigned before=projected;
 assert(points_service_project(api,&p)==ALARM_INVALID);assert(projected==before);assert(p.catalog_revision==0);
}
int main(void){
 points_catalog_projection out={.struct_size=sizeof(out)};
 points_service_v1 original={{{1,sizeof(original),&projected,status,basic,basic,ack,sleep_prepare,basic},resume},{POINTS_SERVICE_PROJECTION_TAG,1,project}};
 assert(points_service_project(&original.base.base,&out)==ALARM_OK);assert(out.catalog_revision==17);
 points_service_v1 malformed=original;malformed.points.tag^=1;rejected(&malformed.base.base);
 malformed=original;malformed.points.version++;rejected(&malformed.base.base);
 malformed=original;malformed.points.projection=NULL;rejected(&malformed.base.base);
 malformed=original;malformed.base.base.struct_size=sizeof(original)-1;rejected(&malformed.base.base);
 points_service_v2 historical={descriptor(sizeof(historical),ALARM_DESCRIPTOR_RESUME_SLEEP),{POINTS_SERVICE_PROJECTION_TAG,1,project}};
 assert(points_service_project(&historical.base.base,&out)==ALARM_OK);assert(out.catalog_revision==17);
 points_clock_edit_service_v2 mixed={{descriptor(sizeof(mixed),ALARM_DESCRIPTOR_RESUME_SLEEP|ALARM_DESCRIPTOR_CLOCK_EDIT),ALARM_CLOCK_EDIT_TAG,1,begin,finish,finish,edit_status},{POINTS_SERVICE_PROJECTION_TAG,1,project}};
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
 assert(alarm_clock_edit_descriptor(&mixed.base.service.base)==&mixed.base);
 assert(points_service_project(&mixed.base.service.base,&out)==ALARM_OK);
 for(uint32_t n=0;n<sizeof(mixed);n++){points_clock_edit_service_v2 bad=mixed;bad.base.service.base.struct_size=n;rejected(&bad.base.service.base);}
 for(size_t n=sizeof(alarm_service_v1);n<sizeof(mixed);n++){
  alarm_service_v1 *short_table=malloc(n);assert(short_table);memcpy(short_table,&mixed,n);
  short_table->struct_size=(uint32_t)n;rejected(short_table);free(short_table);
 }
 points_clock_edit_service_v2 bad=mixed;bad.base.tag^=1;rejected(&bad.base.service.base);
 bad=mixed;bad.base.version++;rejected(&bad.base.service.base);
 bad=mixed;bad.base.begin=NULL;rejected(&bad.base.service.base);
 bad=mixed;bad.base.finish=NULL;rejected(&bad.base.service.base);
 bad=mixed;bad.base.cancel=NULL;rejected(&bad.base.service.base);
 bad=mixed;bad.base.edit_status=NULL;rejected(&bad.base.service.base);
 bad=mixed;bad.points.tag^=1;rejected(&bad.base.service.base);
 bad=mixed;bad.points.version++;rejected(&bad.base.service.base);
 bad=mixed;bad.points.projection=NULL;rejected(&bad.base.service.base);
 bad=mixed;bad.base.service.features|=UINT32_C(0x80000000);rejected(&bad.base.service.base);
 /* An editor-only table must never dispatch its begin callback as projection. */
 alarm_clock_edit_descriptor_v2 editor=mixed.base;editor.service.base.struct_size=sizeof(editor);rejected(&editor.service.base);
 /* Removing negotiation must not redirect the Points call to the editor tail. */
 bad=mixed;bad.base.service.features=ALARM_DESCRIPTOR_RESUME_SLEEP;rejected(&bad.base.service.base);
#else
 /* Existing clients continue rejecting a feature they have not selected. */
 rejected(&mixed.base.service.base);assert(!alarm_clock_edit_descriptor(&mixed.base.service.base));
#endif
 rejected(NULL);out.struct_size=sizeof(out)-1;
 assert(points_service_project(&historical.base.base,&out)==ALARM_INVALID);
 assert(points_service_project(&historical.base.base,NULL)==ALARM_INVALID);
 puts("Points/editor descriptor negotiation passed");return 0;
}
