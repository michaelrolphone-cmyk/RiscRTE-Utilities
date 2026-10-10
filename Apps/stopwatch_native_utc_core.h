#pragma once
#include "stopwatch_core.h"
/* Deliberate domain boundary: namespace 2 stopwatch_utc, SWU1. Never inspect
 * or migrate the legacy stopwatch/SW01 record. UTC seconds are since 2000. */
#define SW_UTC_MAX (UINT32_C(2147483647)-UINT32_C(946684800))
static inline bool sw_utc_decode(sw_record *out,const uint8_t *bytes,uint32_t size){
 if(!out||!bytes||size!=SW_RECORD_SIZE||bytes[0]!='S'||bytes[1]!='W'||bytes[2]!='U'||bytes[3]!='1'||sw_read32(bytes+16)!=sw_checksum(bytes))return false;
 uint8_t raw[SW_RECORD_SIZE];for(unsigned i=0;i<SW_RECORD_SIZE;i++)raw[i]=bytes[i];
 raw[2]='0';sw_write32(raw+16,sw_checksum(raw));
 sw_record result;if(!sw_decode(&result,raw,size)||result.anchor_seconds>SW_UTC_MAX)return false;
 *out=result;return true;
}
static inline void sw_utc_encode(const sw_record *record,uint8_t *bytes){
 sw_encode(record,bytes);bytes[2]='U';sw_write32(bytes+16,sw_checksum(bytes));
}
#define sw_decode sw_utc_decode
#define sw_encode sw_utc_encode
