#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "AlarmServiceFacadeV2.h"
#include "AlarmClockChangeV1.h"
#include "PointsServiceProjection.h"
#include "RiscProviderV2.h"
extern const risc_driver_v2 *alarm_facade_driver_get(uint32_t);
static unsigned calls;static int32_t result;static unsigned owner_context;
static const alarm_token_v1 *seen_token;static const alarm_sleep_v1 *seen_ticket;
static int32_t status(void*c,alarm_status_v1*out){assert(c==&owner_context);++calls;out->snapshot=owner_context;return result;}
static int32_t simple(void*c){assert(c==&owner_context);++calls;return result;}
static int32_t ack(void*c,const alarm_token_v1*t){seen_token=t;return simple(c);}
static int32_t prepare(void*c,alarm_sleep_v1*t){t->snapshot=owner_context;return simple(c);}
static int32_t resume(void*c,const alarm_sleep_v1*t){seen_ticket=t;return simple(c);}
static int32_t changed(void*c,uint32_t seconds){owner_context=seconds;return simple(c);}
static int32_t project(void*c,points_catalog_projection*p){(void)p;return simple(c);}
static alarm_service_v1 base(void){
 return (alarm_service_v1){1,sizeof(alarm_service_v1),&owner_context,status,simple,simple,ack,prepare,simple};
}
int main(void){
 const risc_driver_v2*d=alarm_facade_driver_get(2);assert(d&&!alarm_facade_driver_get(1));
 assert(!strcmp(d->capability_id,ALARM_SERVICE_FACADE_CAPABILITY)&&d->capability_api==2);
 assert(!d->start(NULL,0));unsigned checks=1;
#ifdef ALARM_FACADE_LEGACY_CLOCK_CHANGE
 alarm_clock_change_v1 owner={{base(),resume},ALARM_CLOCK_CHANGE_TAG,1,changed};
 owner.sleep.base.struct_size=sizeof(owner);
 alarm_service_v1*original=&owner.sleep.base;
 (void)project;
#else
 points_service_v2 owner={{base(),ALARM_SERVICE_DESCRIPTOR_TAG,1,ALARM_MODE_BOTH,
  ALARM_DESCRIPTOR_RESUME_SLEEP,resume},{POINTS_SERVICE_PROJECTION_TAG,1,project}};
 owner.base.base.api_version=2;owner.base.base.struct_size=sizeof(owner);
 alarm_service_v1*original=&owner.base.base;
 (void)changed;
#endif
 risc_provider_dependency_v1 dependency={ALARM_SERVICE_CAPABILITY,original->api_version,original};
 for(unsigned fault=0;fault<9;fault++){
  alarm_service_v1 saved=*original;risc_provider_dependency_v1 bad=dependency;
  switch(fault){case 0:bad.capability_id="wrong";break;case 1:bad.api_version=99;break;
  case 2:bad.api=NULL;break;case 3:original->api_version=99;break;
  case 4:original->struct_size=sizeof(alarm_service_v1)-1;break;
  case 5:original->status=NULL;break;case 6:original->prepare_sleep=NULL;break;
  case 7:original->acknowledge=NULL;break;case 8:original->stop_only=NULL;break;}
  assert(!d->start(&bad,1)&&!calls);*original=saved;++checks;
 }
 risc_provider_dependency_v1 duplicate[2]={dependency,dependency};
 assert(!d->start(duplicate,2)&&!calls);
#ifdef ALARM_FACADE_LEGACY_CLOCK_CHANGE
 owner.tag=0;assert(!d->start(&dependency,1));owner.tag=ALARM_CLOCK_CHANGE_TAG;
 owner.sleep.resume_sleep=NULL;assert(!d->start(&dependency,1));owner.sleep.resume_sleep=resume;
#else
 owner.base.output_modes=4;assert(!d->start(&dependency,1));owner.base.output_modes=ALARM_MODE_BOTH;
 owner.points.version=99;assert(!d->start(&dependency,1));owner.points.version=1;
 owner.points.projection=NULL;assert(!d->start(&dependency,1));owner.points.projection=project;
#endif
 assert(d->start(&dependency,1)&&!calls);assert(!d->start(&dependency,1));
 const alarm_service_v1*api=d->capability;const alarm_service_descriptor_v2*desc=alarm_service_descriptor(api);
 assert(desc&&desc->output_modes==ALARM_MODE_BOTH&&api->context==original->context);
 assert(api->status==original->status&&api->step==original->step&&api->refresh==original->refresh&&
  api->acknowledge==original->acknowledge&&api->prepare_sleep==original->prepare_sleep&&api->stop_only==original->stop_only);
 for(int32_t rc=-11;rc<=3;rc++){
  result=rc;alarm_status_v1 s={.struct_size=sizeof(s)};alarm_token_v1 token={1,2,3,4};
  alarm_sleep_v1 ticket={.struct_size=sizeof(ticket)};unsigned before=calls;
  assert(api->status(api->context,&s)==rc&&api->step(api->context)==rc&&api->refresh(api->context)==rc);
  assert(api->acknowledge(api->context,&token)==rc&&seen_token==&token);
  assert(api->prepare_sleep(api->context,&ticket)==rc&&ticket.snapshot==owner_context);
  assert(desc->resume_sleep(api->context,&ticket)==rc&&seen_ticket==&ticket);
  assert(api->stop_only(api->context)==rc&&calls==before+7);++checks;
 }
#ifdef ALARM_FACADE_LEGACY_CLOCK_CHANGE
 result=ALARM_PENDING;assert(owner.clock_changed(original->context,42)==ALARM_PENDING);
 alarm_status_v1 copied={.struct_size=sizeof(copied)};
 assert(api->status(api->context,&copied)==ALARM_PENDING&&copied.snapshot==42);
 assert(api->struct_size==sizeof(alarm_service_descriptor_v2));
#else
 points_catalog_projection projection={.struct_size=sizeof(projection)};
 result=ALARM_RETAINED;unsigned before=calls;
 assert(points_service_project(api,&projection)==ALARM_RETAINED&&calls==before+1);
#endif
 unsigned before=calls;assert(d->quiesce()&&calls==before);d->stop();assert(calls==before);
 assert(!alarm_service_descriptor(api));
#ifndef ALARM_FACADE_LEGACY_CLOCK_CHANGE
 for(unsigned modes=0;modes<=ALARM_MODE_BOTH;modes++){
  owner.base.output_modes=modes;owner.base.features=0;owner.base.resume_sleep=NULL;owner.points.tag=0;
  assert(d->start(&dependency,1));desc=alarm_service_descriptor(d->capability);
  assert(desc&&desc->output_modes==modes&&!desc->features&&!desc->resume_sleep);
  assert(desc->base.struct_size==sizeof(*desc));d->stop();++checks;
 }
#endif
 printf("Alarm facade: %u validation/forwarding/shared-owner cases PASS\n",checks);return 0;
}
