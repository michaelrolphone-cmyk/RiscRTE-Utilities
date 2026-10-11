#ifndef CONTEXT_FINGERPRINT_SERVICE_H
#define CONTEXT_FINGERPRINT_SERVICE_H
#include "ContextsServiceV1.h"
#include "ContextsRules.h"
/* Optional suffix: the complete existing service ABI is an unchanged prefix. */
enum { CONTEXTS_FP_STATUS=1,CONTEXTS_FP_CONFIG=2,CONTEXTS_FP_IMPORT=3,
       CONTEXTS_FP_EXPORT=4,CONTEXTS_FP_SAVED=5,CONTEXTS_FP_CONFIRM=6,
       CONTEXTS_FP_PROFILE=7,CONTEXTS_FP_LEARN=8,CONTEXTS_FP_RULES=9,
       CONTEXTS_FP_EVALUATE=10,CONTEXTS_FP_WORKFLOW=11,CONTEXTS_FP_LOG=12 };
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
enum { CONTEXTS_PROFILE_READ,CONTEXTS_PROFILE_RENAME,CONTEXTS_PROFILE_DELETE };
enum { CONTEXTS_LEARN_BEGIN=1,CONTEXTS_LEARN_POLL,CONTEXTS_LEARN_SAVE,CONTEXTS_LEARN_CANCEL };
enum { CONTEXTS_LEARN_IDLE,CONTEXTS_LEARN_WAITING,CONTEXTS_LEARN_RECORDING,CONTEXTS_LEARN_READY };
typedef struct {uint32_t struct_size,operation,index,kind,sources,samples[2],flags[2];uint64_t updated[2];char name[17],new_name[17];} contexts_profile_v1;
typedef struct {uint32_t struct_size,operation,kind,state,progress,samples,sources;char name[17];} contexts_learning_v1;
typedef struct {uint32_t struct_size;bool set,available;cr_store store;} contexts_rules_v1;
typedef struct {uint32_t struct_size;cr_observation observation;cr_result result;} contexts_evaluate_v1;
/* Claim advances the service-owned cursor BEFORE execution. A failed action is
 * logged and never replayed after app handoff. The next step remains pending. */
typedef struct {uint32_t struct_size,rule,index,result;bool claim,available;cr_step step;} contexts_workflow_v1;
typedef struct {uint32_t struct_size,index,count;cr_log entry;} contexts_log_v1;
typedef struct {
    contexts_service_v1 base;
    uint32_t fingerprint_abi; /* CFP1; reject unrelated future ABI suffixes. */
    bool (*fingerprint)(void *,uint32_t operation,void *request);
} contexts_fingerprint_service_v1;
static inline const contexts_fingerprint_service_v1 *contexts_fingerprint_api(const contexts_service_v1 *p){
    return p&&p->struct_size>=sizeof(contexts_fingerprint_service_v1)&&((const contexts_fingerprint_service_v1*)p)->fingerprint_abi==0x31504643u&&((const contexts_fingerprint_service_v1*)p)->fingerprint?(const contexts_fingerprint_service_v1*)p:NULL;
}
#endif
