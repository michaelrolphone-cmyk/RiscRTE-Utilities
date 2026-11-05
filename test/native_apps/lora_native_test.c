/* Actual LoRa app and actual Portable adapter, with peripheral-only test doubles.
 * Including the app permits observations of its state; no replacement controller
 * or message model is used. Every scripted action enters through raw touch. */
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "RiscKeyValueV1.h"
#include "PortableNavigation.h"
#include "PortableAppSleep.h"
#include "AlarmServiceV1.h"
#include "lora_messages_model.h"
#ifdef PORTABLE_LORA_NATIVE_UI
#include "PortableLoRaNative.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int app_module_init(void);void app_module_fini(void);
static unsigned f_ticks,f_polls,f_grants,f_frames,f_subs,f_presents,f_last_poll;
static unsigned f_width=240,f_height=240,f_stop=240,f_delay=3;
static bool f_paper;
static unsigned f_stride(void){return f_paper?(f_width+7)/8+4:(f_width+4)*2;}
static unsigned f_bytes(void){return f_stride()*f_height;}
static unsigned f_pending_steps,f_completions,f_first_visible,f_retains;
static uint64_t f_pending,f_pending_hash,f_sequence,f_read_sequence;
static bool f_polled,f_manual,f_present_fault,f_unsub_fault,f_release_fault;
static unsigned f_snapshot_fault;
static uint16_t f_pixels[506*502],f_pending_pixels[506*502];
static risc_touch_snapshot_v1 f_touch;
static risc_touch_event_v1 f_events[128];
static struct {unsigned at;int x,y;bool emitted;} f_actions[512];
static unsigned f_action_count;
static const char *f_directory;
static struct {char key[32];uint8_t data[64];uint32_t size;} f_cells[32];
static unsigned f_sends,f_receives,f_configures,f_cancels,f_selects,f_selected,f_radio_state,f_rx_count,f_rx_read,f_writes,f_launches;
static bool f_radio_owned,f_seed;
static void f_observe(void);static void f_capture(void);
static unsigned f_env(const char *key,unsigned fallback){const char*v=getenv(key);return v?(unsigned)strtoul(v,NULL,0):fallback;}
static bool f_flag(const char *key){return f_env(key,0)!=0;}
static void f_live(void){assert(!f_retains);}
static uint64_t f_hash(void){uint64_t h=UINT64_C(1469598103934665603);for(unsigned i=0;i<f_bytes();i++){h^=((const uint8_t*)f_pixels)[i];h*=UINT64_C(1099511628211);}return h;}
static void f_pending_check(void){if(f_pending)assert(!memcmp(f_pixels,f_pending_pixels,f_bytes()));}
static bool f_health(risc_runtime_health_v1*h){f_live();h->uptime_ms=f_ticks;f_pending_check();f_observe();assert(f_ticks<120000);for(unsigned i=0;i<f_action_count;i++)if(!f_actions[i].emitted)return true;return true;}
static bool f_exit_requested(void){f_live();for(unsigned i=0;i<f_action_count;i++)if(!f_actions[i].emitted)return false;return f_polls>=f_stop&&!f_pending;}
static void f_yield(uint32_t n){f_live();f_pending_check();f_ticks+=n;}
static bool f_diag(const char*s){f_live();fprintf(stderr,"%s\n",s);return true;}
static bool f_launch(const char*s){f_live();assert(!f_radio_owned);assert(!strcmp(s,"springboard.elf"));f_launches++;return true;}
static bool f_retain(void){assert(!f_retains);f_retains++;return true;}
static bool f_info(void*c,risc_display_info_v1*s){(void)c;f_live();*s=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*s),.width=f_width,.height=f_height,.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|(f_paper?RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE:0),.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(f_paper?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool f_frame(void*c,uint32_t format,risc_display_surface_v1*s){(void)c;f_live();f_pending_check();assert(!f_frames&&!f_pending&&format==(f_paper?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565));f_frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=f_pixels,.width=f_width,.height=f_height,.stride_bytes=f_stride(),.size_bytes=f_bytes(),.pixel_format=format};return true;}
static void f_frame_release(void*c,risc_display_frame_v1 token){(void)c;f_live();assert(token==1&&f_frames&&!f_pending);f_frames=0;}
static bool f_submit(void*c,risc_display_frame_v1 token,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)o;f_live();assert(token==1&&f_frames&&!f_pending);for(size_t i=0;i<n;i++)assert(r[i].x+r[i].width<=f_width&&r[i].y+r[i].height<=f_height);f_frames=0;*t=++f_presents;f_pending=*t;f_pending_steps=f_delay;f_pending_hash=f_hash();memcpy(f_pending_pixels,f_pixels,f_bytes());f_capture();return true;}
static bool f_present(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*s){(void)c;f_live();assert(token&&token==f_pending);f_pending_check();if(f_present_fault)return false;if(f_pending_steps){f_pending_steps--;s->state=f_pending_steps%2?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_ACTIVE;return true;}f_pending=0;s->state=RISC_DISPLAY_PRESENT_COMPLETE;f_completions++;if(!f_first_visible)f_first_visible=f_ticks+1;printf("VISIBLE frame=%llu ticks=%u polls=%u\n",(unsigned long long)token,f_ticks,f_polls);return true;}
static const risc_display_output_api_v1 f_display={.api_version=1,.struct_size=sizeof(f_display),.get_info=f_info,.acquire=f_frame,.release=f_frame_release,.submit=f_submit,.present_status=f_present};
static void f_edge(unsigned kind,unsigned id,unsigned x,unsigned y){assert(f_sequence-f_read_sequence<127);f_sequence++;f_events[f_sequence%128]=(risc_touch_event_v1){.sequence=f_sequence,.timestamp_ms=f_ticks,.kind=(uint8_t)kind,.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};}
static uint64_t f_subscribe(void*c){(void)c;f_live();assert(!f_subs);f_subs=1;f_read_sequence=f_sequence;return 1;}
static bool f_unsubscribe(void*c,uint64_t n){(void)c;f_live();assert(n==1&&f_subs);if(f_unsub_fault)return false;f_subs=0;return true;}
static bool f_ready(void){if(f_paper)return !f_pending;
#ifdef PORTABLE_LORA_NATIVE_UI
 return portable_lora_native_ready(NULL,NULL);
#else
 return !f_pending;
#endif
}
static bool f_touch_poll(void*c,size_t n){(void)c;f_live();assert(n==1);if(f_manual){f_polls++;return true;}if(f_polled&&f_ticks==f_last_poll)return true;f_polled=true;f_last_poll=f_ticks;f_polls++;
 bool down=false;unsigned x=f_touch.contacts[0].x,y=f_touch.contacts[0].y;
 for(unsigned i=0;i<f_action_count;i++)if(!f_actions[i].emitted&&f_actions[i].at<=f_polls){bool held=f_touch.contact_count&&i&&f_actions[i-1].emitted&&f_actions[i].at==f_actions[i-1].at+1;if(f_flag("LORA_FORCE_TOUCH")||held||(!f_touch.contact_count&&f_ready())){down=true;x=(unsigned)f_actions[i].x;y=(unsigned)f_actions[i].y;f_actions[i].emitted=true;if(f_stop<f_polls+45)f_stop=f_polls+45;}break;}
 if(down!=(f_touch.contact_count!=0)||(down&&(x!=f_touch.contacts[0].x||y!=f_touch.contacts[0].y)))f_edge(!f_touch.contact_count?RISC_TOUCH_EVENT_DOWN:!down?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_MOVE,1,x,y);
 f_touch=(risc_touch_snapshot_v1){.width=f_width,.height=f_height,.sequence=f_sequence,.timestamp_ms=f_ticks,.contact_count=down};f_touch.contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};return true;
}
static int32_t f_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;f_live();assert(n==1&&f_subs);if(f_read_sequence==f_sequence)return 0;*e=f_events[(++f_read_sequence)%128];assert(e->sequence==f_read_sequence);return 1;}
static bool f_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;f_live();if(f_snapshot_fault==1&&!f_pending)return false;*s=f_touch;s->width=f_width;s->height=f_height;if(!f_pending&&f_snapshot_fault==2)s->sequence=0;if(!f_pending&&f_snapshot_fault==3)s->width=0;return true;}
static const risc_touch_api_v1 f_touch_api={1,sizeof(f_touch_api),NULL,f_subscribe,f_unsubscribe,f_touch_poll,f_next,f_snapshot};
static bool f_battery(void*c,risc_battery_sample_v1*s){(void)c;f_live();*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 f_battery_api={1,sizeof(f_battery_api),NULL,f_battery};
static bool f_rtc_read(void*c,twatch_rtc_time_v1*s){(void)c;f_live();*s=(twatch_rtc_time_v1){2026,10,19,1,17,58,12};return true;}
static bool f_rtc_write(void*c,const twatch_rtc_time_v1*s){(void)c;(void)s;assert(!"RTC is read-only");return false;}
static const twatch_rtc_api_v1 f_rtc={.api_version=2,.struct_size=sizeof(f_rtc),.read=f_rtc_read,.write=f_rtc_write};
static int32_t f_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*size){(void)c;f_live();*size=0;for(unsigned i=0;i<32;i++)if(!strcmp(k,f_cells[i].key)){*size=f_cells[i].size;if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,f_cells[i].data,*size);return RISC_KEY_VALUE_OK;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t f_put(void*c,const char*k,const void*b,uint32_t n){(void)c;f_live();assert(n<=64&&strlen(k)<32);if(!f_seed&&!strcmp(k,LM_PROFILE_KEY)){f_writes++;if(f_flag("LORA_SAVE_FAIL"))return RISC_KEY_VALUE_IO;}unsigned i=0;while(i<32&&f_cells[i].key[0]&&strcmp(k,f_cells[i].key))i++;assert(i<32);strcpy(f_cells[i].key,k);memcpy(f_cells[i].data,b,n);f_cells[i].size=n;return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 f_kv={1,sizeof(f_kv),NULL,f_get,f_put};
static int32_t f_alarm_status(void*c,alarm_status_v1*s){(void)c;f_live();*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t f_alarm_step(void*c){(void)c;f_live();return ALARM_OK;}
static int32_t f_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;f_live();return ALARM_OK;}
static int32_t f_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;f_live();*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 f_alarm={1,sizeof(f_alarm),NULL,f_alarm_status,f_alarm_step,f_alarm_step,f_alarm_ack,f_alarm_prepare,f_alarm_step};
static bool f_nav(void*c,risc_input_navigation_frame_v1*s){(void)c;f_live();*s=(risc_input_navigation_frame_v1){0};return true;}
static bool f_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;f_live();return true;}
static bool f_reset(void*c){(void)c;f_live();return true;}
static const risc_input_navigation_api_v1 f_nav_api={1,sizeof(f_nav_api),NULL,f_nav,f_foreground,f_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &f_nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;f_live();}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!"Unexpected hardware sleep");return 0;}
static bool f_configure(void*c,const twatch_lora_config_v2*p){(void)c;f_live();assert(lm_profile_valid(p));assert(!f_radio_owned);f_configures++;f_radio_owned=true;return true;}
static bool f_send(void*c,const uint8_t*b,size_t n,uint32_t ms){(void)c;f_live();assert(f_radio_owned&&b&&n&&n<=LM_TEXT_MAX&&ms==60000);f_sends++;f_radio_state=TW_LORA_TX;return !f_flag("LORA_SEND_FAIL");}
static bool f_receive(void*c,uint32_t ms){(void)c;f_live();assert(f_radio_owned&&ms==5000);f_receives++;f_radio_state=TW_LORA_RX;return true;}
static bool f_radio_poll(void*c,twatch_lora_status_v2*s){(void)c;f_live();assert(f_radio_owned);*s=(twatch_lora_status_v2){.state=f_radio_state==TW_LORA_TX?(f_flag("LORA_SEND_BUSY")?TW_LORA_TX:TW_LORA_SENT):TW_LORA_RX};if(f_radio_state==TW_LORA_RX&&f_rx_read<f_rx_count){s->state=TW_LORA_RECEIVED;s->length=255;s->rssi_dbm=-72;s->snr_quarter_db=-7;}return true;}
static bool f_radio_read(void*c,uint8_t*b,size_t cap,size_t*n){(void)c;f_live();assert(f_radio_owned&&cap>=255&&f_rx_read<f_rx_count);for(unsigned i=0;i<255;i++)b[i]=(uint8_t)('A'+(i+f_rx_read)%26);b[1]=0;b[2]=255;*n=255;f_rx_read++;return true;}
static bool f_cancel(void*c){(void)c;f_live();assert(f_radio_owned);f_cancels++;f_radio_owned=false;return true;}
static bool f_profile_info(void*c,twatch_lora_profile_info_v1*out){(void)c;f_live();assert(out->struct_size==sizeof(*out));*out=(twatch_lora_profile_info_v1){.struct_size=sizeof(*out),.supported_profiles=f_flag("LORA_FIXED")?2:15,.selected_profile=f_selected};return true;}
static bool f_select(void*c,uint32_t choice){(void)c;f_live();assert(!f_radio_owned);f_selects++;f_selected=choice;return true;}
static twatch_radio_api_v2 f_radio={2,sizeof(f_radio),NULL,f_configure,f_send,f_receive,f_radio_poll,f_radio_read,f_cancel,f_profile_info,f_select};
static bool f_acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){f_live();assert(g->struct_size==sizeof(*g));if(!strcmp(n,"display.output")&&v==1)g->api=&f_display;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&f_touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&f_battery_api;else if(!strcmp(n,"rtc.clock")&&v==2){if(f_flag("LORA_NO_RTC"))return false;g->api=&f_rtc;}else if(!strcmp(n,"storage.key-value")&&v==1){if(instance==LM_STORE_INSTANCE&&f_flag("LORA_NO_MEDIA"))return false;g->api=&f_kv;}else if(!strcmp(n,"radio.lora")&&v==2){if(f_flag("LORA_NO_RADIO"))return false;g->api=&f_radio;}else if(!strcmp(n,"alarm.service")&&v==1)g->api=&f_alarm;else return false;g->slot=++f_grants;g->generation=1;return true;}
static bool f_release(risc_runtime_capability_v1*g){f_live();assert(g->api&&f_grants);if(f_release_fault&&g->api==&f_touch_api)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};f_grants--;return true;}
static const risc_runtime_api_v1 f_runtime={.api_version=1,.struct_size=sizeof(f_runtime),.health=f_health,.yield_ms=f_yield,.diagnostic=f_diag,.request_launch=f_launch,.acquire=f_acquire,.release=f_release,.retain_invocation=f_retain,.exit_requested=f_exit_requested};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&f_runtime:NULL;}

#include "lora_messages.c"

static char f_rf_help[192];
static unsigned f_glyph_sevens;
static bool f_long_seeded,f_compose_bytes[LM_TEXT_MAX],f_detail_bytes[255];
static unsigned f_seen_pages,f_native_texts,f_native_icons,f_prior_page=99,f_prior_count=99,f_prior_save=99;
static void f_observe(void){if(!app)return;if(f_flag("LORA_LONG_DRAFT")&&!f_long_seeded&&page==PAGE_COMPOSE){memset(draft,'W',LM_TEXT_MAX);draft[LM_TEXT_MAX]=0;dirty=true;f_long_seeded=true;}f_seen_pages|=1u<<page;if(page!=f_prior_page||model.count!=f_prior_count||profile_save_state!=f_prior_save){printf("STATE ticks=%u polls=%u page=%u count=%u rf_page=%u choice=%u save=%u sends=%u listens=%u\n",f_ticks,f_polls,page,model.count,rf_page,model.choice,profile_save_state,f_sends,f_receives);f_prior_page=page;f_prior_count=model.count;f_prior_save=profile_save_state;}}
static void f_capture(void){
 for(unsigned y=0;y<f_height;y++){if(f_paper){for(unsigned x=(f_width+7)/8;x<f_stride();x++)assert(((uint8_t*)f_pixels)[y*f_stride()+x]==0xa5);}else for(unsigned x=f_width;x<f_width+4;x++)assert(f_pixels[y*(f_width+4)+x]==0xa5a5);}
 if(!f_paper&&page==PAGE_RF_ERROR)assert(!strcmp(f_rf_help,"Set all six RF values. Match radio, antenna, peers and local power limits. Radio stays off."));
 if(!f_directory)return;
 char path[1024];snprintf(path,sizeof(path),"%s/frame-hashes.tsv",f_directory);FILE*log=fopen(path,"a");assert(log);fprintf(log,"%u\t%u\t%u\t%016llx\n",f_presents,page,f_ticks,(unsigned long long)f_hash());assert(!fclose(log));
 if(!f_env("LORA_CAPTURE",1))return;
 /* At most one latest image per app screen, regardless of event count. */
 snprintf(path,sizeof(path),"%s/page-%u.ppm",f_directory,page);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",f_width,f_height);for(unsigned y=0;y<f_height;y++)for(unsigned x=0;x<f_width;x++){uint16_t v=f_paper?((((uint8_t*)f_pixels)[y*f_stride()+x/8]&(0x80u>>(x%8)))?0:65535):f_pixels[y*(f_width+4)+x];unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));
}
#ifdef PORTABLE_LORA_NATIVE_UI
static bool f_label_probe;
static char f_label_output[104];
void __real_portable_nova_native_text(unsigned,int,int,int,const char*,uint32_t);
void __wrap_portable_nova_native_text(unsigned face,int x,int y,int w,const char*s,uint32_t color){assert(x>=0&&y>=0&&x<(int)f_width&&y<(int)f_height&&w>=0&&x+w<=(int)f_width);assert(s);f_native_texts++;
 if(f_label_probe){assert(strlen(s)<sizeof(f_label_output));strcpy(f_label_output,s);}
 bool seven=page==PAGE_RF&&rf_page==0&&!strcmp(s,"7");uint16_t before[32*32];unsigned sw=0,sh=0;
 if(seven){sw=(unsigned)((int)f_width-x);if(sw>32)sw=32;sh=(unsigned)((int)f_height-y);if(sh>32)sh=32;for(unsigned j=0;j<sh;j++)for(unsigned i=0;i<sw;i++)before[j*sw+i]=f_pixels[(y+j)*(f_width+4)+x+i];}
 if(page==PAGE_RF_ERROR&&color==NOVA_TEXT&&x==lm_geometry.body.x+2&&y<lm_geometry.full_footer.y){if(y==lm_geometry.state.y)f_rf_help[0]=0;assert(strlen(f_rf_help)+strlen(s)<sizeof(f_rf_help));strcat(f_rf_help,s);}
 if(color==NOVA_TEXT&&x==lm_geometry.body.x+2){
  if(page==PAGE_COMPOSE&&draft[0]&&y>=lm_geometry.body.y&&y<lm_geometry.body.y+lm_geometry.body.h){unsigned at=compose_offset+(unsigned)(y-lm_geometry.body.y)/lm_geometry.line_height*lm_native_columns();assert(at+strlen(s)<=strlen(draft)&&!strncmp(s,draft+at,strlen(s)));for(unsigned i=0;i<strlen(s);i++)f_compose_bytes[at+i]=true;}
  if(page==PAGE_MESSAGE&&y>=lm_geometry.detail_state.y+lm_geometry.line_height&&y<lm_geometry.detail_info.y){unsigned at=detail_offset+(unsigned)(y-lm_geometry.detail_state.y-lm_geometry.line_height)/lm_geometry.line_height*lm_native_columns();assert(at+strlen(s)<=detail.length&&!strncmp(s,detail.text+at,strlen(s)));for(unsigned i=0;i<strlen(s);i++)f_detail_bytes[at+i]=true;}
 }
 __real_portable_nova_native_text(face,x,y,w,s,color);
 if(seven){unsigned rows=0,changed=0;for(unsigned j=0;j<sh;j++){bool lit=false;for(unsigned i=0;i<sw;i++)if(before[j*sw+i]!=f_pixels[(y+j)*(f_width+4)+x+i]){lit=true;changed++;}rows+=lit;}assert(rows>=3&&changed>=5);f_glyph_sevens++;printf("GLYPH7 face=%u rows=%u pixels=%u PASS\n",face,rows,changed);}}
bool __real_portable_nova_native_icon(int,int,int,const char*,uint32_t);
bool __wrap_portable_nova_native_icon(int x,int y,int z,const char*n,uint32_t c){assert(x>=0&&y>=0&&z>0&&x+z<=(int)f_width&&y+z<=(int)f_height);f_native_icons++;return __real_portable_nova_native_icon(x,y,z,n,c);}
#endif

/* Layout and temporal helpers follow below. */
#ifdef PORTABLE_LORA_NATIVE_UI
#include "lora_messages_native_layout.h"
static void f_rect_json(const char*name,lm_native_rect r,bool comma){assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=(int)f_width&&r.y+r.h<=(int)f_height);assert(lm_native_hit(r,r.x,r.y)&&lm_native_hit(r,r.x+r.w-1,r.y+r.h-1));assert(!lm_native_hit(r,r.x-1,r.y)&&!lm_native_hit(r,r.x+r.w,r.y)&&!lm_native_hit(r,r.x,r.y-1)&&!lm_native_hit(r,r.x,r.y+r.h));printf("%s\"%s\":[%d,%d,%d,%d]",comma?",":"",name,r.x,r.y,r.w,r.h);}
static void f_layout(void){lm_native_layout l;assert(lm_native_layout_make(&l,f_width,f_height));lm_geometry=l;printf("{");f_rect_json("back",l.back,false);f_rect_json("title",l.title,true);f_rect_json("radio",l.radio,true);f_rect_json("state",l.state,true);f_rect_json("notice",l.notice,true);f_rect_json("body",l.body,true);f_rect_json("full_footer",l.full_footer,true);f_rect_json("entry",l.entry,true);f_rect_json("counter",l.counter,true);f_rect_json("detail_state",l.detail_state,true);f_rect_json("detail_info",l.detail_info,true);for(unsigned i=0;i<35;i++){char n[24];snprintf(n,sizeof(n),"key%u",i);f_rect_json(n,l.keys[i],true);}for(unsigned i=0;i<4;i++){char n[24];snprintf(n,sizeof(n),"home%u",i);f_rect_json(n,l.home[i],true);snprintf(n,sizeof(n),"picker%u",i);f_rect_json(n,l.picker[i],true);}for(unsigned i=0;i<3;i++){char n[24];snprintf(n,sizeof(n),"rf%u",i);f_rect_json(n,l.rf[i],true);}for(unsigned i=0;i<2;i++){char n[24];snprintf(n,sizeof(n),"footer%u",i);f_rect_json(n,l.footer[i],true);}for(unsigned i=0;i<l.history_rows;i++){char n[24];snprintf(n,sizeof(n),"history%u",i);f_rect_json(n,l.history[i],true);}printf(",\"history_rows\":%u,\"compose_rows\":%u,\"detail_rows\":%u,\"compose_step\":%u,\"detail_step\":%u}\n",l.history_rows,l.compose_rows,l.detail_rows,lm_native_compose_step(),lm_native_detail_step());}
bool ln_diag_poll(unsigned,t5_app_input_t*),ln_diag_drain(void),ln_diag_visible(void),ln_diag_failed(void);
uint64_t ln_diag_generation(void);unsigned ln_diag_owner(void);
static int f_tx,f_ty;
static bool f_temporal_filter;
static t5_app_input_t f_temporal_one(void){t5_app_input_t in={0};assert(ln_diag_poll(0,&in));assert(!ln_diag_failed());if(f_temporal_filter){lm_native_mapping();lm_native_filter(&in);}return in;}
static void f_neutral(void){for(unsigned i=0;i<4;i++)assert(!f_temporal_one().tapped);}
static void f_temporal_edge(bool down){f_ticks++;f_edge(down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,1,f_tx,f_ty);f_touch=(risc_touch_snapshot_v1){.width=f_width,.height=f_height,.sequence=f_sequence,.timestamp_ms=f_ticks,.contact_count=down};f_touch.contacts[0]=(risc_touch_contact_v1){.id=1,.x=f_tx,.y=f_ty};}
static void f_fresh(void){f_temporal_edge(true);assert(!f_temporal_one().tapped);f_temporal_edge(false);t5_app_input_t in=f_temporal_one();assert(in.tapped&&in.touch_x==f_tx&&in.touch_y==f_ty);f_neutral();}
static void f_submitted(void){for(unsigned i=0;!f_pending&&i<20000;i++)assert(!f_temporal_one().tapped);assert(f_pending&&ln_diag_owner()==11);}
static void f_complete_one(void){unsigned goal=f_completions+1;for(unsigned i=0;f_completions<goal&&i<20000;i++)assert(!f_temporal_one().tapped);assert(f_completions==goal);}
static int f_temporal(unsigned mode){
 f_manual=true;f_stop=1000000;f_delay=30;app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);utility_frame_reset();lm_native_active=true;assert(lm_native_layout_make(&lm_geometry,f_width,f_height));portable_lora_native_begin();app->set_back_exits_app(false);memset(&model,0,sizeof(model));page=PAGE_HOME;dirty=true;notice=NULL;load_preferences();lm_native_mapping();
 lm_native_layout l;assert(lm_native_layout_make(&l,f_width,f_height));f_tx=l.home[0].x+l.home[0].w/2;f_ty=l.home[0].y+l.home[0].h/2;
 if(mode==11){f_tx=l.back.x+l.back.w/2;f_ty=l.back.y+l.back.h/2;f_temporal_filter=true;}
 if(mode==12){page=PAGE_HISTORY;const uint8_t bytes[]="one";assert(lm_add(&model,bytes,3,LM_IN));f_tx=l.history[0].x+l.history[0].w/2;f_ty=l.history[0].y+l.history[0].h/2;lm_native_mapping();f_temporal_filter=true;}
 draw();f_submitted();assert(!ln_diag_visible());
 if(mode<=1){f_temporal_edge(true);if(mode==0)f_temporal_edge(false);assert(ln_diag_drain()&&ln_diag_visible());f_neutral();if(mode==1){f_temporal_edge(false);assert(!f_temporal_one().tapped);f_neutral();}f_fresh();}
 else{
  assert(ln_diag_drain()&&ln_diag_visible());f_neutral();
  if(mode==2||mode==3||mode==6||mode==11||mode==12||(mode>=7&&mode<=9)){f_temporal_edge(true);assert(!f_temporal_one().tapped);}
  dirty=true;draw();f_submitted();
  if(mode==2){assert(ln_diag_drain()&&ln_diag_visible());f_temporal_edge(false);assert(f_temporal_one().tapped);f_neutral();f_fresh();}
  else if(mode==3||mode==11||mode==12){uint64_t generation=ln_diag_generation();if(mode==12){const uint8_t bytes[]="two";assert(lm_add(&model,bytes,3,LM_IN));}else page=PAGE_COMPOSE;dirty=true;lm_native_mapping();draw();assert(ln_diag_generation()>generation);f_complete_one();assert(!ln_diag_visible());if(utility_frame_pending)draw();assert(ln_diag_drain()&&ln_diag_visible());f_neutral();f_temporal_edge(false);assert(!f_temporal_one().tapped);f_neutral();f_fresh();}
  else if(mode==4){uint64_t generation=ln_diag_generation();portable_lora_native_begin();assert(ln_diag_generation()>generation&&f_pending&&ln_diag_owner()==11);lm_native_mapping();draw();f_complete_one();assert(!ln_diag_visible());if(utility_frame_pending)draw();assert(ln_diag_drain()&&ln_diag_visible());f_neutral();f_fresh();}
  else if(mode==5){assert(ln_diag_drain());portable_lora_native_begin();portable_lora_native_layout(999);assert(app->frame_ready());app->clear();app->present(false);for(unsigned i=0;!f_pending&&i<20000;i++)assert(!f_temporal_one().tapped);assert(f_pending&&ln_diag_owner()==0);assert(ln_diag_drain()&&!ln_diag_visible());f_temporal_edge(true);assert(!f_temporal_one().tapped);f_temporal_edge(false);assert(!f_temporal_one().tapped);f_neutral();lm_native_mapping();draw();assert(ln_diag_drain()&&ln_diag_visible());f_neutral();f_fresh();}
  else if(mode==6){f_temporal_edge(false);f_temporal_edge(true);f_temporal_edge(false);assert(ln_diag_drain()&&ln_diag_visible());assert(f_temporal_one().tapped);assert(!f_temporal_one().tapped);f_neutral();f_fresh();}
  else if(mode>=7&&mode<=9){f_snapshot_fault=mode-6;bool drained=ln_diag_drain();assert(!ln_diag_visible());if(mode==7){assert(!drained&&ln_diag_failed());}else{assert(drained);f_snapshot_fault=0;f_temporal_edge(false);f_neutral();lm_native_mapping();draw();assert(ln_diag_drain()&&ln_diag_visible());f_neutral();f_fresh();}}
  else if(mode==10){f_present_fault=true;assert(!ln_diag_drain()&&ln_diag_failed()&&f_pending);f_pending_check();}
  else assert(!"Unknown temporal mode");
 }
 if(mode==7||mode==10){assert(f_retains==1);unsigned grants_before=f_grants,subs_before=f_subs;app_module_fini();assert(f_grants==grants_before&&f_subs==subs_before&&f_grants);f_pending_check();printf("TEMPORAL case=%u retained=%u grants=%u subs=%u pending=%llu PASS\n",mode,f_retains,f_grants,f_subs,(unsigned long long)f_pending);return 0;}
 assert(!f_pending&&!f_frames);for(unsigned i=4;i>0;i--)release_grant(i-1);app_module_fini();assert(!f_grants&&!f_subs&&!f_frames);printf("TEMPORAL case=%u generation=%llu edges=%llu PASS\n",mode,(unsigned long long)ln_diag_generation(),(unsigned long long)f_sequence);return 0;
}
/* Exercise the actual static app label helper and actual font measurement at
 * its 99-byte truncation boundary, then rasterize through the real adapter.
 * The current measurer caps at 96 glyphs; old fit[100] therefore also passes.
 * Explicit ellipsis reserve avoids depending on that separate implementation. */
static int f_label_boundary(void){
 f_manual=true;f_stop=1000000;app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);assert(app&&portable_lora_native_begin());
 assert(lm_native_layout_make(&lm_geometry,f_width,f_height));portable_lora_native_layout(123);page=PAGE_HOME;
 char text[100];memset(text,'W',99);text[99]=0;int whole=portable_nova_measure(2,text);assert(whole>900&&whole+4<=(int)f_width);
 f_label_probe=true;unsigned cases=0;
 for(unsigned tall=0;tall<2;tall++)for(unsigned center=0;center<2;center++)for(unsigned deficit=1;deficit<=3;deficit++){
  assert(app->frame_ready());portable_nova_begin();int w=whole+4-(int)deficit;
  lm_native_rect rect={(int)(f_width-w)/2,20,w,tall?24:14};unsigned before=f_native_texts;f_label_output[0]=0;
  lm_native_label(rect,text,NOVA_WHITE,center!=0);assert(f_native_texts==before+1);
  size_t n=strlen(f_label_output);assert(n>=94&&n<=99&&!strcmp(f_label_output+n-3,"...")&&!strncmp(f_label_output,text,n-3));assert(portable_nova_measure(2,f_label_output)<=w-4);
  printf("LABEL boundary source=99 whole=%d deficit=%u height=%d centered=%u output=%s raster=%016llx\n",whole,deficit,rect.h,center,f_label_output,(unsigned long long)f_hash());
  portable_lora_native_present(false);assert(ln_diag_drain());cases++;
 }
 f_label_probe=false;app_module_fini();assert(!f_grants&&!f_frames&&!f_subs&&!f_pending&&!f_retains);
 printf("LABEL boundary cases=%u actual-helper actual-measure actual-raster cleanup=confirmed PASS\n",cases);return 0;
}
#endif
static void f_expect(const char*key,unsigned actual){if(getenv(key)){unsigned expected=f_env(key,0);fprintf(stderr,"CHECK %s actual=%u expected=%u\n",key,actual,expected);assert(actual==expected);}}
int main(int argc,char**argv){
 setvbuf(stdout,NULL,_IONBF,0);assert(argc>=2);if(argc>=5){f_width=(unsigned)atoi(argv[3]);f_height=(unsigned)atoi(argv[4]);}f_paper=f_flag("LORA_PAPER");assert(f_width<=(f_flag("LORA_LABEL_BOUNDARY")?1024u:502u)&&f_height<=(f_paper?800u:502u)&&f_bytes()<=sizeof(f_pixels));
#ifdef PORTABLE_LORA_NATIVE_UI
 if(!strcmp(argv[1],"--layout")){f_layout();return 0;}
#endif
 f_directory=argv[1];f_delay=f_env("LORA_DELAY",3);f_stop=f_env("LORA_STOP_POLLS",240);f_rx_count=f_env("LORA_RX_COUNT",0);memset(f_pixels,0xa5,sizeof(f_pixels));f_touch=(risc_touch_snapshot_v1){.width=f_width,.height=f_height};
 if(argc>2){FILE*f=fopen(argv[2],"r");assert(f);while(f_action_count<512&&fscanf(f,"%u %d %d",&f_actions[f_action_count].at,&f_actions[f_action_count].x,&f_actions[f_action_count].y)==3)f_action_count++;assert(feof(f));assert(!fclose(f));}
 f_seed=true;uint8_t policy[]={0x51,1,3,0xa6};if(f_flag("LORA_AIRPLANE")){policy[2]=4;policy[3]=0xa1;}assert(f_put(NULL,"quick_radio",policy,4)==0);
 if(f_flag("LORA_PROFILE")||f_flag("LORA_CHOICE")||f_flag("LORA_LEGACY")){twatch_lora_config_v2 p={.frequency_hz=915000000,.bandwidth_hz=125000,.preamble=8,.sf=7,.coding_rate=5,.power_dbm=0};uint8_t record[32];lm_profile_encode(&p,f_flag("LORA_LEGACY")?0:3,!f_flag("LORA_CHOICE"),record);if(f_flag("LORA_LEGACY")){record[2]=1;record[17]=0;lm_put32(record+28,lm_checksum(record));}assert(f_put(NULL,LM_PROFILE_KEY,record,32)==0);}
 f_seed=false;if(f_flag("LORA_OLD_API"))f_radio.struct_size=offsetof(twatch_radio_api_v2,profile_info);
 assert(app_module_init()==0);
#ifdef PORTABLE_LORA_NATIVE_UI
 if(f_flag("LORA_LABEL_BOUNDARY"))return f_label_boundary();
 if(getenv("LORA_TEMPORAL"))return f_temporal(f_env("LORA_TEMPORAL",0));
#endif
 app_main();f_observe();
 if(f_flag("LORA_EXPECT_GLYPH7"))assert(f_glyph_sevens);
 f_expect("LORA_EXPECT_FREQUENCY",model.profile.frequency_hz);f_expect("LORA_EXPECT_PAGE",page);f_expect("LORA_EXPECT_CHOICE",model.choice);f_expect("LORA_EXPECT_COUNT",model.count);f_expect("LORA_EXPECT_SAVE",profile_save_state);f_expect("LORA_EXPECT_WRITES",f_writes);f_expect("LORA_EXPECT_SENDS",f_sends);f_expect("LORA_EXPECT_LISTENS",f_receives);f_expect("LORA_EXPECT_CONFIGURES",f_configures);f_expect("LORA_EXPECT_RF_PAGE",rf_page);f_expect("LORA_EXPECT_HISTORY_PAGE",history_page);f_expect("LORA_EXPECT_DETAIL_OFFSET",detail_offset);f_expect("LORA_EXPECT_COMPOSE_OFFSET",compose_offset);f_expect("LORA_EXPECT_KEY_PAGE",key_page);f_expect("LORA_EXPECT_LAUNCHES",f_launches);
 if(getenv("LORA_EXPECT_COMPOSE_BYTES"))for(unsigned i=0;i<f_env("LORA_EXPECT_COMPOSE_BYTES",0);i++)assert(f_compose_bytes[i]);
 if(getenv("LORA_EXPECT_DETAIL_BYTES"))for(unsigned i=0;i<f_env("LORA_EXPECT_DETAIL_BYTES",0);i++)assert(f_detail_bytes[i]);
 if(getenv("LORA_EXPECT_DRAFT"))assert(!strcmp(draft,getenv("LORA_EXPECT_DRAFT")));
 if(getenv("LORA_EXPECT_NOTICE"))assert(notice&&strstr(notice,getenv("LORA_EXPECT_NOTICE")));
 if(getenv("LORA_EXPECT_SEEN")){unsigned mask=f_env("LORA_EXPECT_SEEN",0);assert((f_seen_pages&mask)==mask);}
 if(getenv("LORA_EXPECT_STATUS")){assert(model.count);assert(lm_at(&model,0)->status==f_env("LORA_EXPECT_STATUS",0));}
 if(getenv("LORA_EXPECT_CLOCK")){assert(model.count);assert(lm_at(&model,0)->clock_valid==f_flag("LORA_EXPECT_CLOCK"));}
 if(f_rx_count){assert(f_rx_read==f_rx_count);assert(lm_at(&model,0)->binary&&lm_at(&model,0)->length==255&&lm_at(&model,0)->text[1]=='.');}
 if(f_flag("LORA_VERIFY_STORED")){uint8_t record[32];uint32_t n=0;twatch_lora_config_v2 p;unsigned choice;bool present;assert(f_get(NULL,LM_PROFILE_KEY,record,32,&n)==0&&lm_profile_decode(&p,&choice,&present,record,n));assert(choice==model.choice&&present==model.profile_valid);if(present)assert(!memcmp(&p,&model.profile,sizeof(p)));}
 app_module_fini();fprintf(stderr,"CLEANUP grants=%u frames=%u subs=%u pending=%llu radio=%u retains=%u\n",f_grants,f_frames,f_subs,(unsigned long long)f_pending,f_radio_owned,f_retains);assert(!f_grants&&!f_frames&&!f_subs&&!f_pending&&!f_radio_owned);
#ifdef PORTABLE_LORA_NATIVE_UI
 if(f_paper){assert(!f_native_texts&&!f_native_icons&&!lm_native_active&&ln_diag_owner()==0&&ln_diag_generation()==0);}else assert(f_native_texts);
#else
 assert(!f_native_texts&&!f_native_icons);
#endif
 for(unsigned i=0;i<f_action_count;i++)assert(f_actions[i].emitted);
 if(f_flag("LORA_REENTER")){unsigned choice=model.choice,sends=f_sends;twatch_lora_config_v2 profile=model.profile;bool valid=model.profile_valid;f_action_count=0;f_stop=f_polls+60;f_touch.contact_count=0;assert(app_module_init()==0);app_main();assert(page==PAGE_HOME&&model.count==0&&draft[0]==0&&model.choice==choice&&model.profile_valid==valid&&f_sends==sends);if(valid)assert(!memcmp(&profile,&model.profile,sizeof(profile)));app_module_fini();assert(!f_grants&&!f_frames&&!f_subs&&!f_pending&&!f_radio_owned);puts("REENTRY history-and-draft-cleared persisted-profile-restored no-automatic-radio PASS");}
 if(!getenv("LORA_EXPECT_SENDS"))assert(!f_sends);
 if(!getenv("LORA_EXPECT_LISTENS")&&!f_rx_count)assert(!f_receives);
 printf("PASS width=%u height=%u frames=%u polls=%u edges=%llu pages=%u sends=%u listens=%u configures=%u cancels=%u writes=%u texts=%u icons=%u cleanup=confirmed\n",f_width,f_height,f_presents,f_polls,(unsigned long long)f_sequence,f_seen_pages,f_sends,f_receives,f_configures,f_cancels,f_writes,f_native_texts,f_native_icons);return 0;
}
