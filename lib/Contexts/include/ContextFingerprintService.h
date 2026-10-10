#ifndef CONTEXT_FINGERPRINT_SERVICE_H
#define CONTEXT_FINGERPRINT_SERVICE_H
#include "ContextsServiceV1.h"
/* Optional suffix: the complete existing service ABI is an unchanged prefix. */
enum { CONTEXTS_FP_STATUS=1,CONTEXTS_FP_CONFIG=2,CONTEXTS_FP_IMPORT=3,
       CONTEXTS_FP_EXPORT=4,CONTEXTS_FP_SAVED=5,CONTEXTS_FP_CONFIRM=6 };
typedef struct {
    uint32_t struct_size,sources,generation,dirty_sources;
    int32_t room_slot,event_slot;
    uint32_t room_confidence,event_confidence;
    char room_name[17],event_name[17];
    uint32_t feature_flags[2],fresh_windows[2];
    bool temporal_only,room_ambiguous,event_ambiguous;
} contexts_fingerprint_status_v1;
typedef struct {
    uint32_t struct_size,sources;
    bool temporal_only;
    uint64_t utc_seconds; /* zero means wall clock unavailable, never uptime. */
} contexts_fingerprint_config_v1;
typedef struct {
    uint32_t struct_size,source,size,capacity,generation;
    void *bytes;
} contexts_fingerprint_record_v1;
typedef struct {
    uint32_t struct_size,kind;
    char name[17];
} contexts_fingerprint_confirm_v1;
typedef struct {
    contexts_service_v1 base;
    uint32_t fingerprint_abi; /* CFP1; reject unrelated future ABI suffixes. */
    bool (*fingerprint)(void *,uint32_t operation,void *request);
} contexts_fingerprint_service_v1;
static inline const contexts_fingerprint_service_v1 *contexts_fingerprint_api(const contexts_service_v1 *p){
    return p&&p->struct_size>=sizeof(contexts_fingerprint_service_v1)&&((const contexts_fingerprint_service_v1*)p)->fingerprint_abi==0x31504643u&&((const contexts_fingerprint_service_v1*)p)->fingerprint?(const contexts_fingerprint_service_v1*)p:NULL;
}
#endif
