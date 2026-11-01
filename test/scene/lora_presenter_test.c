#include "../../Apps/lora_scene_presenter.h"
#include <assert.h>
#include <stdio.h>
static risc_components_document_v1 doc;
int main(void){
 lm_model m={0};char draft[241];for(unsigned i=0;i<240;i++)draft[i]=(char)(32+i%95);draft[240]=0;
 lora_scene_view v={.model=&m,.draft=draft,.notice="",.page=LSP_COMPOSE,.radio_available=true,.text_available=true};
 assert(lsp_build(&doc,&v,1));char reconstructed[256]={0};unsigned n=0;
 for(unsigned i=0;i<doc.node_count;i++)if(doc.nodes[i].kind==RISC_SCENE_TEXT_NODE&&!strcmp(doc.nodes[i].label,"Draft")){size_t z=strlen(doc.nodes[i].text);memcpy(reconstructed+n,doc.nodes[i].text,z);n+=(unsigned)z;}
 assert(n==240&&!memcmp(draft,reconstructed,241));
 uint32_t action=0;risc_scene_event_v1 e={sizeof(e),RISC_SCENE_ACTION_EVENT,1,0,LSP_BACK,0,1};assert(lsp_intent(&doc,&e,&action)&&action==LSP_BACK);
 e.action=LSP_SEND;assert(!lsp_intent(&doc,&e,&action));
 v.page=LSP_PICKER;v.mask=4;assert(lsp_build(&doc,&v,2));
 for(unsigned i=0;i<doc.node_count;i++)if(doc.nodes[i].action>=LSP_CHOOSE_BASE){e=(risc_scene_event_v1){sizeof(e),RISC_SCENE_ACTION_EVENT,2,doc.nodes[i].id,doc.nodes[i].action,0,2};assert(lsp_intent(&doc,&e,&action)==(e.action==LSP_CHOOSE_BASE+3));e.document_revision=1;assert(!lsp_intent(&doc,&e,&action));}
 for(unsigned i=0;i<20;i++)assert(lm_add(&m,(const uint8_t*)"hello",5,LM_IN));
 v.page=LSP_HISTORY;assert(lsp_build(&doc,&v,3));unsigned rows=0;for(unsigned i=0;i<doc.node_count;i++)rows+=doc.nodes[i].action>=LSP_MESSAGE_BASE;assert(rows==16);
 v.page=LSP_DETAIL;v.detail=(lm_message){.length=255,.status=LM_IN,.snr=-7,.rssi=-70,.binary=true};memset(v.detail.text,'.',255);
 assert(lsp_build(&doc,&v,4));n=0;bool signal=false;for(unsigned i=0;i<doc.node_count;i++){if(!strcmp(doc.nodes[i].label,"Text"))n+=(unsigned)strlen(doc.nodes[i].text);if(strstr(doc.nodes[i].text,"-1.75 dB"))signal=true;}assert(n==255&&signal);
 v.page=LSP_RF;v.fields=63;assert(lsp_build(&doc,&v,5));rows=0;for(unsigned i=0;i<doc.node_count;i++)rows+=doc.nodes[i].action>=LSP_FIELD_BASE&&doc.nodes[i].action<LSP_FIELD_BASE+6;assert(rows==6);
 printf("LoRa semantic presenter: full 240-byte draft, 255-byte packet, all RF fields, bounded history and stale/disabled intents PASS\n");
}
