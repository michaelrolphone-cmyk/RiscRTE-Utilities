#include <assert.h>
#include <string.h>
#include "../Apps/gps.c"
static int mode, starts, stops, renders, reads;
static bool supported(void) { return mode != 1; }
static bool start(void) { starts++; return false; }
static void stop(void) { stops++; }
static bool read_state(t5_gps_state_t *s) { (void)s; reads++; return false; }
static bool poll(t5_app_input_t *i,uint32_t ms) { (void)ms; memset(i,0,sizeof(*i)); i->exit_requested=true; return true; }
static uint32_t millis(void) { return 0; }
static void render_list(const t5_ui_chrome_t *c,const t5_ui_list_row_t *r,uint32_t n,int32_t selected) {
 (void)selected; assert(n==4); renders++; assert(strcmp(r[1].value,"--")==0);
 assert(strstr(c->status, mode==1 ? "unavailable" : "could not start"));
}
static t5_app_api_v1 a={.poll=poll,.millis=millis};
static t5_gps_api_v1 g={.supported=supported,.start=start,.stop=stop,.read=read_state};
static t5_ui_api_v1 u={.render_list=render_list};
const t5_app_api_v1 *t5_app_get_api(uint32_t v) {(void)v;return mode==0?NULL:&a;}
const t5_gps_api_v1 *t5_gps_get_api(uint32_t v) {(void)v;return &g;}
const t5_ui_api_v1 *t5_ui_get_api(uint32_t v) {(void)v;return &u;}
int main(void) {
 app_main();assert(!renders&&!starts&&!stops);
 mode=1;app_main();assert(renders==1&&starts==0&&stops==1);
 mode=2;app_main();assert(renders==2&&starts==1&&stops==2&&reads==0);
 g.read=NULL;app_main();assert(renders==2&&starts==1);
 return 0;
}
