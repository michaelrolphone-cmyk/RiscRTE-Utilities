#include <assert.h>
#include "ContextsServiceV1.h"
#include "AudioInputV1.h"
#include "RiscRadioIqV1.h"
#include "RiscPlatformClockV1.h"
static uint64_t now(void*c){(void)c;return 1000;}
static bool suspend_rf(void*c){(void)c;return true;}
static int capture(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){(void)c;(void)p;(void)n;(void)s;(void)f;assert(0);return -1;}
static bool mic_open(void*c,uint32_t rate){(void)c;(void)rate;assert(0);return false;}
static bool mic_read(void*c,int16_t*p,size_t n,size_t*g){(void)c;(void)p;(void)n;(void)g;assert(0);return false;}
static bool mic_close(void*c){(void)c;return true;}
const contexts_service_v1 *fixture_service(void){
 static const twatch_audio_in_api_v1 mic={.api_version=1,.struct_size=sizeof(mic),.open=mic_open,.read=mic_read,.close=mic_close};
 static const risc_platform_clock_api_v1 clock={1,sizeof(clock),0,now};
 static const risc_radio_iq_extended_api_v1 radio={.base={.base={.api_version=1,.struct_size=sizeof(radio),.suspend=suspend_rf}},.capture_configured=capture};
 static const risc_provider_dependency_v1 deps[]={{"platform.clock",1,&clock},{"radio.iq",1,&radio},{"audio.input",1,&mic}};
 const risc_driver_v2*d=t5_driver_get(2);assert(d&&d->start(deps,3));return d->capability;
}
