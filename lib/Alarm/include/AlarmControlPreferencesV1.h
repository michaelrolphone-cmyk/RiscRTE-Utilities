#ifndef RISC_ALARM_CONTROL_PREFERENCES_V1_H
#define RISC_ALARM_CONTROL_PREFERENCES_V1_H
#include "AlarmControlV1.h"
#include <stddef.h>
#define ALARM_CONTROL_PREFERENCES_TAG UINT32_C(0x41504331)
/* Additive descriptor, preserving the complete API1 prefix. The provider only
 * projects validated durable intent into copied bytes; it never borrows an app
 * capability or owns an app transaction. NOT_APPLICABLE means use ordinary
 * apply. A successful plan must be reconciled through the caller's explicitly
 * authorized semantic preference grant before the read/refresh-only reconciliation callback observes the result. */
#define ALARM_CONTROL_NOT_APPLICABLE 2
#define ALARM_CONTROL_PREFERENCE_BYTES 32u
typedef struct {
 uint32_t struct_size,namespace_id,before_size,after_size;
 char key[16];uint8_t before[32],after[32];
} alarm_control_preference_plan_v1;
typedef struct {
 alarm_control_api_v1 base;uint32_t tag,version;
 int32_t (*preference_plan)(void*,const alarm_control_command_v1*,alarm_control_preference_plan_v1*);
 /* Read/refresh only: never falls back to a legacy write after an atomic
  * receipt or desired-value restart match, even if another writer intervenes. */
 int32_t (*reconcile_preferences)(void*,const alarm_control_command_v1*);
} alarm_control_preferences_v1;
static inline const alarm_control_preferences_v1 *alarm_control_preferences(const alarm_control_api_v1 *base){
 if(!base||base->api_version!=1||base->struct_size<sizeof(alarm_control_preferences_v1))return NULL;
 const alarm_control_preferences_v1 *p=(const alarm_control_preferences_v1*)base;
 return p->tag==ALARM_CONTROL_PREFERENCES_TAG&&p->version==1&&p->preference_plan&&p->reconcile_preferences?p:NULL;
}
#endif
