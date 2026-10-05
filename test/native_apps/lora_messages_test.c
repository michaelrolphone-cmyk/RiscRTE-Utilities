#include <assert.h>
#include <setjmp.h>
#define LORA_RETURN_APP "springboard.elf"
#include "../../Apps/lora_messages.c"
static unsigned radio_configures,radio_sends,radio_receives,radio_cancels,radio_polls,leases,writes,frames,api_polls;
static bool denied_radio,denied_store,bad_radio,fail_save,fail_cancel,fail_release,fail_radio_poll;
static unsigned returned_state;
static uint8_t saved[32];static uint32_t saved_size;
static const twatch_lora_config_v2 test_profile={.frequency_hz=915000000,.bandwidth_hz=125000,.preamble=8,.sf=7,.coding_rate=5,.power_dbm=0};
static bool airplane,policy_error,return_denied;static unsigned launches;
static unsigned ticks;static jmp_buf retained;
static bool c_config(void*c,const twatch_lora_config_v2*p){(void)c;assert(lm_profile_valid(p));radio_configures++;return true;}
static bool c_send(void*c,const uint8_t*b,size_t n,uint32_t ms){(void)c;assert(b&&n&&ms==60000);radio_sends++;returned_state=TW_LORA_TX;return true;}
static bool c_receive(void*c,uint32_t ms){(void)c;assert(ms==5000);radio_receives++;returned_state=TW_LORA_RX;return true;}
static bool c_poll(void*c,twatch_lora_status_v2*s){(void)c;radio_polls++;*s=(twatch_lora_status_v2){.state=(uint8_t)returned_state,.length=7,.rssi_dbm=-104,.snr_quarter_db=-5};return !fail_radio_poll;}
static bool c_read(void*c,uint8_t*b,size_t cap,size_t*n){(void)c;assert(cap>=7);memcpy(b,"Hello!?",7);*n=7;return true;}
static bool c_cancel(void*c){(void)c;radio_cancels++;return !fail_cancel;}
static twatch_radio_api_v2 radio_api={2,sizeof(radio_api),NULL,c_config,c_send,c_receive,c_poll,c_read,c_cancel};
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;*n=0;if(!strcmp(k,"quick_radio")){if(policy_error)return RISC_KEY_VALUE_IO;if(!airplane)return RISC_KEY_VALUE_NOT_FOUND;uint8_t p[]={0x51,1,4,0xa1};assert(cap>=4);memcpy(b,p,4);*n=4;return 0;}if(!strcmp(k,"time_format")){assert(cap>=4);uint8_t t[]={0x54,1,1,0xa4};memcpy(b,t,4);*n=4;return 0;}
 assert(!strcmp(k,LM_PROFILE_KEY));if(!saved_size)return RISC_KEY_VALUE_NOT_FOUND;
 assert(cap>=saved_size);memcpy(b,saved,saved_size);*n=saved_size;return 0;
}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!strcmp(k,LM_PROFILE_KEY)&&n==32);memcpy(saved,b,n);saved_size=n;writes++;return fail_save?RISC_KEY_VALUE_IO:0;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;*t=(twatch_rtc_time_v1){2026,10,5,1,23,59,0};return true;}
static const twatch_rtc_api_v1 clock_api={.api_version=2,.struct_size=sizeof(clock_api),.read=rtc_read};
static bool rt_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 assert(g->struct_size==sizeof(*g));
 if(!strcmp(n,"radio.lora")){assert(v==2&&id==0);if(denied_radio)return false;radio_api.api_version=bad_radio?1:2;g->api=&radio_api;}
 else if(!strcmp(n,"storage.key-value")){assert(v==1&&(id==1||id==9));if(denied_store&&id==9)return false;g->api=&kv;}
 else if(!strcmp(n,"rtc.clock")){assert(v==2&&id==0);g->api=&clock_api;}else return false;
 leases++;return true;
}
static bool rt_release(risc_runtime_capability_v1*g){assert(g->api&&leases);if(fail_release)return false;g->api=NULL;leases--;return true;}
static bool diag(const char*s){assert(strstr(s,"cleanup-unconfirmed"));return true;}
static void yield(uint32_t ms){(void)ms;longjmp(retained,1);}
static bool rt_launch(const char*s){assert(!strcmp(s,"springboard.elf")&&!model.owned);launches++;return !return_denied;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.yield_ms=yield,.diagnostic=diag,.acquire=rt_acquire,.release=rt_release,.request_launch=rt_launch};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return &rt;}
static int32_t size(void){return 240;}
static uint32_t now(void){return ticks;}
static void present(bool full){(void)full;frames++;}
static void back_exits(bool enabled){assert(!enabled);}
static bool poll_input(t5_app_input_t *in,uint32_t wait){assert(wait==30);api_polls++;*in=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};return true;}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),.screen_width=size,.screen_height=size,.millis=now,.present=present,.set_back_exits_app=back_exits,.poll=poll_input};
const t5_app_api_v1 *t5_app_get_api(uint32_t v){assert(v==1);return &app_api;}
void portable_nova_begin(void){}
void portable_nova_text(unsigned f,int x,int y,int w,const char*s,uint32_t c){(void)f;(void)c;assert(s&&x>=0&&y>=0&&y<240&&w>=0&&x+w<=240);}
void portable_nova_center(unsigned f,int x,int y,int w,const char*s,uint32_t c){portable_nova_text(f,x,y,w,s,c);}
void portable_nova_rule(int x,int y,int w){assert(x>=0&&y>=0&&x+w<=240);}
void portable_nova_button(int x,int y,int w,int h,const char*s,bool selected){(void)selected;assert(s&&x>=0&&y>=0&&x+w<=240&&y+h<=240);}
void portable_nova_row(int x,int y,int w,int h,const char*a,const char*b,bool selected){assert(a&&b);portable_nova_button(x,y,w,h,a,selected);}
void portable_nova_round(int x,int y,int w,int h,int radius,uint32_t c){(void)radius;(void)c;assert(x>=0&&y>=0&&x+w<=240&&y+h<=240);}
bool portable_nova_wrap(unsigned f,int x,int y,int w,int line,unsigned count,const char*s,uint32_t c){assert(y+line*(int)count<=240);portable_nova_text(f,x,y,w,s,c);return true;}
bool portable_nova_hit(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
static void reset(void){
 memset(&model,0,sizeof(model));memset(grants,0,sizeof(grants));memset(acquired,0,sizeof(acquired));memset(draft,0,sizeof(draft));memset(entry,0,sizeof(entry));
 app=&app_api;runtime=&rt;store=preferences=NULL;rtc=NULL;page=PAGE_HOME;notice=NULL;dirty=true;time_format=0;compose_offset=0;
 radio_configures=radio_sends=radio_receives=radio_cancels=radio_polls=leases=writes=frames=api_polls=0;
 airplane=policy_error=return_denied=false;launches=0;
 denied_radio=denied_store=bad_radio=fail_save=fail_cancel=fail_release=fail_radio_poll=false;saved_size=0;
}
static void tap(int x,int y){t5_app_input_t in={.tapped=true,.touch_x=(int16_t)x,.touch_y=(int16_t)y};assert(input(&in));if(dirty)draw();}
static void type(const char*s){for(;*s;s++){unsigned ch=(unsigned char)*s;key_page=(ch-32)/32;key((ch-32)%32);}}
static void clean(void){assert(portable_radio_suspend());for(unsigned i=4;i>0;i--)release_grant(i-1);assert(!leases);}
int main(void){
 reset();app_main();assert(!leases&&!radio_configures&&!radio_sends&&!radio_receives&&!writes&&frames==1&&api_polls==1);
 reset();lm_profile_encode(&test_profile,saved);saved_size=32;app_main();assert(!radio_configures&&!radio_receives&&!radio_sends&&!writes&&!leases);
 reset();load_preferences();assert(time_format==1&&!model.profile_valid&&leases==3);tap(50,200);assert(!radio_configures);tap(175,200);assert(page==PAGE_RF&&!rf_fields);
 tap(175,210);assert(page==PAGE_RF_ERROR);tap(100,210);assert(page==PAGE_RF);
 const char *values[]={"915000000","125000","7","5","0","8"};
 for(unsigned i=0;i<6;i++){edit(i);type(values[i]);key(PWK_DONE);assert(page==PAGE_RF);}
 assert(rf_fields==63);apply_profile();assert(page==PAGE_HOME&&model.profile_valid&&writes==1&&!radio_configures&&!radio_sends);
 tap(50,200);assert(model.listening&&radio_receives==1&&radio_configures==1);assert(portable_radio_suspend());assert(!model.owned&&!model.listening);assert(!radio_sends);
 tap(100,100);assert(page==PAGE_COMPOSE);tap(80,80);assert(page==PAGE_KEYBOARD);
 for(unsigned ch=32;ch<=126;ch++){entry[0]=0;char text[2]={(char)ch,0};type(text);assert((unsigned char)entry[0]==ch);}
 entry[0]=0;type("Hello, World! 123");tap(200,190);assert(page==PAGE_COMPOSE&&!strcmp(draft,"Hello, World! 123")&&!radio_sends);
 tap(50,210);type("changed");assert(back());assert(page==PAGE_COMPOSE&&!strcmp(draft,"Hello, World! 123"));
 tap(170,210);assert(model.state==LM_SENDING&&radio_sends==1&&model.count==1);assert(model.history[0].clock_valid&&model.history[0].hour==23);
 assert(!lm_send(&model,draft,now())&&radio_sends==1);returned_state=TW_LORA_SENT;assert(lm_step(&model,1));assert(model.history[0].status==LM_OUT_SENT);draw();
 tap(170,210);assert(radio_sends==2);fail_radio_poll=true;assert(!lm_step(&model,2));assert(model.state==LM_SEND_UNKNOWN&&!strcmp(draft,"Hello, World! 123"));draw();fail_radio_poll=false;
 tap(170,210);assert(radio_sends==3);tap(170,210);assert(model.state==LM_STOPPED&&model.history[2].status==LM_OUT_UNKNOWN);
 assert(back());tap(100,150);assert(page==PAGE_HISTORY);tap(100,50);assert(page==PAGE_MESSAGE);draw();assert(back()&&page==PAGE_HISTORY);assert(back()&&page==PAGE_HOME);
 char clock[32];lm_message msg={.clock_valid=true,.month=10,.day=5,.hour=0,.minute=4};time_format=0;time_text(&msg,clock);assert(!strcmp(clock,"10/05 12:04 AM"));msg.hour=12;time_text(&msg,clock);assert(strstr(clock,"12:04 PM"));time_format=1;time_text(&msg,clock);assert(!strcmp(clock,"10/05 12:04"));msg.clock_valid=false;time_text(&msg,clock);assert(!strcmp(clock,"Time unset"));
 edit(6);memset(entry,'W',LM_TEXT_MAX);entry[LM_TEXT_MAX]=0;key(1);assert(strlen(entry)==LM_TEXT_MAX);key(PWK_DONE);assert(strlen(draft)==LM_TEXT_MAX);tap(50,100);assert(compose_offset==90);tap(50,100);assert(compose_offset==180);tap(50,100);assert(compose_offset==0);draw();clean();
 reset();load_preferences();editing_profile=test_profile;rf_fields=63;fail_save=true;apply_profile();assert(model.profile_valid&&strstr(notice,"unconfirmed")&&!radio_configures);clean();
 reset();denied_store=true;load_preferences();editing_profile=test_profile;rf_fields=63;apply_profile();assert(strstr(notice,"session only")&&!writes);clean();
 reset();denied_radio=true;assert(!open_radio()&&leases==1);denied_radio=false;bad_radio=true;assert(!open_radio()&&leases==1);bad_radio=false;
 airplane=true;assert(!open_radio()&&leases==1&&!radio_configures);airplane=false;policy_error=true;assert(!open_radio()&&leases==1);policy_error=false;
 assert(open_radio()&&leases==2);model.profile=test_profile;model.profile_valid=true;assert(lm_listen(&model,0));fail_cancel=true;
 if(!setjmp(retained)){if(!portable_radio_suspend())retain();assert(!"must retain");}assert(model.uncertain&&model.owned&&leases==2);assert(!portable_radio_services_safe());
 reset();return_denied=true;assert(back()&&page==PAGE_HOME&&launches==1&&strstr(notice,"retry"));return_denied=false;assert(!back()&&launches==2);
 reset();load_preferences();fail_release=true;if(!setjmp(retained)){release_grant(1);assert(!"must retain");}assert(leases==3&&acquired[1]);
 puts("LoRa app: no implicit RF, exact standard keyboard, compose/cancel/retry/history,12/24h, profile errors, bounds and fail-closed cleanup passed");return 0;
}
