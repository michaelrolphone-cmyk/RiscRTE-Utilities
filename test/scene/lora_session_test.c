#include "../../Apps/lora_scene_session.h"
#include <assert.h>
#include <stdio.h>
enum {INFO=1,SELECT,CONFIGURE,SEND,RECEIVE,POLL,READ,CANCEL};
static unsigned calls,lose,fail,retains,selected,status;static bool live;
static bool alive(void*c){(void)c;return live;}
static void retain(void*c){(void)c;++retains;}
static bool call(unsigned kind){assert(live);++calls;if(lose==kind)live=false;return fail!=kind;}
static bool info(void*c,twatch_lora_profile_info_v1*out){(void)c;*out=(twatch_lora_profile_info_v1){sizeof(*out),15,selected,0};return call(INFO);}
static bool select_radio(void*c,uint32_t n){(void)c;selected=n;return call(SELECT);}
static bool configure(void*c,const twatch_lora_config_v2*p){(void)c;assert(p->frequency_hz==915000000);return call(CONFIGURE);}
static bool send_radio(void*c,const uint8_t*p,size_t n,uint32_t t){(void)c;assert(n==5&&!memcmp(p,"hello",5)&&t==60000);return call(SEND);}
static bool receive(void*c,uint32_t t){(void)c;assert(t==5000);return call(RECEIVE);}
static bool poll(void*c,twatch_lora_status_v2*out){(void)c;*out=(twatch_lora_status_v2){.state=status,.length=3,.rssi_dbm=-70,.snr_quarter_db=-7};return call(POLL);}
static bool read_radio(void*c,uint8_t*p,size_t cap,size_t*n){(void)c;assert(cap>=3);memcpy(p,"hi!",3);*n=3;return call(READ);}
static bool cancel(void*c){(void)c;return call(CANCEL);}
static const twatch_radio_api_v2 api={2,sizeof(api),NULL,configure,send_radio,receive,poll,read_radio,cancel,info,select_radio};
static void setup(lora_scene_session*s){
 calls=lose=fail=retains=selected=0;status=TW_LORA_RX;live=true;
 lss_init(s,alive,retain,NULL);assert(lss_bind(s,&api));
 s->model.profile=(twatch_lora_config_v2){915000000,125000,8,7,5,10,{0}};
 s->model.profile_valid=true;s->model.choice=3;
}
static void fenced(lora_scene_session*s){
 assert(s->retained&&retains==1);unsigned before=calls;
 assert(!lss_stop(s));assert(!lss_step(s,99));assert(!lss_choose(s,1));
 assert(!lss_listen(s,99));assert(!lss_transmit(s,"hello",99));assert(!lss_unbind(s));
 assert(calls==before&&retains==1);
}
int main(void){
 lora_scene_session s;unsigned cases=0;
 setup(&s);assert(lss_choose(&s,3));assert(calls==3&&!s.model.owned);++cases;
 setup(&s);assert(lss_listen(&s,0));status=TW_LORA_RECEIVED;assert(lss_step(&s,1));
 assert(s.model.count==1&&!strcmp(lm_at(&s.model,0)->text,"hi!"));assert(lss_stop(&s));assert(!s.model.listening);++cases;
 setup(&s);assert(lss_transmit(&s,"hello",0));status=TW_LORA_SENT;assert(lss_step(&s,2));
 assert(s.model.state==LM_TRANSMITTED&&lm_at(&s.model,0)->status==LM_OUT_SENT);++cases;
 setup(&s);assert(lss_transmit(&s,"hello",0));assert(lss_stop(&s));
 assert(lm_at(&s.model,0)->status==LM_OUT_UNKNOWN);++cases;
 for(unsigned k=INFO;k<=RECEIVE;k++){setup(&s);lose=k;if(k==SEND)(void)lss_transmit(&s,"hello",0);else(void)lss_listen(&s,0);fenced(&s);++cases;}
 for(unsigned k=POLL;k<=CANCEL;k++){setup(&s);assert(lss_listen(&s,0));lose=k;status=TW_LORA_RECEIVED;if(k==CANCEL)(void)lss_stop(&s);else(void)lss_step(&s,1);fenced(&s);++cases;}
 setup(&s);assert(lss_listen(&s,0));fail=CANCEL;assert(!lss_stop(&s));fenced(&s);++cases;
 setup(&s);fail=CONFIGURE;assert(!lss_listen(&s,0));assert(!s.model.owned&&!s.retained&&s.model.state==LM_RADIO_ERROR);++cases;
 setup(&s);assert(lss_listen(&s,0));assert(lss_unbind(&s));unsigned before=calls;assert(!lss_listen(&s,1)&&calls==before);++cases;
 printf("LoRa fenced session: %u cases PASS\n",cases);
}
