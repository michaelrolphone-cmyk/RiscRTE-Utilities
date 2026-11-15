#ifndef UTILITIES_POINTS_SERVICE_PROJECTION_H
#define UTILITIES_POINTS_SERVICE_PROJECTION_H
#include "AlarmServiceExtensionV2.h"
#include "PointsCatalogProjection.h"
#define POINTS_SERVICE_PROJECTION_TAG UINT32_C(0x50545032)
#define POINTS_SERVICE_PROJECTION_VERSION 1u
typedef struct {uint32_t tag,version;int32_t (*projection)(void *,points_catalog_projection *);} points_service_projection_suffix;
typedef struct {alarm_service_sleep_v1 base;points_service_projection_suffix points;} points_service_v1;
typedef struct {alarm_service_descriptor_v2 base;points_service_projection_suffix points;} points_service_v2;
/* The editor prefix keeps its published layout. Mixed clients negotiate the
 * CLOCK_EDIT feature and validate that complete prefix before reading the
 * appended Points suffix; a bare editor is never a Points provider. */
typedef struct {alarm_clock_edit_descriptor_v2 base;points_service_projection_suffix points;} points_clock_edit_service_v2;
#if defined(__cplusplus)
static_assert(offsetof(points_clock_edit_service_v2,points)==sizeof(alarm_clock_edit_descriptor_v2),"mixed editor prefix");
#else
_Static_assert(offsetof(points_clock_edit_service_v2,points)==sizeof(alarm_clock_edit_descriptor_v2),"mixed editor prefix");
#endif
#if UINTPTR_MAX == UINT32_MAX
#if defined(__cplusplus)
static_assert(sizeof(points_clock_edit_service_v2)==92,"mixed target ABI");
#else
_Static_assert(sizeof(points_clock_edit_service_v2)==92,"mixed target ABI");
#endif
#endif
static inline int32_t points_service_project(const alarm_service_v1 *api,points_catalog_projection *out) {
    if(!api||!out||out->struct_size<sizeof(*out))return ALARM_INVALID;
    const points_service_projection_suffix *p=NULL;
    if(api->api_version==1&&api->struct_size>=sizeof(points_service_v1))p=&((const points_service_v1 *)api)->points;
    else p=(const points_service_projection_suffix *)alarm_service_extension_v2(
        api,sizeof(*p),POINTS_SERVICE_PROJECTION_TAG,POINTS_SERVICE_PROJECTION_VERSION);
    return p&&p->tag==POINTS_SERVICE_PROJECTION_TAG&&p->version==POINTS_SERVICE_PROJECTION_VERSION&&p->projection?
        p->projection(api->context,out):ALARM_INVALID;
}
#endif
