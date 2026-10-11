#pragma once
#include "TelemetryBroadcastV1.h"
static bool resident_bt_pause(void *c){(void)c;TEST_RESIDENT_IO();return true;}
static bool resident_bt_step(void *c,bool allow,const telemetry_broadcast_policy_v1 *p){(void)c;(void)allow;(void)p;TEST_RESIDENT_IO();return true;}
static bool resident_bt_status(void *c,telemetry_broadcast_status_v1 *s){(void)c;TEST_RESIDENT_IO();*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t resident_bt_enum(void *c,uint32_t i,risc_telemetry_field_v1 *f){(void)c;(void)i;(void)f;TEST_RESIDENT_IO();return 0;}
static int32_t resident_bt_read(void *c,uint32_t i,int32_t *v){(void)c;(void)i;(void)v;TEST_RESIDENT_IO();return 0;}
static const telemetry_broadcast_v1 resident_bt={1,sizeof(resident_bt),NULL,resident_bt_step,resident_bt_pause,resident_bt_status,resident_bt_enum,resident_bt_read};
