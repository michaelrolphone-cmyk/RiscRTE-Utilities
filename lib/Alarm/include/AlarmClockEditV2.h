#ifndef UTILITIES_ALARM_CLOCK_EDIT_V2_H
#define UTILITIES_ALARM_CLOCK_EDIT_V2_H
#include "AlarmServiceV2.h"
#define ALARM_CLOCK_EDIT_TAG UINT32_C(0x41434532)
#define ALARM_CLOCK_EDIT_VERSION 1u
#define ALARM_CLOCK_EDIT_KEY "clock_edit_auth"
/* UTC seconds since2000, never Unix seconds. The copied permit is one-use and
 * invocation-local. A reset invalidates it, not its durable intent record. */
typedef struct {uint32_t struct_size,nonce,generation,requested,previous,snapshot,reserved[2];} alarm_clock_edit_permit_v2;
enum {ALARM_CLOCK_EDIT_NONE,ALARM_CLOCK_EDIT_INTENT,ALARM_CLOCK_EDIT_VERIFIED};
typedef struct {uint32_t struct_size,phase,generation,requested,previous,floor[2];} alarm_clock_edit_status_v2;
typedef struct {
 alarm_service_descriptor_v2 service;
 uint32_t tag,version;
 int32_t (*begin)(void *,uint32_t requested_utc,alarm_clock_edit_permit_v2 *);
 int32_t (*finish)(void *,const alarm_clock_edit_permit_v2 *);
 int32_t (*cancel)(void *,const alarm_clock_edit_permit_v2 *);
 int32_t (*edit_status)(void *,alarm_clock_edit_status_v2 *);
} alarm_clock_edit_descriptor_v2;
static inline const alarm_clock_edit_descriptor_v2 *alarm_clock_edit_descriptor(const alarm_service_v1 *s){
 const alarm_service_descriptor_v2 *d=alarm_service_descriptor(s);
 if(!d || !(d->features&ALARM_DESCRIPTOR_CLOCK_EDIT) || s->struct_size<sizeof(alarm_clock_edit_descriptor_v2))return NULL;
 const alarm_clock_edit_descriptor_v2 *e=(const alarm_clock_edit_descriptor_v2 *)s;
 return e->tag==ALARM_CLOCK_EDIT_TAG && e->version==ALARM_CLOCK_EDIT_VERSION && e->begin && e->finish && e->cancel && e->edit_status ? e:NULL;
}
#if defined(__cplusplus)
#define ALARM_EDIT_ASSERT static_assert
#else
#define ALARM_EDIT_ASSERT _Static_assert
#endif
ALARM_EDIT_ASSERT(sizeof(alarm_clock_edit_permit_v2)==32,"copied edit permit ABI");
ALARM_EDIT_ASSERT(sizeof(alarm_clock_edit_status_v2)==28,"copied edit status ABI");
ALARM_EDIT_ASSERT(offsetof(alarm_clock_edit_descriptor_v2,service)==0,"tagged service prefix");
ALARM_EDIT_ASSERT(offsetof(alarm_clock_edit_descriptor_v2,tag)==sizeof(alarm_service_descriptor_v2),"editor tagged tail");
#if UINTPTR_MAX == UINT32_MAX
ALARM_EDIT_ASSERT(sizeof(alarm_clock_edit_descriptor_v2)==80,"editor target ABI");
#endif
#undef ALARM_EDIT_ASSERT
/* begin durably authorizes intent before clock write; finish verifies native
 * time and persists replacement authorization; cancel restores the prior
 * authorization. Clock and KV are not atomic. Unknown custody is terminal.
 * All holders see this extension; selected Settings alone dispatches edits. */
#endif
