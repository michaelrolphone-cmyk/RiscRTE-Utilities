#include <assert.h>
#include <string.h>
#include "../Apps/lora.c"
static int mode, stops, renders, transmits, events;
static bool supported(void) { return false; }
static void defaults(t5_lora_config_t *c) { memset(c,0,sizeof(*c)); }
static bool start(const t5_lora_config_t *c) {(void)c;assert(0);return false;}
static void stop(void) { stops++; }
static bool read_state(t5_lora_state_t *s) {(void)s;return false;}
static bool packet_read(t5_lora_packet_t *p) {(void)p;return false;}
static bool transmit(const uint8_t *p,uint16_t n) {(void)p;(void)n;transmits++;return false;}
static bool poll(t5_ui_event_t *e,uint32_t ms) {assert(ms==100);memset(e,0,sizeof(*e));e->type=events++?T5_UI_EVENT_BACK:T5_UI_EVENT_CONFIRM;return true;}
static void render_list(const t5_ui_chrome_t *c,const t5_ui_list_row_t *r,uint32_t n,int32_t s) {(void)c;(void)s;assert(n==7);assert(strcmp(r[0].value,"Unsupported")==0);renders++;}
static t5_lora_api_v1 l={.supported=supported,.default_config=defaults,.start=start,.stop=stop,.read_state=read_state,.poll_packet=packet_read,.transmit=transmit};
static t5_ui_api_v1 u={.render_list=render_list,.poll_event=poll};
const t5_lora_api_v1 *t5_lora_get_api(uint32_t v) {(void)v;return mode?&l:NULL;}
const t5_ui_api_v1 *t5_ui_get_api(uint32_t v) {(void)v;return &u;}
int main(void) {
 app_main();assert(!renders&&!stops);mode=1;app_main();assert(renders==1&&stops==1&&!transmits);
 t5_lora_packet_t p={.length=T5_LORA_MAX_PACKET};memset(p.data,0xff,sizeof(p.data));packet_to_raw_views(&p);
 assert(strlen(packet_hex_value)==3*T5_LORA_MAX_PACKET-1);assert(strlen(packet_ascii_value)==T5_LORA_MAX_PACKET);
 assert(packet_ascii_value[0]=='.');p.length=0;packet_to_raw_views(&p);assert(strlen(packet_ascii_value)==T5_LORA_MAX_PACKET);
 l.transmit=NULL;app_main();assert(renders==1);return 0;
}
