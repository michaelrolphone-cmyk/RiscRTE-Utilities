/* Actual Nova production controller and adapter. Only hardware is modeled. */
#define grants fixture_grants
#define main capture_fixture_main
#define risc_runtime_get_api fixture_runtime_get_api
#include "nova_peripherals.h"
#undef risc_runtime_get_api
#undef main
#undef grants
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version);
#include NOVA_APP_SOURCE
static unsigned launches;
static bool refuse_first_launch;
static bool observed_launch(const char *name) {
    assert(!strcmp(name,"springboard.elf"));
    launches++;
    if(refuse_first_launch && launches==1)return false;
    return fake_launch(name);
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
    static risc_runtime_api_v1 observed;
    observed=runtime_api;observed.request_launch=observed_launch;
    return version==1?&observed:NULL;
}
static void touch(unsigned at,int x,int y) {
    assert(action_count<256);
    actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;
}
int main(int argc,char **argv) {
    assert(argc==3);directory=argv[1];unsigned scenario=(unsigned)atoi(argv[2]);
    memset(pixels,0xa5,sizeof(pixels));stop_poll=400;
    touch(20,40,74); /* 7 */
    switch(scenario) {
    case 0: case 1:
        touch(40,199,212);touch(60,92,166);touch(80,145,212); /* +2= */
        if(scenario){touch(100,210,26);touch(120,120,152);} /* Sign */
        break;
    case 2: /* Nested Back must not queue an app exit. Root Back must. */
        touch(40,210,26);touch(60,30,26);touch(80,92,74);touch(100,30,26);
        break;
    case 3: /* Delete, Clear and continued input. */
        touch(40,92,74);touch(60,210,26);touch(80,120,204);
        touch(100,210,26);touch(120,120,100);touch(140,145,74);
        break;
    case 4: case 5: /* Divide-by-zero and a fresh digit after the error. */
        touch(40,199,74);touch(60,40,212);touch(80,145,212);
        if(scenario==5)touch(100,145,166);
        break;
    case 6: /* Rejected root return leaves controls usable; later retry succeeds. */
        refuse_first_launch=true;touch(40,30,26);touch(60,92,74);touch(80,30,26);
        break;
    default:assert(!"unknown scenario");
    }
    assert(app_module_init()==0);app_main();
    assert(!calculator_menu);
    switch(scenario) {
    case 0:assert(!strcmp(calculator_state.text,"9"));break;
    case 1:assert(!strcmp(calculator_state.text,"-9"));break;
    case 2:assert(!strcmp(calculator_state.text,"78") && launches==1);break;
    case 3:assert(!strcmp(calculator_state.text,"9"));break;
    case 4:assert(calculator_state.error==CALC_DIV_ZERO);break;
    case 5:assert(!calculator_state.error && !strcmp(calculator_state.text,"3"));break;
    case 6:assert(!strcmp(calculator_state.text,"78") && launches==2);break;
    }
    app_module_fini();assert(!fixture_grants && !frames && !subs);
    printf("Calculator Nova scenario %u: real touches, navigation, recovery and cleanup passed\n",scenario);
    return 0;
}
