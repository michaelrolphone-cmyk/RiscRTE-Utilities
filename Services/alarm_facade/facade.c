/* No scheduler, poll, storage, RTC or output ownership. Every callable pointer
 * and its context belong to the one dependency owner, pinned by ProviderGraph.
 * Existing alarm.service consumers (including clock-change editors) coexist. */
#include "AlarmServiceFacadeV2.h"
#include "AlarmClockChangeV1.h"
#include "PointsServiceProjection.h"
#include "RiscProviderV2.h"
#include <string.h>
#if defined(ALARM_FACADE_LEGACY_CLOCK_CHANGE) == defined(ALARM_FACADE_TAGGED_OWNER)
#error "Select exactly one source-owned facade dependency contract"
#endif
static points_service_v2 facade;
static bool started;
static bool start(const risc_provider_dependency_v1 *dependencies,size_t count) {
    if(started||!dependencies||count!=1)return false;
    const risc_provider_dependency_v1 *d=&dependencies[0];
    if(!d->capability_id||strcmp(d->capability_id,ALARM_SERVICE_CAPABILITY)||!d->api)return false;
    const alarm_service_v1 *owner=d->api;
    alarm_service_descriptor_v2 descriptor;
    points_service_projection_suffix points={0};
#ifdef ALARM_FACADE_LEGACY_CLOCK_CHANGE
    /* Only this explicitly tagged historical audio/haptic contract supplies
     * the legacy resume offset. Never infer a suffix from table size alone. */
    if(d->api_version!=1||owner->api_version!=1)return false;
    const alarm_clock_change_v1 *legacy=alarm_clock_change_control(owner);
    if(!legacy||!owner->status||!owner->step||!owner->refresh||!owner->acknowledge||
       !owner->prepare_sleep||!owner->stop_only||!legacy->sleep.resume_sleep)return false;
    descriptor=(alarm_service_descriptor_v2){*owner,ALARM_SERVICE_DESCRIPTOR_TAG,
        ALARM_SERVICE_DESCRIPTOR_VERSION,ALARM_MODE_BOTH,ALARM_DESCRIPTOR_RESUME_SLEEP,
        legacy->sleep.resume_sleep};
#else
    if(d->api_version!=2)return false;
    const alarm_service_descriptor_v2 *tagged=alarm_service_descriptor(owner);
    if(!tagged)return false;
    descriptor=*tagged;
    if(owner->struct_size>=sizeof(points_service_v2)){
        const points_service_projection_suffix *p=&((const points_service_v2*)owner)->points;
        if(p->tag==POINTS_SERVICE_PROJECTION_TAG){
            if(p->version!=POINTS_SERVICE_PROJECTION_VERSION||!p->projection)return false;
            points=*p;
        }
    }
#endif
    descriptor.base.api_version=ALARM_SERVICE_FACADE_API;
    descriptor.base.struct_size=points.projection?sizeof(facade):sizeof(descriptor);
    facade=(points_service_v2){descriptor,points};started=true;return true;
}
/* There are no facade-owned sessions to stop. Runtime independently quiesces
 * the still-pinned owner; this does not attest that its outputs have stopped. */
static bool quiesce(void){return true;}
static void stop(void){started=false;memset(&facade,0,sizeof(facade));}
static const risc_driver_v2 driver={2,sizeof(driver),"alarm-facade",
    ALARM_SERVICE_FACADE_CAPABILITY,ALARM_SERVICE_FACADE_API,&facade,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){
    return abi==2?&driver:NULL;
}
