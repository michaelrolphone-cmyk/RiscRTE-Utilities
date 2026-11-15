#ifndef TELEMETRY_BROADCAST_POLICY_V1_H
#define TELEMETRY_BROADCAST_POLICY_V1_H
#include "TelemetryBroadcastV1.h"
/* Additive copied deployment policy, not user settings or a setter. Prefix
 * consumers remain compatible. A missing preference never writes a default. */
#define TELEMETRY_BROADCAST_POLICY_TAG UINT32_C(0x42504431)
#define TELEMETRY_BROADCAST_POLICY_VERSION 1u
typedef struct {
 telemetry_broadcast_v1 base;
 uint32_t tag,version;
 bool (*defaults)(void *context,telemetry_broadcast_policy_v1 *out);
} telemetry_broadcast_policy_api_v1;
#endif
