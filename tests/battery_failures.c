#include <assert.h>
#include <string.h>
#include "../Apps/battery.c"
static int mode, reads, renders, polls;
static bool read_state(t5_battery_state_t *s) {reads++;memset(s,0,sizeof(*s));return mode==2&&reads==1;}
static bool poll(t5_app_input_t *i,uint32_t ms) {assert(ms==50);memset(i,0,sizeof(*i));i->buttons=polls++?T5_APP_BUTTON_BACK:T5_APP_BUTTON_CONFIRM;return true;}
static void render_list(const t5_ui_chrome_t *c,const t5_ui_list_row_t *r,uint32_t n,int32_t s) {(void)r;(void)n;(void)s;assert(strcmp(c->status,"Battery management unavailable")==0);renders++;}
static int32_t hit(int16_t x,int16_t y) {(void)x;(void)y;return -1;}
static int32_t index_move(int32_t s,uint32_t n) {(void)n;return s;}
static t5_app_api_v1 a={.poll=poll};static t5_battery_api_v1 b={.read=read_state};
static t5_ui_api_v1 u={.render_list=render_list,.hit_test=hit,.next_index=index_move,.previous_index=index_move};
const t5_app_api_v1 *t5_app_get_api(uint32_t v) {(void)v;return mode?&a:NULL;}
const t5_battery_api_v1 *t5_battery_get_api(uint32_t v) {(void)v;return &b;}
const t5_ui_api_v1 *t5_ui_get_api(uint32_t v) {(void)v;return &u;}
int main(void) {
 app_main();assert(!reads&&!renders);mode=1;app_main();assert(reads==1&&!renders&&!polls);
 mode=2;reads=0;app_main();assert(reads==2&&renders==1&&polls==2);
 b.read=NULL;app_main();assert(reads==2);return 0;
}
