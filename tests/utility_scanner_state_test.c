/* Production Scanner controller/row actions; explicit display, radio and clock
 * transports. Shared text-entry ownership remains in the production client. */
#define PORTABLE_APP_OWNS_TOUCH_CHROME
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_RETURN_APP "springboard.elf"
#include "../Apps/ble_scanner.c"
#include <assert.h>
static t5_app_api_v1 api;
static risc_runtime_api_v1 rt;
static uint32_t tick;
static unsigned polls,presents,ready_checks,provider_polls,provider_count,closes,grants;
static unsigned last_selected,last_scroll,last_detail,trace[2][4],trace_at,pass;
static bool available,use_paper,cadence;
static springboard_contact contact_sample;
static const paper_presentation test_paper;
static uint32_t clock_ms(void){return tick;}
static int32_t width(void){return paper?480:240;}
static int32_t height(void){return paper?800:240;}
static bool ready(void){ready_checks++;return available;}
static void present(bool full){
 assert(!full&&available);presents++;last_selected=selected;last_scroll=scroll;last_detail=detail;
 if(scan.count){unsigned list[BLE_MAX_DEVICES],n=indexes(list);assert(selected<n&&scroll<n);}
}
static void back_exits(bool b){(void)b;}
static bool touch(t5_app_contact_t*out){*out=(t5_app_contact_t){0};return true;}
static void fill_rect(int x,int y,int w,int h,bool b){(void)b;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=width()&&y+h<=height());}
static bool icon(int x,int y,const char*s,uint8_t size,bool b){(void)x;(void)y;(void)s;(void)size;(void)b;return true;}
static void paper_begin(void){}
static int measure(const char*s,bool h){(void)h;return (int)strlen(s)*8;}
static void text(int x,int y,int w,const char*s,unsigned scale,bool h,bool b){(void)scale;(void)h;(void)b;(void)s;assert(x>=0&&y>=0&&x+w<=width()&&y<height());}
static void paper_contact_sample(springboard_contact*out){*out=contact_sample;}
static const paper_presentation test_paper={.struct_size=sizeof(test_paper),.begin=paper_begin,.measure=measure,.text=text,.contact=paper_contact_sample};
void portable_nova_begin(void){}
void portable_nova_header(const char*s){(void)s;}
void portable_nova_text(unsigned f,int x,int y,int w,const char*s,uint32_t c){(void)f;(void)c;text(x,y,w,s,1,false,false);}
void portable_nova_center(unsigned f,int x,int y,int w,const char*s,uint32_t c){portable_nova_text(f,x,y,w,s,c);}
void portable_nova_fill(int x,int y,int w,int h,uint32_t c){fill_rect(x,y,w,h,c!=0);}
void portable_nova_round(int x,int y,int w,int h,int r,uint32_t c){(void)r;fill_rect(x,y,w,h,c!=0);}
void portable_nova_button(int x,int y,int w,int h,const char*s,bool b){(void)s;fill_rect(x,y,w,h,b);}
bool portable_nova_hit(int x,int y,int l,int t,int w,int h){return x>=l&&y>=t&&x<l+w&&y<t+h;}
int portable_text_adapter_suspend(void){assert(!"Unexpected text modal");return RISC_TEXT_ENTRY_UNAVAILABLE;}
int portable_text_adapter_resume(void){assert(!"Unexpected text resume");return RISC_TEXT_ENTRY_OK;}
int portable_text_adapter_attention(void){return 0;}
void portable_text_adapter_retain(void){assert(!"Unexpected retained modal");}
bool portable_text_adapter_retained(void){return false;}
bool portable_app_sleep_retained(void){return false;}
static int32_t get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){
 (void)c;if(!strcmp(k,"quick_radio")){assert(cap==4);const uint8_t b[]={0x51,1,3,3^0xa5};memcpy(out,b,4);*n=4;return 0;}
 *n=0;return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t put(void*c,const char*k,const void*p,uint32_t n){(void)c;(void)k;(void)p;(void)n;assert(!"Unexpected write");return RISC_KEY_VALUE_IO;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool open_radio(void*c,uint64_t*out){(void)c;*out=1;return true;}
static bool poll_radio(void*c,uint64_t session,uint32_t budget){(void)c;assert(session==1&&budget==6);provider_polls++;return true;}
static bool radio_status(void*c,risc_ble_sensor_status_v1*out){(void)c;*out=(risc_ble_sensor_status_v1){.struct_size=sizeof(*out),.state=BLE_SCANNING,.count=provider_count};return true;}
static bool device(void*c,uint32_t i,ble_device*out){(void)c;assert(i<provider_count);*out=(ble_device){.address={(uint8_t)i},.rssi=-50,.bthome=true};snprintf(out->name,sizeof(out->name),"Sensor %u",i);return true;}
static bool close_radio(void*c,uint64_t session){(void)c;assert(session==1);closes++;return true;}
static const risc_bluetooth_sensors_v1 radio={1,sizeof(radio),NULL,open_radio,poll_radio,radio_status,device,close_radio};
static bool acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out){
 assert(version==1);if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);out->api=&kv;}else{assert(!strcmp(name,RISC_BLUETOOTH_SENSORS_CAPABILITY)&&instance==0);out->api=&radio;}
 out->slot=++grants;out->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*out){assert(grants&&out->api);grants--;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out)};return true;}
static bool diagnostic(const char*s){(void)s;return true;}
static void yield(uint32_t ms){(void)ms;assert(!"Unexpected retain");}
static void record(void){assert(trace_at<4);trace[pass][trace_at++]=selected+100*scroll+10000*detail+100000*detail_index;}
static void tap_row(t5_app_input_t*out,unsigned row){
 if(paper){t5_app_input_t empty={0};contact_sample=(springboard_contact){.down=true,.x=100,.y=120+(int)row*88};assert(paper_input(&empty));contact_sample.down=false;}
 else *out=(t5_app_input_t){.tapped=true,.touch_x=100,.touch_y=(int16_t)(72+row*44)};
}
static bool poll_input(t5_app_input_t*out,uint32_t ms){
 assert(ms==20);tick+=ms;polls++;*out=(t5_app_input_t){0};
 if(polls==1){if(use_paper)paper=&test_paper;out->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 if(cadence){
  if(polls==2){assert(active()&&provider_polls==1);available=false;out->buttons=T5_APP_BUTTON_DOWN;}
  if(polls==3){assert(utility_frame_pending&&presents==1);available=true;}
  if(polls==4){assert(!utility_frame_pending&&presents==2&&tick<1000&&provider_polls==3);}
  if(polls>4&&tick<3040)assert(presents==2);
  if(polls==154){assert(presents==3&&provider_polls==153);out->exit_requested=true;}
  return true;
 }
 if(polls>=2&&polls<=9)out->buttons=T5_APP_BUTTON_DOWN;
 if(polls==10){assert(selected==7&&scroll==(use_paper?2u:5u));record();provider_count=2;}
 if(polls==11){assert(selected==1&&!scroll&&scan.count==2);record();if(use_paper)tap_row(out,0);else out->buttons=T5_APP_BUTTON_CONFIRM;}
 if(polls==12){assert(detail&&detail_index==(use_paper?0u:1u));provider_count=0;}
 if(polls==13){assert(!detail&&!selected&&!scroll&&!scan.count);record();provider_count=8;}
 if(polls==14){assert(!detail&&!selected&&!scroll&&scan.count==8);tap_row(out,1);}
 if(polls==15){assert(detail&&detail_index==1);record();out->buttons=T5_APP_BUTTON_BACK;}
 if(polls==16){assert(!detail);if(pass)assert(!presents&&utility_frame_pending);available=true;dirty=true;}
 if(polls==17){assert(presents&&!utility_frame_pending);if(pass)assert(presents==1&&last_selected==1&&!last_scroll&&!last_detail);out->exit_requested=true;}
 return true;
}
const t5_app_api_v1*t5_app_get_api(uint32_t v){assert(v==1);return &api;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return &rt;}
static void setup(void){
 tick=polls=presents=ready_checks=provider_polls=closes=grants=trace_at=0;provider_count=8;paper=NULL;contact_sample=(springboard_contact){0};available=pass==0;
 api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(api),.screen_width=width,.screen_height=height,.millis=clock_ms,.present=present,.poll=poll_input,.set_back_exits_app=back_exits,.touch_contact=touch,.fill_rect=fill_rect,.draw_icon=icon,.frame_ready=ready};
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.diagnostic=diagnostic,.yield_ms=yield};
}
int main(void){
 for(unsigned profile=0;profile<2;profile++){
  use_paper=profile!=0;cadence=false;memset(trace,0,sizeof(trace));
  for(pass=0;pass<2;pass++){setup();app_main();assert(!grants&&!active()&&closes==1&&polls==17&&provider_polls==(use_paper?16u:15u)&&trace_at==4);}
  assert(!memcmp(trace[0],trace[1],sizeof(trace[0])));
 }
 use_paper=cadence=true;pass=0;setup();app_main();assert(polls==154&&presents==3&&closes==1&&!grants);
 puts("PASS Scanner LCD/paper ready/withheld equivalence: shrink/empty/repopulate, row actions, prompt retry, unchanged scan cadence");return 0;
}
