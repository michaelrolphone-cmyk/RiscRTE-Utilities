#pragma once
#include "RiscBatteryGaugeV1.h"
#include "RiscRuntimeV1.h"
/* Same optional sample flags as the established Watch power profile. Basic
 * providers leave STATUS_VALID clear; no extended status is inferred. */
enum {
 PORTABLE_POWER_STATUS_VALID=1u<<2, PORTABLE_POWER_INPUT_READY=1u<<3,
 PORTABLE_POWER_CHARGER_ENABLED=1u<<4, PORTABLE_POWER_CHARGE_DONE=1u<<5,
 PORTABLE_POWER_BATTERY_PRESENT=1u<<6, PORTABLE_POWER_THERMAL_LIMIT=1u<<7
};
/* Pinned paper adapter predates the power accessor. Read the authorized default
 * battery binding directly at the app's settled poll boundary, then release. */
static bool portable_power_read(risc_battery_sample_v1 *out){
 *out=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
 if(!rt||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release)return false;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!rt->acquire("board.battery",1,0,&grant))return false;
 const risc_battery_gauge_api_v1 *b=grant.api;
 risc_battery_sample_v1 sample=*out;
 bool ok=b&&b->api_version==1&&b->struct_size>=sizeof(*b)&&b->read&&b->read(b->context,&sample);
 rt->release(&grant);if(ok)*out=sample;return ok;
}
