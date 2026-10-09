#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#ifndef DAILY_ALARM_KIND
#define DAILY_ALARM_KIND 1
#endif
#define ALARM_NATIVE_UTC
#define ALARM_SERVICE_TAGGED_V2
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_APP_LAUNCH_GUARD
#define PORTABLE_NATIVE_TIME_TOOLBAR
#include "../../Apps/alarm_app_shared.h"
static uint8_t stored[32];static uint32_t stored_n;
static uint32_t ticks,put_calls,release_calls,event_index,service_steps,stop_calls;
static bool retain_test,stop_fails,diagnosed;static jmp_buf retained;static char status_text[80];
static int32_t get_error,put_error;static bool persist_error,clock_good,deny_service,has_alert;
static twatch_rtc_time_v1 rtc_value;
static risc_realtime_snapshot_v1 native_sample;
static risc_realtime_api_v1 native_api;
static unsigned native_reads,retains,acquire_calls,deny_acquire_at;
static bool dirty_denial;
static int native_error;
static bool reader_live,deny_native,fail_release;static bool retained_status,retained_refresh;
static int zone_error,readback_error;static unsigned recover_at;
static uint8_t zone_bytes[44];static uint32_t zone_size;
static int width_value=240,height_value=240;
static t5_app_input_t events[64];static unsigned event_count;
static t5_app_api_v1 fake_app;static risc_runtime_api_v1 rt_api;
static risc_key_value_v1 kv_api;static twatch_rtc_api_v1 time_api;static alarm_service_descriptor_v2 service_api;
static int width(void){return width_value;}static int height(void){return height_value;}static uint32_t millis(void){return ticks;}
static void clear(void){assert(!native_retained);}static void text(int x,int y,const char*s){assert(!native_retained);(void)x;(void)y;assert(s);}
static void label(int x,int y,int w,const char*s){assert(!native_retained);assert(x>=0&&y>=0&&w>0&&x+w<=width_value&&s);if(y==141)snprintf(status_text,sizeof(status_text),"%s",s);}
static void rect(int x,int y,int w,int h,bool black){(void)black;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=width_value&&y+h<=height_value);}
static void present(bool full){assert(!native_retained&&!reader_live&&!full);}
static bool poll(t5_app_input_t*out,uint32_t wait){assert(!native_retained&&!reader_live);ticks+=wait;*out=(t5_app_input_t){0};assert(event_index<10000);if(recover_at&&event_index==recover_at){readback_error=put_error=0;}if(event_index==event_count)return false;*out=events[event_index++];return true;}
static void yield_ms(uint32_t n){ticks+=n;if(retain_test&&n==50)longjmp(retained,1);}
static bool diagnostic(const char*s){assert(strstr(s,"output-stop-unconfirmed"));diagnosed=true;return true;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){assert(!native_retained);(void)c;if(!strcmp(k,"time_zone")){*n=zone_size;if(zone_error)return zone_error;if(!zone_size)return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=zone_size);memcpy(b,zone_bytes,zone_size);return 0;}if(!strcmp(k,"time_format")){*n=0;return RISC_KEY_VALUE_NOT_FOUND;}assert(!strcmp(k,DAILY_ALARM_KIND==1?ALARM_CONFIG_KEY:ALARM_TIMER_KEY));*n=0;if(readback_error&&put_calls)return readback_error;if(get_error)return get_error;if(!stored_n)return -1;assert(cap>=stored_n);memcpy(b,stored,stored_n);*n=stored_n;return 0;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){assert(!native_retained);(void)c;assert(!strcmp(k,DAILY_ALARM_KIND==1?ALARM_CONFIG_KEY:ALARM_TIMER_KEY)&&n==32);put_calls++;if(!put_error||persist_error){memcpy(stored,b,n);stored_n=n;}return put_error;}
static bool read_time(void*c,twatch_rtc_time_v1*out){(void)c;*out=rtc_value;return clock_good;}
static int32_t state(void*c,alarm_status_v1*out){assert(!native_retained);if(retained_status){*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_BLOCKED,.error=ALARM_RETAINED,.output_uncertain=1};return ALARM_OK;}(void)c;assert(out->struct_size==sizeof(*out));*out=(alarm_status_v1){.struct_size=sizeof(*out),.api_version=1,.state=ALARM_STATE_READY,.rtc_seconds=1000};if(has_alert){out->state=ALARM_STATE_ALERT;out->occurrence=(alarm_token_v1){1,1,1000,1};strcpy(out->label,"ALARM");}if(stored_n){alarm_config cfg;assert(alarm_config_decode(&cfg,stored,stored_n,DAILY_ALARM_KIND));out->schedules[DAILY_ALARM_KIND-1]=(alarm_schedule_status_v1){cfg.revision,cfg.deadline,cfg.enabled?ALARM_SCHEDULE_ARMED:ALARM_SCHEDULE_OFF};}return 0;}
static int32_t prepare_fake(void*c,alarm_sleep_v1*t){(void)c;(void)t;return ALARM_INVALID;}
static int32_t step_fake(void*c){(void)c;service_steps++;return 0;}static int32_t refresh_fake(void*c){(void)c;assert(!native_retained);return retained_refresh?ALARM_RETAINED:1;}
static int32_t stop_only_fake(void*c){(void)c;stop_calls++;if(stop_fails)return stop_calls<3?ALARM_PENDING:ALARM_OUTPUT;return 0;}
static int32_t ack(void*c,const alarm_token_v1*t){(void)c;assert(has_alert&&t->generation==1);has_alert=false;return 1;}
static int32_t read_native(void *context,risc_realtime_snapshot_v1 *out){assert(context&&reader_live&&!native_retained);native_reads++;if(native_error)return native_error;*out=native_sample;return 0;}
static bool retain_invocation(void){return true;}
void portable_adapter_retain(void){retains++;}
bool portable_adapter_retained(void){return retains!=0;}
static bool acquire(const char*cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){assert(!native_retained);acquire_calls++;if(deny_acquire_at==acquire_calls){*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=dirty_denial?9:0};return false;}g->slot=1;g->generation=1;if(!strcmp(cap,"storage.key-value")){assert(version==1&&(instance==1||instance==3));g->api=&kv_api;}else if(!strcmp(cap,RISC_REALTIME_CAPABILITY)){assert(version==1&&!instance&&!reader_live);if(deny_native){*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return false;}reader_live=true;g->slot=2;g->api=&native_api;}else {assert(!strcmp(cap,ALARM_SERVICE_CAPABILITY)&&version==2&&!instance);if(deny_service){*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return false;}g->api=&service_api;}return true;}
static bool release(risc_runtime_capability_v1*g){assert(!native_retained);release_calls++;if(fail_release)return false;if(g->slot==2){assert(reader_live);reader_live=false;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
const t5_app_api_v1*t5_app_get_api(uint32_t version){assert(version==1);return &fake_app;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){assert(version==1);return &rt_api;}
static void setup(void){acquire_calls=deny_acquire_at=0;dirty_denial=false;native_retained=false;native_client=(portable_realtime_client){0};native_reads=retains=0;native_error=zone_error=readback_error=0;recover_at=0;retained_status=retained_refresh=false;reader_live=deny_native=fail_release=false;zone_size=0;native_sample=(risc_realtime_snapshot_v1){.struct_size=sizeof(native_sample),.validity=RISC_REALTIME_VALID,.epoch_seconds=1791115200};native_api=(risc_realtime_api_v1){1,sizeof(native_api),&native_api,read_native};memset(stored,0,32);stored_n=ticks=put_calls=release_calls=event_index=event_count=service_steps=stop_calls=0;retain_test=stop_fails=diagnosed=false;status_text[0]=0;get_error=put_error=0;persist_error=deny_service=has_alert=false;clock_good=true;width_value=height_value=240;rtc_value=(twatch_rtc_time_v1){2026,10,4,0,12,0,0};fake_app=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=width,.screen_height=height,.clear=clear,.draw_text=text,.draw_label=label,.fill_rect=rect,.present=present,.poll=poll,.millis=millis};rt_api=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt_api),.acquire=acquire,.release=release,.yield_ms=yield_ms,.diagnostic=diagnostic,.retain_invocation=retain_invocation};kv_api=(risc_key_value_v1){1,sizeof(kv_api),NULL,get,put};time_api=(twatch_rtc_api_v1){2,sizeof(time_api),NULL,read_time,NULL,NULL,NULL};service_api=(alarm_service_descriptor_v2){.base={2,sizeof(service_api),NULL,state,step_fake,refresh_fake,ack,prepare_fake,stop_only_fake},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1};}
static void tap(int x,int y){events[event_count++]=(t5_app_input_t){.tapped=true,.touch_x=x,.touch_y=y};}
static alarm_config saved(void){alarm_config c;assert(alarm_config_decode(&c,stored,stored_n,DAILY_ALARM_KIND));return c;}
#ifdef PORTABLE_ALARM_CLIENT
bool portable_app_sleep_retained(void){return retain_test;}
#endif
static void zone(const char *id){memset(zone_bytes,0,sizeof(zone_bytes));zone_bytes[0]='T';zone_bytes[1]='Z';zone_bytes[2]=1;strcpy((char *)zone_bytes+4,id);zone_bytes[3]=0xa5;for(unsigned i=0;i<sizeof(zone_bytes);i++)if(i!=3)zone_bytes[3]^=zone_bytes[i];zone_size=44;}
int main(void){
 for(unsigned dirty=0;dirty<2;dirty++)for(unsigned failed=1;failed<=3;failed++){setup();deny_acquire_at=failed;dirty_denial=dirty;app_main();assert(native_retained&&retains==1&&acquire_calls==failed&&!release_calls&&!put_calls&&!event_index&&!native_reads);}

 setup();app_main();assert(!put_calls&&!reader_live&&!retains);
 setup();tap(50,195);app_main();assert(put_calls==1&&saved().enabled&&saved().revision==1&&!reader_live);
 setup();tap(50,195);tap(180,195);app_main();assert(put_calls==2&&!saved().enabled&&saved().revision==2);
 setup();put_error=RISC_KEY_VALUE_IO;persist_error=true;tap(50,195);app_main();assert(put_calls==1&&saved().enabled&&!writer.uncertain&&!retains);
 setup();put_error=RISC_KEY_VALUE_IO;tap(50,195);tap(50,195);app_main();assert(put_calls==2&&!stored_n&&writer.uncertain&&!retains);
 setup();put_error=RISC_KEY_VALUE_IO;tap(50,195);tap(180,195);app_main();assert(put_calls==1&&writer.uncertain&&!stored_n&&!retains);
 setup();get_error=RISC_KEY_VALUE_IO;tap(50,195);app_main();assert(!put_calls&&!writer.loaded&&!retains);
 setup();get_error=RISC_KEY_VALUE_CONTEXT;tap(50,195);app_main();assert(!put_calls&&retains==1&&!release_calls);
 setup();put_error=RISC_KEY_VALUE_CONTEXT;tap(50,195);app_main();assert(put_calls==1&&retains==1);
 setup();put_error=-99;tap(50,195);app_main();assert(put_calls==1&&retains==1);
 setup();native_error=RISC_REALTIME_CONTEXT;tap(50,195);app_main();assert(!put_calls&&retains==1&&reader_live);
 setup();native_error=-99;tap(50,195);app_main();assert(!put_calls&&retains==1&&reader_live);
 setup();native_error=RISC_REALTIME_IO;tap(50,195);app_main();assert(!put_calls&&!retains&&!reader_live);
 setup();native_sample.validity=RISC_REALTIME_UNSET;native_sample.epoch_seconds=0;tap(50,195);app_main();assert(!put_calls&&!retains&&!reader_live);
 setup();native_sample.reserved=1;tap(50,195);app_main();assert(!put_calls&&!retains&&!reader_live);
 setup();deny_native=true;tap(50,195);app_main();assert(!put_calls&&!retains&&!native_reads);
 setup();native_sample.epoch_seconds=INT64_C(2147483648);tap(50,195);app_main();assert(!put_calls&&!retains&&!reader_live);
 setup();zone_error=RISC_KEY_VALUE_IO;tap(50,195);app_main();assert(put_calls==(DAILY_ALARM_KIND==2?1u:0u)&&!retains);
 setup();zone_error=RISC_KEY_VALUE_CONTEXT;app_main();assert(!put_calls&&retains==1&&!release_calls);
 setup();fail_release=true;tap(50,195);app_main();assert(!put_calls&&retains==1&&release_calls==1);
 setup();retained_status=true;app_main();assert(retains==1&&!put_calls);
 setup();retained_refresh=true;tap(50,195);app_main();assert(retains==1&&put_calls==1);
 setup();has_alert=true;events[event_count++]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};tap(50,195);app_main();assert(!has_alert&&!put_calls);
 setup();put_error=RISC_KEY_VALUE_IO;tap(50,195);events[event_count++]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};tap(50,195);app_main();assert(put_calls==2&&writer.uncertain&&!portable_app_before_launch("default.elf")&&!retains);
 setup();zone("Asia/Kathmandu");tap(50,195);app_main();assert(put_calls==1&&!retains);
#if DAILY_ALARM_KIND == 1
 setup();app=&fake_app;assert(open_dependencies());zone("America/Denver");assert(native_load_zone());uint32_t now,deadline;twatch_rtc_time_v1 raw;
 native_sample.epoch_seconds=1772956800; /* 2026-03-08 08:00 UTC */
 assert(read_clock(&raw,&now));values[0]=2;values[1]=30;assert(!next_alarm(&raw,now,&deadline)&&strstr(notice,"GAP"));
 native_sample.epoch_seconds=1793516400; /* 2026-11-01 07:00 UTC */
 assert(read_clock(&raw,&now));values[0]=1;values[1]=30;assert(!next_alarm(&raw,now,&deadline)&&strstr(notice,"AMBIGUOUS"));close_dependencies();
#endif
 for(unsigned bad=0;bad<6;bad++){setup();switch(bad){case 0:native_sample.nanoseconds=1;break;case 1:native_sample.nanoseconds=1000000000;break;case 2:native_sample.monotonic_before_us=2;native_sample.monotonic_after_us=1;break;case 3:native_sample.struct_size=1;break;case 4:native_sample.validity=99;break;default:native_sample.epoch_seconds=946684799;break;}tap(50,195);app_main();assert(!put_calls&&!retains&&!reader_live);}
 for(unsigned bad=0;bad<5;bad++){setup();switch(bad){case 0:service_api.base.api_version=1;break;case 1:service_api.tag=0;break;case 2:service_api.features=99;break;case 3:service_api.output_modes=ALARM_MODE_BOTH;break;default:service_api.base.prepare_sleep=NULL;break;}tap(50,195);app_main();assert(!put_calls&&!retains);}
 setup();readback_error=RISC_KEY_VALUE_IO;persist_error=true;put_error=RISC_KEY_VALUE_IO;tap(50,195);tap(180,195);app_main();assert(writer.uncertain&&writer.pending.revision==1&&put_calls==1&&stored_n==32&&!retains);
 setup();readback_error=RISC_KEY_VALUE_IO;persist_error=true;put_error=RISC_KEY_VALUE_IO;recover_at=1;tap(50,195);tap(50,195);app_main();assert(!writer.uncertain&&writer.saved.revision==1&&put_calls==2&&!retains);
 setup();app=&fake_app;writer=(alarm_writer){0};assert(open_dependencies());assert(!alarm_writer_load(&writer,storage,DAILY_ALARM_KIND));values[0]=12;values[1]=1;values[2]=0;service_valid=true;service_state.state=ALARM_STATE_READY;save_action(false);assert(writer.saved.enabled);uint32_t deadline_before=writer.saved.deadline,puts_before=put_calls;zone("Asia/Kathmandu");assert(native_load_zone());restore_saved_fields();assert(writer.saved.deadline==deadline_before&&put_calls==puts_before);twatch_rtc_time_v1 local;assert(portable_app_native_local_time(&local));
 for(unsigned i=0;i<portable_timezone_count();i++){const portable_timezone_entry *entry=portable_timezone_get(i);zone(entry->id);assert(native_load_zone()&&portable_app_native_local_time(&local)&&!reader_live);assert(writer.saved.deadline==deadline_before&&put_calls==puts_before);}
 close_dependencies();
 puts("Native UTC controllers: isolated records, typed errors, closed reader, DST and custody passed");return 0;
}
