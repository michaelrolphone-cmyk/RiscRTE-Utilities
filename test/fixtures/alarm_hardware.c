/* Test-only hardware boundary; no production module substitutes these tables. */
#include "RiscProviderV2.h"
#include "RiscPlatformClockV1.h"
#include "PortableRtcClock.h"
#include "AlarmOutputV1.h"
extern uint64_t alarm_test_ms(void);
extern bool alarm_test_time(twatch_rtc_time_v1 *);
extern bool alarm_test_output(unsigned action);
#if FIXTURE == 0
static uint64_t mono(void*c){(void)c;return alarm_test_ms();}
static void sleep_ms(void*c,uint32_t n){(void)c;(void)n;}
static const risc_platform_clock_api_v1 table={1,sizeof(table),NULL,mono,sleep_ms};
#define ID "test-clock"
#define CAP "platform.clock"
#define VER 1
#elif FIXTURE == 1
static bool read_time(void*c,twatch_rtc_time_v1*out){(void)c;return alarm_test_time(out);}
static const twatch_rtc_api_v1 table={2,sizeof(table),NULL,read_time,NULL,NULL,NULL};
#define ID "test-rtc"
#define CAP "rtc.clock"
#define VER 2
#elif FIXTURE == 2
static bool effect(void*c,uint8_t e){(void)c;(void)e;return alarm_test_output(1);}
static bool stop_effect(void*c){(void)c;return alarm_test_output(2);}
static const twatch_haptic_api_v1 table={1,sizeof(table),NULL,effect,stop_effect};
#define ID "test-haptic"
#define CAP "haptic.effect"
#define VER 1
#else
static bool open_audio(void*c,uint32_t r,uint8_t n){(void)c;(void)r;(void)n;return alarm_test_output(3);}
static bool write_audio(void*c,const int16_t*p,size_t n){(void)c;(void)p;(void)n;return alarm_test_output(4);}
static bool gain(void*c,uint16_t g,uint16_t m){(void)c;(void)g;(void)m;return true;}
static bool silence(void*c){(void)c;return alarm_test_output(5);}
static bool close_audio(void*c){(void)c;return alarm_test_output(6);}
static const twatch_audio_out_api_v1 table={1,sizeof(table),NULL,open_audio,write_audio,gain,silence,close_audio};
#define ID "test-audio"
#define CAP "audio.output"
#define VER 1
#endif
static bool start(const risc_provider_dependency_v1*d,size_t n){(void)d;return n==0;}
static bool quiesce(void){return true;}static void stop(void){}
static const risc_driver_v2 provider={2,sizeof(provider),ID,CAP,VER,&table,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&provider:NULL;}
