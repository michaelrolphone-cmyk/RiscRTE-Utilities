#ifndef UTILITIES_POINTS_SERVICE_CONTEXT_H
#define UTILITIES_POINTS_SERVICE_CONTEXT_H
#ifndef ALARM_NATIVE_UTC
#error "Points service context requires explicit native UTC selection"
#endif
#include "AlarmServiceExtensionV2.h"
#include "PointsRecords.h"
#define POINTS_SERVICE_CONTEXT_TAG UINT32_C(0x50544332)
#define POINTS_SERVICE_CONTEXT_VERSION 1u
#define POINTS_CONTEXT_ZONE_BYTES 40u
typedef struct {
    uint32_t struct_size;
    uint8_t config[POINTS_RECORD_SIZE];
    char timezone_id[POINTS_CONTEXT_ZONE_BYTES];
    uint32_t timezone_index,sampled_utc,effective_created,snapshot;
} points_service_context;
typedef struct {
    uint32_t tag,version;
    int32_t (*context)(void *,points_service_context *);
} points_service_context_suffix;
typedef struct {alarm_service_descriptor_v2 base;points_service_context_suffix points;} points_context_service_v2;
typedef struct {alarm_clock_edit_descriptor_v2 base;points_service_context_suffix points;} points_clock_edit_context_service_v2;
/* Copy-only read of a reconciled service snapshot. Never write occurrence
 * storage or silently accept a projection snapshot for another config/zone.
 * The effective UTC creation floor belongs to exact config/provenance; current
 * zone identifies this projection, not a new lifetime for clock authorization.
 * The caller uses sampled_utc and effective_created together for projection;
 * a refused or stale sample leaves the caller's output unchanged. */
static inline int32_t points_service_read_context(const alarm_service_v1 *api,
    const points_config *config,const char zone[POINTS_CONTEXT_ZONE_BYTES],
    uint32_t zone_index,points_service_context *out) {
    if(!config||!points_config_valid(config)||!zone||zone_index>=419u||!out||out->struct_size<sizeof(*out))return ALARM_INVALID;
    unsigned end=0;while(end<POINTS_CONTEXT_ZONE_BYTES&&zone[end])++end;
    if(!end||end==POINTS_CONTEXT_ZONE_BYTES)return ALARM_INVALID;
    for(unsigned i=end+1;i<POINTS_CONTEXT_ZONE_BYTES;i++)if(zone[i])return ALARM_INVALID;
    const points_service_context_suffix *p=(const points_service_context_suffix *)alarm_service_extension_v2(
        api,sizeof(*p),POINTS_SERVICE_CONTEXT_TAG,POINTS_SERVICE_CONTEXT_VERSION);
    if(!p||!p->context)return ALARM_INVALID;
    points_service_context value={.struct_size=sizeof(value)};
    int32_t result=p->context(api->context,&value);
    if(result!=ALARM_OK)return result;
    uint8_t encoded[POINTS_RECORD_SIZE];points_config_encode(config,encoded);
    if(value.struct_size!=sizeof(value)||!value.snapshot||value.sampled_utc>ALARM_RTC_MAX||
       value.effective_created>config->created||value.sampled_utc<value.effective_created)return ALARM_INVALID;
    if(memcmp(encoded,value.config,sizeof(encoded))||value.timezone_index!=zone_index||
       memcmp(zone,value.timezone_id,POINTS_CONTEXT_ZONE_BYTES))return ALARM_STALE;
    *out=value;return ALARM_OK;
}
#if defined(__cplusplus)
#define POINTS_CONTEXT_ASSERT static_assert
#else
#define POINTS_CONTEXT_ASSERT _Static_assert
#endif
POINTS_CONTEXT_ASSERT(sizeof(points_service_context)==124,"copied Points context ABI");
POINTS_CONTEXT_ASSERT(offsetof(points_clock_edit_context_service_v2,points)==sizeof(alarm_clock_edit_descriptor_v2),"editor prefix");
#undef POINTS_CONTEXT_ASSERT
#endif
