#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/calculator.c"

typedef struct {
    t5_app_input_t input;
    t5_app_contact_t contact;
    bool contact_valid;
    const char *before;
    calc_error before_error;
    int before_pressed;
} event;
static event events[180];
static unsigned event_count, event_index, polls, frames, fills;
static int width=240, height=240;
static bool missing_api, poll_failure;
static unsigned char pixels[4096 * 4096];
static t5_app_api_v1 api;

static int32_t screen_width(void) { return width; }
static int32_t screen_height(void) { return height; }
static void clear(void) { memset(pixels,255,(size_t)width*height); }
static void fill_rect(int32_t x,int32_t y,int32_t w,int32_t h,bool black) {
    assert(x>=0 && y>=0 && w>0 && h>0 && x<=width-w && y<=height-h);
    ++fills;
    for (int row=y;row<y+h;++row) memset(pixels+(size_t)row*width+x,black?0:255,(size_t)w);
}
static void present(bool full) { assert(!full); ++frames; }
static bool poll(t5_app_input_t *input,uint32_t wait_ms) {
    assert(wait_ms==30); ++polls;
    if (poll_failure) return false;
    assert(event_index<event_count);
    event *next=&events[event_index++];
    if (next->before) assert(!strcmp(calculator_state.text,next->before));
    assert(calculator_state.error==next->before_error);
    if (next->before_pressed>=-1) assert(calculator_pressed==next->before_pressed);
    *input=next->input;
    return true;
}
static bool contact(t5_app_contact_t *out) {
    assert(event_index && event_index<=event_count);
    *out=events[event_index-1].contact;
    return events[event_index-1].contact_valid;
}
const t5_app_api_v1 *t5_app_get_api(uint32_t version) {
    assert(version==T5_APP_ABI_VERSION); return missing_api?NULL:&api;
}
static void reset(void) {
    width=height=240; event_count=event_index=polls=frames=fills=0;
    missing_api=poll_failure=false;
    api=(t5_app_api_v1){.abi_version=T5_APP_ABI_VERSION,.struct_size=sizeof(api),
        .screen_width=screen_width,.screen_height=screen_height,.clear=clear,
        .fill_rect=fill_rect,.present=present,.poll=poll};
}
static void add(t5_app_input_t input,const char *before,calc_error error) {
    assert(event_count<sizeof(events)/sizeof(events[0]));
    events[event_count++]=(event){.input=input,.before=before,.before_error=error,.before_pressed=-2};
}
static void tap_index(int index,const char *before,calc_error error) {
    /* Independent watch-grid fixture coordinates, not implementation hit-test. */
    int column=index%4,row=index/4;
    add((t5_app_input_t){.tapped=true,.touch_x=(int16_t)(33+column*58),
        .touch_y=(int16_t)(102+row*30)},before,error);
}
static void tap_sequence(const char *sequence) {
    for (;*sequence;++sequence) {
        const char *key=memchr(calculator_keys,*sequence,CALCULATOR_KEYS); assert(key);
        tap_index((int)(key-calculator_keys),NULL,CALC_OK);
    }
}
static void stop(const char *before,calc_error error) {
    add((t5_app_input_t){.buttons=T5_APP_BUTTON_BACK},before,error);
}
static void save_frame(const char *directory,const char *name) {
    if (!directory) return;
    char path[1024]; assert(snprintf(path,sizeof(path),"%s/calculator-%s.pgm",directory,name)>0);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P5\n%d %d\n255\n",width,height);
    assert(fwrite(pixels,1,(size_t)width*height,f)==(size_t)width*height);
    assert(fclose(f)==0);
}

int main(int argc,char **argv) {
    const char *output=argc>1?argv[1]:NULL;
    reset(); stop("0",CALC_OK); app_main(); assert(frames==1 && polls==1);
    save_frame(output,"initial");
    reset(); tap_sequence("0.1+0.2="); stop("0.3",CALC_OK); app_main(); assert(frames==9);
    save_frame(output,"decimal");
    reset(); tap_sequence("2+3");
    add((t5_app_input_t){.tapped=true,.touch_x=218,.touch_y=222},"3",CALC_OK);
    stop("5",CALC_OK); app_main(); /* Right half of wide equals button. */
    reset(); tap_sequence("111.22BB+3=="); stop("117",CALC_OK); app_main();
    reset(); tap_sequence("S999999999.999999"); stop("-999999999.999999",CALC_OK); app_main();
    save_frame(output,"maximum");
    reset(); tap_sequence("1/0="); stop("0",CALC_DIV_ZERO); app_main();
    save_frame(output,"divide-zero");
    reset(); tap_sequence("1/0="); tap_index(0,NULL,CALC_DIV_ZERO); tap_sequence("8*9="); stop("72",CALC_OK); app_main();
    reset(); tap_sequence("999999999*2="); stop(NULL,CALC_OVERFLOW); app_main();
    reset(); tap_sequence("999999999*2="); tap_index(4,NULL,CALC_OVERFLOW); stop("7",CALC_OK); app_main();
    reset();
    add((t5_app_input_t){.tapped=true,.touch_x=-1,.touch_y=102},"0",CALC_OK);
    add((t5_app_input_t){.tapped=true,.touch_x=240,.touch_y=102},"0",CALC_OK);
    add((t5_app_input_t){.tapped=true,.touch_x=33,.touch_y=240},"0",CALC_OK);
    add((t5_app_input_t){.tapped=true,.touch_x=33,.touch_y=-1},"0",CALC_OK);
    add((t5_app_input_t){.tapped=true,.touch_x=62,.touch_y=102},"0",CALC_OK); /* gutter */
    add((t5_app_input_t){.tapped=true,.touch_x=33,.touch_y=87},"0",CALC_OK);
    stop("0",CALC_OK); app_main(); assert(frames==1);
    reset(); add((t5_app_input_t){.buttons=T5_APP_BUTTON_DOWN},"0",CALC_OK);
    add((t5_app_input_t){.buttons=T5_APP_BUTTON_CONFIRM},"0",CALC_OK);
    add((t5_app_input_t){.buttons=T5_APP_BUTTON_CONFIRM},"7",CALC_OK);
    stop("77",CALC_OK); app_main(); assert(calculator_selected==4);
    reset(); api.touch_contact=contact;
    add((t5_app_input_t){0},"0",CALC_OK);
    events[0].contact_valid=true; events[0].contact=(t5_app_contact_t){true,33,132};
    add((t5_app_input_t){0},"0",CALC_OK); events[1].before_pressed=4;
    events[1].contact_valid=true; events[1].contact=(t5_app_contact_t){true,33,132};
    tap_index(4,"0",CALC_OK); events[2].before_pressed=4; events[2].contact_valid=true;
    stop("7",CALC_OK); events[3].before_pressed=-1; app_main(); assert(frames==3);
    reset(); api.touch_contact=contact;
    add((t5_app_input_t){0},"0",CALC_OK); events[0].contact_valid=true;
    events[0].contact=(t5_app_contact_t){true,33,132};
    add((t5_app_input_t){0},"0",CALC_OK); events[1].before_pressed=4;
    events[1].contact_valid=true; events[1].contact=(t5_app_contact_t){true,240,132};
    add((t5_app_input_t){0},"0",CALC_OK); events[2].before_pressed=-1;
    events[2].contact_valid=true; events[2].contact=(t5_app_contact_t){true,33,132};
    stop("0",CALC_OK); events[3].before_pressed=-1; app_main(); assert(frames==3);
    reset(); add((t5_app_input_t){.tapped=true,.touch_x=20,.touch_y=20},"0",CALC_OK); app_main(); assert(frames==1);
    reset(); add((t5_app_input_t){.exit_requested=true},"0",CALC_OK); app_main(); assert(frames==1);
    reset(); poll_failure=true; app_main(); assert(frames==1 && polls==1);
    reset(); api.struct_size=offsetof(t5_app_api_v1,poll)+sizeof(api.poll); stop("0",CALC_OK); app_main(); assert(frames==1);
    reset(); missing_api=true; app_main(); assert(!frames && !polls);
    reset(); api.abi_version=99; app_main(); assert(!frames && !polls);
    reset(); api.struct_size=offsetof(t5_app_api_v1,poll); app_main(); assert(!frames && !polls);
#define MISSING(field) do { reset(); api.field=NULL; app_main(); assert(!frames && !polls); } while (0)
    MISSING(screen_width); MISSING(screen_height); MISSING(clear); MISSING(fill_rect); MISSING(present); MISSING(poll);
    int invalid[][2]={{0,240},{199,240},{240,239},{-1,240},{240,-1},{4097,240},{240,4097},{INT_MAX,INT_MAX}};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        reset(); width=invalid[i][0]; height=invalid[i][1]; app_main(); assert(!frames && !polls);
    }
    int valid[][2]={{200,240},{320,320},{480,800},{4096,4096}};
    for(unsigned i=0;i<sizeof(valid)/sizeof(valid[0]);++i) {
        reset(); width=valid[i][0]; height=valid[i][1]; stop("0",CALC_OK); app_main(); assert(frames==1 && fills);
    }
    unsigned previous_fills=fills;
    assert(daily_draw_width(NULL,2)==0 && daily_draw_width("",2)==0);
    assert(daily_draw_width("1:23.45",2)==82);
    assert(daily_draw_width("1",0)==0 && daily_draw_width("1",65)==0);
    daily_draw_text(NULL,0,0,"0",2);
    daily_draw_text(&api,0,0,NULL,2);
    daily_draw_text(&api,0,0,"0",0);
    daily_draw_text(&api,0,0,"0",65);
    daily_draw_text(&api,INT_MAX,0,"0",2);
    daily_draw_text(&api,0,INT_MAX,"0",2);
    assert(previous_fills==fills);
    puts("calculator UI: rendering, direct touch, contact/release, crownBack, repeat, failures and geometry passed");
    return 0;
}
