#pragma once
/* Semantic LoRa content; presentation, geometry and pagination belong to ui.scene.
 * The committed document, not current radio state, validates incoming intents. */
#include "RiscSceneComponentsV1.h"
#include "lora_messages_model.h"
#include <stdio.h>
enum { LSP_HOME=1,LSP_COMPOSE,LSP_RF,LSP_PICKER,LSP_HISTORY,LSP_DETAIL };
enum { LSP_BACK=1,LSP_OPEN_COMPOSE,LSP_OPEN_RF,LSP_OPEN_PICKER,LSP_OPEN_HISTORY,
 LSP_LISTEN,LSP_SEND,LSP_EDIT_MESSAGE,LSP_APPLY_RF,LSP_RETRY,
 LSP_CHOOSE_BASE=32,LSP_FIELD_BASE=48,LSP_MESSAGE_BASE=64 };
typedef struct {
 const lm_model *model;const char *draft,*notice;
 twatch_lora_config_v2 editing;
 lm_message detail;unsigned page,fields,mask,time_format,save_state;
 bool radio_available,text_available,policy_allowed;
} lora_scene_view;
static inline const char*lsp_radio(unsigned n){
 static const char*names[]={"Choose radio","SX1262 / 433 MHz","SX1262 / 868 MHz","SX1262 / 915 MHz","SX1280 / 2.4 GHz"};
 return names[n<=4?n:0];
}
static inline const char*lsp_status(const lm_model*m){
 switch(m->state){
 case LM_LISTENING:return "Listening / raw broadcast";
 case LM_SENDING:return "Sending / no automatic retry";
 case LM_TRANSMITTED:return "Transmitted / no receipt";
 case LM_SEND_UNKNOWN:return "Send unconfirmed";
 case LM_RADIO_ERROR:return "Radio error / retry";
 case LM_CLEANUP_ERROR:return "Cleanup unconfirmed";
 default:return !m->choice?"Radio off / choose radio":!m->profile_valid?"Radio off / set RF":!lm_profile_matches(&m->profile,m->choice)?"RF does not match radio":"Radio off / ready";
 }
}
static inline const char*lsp_message_status(const lm_message*m){
 return m->status==LM_IN?"Received":m->status==LM_OUT_SENT?"Transmitted / no receipt":m->status==LM_OUT_UNKNOWN?"Send unconfirmed":"Sending";
}
static inline risc_scene_node_v1*lsp_node(risc_components_document_v1*d,unsigned kind,unsigned action,const char*label,const char*text){
 if(d->node_count>=RISC_COMPONENTS_MAX_NODES)return NULL;
 unsigned i=d->node_count++;risc_scene_node_v1*n=&d->nodes[i];
 *n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action};
 snprintf(n->label,sizeof(n->label),"%s",label?label:"");snprintf(n->text,sizeof(n->text),"%s",text?text:"");return n;
}
static inline void lsp_text(risc_components_document_v1*d,const char*label,const char*text,size_t n){
 if(!n){lsp_node(d,RISC_SCENE_TEXT_NODE,0,label,"Empty");return;}
 /* Copied chunks preserve every byte, including spaces. No truncation-based
  * review or screen-width inference. The provider scrolls semantic rows. */
 for(size_t off=0;off<n;off+=RISC_SCENE_TEXT-1){
  char part[RISC_SCENE_TEXT];size_t take=n-off;if(take>=sizeof(part))take=sizeof(part)-1;
  memcpy(part,text+off,take);part[take]=0;lsp_node(d,RISC_SCENE_TEXT_NODE,0,label,part);
 }
}
static inline void lsp_time(const lm_message*m,unsigned format,char*out,size_t n){
 if(!m->clock_valid){snprintf(out,n,"Time unset");return;}
 if(format==1)snprintf(out,n,"%02u/%02u %02u:%02u",m->month,m->day,m->hour,m->minute);
 else snprintf(out,n,"%02u/%02u %u:%02u %s",m->month,m->day,m->hour%12?m->hour%12:12,m->minute,m->hour<12?"AM":"PM");
}
static inline bool lsp_build(risc_components_document_v1*d,const lora_scene_view*v,uint32_t revision){
 if(!d||!v||!v->model||!v->draft||!v->notice||!revision||v->page<LSP_HOME||v->page>LSP_DETAIL||v->model->count>LM_HISTORY_MAX||v->model->next>=LM_HISTORY_MAX||v->detail.length>255||!memchr(v->draft,0,LM_TEXT_MAX+1))return false;
 const lm_model*m=v->model;
 *d=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*d),.revision=revision,.root=1,.route_count=1,.screen_key=v->page};
 d->routes[0]=(risc_scene_route_v1){.id=1,.back_action=LSP_BACK};
 static const char*titles[]={"","LORA MESSAGES","REVIEW MESSAGE","RF SETTINGS","RADIO HARDWARE","SESSION HISTORY","MESSAGE"};
 snprintf(d->routes[0].title,sizeof(d->routes[0].title),"%s",titles[v->page]);
 lsp_node(d,RISC_COMPONENT_HEADER_ACTION,LSP_BACK,"Back","");d->details[0].symbol=RISC_SYMBOL_BACK;
 if(v->notice[0])lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Status",v->notice);
 if(!v->radio_available)lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Capability","Radio unavailable; explicit retry is safe");
 if(v->page==LSP_HOME){
  lsp_node(d,RISC_SCENE_TEXT_NODE,0,lsp_radio(m->choice),lsp_status(m));
  lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Raw broadcasts","No encryption, addressing or delivery receipts");
  if(v->save_state)lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Saved settings",v->save_state==1?"Session only":"Save unconfirmed");
  lsp_node(d,RISC_SCENE_ACTION,LSP_OPEN_COMPOSE,"Compose","");
  lsp_node(d,RISC_SCENE_ACTION,LSP_OPEN_HISTORY,"Session history","Clears when this invocation exits");
  lsp_node(d,RISC_SCENE_ACTION,LSP_LISTEN,m->owned?"Stop":"Listen","");
  lsp_node(d,RISC_SCENE_ACTION,LSP_OPEN_RF,"RF setup","");
  lsp_node(d,RISC_SCENE_ACTION,LSP_OPEN_PICKER,"Choose radio","");
  if(!v->policy_allowed)lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Radio policy","Unavailable or Airplane mode; no transmission");
 }else if(v->page==LSP_COMPOSE){
  lsp_text(d,"Draft",v->draft,strlen(v->draft));
  lsp_node(d,RISC_SCENE_ACTION,LSP_EDIT_MESSAGE,v->text_available?"Edit":"Retry text input","");
  lsp_node(d,RISC_SCENE_ACTION,LSP_SEND,m->state==LM_SENDING?"Cancel send":"Send","Explicit raw broadcast; no receipt or automatic retry");
 }else if(v->page==LSP_PICKER){
  for(unsigned i=1;i<=4;i++){
   risc_scene_node_v1*n=lsp_node(d,RISC_COMPONENT_ROW,LSP_CHOOSE_BASE+i,lsp_radio(i),(v->mask&(1u<<(i-1)))?"Provider-supported choice":"Unavailable");
   if(!(v->mask&(1u<<(i-1))))n->flags=RISC_SCENE_DISABLED;
  }
 }else if(v->page==LSP_RF){
  static const char*labels[]={"Frequency Hz","Bandwidth Hz","Spreading factor","Coding rate 4/N","Power dBm","Preamble symbols"};
  const int64_t values[]={v->editing.frequency_hz,v->editing.bandwidth_hz,v->editing.sf,v->editing.coding_rate,v->editing.power_dbm,v->editing.preamble};
  for(unsigned i=0;i<6;i++){char value[32];if(v->fields&(1u<<i))snprintf(value,sizeof(value),"%ld",(long)values[i]);else snprintf(value,sizeof(value),"Unset");lsp_node(d,RISC_COMPONENT_ROW,LSP_FIELD_BASE+i,labels[i],value);}
  lsp_node(d,RISC_SCENE_ACTION,LSP_OPEN_PICKER,"Choose radio",lsp_radio(m->choice));
  lsp_node(d,RISC_SCENE_ACTION,LSP_APPLY_RF,"Apply RF","Does not start listening or transmission");
 }else if(v->page==LSP_HISTORY){
  if(!m->count)lsp_node(d,RISC_COMPONENT_EMPTY,0,"No packets","Session history is not saved");
  for(unsigned i=0;i<m->count;i++){
   const lm_message*p=&m->history[(m->next+LM_HISTORY_MAX-1-i)%LM_HISTORY_MAX];char when[40];
   lsp_time(p,v->time_format,when,sizeof(when));lsp_node(d,RISC_COMPONENT_ROW,LSP_MESSAGE_BASE+i,when,lsp_message_status(p));
  }
 }else{
  const lm_message*p=&v->detail;char value[72];lsp_time(p,v->time_format,value,sizeof(value));
  lsp_node(d,RISC_SCENE_TEXT_NODE,0,lsp_message_status(p),value);
  if(p->status==LM_IN){unsigned snr=p->snr<0?0u-(unsigned)p->snr:(unsigned)p->snr;snprintf(value,sizeof(value),"RSSI %d dBm / SNR %s%u.%02u dB",p->rssi,p->snr<0?"-":"",snr/4,(snr%4)*25);lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Signal",value);}
  if(p->binary)lsp_node(d,RISC_SCENE_TEXT_NODE,0,"Binary packet","Non-printable bytes shown as dots");
  lsp_text(d,"Text",p->text,p->length);
 }
 return true;
}
static inline bool lsp_intent(const risc_components_document_v1*d,const risc_scene_event_v1*e,uint32_t*out){
 if(!d||!e||!out||e->struct_size!=sizeof(*e)||!e->sequence||e->kind!=RISC_SCENE_ACTION_EVENT||e->value||e->document_revision!=d->revision)return false;
 if(!e->node){if(e->action!=LSP_BACK||d->routes[0].back_action!=LSP_BACK)return false;*out=LSP_BACK;return true;}
 for(unsigned i=0;i<d->node_count&&i<RISC_COMPONENTS_MAX_NODES;i++){const risc_scene_node_v1*n=&d->nodes[i];
  if(n->id==e->node&&n->route==1&&n->action&&n->action==e->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))&&(n->kind==RISC_SCENE_ACTION||n->kind==RISC_COMPONENT_ROW||n->kind==RISC_COMPONENT_HEADER_ACTION)){*out=e->action;return true;}
 }return false;
}
