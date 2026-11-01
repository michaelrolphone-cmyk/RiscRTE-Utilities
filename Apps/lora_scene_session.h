#pragma once
/* Shared LoRa invocation fence. Reuse the published message/profile model.
 * Provider callbacks may invalidate authority while returning any value.
 * Every callback is fenced before and after; a lost context is terminal.
 * This wrapper owns neither hardware configuration nor physical lifecycle. */
#include "lora_messages_model.h"
typedef struct {
 lm_model model;
 const twatch_radio_api_v2 *provider;
 twatch_radio_api_v2 facade;
 bool (*alive)(void *);
 void (*retain)(void *);
 void *owner;
 bool retained;
} lora_scene_session;
static inline bool lss_fence(lora_scene_session *s){
 if(!s || s->retained)return false;
 if(s->alive && s->alive(s->owner))return true;
 s->retained=true;s->model.uncertain=true;
 if(s->retain)s->retain(s->owner);
 return false;
}
static inline bool lss_after(lora_scene_session*s,bool ok){return lss_fence(s)&&ok;}
static bool lss_configure(void*c,const twatch_lora_config_v2*p){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->configure(s->provider->context,p));
}
static bool lss_send(void*c,const uint8_t*p,size_t n,uint32_t timeout){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->send(s->provider->context,p,n,timeout));
}
static bool lss_receive(void*c,uint32_t timeout){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->receive(s->provider->context,timeout));
}
static bool lss_poll(void*c,twatch_lora_status_v2*out){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->poll(s->provider->context,out));
}
static bool lss_read(void*c,uint8_t*p,size_t n,size_t*out){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->read(s->provider->context,p,n,out));
}
static bool lss_cancel(void*c){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->cancel(s->provider->context));
}
static bool lss_info(void*c,twatch_lora_profile_info_v1*out){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->profile_info(s->provider->context,out));
}
static bool lss_select(void*c,uint32_t choice){
 lora_scene_session*s=c;if(!lss_fence(s))return false;
 return lss_after(s,s->provider->select_profile(s->provider->context,choice));
}
static inline void lss_init(lora_scene_session*s,bool(*alive)(void*),void(*retain)(void*),void*owner){
 memset(s,0,sizeof(*s));s->alive=alive;s->retain=retain;s->owner=owner;
 s->facade=(twatch_radio_api_v2){2,sizeof(s->facade),s,lss_configure,lss_send,lss_receive,lss_poll,lss_read,lss_cancel,lss_info,lss_select};
}
static inline bool lss_bind(lora_scene_session*s,const twatch_radio_api_v2*provider){
 if(!lss_fence(s)||s->model.owned||s->model.uncertain||!lm_selector_valid(provider))return false;
 s->provider=provider;s->model.radio=&s->facade;return true;
}
static inline bool lss_result(lora_scene_session*s,bool ok){
 if(!lss_fence(s))return false;
 if(s->model.uncertain){s->retained=true;if(s->retain)s->retain(s->owner);return false;}
 return ok;
}
static inline bool lss_stop(lora_scene_session*s){
 if(!lss_fence(s))return false;
 return lss_result(s,lm_stop(&s->model));
}
static inline bool lss_unbind(lora_scene_session*s){
 if(!lss_stop(s))return false;
 s->model.radio=NULL;s->provider=NULL;return true;
}
static inline bool lss_choose(lora_scene_session*s,unsigned choice){
 if(!lss_fence(s)||!s->provider)return false;
 return lss_result(s,lm_select(&s->model,choice));
}
static inline bool lss_listen(lora_scene_session*s,uint32_t now){
 if(!lss_fence(s)||!s->provider)return false;
 return lss_result(s,lm_listen(&s->model,now));
}
static inline bool lss_transmit(lora_scene_session*s,const char*text,uint32_t now){
 if(!lss_fence(s)||!s->provider)return false;
 return lss_result(s,lm_send(&s->model,text,now));
}
static inline bool lss_step(lora_scene_session*s,uint32_t now){
 if(!lss_fence(s)||!s->provider)return false;
 return lss_result(s,lm_step(&s->model,now));
}
