#include "../../Apps/PortableLoRaV2.h"
#include "RiscProviderV2.h"
#include "RiscHardwareConfigV1.h"
#include <assert.h>
#include <string.h>
extern unsigned lora_radio_event(unsigned);
extern unsigned lora_radio_mode(void);
static unsigned choice,tx,polls,rx,delivered;
static bool configure(void*c,const twatch_lora_config_v2*p){(void)c;lora_radio_event(1);assert(p->frequency_hz==915000000&&p->bandwidth_hz==125000&&p->preamble==8&&p->sf==7&&p->coding_rate==5&&p->power_dbm==14);return true;}
static bool send(void*c,const uint8_t*p,size_t n,uint32_t timeout){(void)c;lora_radio_event(2);assert(n==240&&timeout==60000);for(unsigned i=0;i<n;i++)assert(p[i]=='a');tx=1;polls=0;return true;}
static bool receive(void*c,uint32_t timeout){(void)c;assert(timeout==5000);lora_radio_event(3);rx=1;return true;}
static bool poll(void*c,twatch_lora_status_v2*out){(void)c;lora_radio_event(4);memset(out,0,sizeof(*out));out->state=tx?(++polls<8?TW_LORA_TX:TW_LORA_SENT):rx&&!delivered&&lora_radio_mode()!=3?TW_LORA_RECEIVED:TW_LORA_RX;if(out->state==TW_LORA_RECEIVED){out->length=255;out->rssi_dbm=-80;out->snr_quarter_db=7;}return true;}
static bool read_packet(void*c,uint8_t*p,size_t n,size_t*used){(void)c;assert(n>=255);lora_radio_event(8);memset(p,'z',255);*used=255;delivered=1;return true;}
static bool cancel(void*c){(void)c;unsigned ok=lora_radio_event(5);if(ok)tx=rx=0;return ok!=0;}
static bool info(void*c,twatch_lora_profile_info_v1*out){(void)c;lora_radio_event(6);*out=(twatch_lora_profile_info_v1){sizeof(*out),15,(lora_radio_mode()==2&&choice)?2:choice,0};return true;}
static bool select_profile(void*c,uint32_t n){(void)c;lora_radio_event(7);choice=n;return true;}
static twatch_radio_api_v2 api={2,sizeof(api),NULL,configure,send,receive,poll,read_packet,cancel,info,select_profile};
static bool start(const risc_provider_dependency_v1*d,size_t n){
 assert(n==1&&d&&d[0].api_version==1&&!strcmp(d[0].capability_id,"hardware.device"));
 const risc_hardware_device_v1*hw=d[0].api;assert(hw&&hw->instance_id==TEST_RADIO_INSTANCE);
 return true;
}
static bool quiesce(void){return tx==0&&rx==0;}
static void stop(void){assert(!tx&&!rx);}
static const risc_driver_v2 driver={2,sizeof(driver),"radio","radio.lora",2,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){if(lora_radio_mode()==1)api.struct_size=offsetof(twatch_radio_api_v2,profile_info);return abi==2?&driver:NULL;}
