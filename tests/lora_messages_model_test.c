#include "../Apps/lora_messages_model.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls[6],provider_state;
static bool fail_config,fail_send,fail_receive,fail_poll,fail_read,fail_cancel;
static size_t packet_size;
static bool configure(void*c,const twatch_lora_config_v2*p){(void)c;calls[0]++;assert(lm_profile_valid(p));return !fail_config;}
static bool send_packet(void*c,const uint8_t*d,size_t n,uint32_t ms){(void)c;calls[1]++;assert(d&&n&&n<=240&&ms==60000);provider_state=TW_LORA_TX;return !fail_send;}
static bool receive(void*c,uint32_t ms){(void)c;calls[2]++;assert(ms==5000);provider_state=TW_LORA_RX;return !fail_receive;}
static bool poll(void*c,twatch_lora_status_v2*s){(void)c;calls[3]++;*s=(twatch_lora_status_v2){.state=(uint8_t)provider_state,.length=(uint8_t)packet_size,.rssi_dbm=-99,.snr_quarter_db=-13};return !fail_poll;}
static bool read_packet(void*c,uint8_t*b,size_t cap,size_t*n){(void)c;calls[4]++;assert(cap==255);*n=packet_size;if(packet_size<=cap){memset(b,'x',packet_size);if(packet_size)b[0]=0;}return !fail_read;}
static bool cancel(void*c){(void)c;calls[5]++;return !fail_cancel;}
static const twatch_radio_api_v2 api={2,sizeof(api),NULL,configure,send_packet,receive,poll,read_packet,cancel};
static const twatch_lora_config_v2 profile={.frequency_hz=915000000,.bandwidth_hz=125000,.preamble=8,.sf=7,.coding_rate=5,.power_dbm=0};
static lm_model fresh(void){memset(calls,0,sizeof(calls));fail_config=fail_send=fail_receive=fail_poll=fail_read=fail_cancel=false;packet_size=12;provider_state=0;return (lm_model){.profile=profile,.profile_valid=true,.radio=&api};}
int main(void){
 uint8_t bytes[32],copy[32];twatch_lora_config_v2 p={0};
 assert(lm_profile_valid(&profile));assert(!lm_profile_valid(&p));
 lm_profile_encode(&profile,bytes);assert(lm_profile_decode(&p,bytes,32));assert(!memcmp(&p,&profile,sizeof(p)));
 for(unsigned i=0;i<32;i++){memcpy(copy,bytes,32);copy[i]^=1;assert(!lm_profile_decode(&p,copy,32));}
 for(unsigned n=0;n<32;n++)assert(!lm_profile_decode(&p,bytes,n));
 p=profile;p.frequency_hz=868000000;assert(lm_profile_valid(&p));p.frequency_hz=433000000;assert(lm_profile_valid(&p));p.frequency_hz=900000000;assert(!lm_profile_valid(&p));
 p=profile;p.frequency_hz=2400000000u;p.bandwidth_hz=203125;p.power_dbm=-18;assert(lm_profile_valid(&p));p.power_dbm=14;assert(!lm_profile_valid(&p));
 int64_t n;assert(lm_number("2500000000",false,&n)&&n==2500000000LL);assert(lm_number("-18",true,&n)&&n==-18);
 const char *bad[]={"","-"," 1","1 ","1e9","1.5","4294967296","00000000001","+1","--1"};for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!lm_number(bad[i],true,&n));assert(!lm_number("-1",false,&n));
 lm_model m=fresh();m.profile_valid=false;assert(!lm_send(&m,"hello",1));assert(!lm_listen(&m,1));assert(!calls[0]&&!calls[1]&&!calls[2]);
 m=fresh();assert(!lm_send(&m,"",1));assert(!lm_send(&m,"   ",1));assert(!lm_send(&m,"a\nb",1));assert(!calls[0]);
 char text[242];memset(text,'a',241);text[241]=0;assert(!lm_send(&m,text,1));text[240]=0;
 assert(lm_send(&m,text,0xfffffff0u));assert(m.count==1 && m.state==LM_SENDING && m.history[0].status==LM_OUT_PENDING);
 assert(!lm_send(&m,"duplicate",5)&&calls[1]==1);assert(!lm_step(&m,10));provider_state=TW_LORA_SENT;
 assert(lm_step(&m,30));assert(m.state==LM_TRANSMITTED&&!m.owned&&m.history[0].status==LM_OUT_SENT);
 assert(!lm_step(&m,40)&&calls[1]==1);assert(lm_stop(&m));
 m=fresh();fail_send=true;assert(!lm_send(&m,"hello",1));assert(m.state==LM_SEND_UNKNOWN&&!m.owned&&m.history[0].status==LM_OUT_UNKNOWN);assert(calls[1]==1);fail_send=false;assert(lm_send(&m,"hello",2)&&calls[1]==2&&m.count==2);assert(lm_stop(&m)&&m.history[1].status==LM_OUT_UNKNOWN);
 m=fresh();fail_config=true;assert(!lm_send(&m,"hello",1));assert(!calls[1]&&calls[5]==1&&!m.owned);
 m=fresh();assert(lm_send(&m,"hello",1));fail_poll=true;assert(!lm_step(&m,3));assert(m.state==LM_SEND_UNKNOWN&&m.history[0].status==LM_OUT_UNKNOWN&&!m.owned);
 m=fresh();assert(lm_send(&m,"hello",1));assert(!lm_step(&m,61002));assert(m.state==LM_SEND_UNKNOWN);
 m=fresh();assert(lm_send(&m,"hello",1));provider_state=TW_LORA_TIMEOUT;assert(!lm_step(&m,2));assert(m.state==LM_SEND_UNKNOWN);
 m=fresh();assert(lm_listen(&m,1));provider_state=TW_LORA_TIMEOUT;assert(!lm_step(&m,5001));assert(m.state==LM_LISTENING&&calls[2]==2);
 provider_state=TW_LORA_CRC_ERROR;assert(lm_step(&m,5002));assert(m.count==0&&calls[2]==3);
 for(unsigned i=0;i<21;i++){provider_state=TW_LORA_RECEIVED;packet_size=255;assert(lm_step(&m,5003+i));assert(m.history[m.active].binary&&m.history[m.active].rssi==-99&&m.history[m.active].snr==-13);}
 assert(m.count==16 && m.next==5 && lm_at(&m,15) && !lm_at(&m,16));assert(lm_at(&m,0)->length==255&&strlen(lm_at(&m,0)->text)==255);
 assert(lm_send(&m,"stop rx to send",6000));assert(!m.listening&&m.state==LM_SENDING);assert(lm_stop(&m));
 m=fresh();assert(lm_listen(&m,1));provider_state=TW_LORA_RECEIVED;packet_size=0;assert(!lm_step(&m,2));assert(m.state==LM_RADIO_ERROR&&!m.owned&&!m.count);
 m=fresh();assert(lm_listen(&m,1));fail_read=true;provider_state=TW_LORA_RECEIVED;assert(!lm_step(&m,2));assert(!m.count&&!m.owned);
 m=fresh();fail_receive=true;assert(!lm_listen(&m,1));assert(calls[5]==1&&!m.owned);
 m=fresh();assert(lm_listen(&m,1));fail_cancel=true;assert(!lm_stop(&m));assert(m.uncertain&&m.owned&&m.state==LM_CLEANUP_ERROR);unsigned before=calls[5];assert(!lm_stop(&m)&&calls[5]==before);assert(!lm_send(&m,"unsafe",2));assert(!lm_listen(&m,2));assert(!lm_step(&m,3));
 twatch_radio_api_v2 broken=api;broken.struct_size=offsetof(twatch_radio_api_v2,cancel);assert(!lm_api_valid(&broken));broken=api;broken.cancel=NULL;assert(!lm_api_valid(&broken));broken=api;broken.api_version=1;assert(!lm_api_valid(&broken));
 puts("LoRa model: RF validation/record corruption, bounded packet/history, normal/error/timeout/retry, wrap and retained cleanup passed");return 0;
}
