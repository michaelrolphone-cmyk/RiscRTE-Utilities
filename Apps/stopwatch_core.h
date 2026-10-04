#ifndef RISC_UTILITIES_STOPWATCH_CORE_H
#define RISC_UTILITIES_STOPWATCH_CORE_H
/* Application-owned encoding/policy. No flash writes while time advances. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SW_MAX_MS 359999990u /* 99:59:59.99 */
#define SW_RECORD_SIZE 20u
#define SW_RTC_MAX 3155759999u /* 2099-12-31 23:59:59, seconds since 2000 */
typedef struct { uint32_t elapsed_ms, anchor_seconds; bool running, approximate; } sw_record;
typedef struct { sw_record saved; uint32_t elapsed_ms, last_ms, rtc_anchor, ms_anchor;
                 bool running, approximate, clock_changed, limit; } sw_clock;
static inline uint32_t sw_read32(const uint8_t *p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline void sw_write32(uint8_t*p,uint32_t n) { for(unsigned i=0;i<4;i++){p[i]=(uint8_t)n;n>>=8;} }
static inline uint32_t sw_checksum(const uint8_t*p) {
 uint32_t h=2166136261u;for(unsigned i=0;i<16;i++)h=(h^p[i])*16777619u;return h;
}
static inline bool sw_decode(sw_record*out,const uint8_t*p,uint32_t n) {
 if(!out||!p||n!=SW_RECORD_SIZE||p[0]!='S'||p[1]!='W'||p[2]!='0'||p[3]!='1'||p[4]>1||p[5]>1||p[6]||p[7]||sw_read32(p+16)!=sw_checksum(p))return false;
 sw_record r={sw_read32(p+8),sw_read32(p+12),p[4]!=0,p[5]!=0};
 if(r.elapsed_ms>SW_MAX_MS||r.anchor_seconds>SW_RTC_MAX||(!r.running&&r.anchor_seconds)|| (r.running&&r.elapsed_ms==SW_MAX_MS))return false;
 *out=r;return true;
}
static inline void sw_encode(const sw_record*r,uint8_t*p) {
 p[0]='S';p[1]='W';p[2]='0';p[3]='1';p[4]=r->running;p[5]=r->approximate;p[6]=p[7]=0;
 sw_write32(p+8,r->elapsed_ms);sw_write32(p+12,r->running?r->anchor_seconds:0);sw_write32(p+16,sw_checksum(p));
}
static inline bool sw_calendar_seconds(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second,uint32_t*out) {
 static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 if(!out||year<2000||year>2099||month<1||month>12||day<1||day>days[month-1]+(month==2&&year%4==0)||hour>23||minute>59||second>59)return false;
 uint32_t d=(uint32_t)(year-2000)*365u+(year-2000+3u)/4u;
 for(unsigned m=1;m<month;m++)d+=days[m-1]+(m==2&&year%4==0);
 *out=((d+day-1u)*24u+hour)*3600u+(uint32_t)minute*60u+second;return true;
}
static inline void sw_restore(sw_clock*c,const sw_record*r,bool rtc_ok,uint32_t seconds,uint32_t ms) {
 *c=(sw_clock){.saved=*r,.elapsed_ms=r->elapsed_ms,.last_ms=ms,.rtc_anchor=seconds,.ms_anchor=ms,.running=r->running,.approximate=r->approximate};
 if(!r->running)return;
 if(!rtc_ok||seconds<r->anchor_seconds){c->running=false;c->clock_changed=true;return;}
 uint32_t delta=seconds-r->anchor_seconds;
 c->approximate=true; /* PCF calendar restores whole seconds; uncertainty <1 s absent clock edits. */
 if(delta>=(SW_MAX_MS-r->elapsed_ms+999u)/1000u){c->elapsed_ms=SW_MAX_MS;c->running=false;c->limit=true;}
 else c->elapsed_ms+=delta*1000u;
}
static inline void sw_tick(sw_clock*c,uint32_t ms) {
 uint32_t delta=ms-c->last_ms;c->last_ms=ms;
 if(!c->running)return;
 if(delta>=SW_MAX_MS-c->elapsed_ms){c->elapsed_ms=SW_MAX_MS;c->running=false;c->limit=true;}
 else c->elapsed_ms+=delta;
}
/* Samples from the same awake invocation detect RTC edits/failure. Offline
 * forward RTC edits cannot be distinguished from elapsed time and are documented. */
static inline bool sw_clock_consistent(const sw_clock*c,bool ok,uint32_t seconds,uint32_t ms) {
 if(!ok||seconds<c->rtc_anchor)return false;
 uint32_t rtc_delta=seconds-c->rtc_anchor,mono_delta=(ms-c->ms_anchor)/1000u;
 return rtc_delta>mono_delta?rtc_delta-mono_delta<=2u:mono_delta-rtc_delta<=2u;
}
static inline void sw_format(uint32_t ms,char out[12]) {
 unsigned h=ms/3600000u,m=(ms/60000u)%60u,s=(ms/1000u)%60u,cs=(ms/10u)%100u;
 out[0]='0'+h/10u;out[1]='0'+h%10u;out[2]=':';out[3]='0'+m/10u;out[4]='0'+m%10u;out[5]=':';
 out[6]='0'+s/10u;out[7]='0'+s%10u;out[8]='.';out[9]='0'+cs/10u;out[10]='0'+cs%10u;out[11]=0;
}
#endif
