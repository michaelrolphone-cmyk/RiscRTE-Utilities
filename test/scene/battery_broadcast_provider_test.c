#include "TelemetryBroadcastPolicyV1.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main original_broadcast_regressions
#include "../native_apps/telemetry_broadcast_test.c"
#undef main
#pragma GCC diagnostic pop
int main(void){original_broadcast_regressions();reset();
 const telemetry_broadcast_policy_api_v1 *extension=(const void*)api;
 assert(api->api_version==1&&api->struct_size==sizeof(*extension));
 assert(extension->tag==TELEMETRY_BROADCAST_POLICY_TAG&&extension->version==1&&extension->defaults);
 telemetry_broadcast_policy_v1 defaults={.struct_size=sizeof(defaults)};
 assert(extension->defaults(api->context,&defaults)&&defaults.settings_valid&&!defaults.radios_allowed);
 assert(defaults.enabled==(TELEMETRY_BROADCAST_DEFAULT_ENABLED!=0));
 assert(!publishes&&!reads);defaults.struct_size=0;assert(!extension->defaults(api->context,&defaults));
 assert(driver->quiesce());driver->stop();defaults.struct_size=sizeof(defaults);assert(!extension->defaults(api->context,&defaults));
 puts("Broadcast provider: legacy prefix plus copied deployment default verified");}
