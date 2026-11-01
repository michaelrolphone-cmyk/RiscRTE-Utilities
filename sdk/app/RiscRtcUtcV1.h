#pragma once
/* Additive, read-only UTC suffix for rtc.clock@2. The original calendar
 * prefix remains unchanged. UTC basis is provider configuration, never a
 * consumer inference from a device identity. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define RISC_RTC_UTC_TAG UINT32_C(0x55544331)
typedef struct { uint16_t year; uint8_t month,day,weekday,hour,minute,second; } risc_rtc_calendar_v2;
typedef struct {
 uint32_t api_version,struct_size; void *context;
 bool (*read)(void*,risc_rtc_calendar_v2*);
 bool (*write)(void*,const risc_rtc_calendar_v2*);
 bool (*alarm)(void*,uint8_t,uint8_t,uint8_t,uint8_t,bool);
 bool (*alarm_pending)(void*,bool*,bool);
} risc_rtc_clock_prefix_v2;
typedef struct { uint32_t struct_size,reserved; int64_t epoch_seconds; } risc_rtc_utc_snapshot_v1;
typedef struct {
 risc_rtc_clock_prefix_v2 base;
 uint32_t utc_tag,utc_version;
 bool (*read_utc)(void*,risc_rtc_utc_snapshot_v1*);
} risc_rtc_utc_api_v1;
/* A false read leaves the entire caller snapshot unchanged. A true read
 * returns UTC seconds from the validated provider calendar and explicit basis.
 * No clock writes, alarm acknowledgement, or native clock seeding occur. */
typedef struct {void *context;bool (*read)(void*,risc_rtc_utc_snapshot_v1*);} risc_rtc_utc_client_v1;
/* Copy the ABI suffix rather than aliasing a provider's original prefix type. */
static inline bool risc_rtc_utc_bind_v1(const void *table,risc_rtc_utc_client_v1 *out) {
 uint32_t header[2];
 if(!table||!out)return false;
 memcpy(header,table,sizeof(header));
 if(header[0]!=2||header[1]<sizeof(risc_rtc_utc_api_v1))return false;
 risc_rtc_utc_api_v1 a;memcpy(&a,table,sizeof(a));
 if(a.utc_tag!=RISC_RTC_UTC_TAG||a.utc_version!=1||!a.read_utc)return false;
 *out=(risc_rtc_utc_client_v1){a.base.context,a.read_utc};return true;
}

/* 0: old/unknown suffix; 1: valid UTC suffix; -1: recognized but malformed.
 * A malformed advertised UTC authority must not fall back to a stale clock. */
static inline int risc_rtc_utc_status_v1(const void *table) {
 uint32_t header[2],tag;
 if(!table)return 0;
 memcpy(header,table,sizeof(header));
 if(header[0]!=2||header[1]<sizeof(risc_rtc_clock_prefix_v2))return -1;
 if(header[1]<offsetof(risc_rtc_utc_api_v1,utc_tag)+sizeof(tag))return 0;
 memcpy(&tag,(const uint8_t*)table+offsetof(risc_rtc_utc_api_v1,utc_tag),sizeof(tag));
 if(tag!=RISC_RTC_UTC_TAG)return 0;
 risc_rtc_utc_client_v1 copied;
 return risc_rtc_utc_bind_v1(table,&copied)?1:-1;
}
