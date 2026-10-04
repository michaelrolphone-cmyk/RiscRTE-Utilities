#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#ifndef DAILY_ALARM_KIND
#define DAILY_ALARM_KIND 1
#endif
#include "../../Apps/alarm_app_shared.h"
static uint8_t stored[32];static uint32_t stored_n;
static uint32_t ticks,put_calls,release_calls,event_index,service_steps,stop_calls;
static bool retain_test,stop_fails,diagnosed;static jmp_buf retained;static char status_text[80];
static int32_t get_error,put_error;static bool persist_error,clock_good,deny_service,has_alert;
static twatch_rtc_time_v1 rtc_value;
static int width_value=240,height_value=240;
static t5_app_input_t events[64];static unsigned event_count;
static t5_app_api_v1 fake_app;static risc_runtime_api_v1 rt_api;
static risc_key_value_v1 kv_api;static twatch_rtc_api_v1 time_api;static alarm_service_v1 service_api;
static int width(void){return width_value;}static int height(void){return height_value;}static uint32_t millis(void){return ticks;}
static void clear(void){}static void text(int x,int y,const char*s){(void)x;(void)y;assert(s);}
static void label(int x,int y,int w,const char*s){assert(x>=0&&y>=0&&w>0&&x+w<=width_value&&s);if(y==141)snprintf(status_text,sizeof(status_text),"%s",s);}
static void rect(int x,int y,int w,int h,bool black){(void)black;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=width_value&&y+h<=height_value);}
static void present(bool full){assert(!full);}
static bool poll(t5_app_input_t*out,uint32_t wait){ticks+=wait;*out=(t5_app_input_t){0};assert(event_index<10000);if(event_index==event_count)return false;*out=events[event_index++];return true;}
static void yield_ms(uint32_t n){ticks+=n;if(retain_test&&n==50)longjmp(retained,1);}
static bool diagnostic(const char*s){assert(strstr(s,"output-stop-unconfirmed"));diagnosed=true;return true;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(!strcmp(k,DAILY_ALARM_KIND==1?ALARM_CONFIG_KEY:ALARM_TIMER_KEY));*n=0;if(get_error)return get_error;if(!stored_n)return -1;assert(cap>=stored_n);memcpy(b,stored,stored_n);*n=stored_n;return 0;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!strcmp(k,DAILY_ALARM_KIND==1?ALARM_CONFIG_KEY:ALARM_TIMER_KEY)&&n==32);put_calls++;if(!put_error||persist_error){memcpy(stored,b,n);stored_n=n;}return put_error;}
static bool read_time(void*c,twatch_rtc_time_v1*out){(void)c;*out=rtc_value;return clock_good;}
static int32_t state(void*c,alarm_status_v1*out){(void)c;assert(out->struct_size==sizeof(*out));*out=(alarm_status_v1){.struct_size=sizeof(*out),.api_version=1,.state=ALARM_STATE_READY,.rtc_seconds=1000};if(has_alert){out->state=ALARM_STATE_ALERT;out->occurrence=(alarm_token_v1){1,1,1000,1};strcpy(out->label,"ALARM");}if(stored_n){alarm_config cfg;assert(alarm_config_decode(&cfg,stored,stored_n,DAILY_ALARM_KIND));out->schedules[DAILY_ALARM_KIND-1]=(alarm_schedule_status_v1){cfg.revision,cfg.deadline,cfg.enabled?ALARM_SCHEDULE_ARMED:ALARM_SCHEDULE_OFF};}return 0;}
static int32_t step_fake(void*c){(void)c;service_steps++;return 0;}static int32_t refresh_fake(void*c){(void)c;return 1;}
static int32_t stop_only_fake(void*c){(void)c;stop_calls++;if(stop_fails)return stop_calls<3?ALARM_PENDING:ALARM_OUTPUT;return 0;}
static int32_t ack(void*c,const alarm_token_v1*t){(void)c;assert(has_alert&&t->generation==1);has_alert=false;return 1;}
static bool acquire(const char*cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){assert(!instance);if(!strcmp(cap,"storage.key-value")){assert(version==1);g->api=&kv_api;}else if(!strcmp(cap,"rtc.clock")){assert(version==2);g->api=&time_api;}else {assert(!strcmp(cap,ALARM_SERVICE_CAPABILITY)&&version==1);if(deny_service)return false;g->api=&service_api;}return true;}
static bool release(risc_runtime_capability_v1*g){(void)g;release_calls++;return true;}
const t5_app_api_v1*t5_app_get_api(uint32_t version){assert(version==1);return &fake_app;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){assert(version==1);return &rt_api;}
static void setup(void){memset(stored,0,32);stored_n=ticks=put_calls=release_calls=event_index=event_count=service_steps=stop_calls=0;retain_test=stop_fails=diagnosed=false;status_text[0]=0;get_error=put_error=0;persist_error=deny_service=has_alert=false;clock_good=true;width_value=height_value=240;rtc_value=(twatch_rtc_time_v1){2026,10,4,0,12,0,0};fake_app=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=width,.screen_height=height,.clear=clear,.draw_text=text,.draw_label=label,.fill_rect=rect,.present=present,.poll=poll,.millis=millis};rt_api=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt_api),.acquire=acquire,.release=release,.yield_ms=yield_ms,.diagnostic=diagnostic};kv_api=(risc_key_value_v1){1,sizeof(kv_api),NULL,get,put};time_api=(twatch_rtc_api_v1){2,sizeof(time_api),NULL,read_time,NULL,NULL,NULL};service_api=(alarm_service_v1){1,sizeof(service_api),NULL,state,step_fake,refresh_fake,ack,NULL,stop_only_fake};}
static void tap(int x,int y){events[event_count++]=(t5_app_input_t){.tapped=true,.touch_x=x,.touch_y=y};}
static alarm_config saved(void){alarm_config c;assert(alarm_config_decode(&c,stored,stored_n,DAILY_ALARM_KIND));return c;}
#ifdef PORTABLE_ALARM_CLIENT
bool portable_app_sleep_retained(void){return retain_test;}
#endif
int main(void){
#ifdef PORTABLE_ALARM_CLIENT
 setup();app_main();assert(!put_calls&&release_calls==3&&!service_steps&&!stop_calls);
 setup();stop_fails=true;app_main();assert(release_calls==3&&!stop_calls&&!service_steps&&!diagnosed);
 setup();retain_test=true;app_main();assert(!release_calls&&!stop_calls&&!service_steps);
#else
 setup();app_main();assert(!put_calls&&release_calls==3&&!service_steps&&stop_calls==1);
 setup();stop_fails=retain_test=true;if(!setjmp(retained))app_main();assert(!release_calls&&stop_calls==3&&!service_steps&&diagnosed);
#endif
 setup();tap(50,195);app_main();assert(put_calls==1&&saved().enabled&&saved().revision==1);
 setup();tap(50,195);tap(180,195);app_main();assert(put_calls==2&&!saved().enabled&&saved().revision==2);
 setup();put_error=-5;persist_error=true;tap(50,195);app_main();assert(put_calls==1&&saved().enabled&&!writer.uncertain);
 setup();put_error=-5;tap(50,195);tap(50,195);app_main();assert(put_calls==2&&!stored_n&&writer.uncertain);
 setup();put_error=-5;tap(50,195);tap(180,195);app_main();assert(put_calls==1&&writer.uncertain&&!stored_n);
 setup();get_error=-5;tap(50,195);app_main();assert(!put_calls&&!writer.loaded);
 setup();clock_good=false;tap(50,195);app_main();assert(!put_calls);
 setup();deny_service=true;app_main();assert(!put_calls&&release_calls==2);
 setup();has_alert=true;events[event_count++]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};tap(50,195);app_main();assert(!has_alert&&!put_calls);
 setup();width_value=160;tap(50,45);tap(100,115);app_main();assert(editing&&!put_calls);
 setup();width_value=159;app_main();assert(!release_calls);
 setup();app=&fake_app;writer=(alarm_writer){0};values[0]=values[1]=values[2]=0;service_valid=true;
 service_state=(alarm_status_v1){.state=ALARM_STATE_BLOCKED,.error=ALARM_RTC,.occurrence={1,1,100,1}};strcpy(service_state.label,"ALARM");draw();assert(strstr(status_text,"RTC CHANGED"));
#if DAILY_ALARM_KIND == 1
 setup();uint32_t saved_seconds;assert(alarm_calendar_seconds(2026,10,5,7,0,0,&saved_seconds));
 alarm_config armed={1,saved_seconds,saved_seconds-3600,0,1,1};alarm_config_encode(&armed,stored);stored_n=32;
 twatch_rtc_time_v1 saved_raw,display;assert(deadline_time(saved_seconds,&saved_raw)&&portable_time_forward(&saved_raw,&display));
 tap(50,45);tap(50,225);app_main();assert(values[0]==display.hour&&values[1]==display.minute&&!editing);
 setup();rtc=&time_api;uint32_t now,deadline;values[0]=0;values[1]=0;
#ifdef PORTABLE_RTC_UTC8_DENVER
 rtc_value=(twatch_rtc_time_v1){2026,10,5,1,13,59,50};
#else
 rtc_value=(twatch_rtc_time_v1){2026,10,4,0,23,59,50};
#endif
 assert(read_clock(&rtc_value,&now));assert(next_alarm(&rtc_value,now,&deadline)&&deadline==now+10);
#ifdef PORTABLE_RTC_UTC8_DENVER
 values[0]=1;values[1]=30;rtc_value=(twatch_rtc_time_v1){2026,11,1,0,14,0,0};assert(read_clock(&rtc_value,&now));assert(!next_alarm(&rtc_value,now,&deadline));
 values[0]=2;values[1]=30;rtc_value=(twatch_rtc_time_v1){2026,3,8,0,15,0,0};assert(read_clock(&rtc_value,&now));assert(!next_alarm(&rtc_value,now,&deadline));
#endif
#endif
 puts("Production schedule app UI, commit, uncertainty, cancel, invalid RTC, alert and grant fixtures passed");return 0;
}
