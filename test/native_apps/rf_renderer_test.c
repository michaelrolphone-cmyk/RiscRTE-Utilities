/* Production RF controller included only to inspect state at user input boundaries.
 * All commands use the real System adapter, renderer and touch navigation path.
 * RF/display/storage transports are explicit host peripherals, never hardware MMIO.
 * The separate runtime fixture exercises the real AppDataFiles implementation. */
#include "../../Apps/waterfall.c"
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "PortableNavigation.h"
#include "AlarmServiceV1.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdlib.h>

int app_module_init(void);void app_module_fini(void);
void rf_serial_start(void);void rf_serial_line(const char*);void rf_serial_finish(int,int);
#ifdef RF_RENDER_WATCH_TOUCH
const risc_touch_api_v1 *hid_watch_touch_start(void);void hid_watch_touch_stop(void);
static const risc_touch_api_v1 *fx_watch;
#endif
#ifdef RF_RENDER_PAPER
#define FX_W 800u
#define FX_H 480u
#define FX_STRIDE 104u
#define FX_FORMAT RISC_DISPLAY_FORMAT_MONO1
#else
#define FX_W 240u
#define FX_H 240u
#define FX_STRIDE 488u
#define FX_FORMAT RISC_DISPLAY_FORMAT_RGB565
#endif
static struct {uint8_t before[32],pixels[FX_H*FX_STRIDE],after[32];} fx_raster;
static unsigned fx_ticks=1000,fx_polls,fx_grants,fx_frames,fx_subs,fx_presents,fx_captures,fx_count,fx_suspend,fx_checks,fx_writes,fx_app_writes,fx_launches,fx_retries,fx_retained_yields,fx_poll_ms=120,fx_radio_acquires,fx_release_failures,fx_sleeps,fx_diag_total,fx_diag_captures,fx_diag_stages,fx_diag_details,fx_diag_dumps,fx_expected_launch;
static uint32_t fx_times[8192];
static bool fx_done,fx_radio_retained,fx_storage_retained,fx_serial,fx_legacy,fx_no_radio,fx_launch_refuse,fx_returned,fx_app_api_phase;
static int fx_signal=1,fx_rf_failure,fx_storage_failure,fx_kv_failure,fx_format_failure,fx_slow;
static risc_radio_iq_settings_v1 fx_settings;
static FILE *fx_commands;static const char *fx_directory;
static char fx_line[256];static unsigned fx_lineno,fx_phase,fx_wait;
static int fx_touch_x=-1,fx_touch_y=-1;static uint32_t fx_buttons;
static long long fx_remember[32];static char fx_remember_keys[32][40];static unsigned fx_remember_count;
static jmp_buf fx_retained_jump;
static struct {char key[24];uint8_t bytes[2048];uint32_t size;} fx_cells[32];
static struct {char name[32];uint8_t bytes[RT_BANK_MAX];uint32_t size;uint64_t revision;} fx_files[3];
static uint64_t fx_revision=1;
static void fx_io(void){assert(!fx_radio_retained&&!fx_storage_retained);}
static void fx_guards(void){for(unsigned i=0;i<32;i++)assert(fx_raster.before[i]==0xa5&&fx_raster.after[i]==0xa5);for(unsigned y=0;y<FX_H;y++)for(unsigned x=(FX_FORMAT==RISC_DISPLAY_FORMAT_MONO1?100u:480u);x<FX_STRIDE;x++)assert(fx_raster.pixels[y*FX_STRIDE+x]==0xa5);}
static unsigned fx_pixel(unsigned x,unsigned y){
#ifdef RF_RENDER_PAPER
 unsigned nx=y,ny=479u-x;return (fx_raster.pixels[ny*FX_STRIDE+nx/8u]&(0x80u>>(nx%8u)))?0u:255u;
#else
 uint16_t v;memcpy(&v,fx_raster.pixels+y*FX_STRIDE+x*2,2);return (v>>11)*255/31<<16|((v>>5)&63)*255/63<<8|(v&31)*255/31;
#endif
}
static void fx_snapshot(const char *name){
 fx_guards();
#ifdef RF_RENDER_PAPER
 if(!strcmp(name,"spec")||!strcmp(name,"event-review")){unsigned black=0,white=0,x0=!strcmp(name,"spec")?10u:26u,x1=!strcmp(name,"spec")?106u:230u,y0=!strcmp(name,"spec")?30u:585u,y1=!strcmp(name,"spec")?72u:700u;for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++){if(fx_pixel(x,y))white++;else black++;}assert(black>100&&white>25);if(!strcmp(name,"spec")){unsigned trace=0;for(unsigned y=180;y<225;y++)for(unsigned x=270;x<282;x++)trace+=fx_pixel(x,y)==0;assert(trace>10);}fx_checks++;}
#endif
 if(getenv("RF_RENDER_NO_IMAGES"))return;
 char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",fx_directory,name);FILE*f=fopen(path,"wb");assert(f);
#ifdef RF_RENDER_PAPER
 unsigned width=480,height=800;
#else
 unsigned width=240,height=240;
#endif
 fprintf(f,"P6\n%u %u\n255\n",width,height);for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){unsigned v=fx_pixel(x,y);
#ifdef RF_RENDER_PAPER
 unsigned char rgb[3]={(unsigned char)v,(unsigned char)v,(unsigned char)v};
#else
 unsigned char rgb[3]={(unsigned char)(v>>16),(unsigned char)(v>>8),(unsigned char)v};
#endif
 assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));
}
static void fx_corners(void){
 const t5_app_api_v1*api=t5_app_get_api(1);assert(api);unsigned w=(unsigned)api->screen_width(),h=(unsigned)api->screen_height();
#ifdef RF_RENDER_PAPER
 assert(w==480&&h==800);
#else
 assert(w==240&&h==240);
#endif
 api->clear();api->fill_rect(-10,-10,11,11,true);api->fill_rect((int)w-2,0,5,2,true);api->fill_rect(0,(int)h-3,3,7,true);api->fill_rect((int)w-4,(int)h-4,9,9,true);api->present(true);
 assert(fx_pixel(0,0)==0&&fx_pixel(w-1,0)==0&&fx_pixel(0,h-1)==0&&fx_pixel(w-1,h-1)==0);assert(fx_pixel(1,1)!=0&&fx_pixel(w-3,0)!=0&&fx_pixel(3,h-1)!=0&&fx_pixel(w-5,h-1)!=0);fx_guards();fx_checks++;
}
static long long fx_value(const char*k){
#define V(n,v) if(!strcmp(k,n))return (long long)(v)
 V("page",page);V("radio_acquires",fx_radio_acquires);V("sleep",fx_sleeps);V("diag_total",fx_diag_total);V("diag_captures",fx_diag_captures);V("diag_stages",fx_diag_stages);V("diag_details",fx_diag_details);V("diag_dumps",fx_diag_dumps);V("view",view);V("running",running);V("frozen",frozen);V("history",history_count);V("captures",fx_captures);V("transforms",spectrum.transforms);V("canonical",signature_audio.transforms);V("fft",prefs.fft_size);V("peak_hz",rf_dsp_bin_hz(&spectrum.config,spectrum.peak_bin));V("cursor",cursor_visible);V("label_active",label_active[0]);V("label_level_valid",level_valid[0]);V("cursor_hz",cursor_hz);V("labels",saved_count());V("color",labels[0].color);V("pending",pending_save);V("load_errors",load_errors);V("key_page",key_page);V("key_choice",key_choice);V("name_length",strlen(editing.name));V("char",(unsigned char)editing.name[0]);V("neural_updates",event_neural.updates);V("neural_epoch",event_neural.epoch);V("scroll",controls_scroll);V("receiver_scroll",receiver_scroll);V("gain",prefs.gain_db);V("threshold",prefs.threshold_db);V("palette",prefs.palette);V("window",prefs.window);V("log_frequency",prefs.log_frequency);V("log_amplitude",prefs.log_amplitude);V("show_labels",prefs.show_labels);V("low",prefs.low_hz);V("high",prefs.high_hz);V("lo",prefs.identity.lo_hz);V("rate",prefs.identity.sample_rate_hz);V("width",prefs.identity.width_hz);V("raw_gain",prefs.identity.raw_gain);V("rf_gain",prefs.identity.rf_gain);V("bb_gain",prefs.identity.bb_gain);V("filter",prefs.identity.filter);V("dc0",prefs.identity.dc[0]);V("dc1",prefs.identity.dc[1]);V("dc2",prefs.identity.dc[2]);V("dc3",prefs.identity.dc[3]);V("iq",prefs.identity.iq_correction);V("signature_goal",signature_goal);V("signature_frames",signature_capture.frames);V("room_frames",signatures[0].frames);V("room_kind",signatures[0].kind);V("signature_pending",signature_pending);V("signature_mode",signature_mode);V("manual_room",manual_room);V("room_selected",room_tracker.selected);V("ambient_ready",ambient.ready);V("ambient_frames",ambient.frames);V("foreground",ambient.foreground);V("event_armed",event_armed);V("event_ready",event_training.ready);V("event_wait_quiet",event_wait_quiet);V("event_flags",event_training.event.flags);V("event_count",event_training.event.count);V("event_labels",event_library.labels[0].present);V("examples",rt_example_count(&event_library.labels[0],0));V("positive",rt_example_count(&event_library.labels[0],RT_POSITIVE));V("negative",rt_example_count(&event_library.labels[0],RT_NEGATIVE));V("event_pending",event_files.pending);V("event_files_ready",event_files.ready);V("neural_state",event_neural.state);V("neural_active",event_neural.has_active);V("launches",fx_launches);V("suspends",fx_suspend);V("capture_error",capture_error);V("last_count",fx_count);V("writes",fx_writes);V("app_writes",fx_app_writes);V("presents",fx_presents);
#undef V
 fprintf(stderr,"Unknown check: %s\n",k);abort();
}
static void fx_check(const char*k,const char*op,long long wanted){long long got=fx_value(k);bool ok=!strcmp(op,"eq")?got==wanted:!strcmp(op,"ge")?got>=wanted:!strcmp(op,"le")?got<=wanted:false;if(!ok){fprintf(stderr,"line %u check %s %s %lld: got %lld; page=%u polls=%u captures=%u message=%s\n",fx_lineno,k,op,wanted,got,page,fx_polls,fx_captures,message?message:"-");abort();}fx_checks++;}
static void fx_verify_times(void){
 const rt_example*e=event_training.ready?&event_training.event:&event_library.labels[0].examples[0];assert(e->count>=2);unsigned previous=0;
 for(unsigned i=0;i<e->count;i++){bool found=false;for(unsigned j=previous;j<fx_captures;j++)if(fx_times[j]==e->frames[i].timestamp_ms){previous=j+1;found=true;break;}assert(found&&(e->frames[i].flags&RT_GAP_BEFORE));}
 assert(e->duration_ms==e->frames[e->end].timestamp_ms-e->frames[e->onset].timestamp_ms+1u);fx_checks++;
}
static void fx_script(void){
 fx_touch_x=fx_touch_y=-1;fx_buttons=0;if(fx_polls==1)return;
 for(;;){
  if(!fx_line[0]){assert(fgets(fx_line,sizeof(fx_line),fx_commands));fx_lineno++;fx_phase=0;fx_wait=0;}
  char op[40]={0},key[64]={0},cmp[16]={0};long long value=0;int x=0,y=0;sscanf(fx_line,"%39s",op);
  if(op[0]=='#'||!op[0]){fx_line[0]=0;continue;}
  if(!strcmp(op,"wait")){if(!fx_wait)assert(sscanf(fx_line,"%*s %u",&fx_wait)==1&&fx_wait);if(!--fx_wait)fx_line[0]=0;return;}
  if(!strcmp(op,"tap")||!strcmp(op,"raw")){assert(sscanf(fx_line,"%*s %d %d",&x,&y)==2);if(!fx_phase++){
#ifdef RF_RENDER_PAPER
   if(!strcmp(op,"tap")){x*=2;y=((page==PAGE_MAIN&&started&&view<2)||page==PAGE_CONTROLS)?y*2:y*800/240;}
#endif
   fx_touch_x=x;fx_touch_y=y;
  }else fx_line[0]=0;return;}
  if(!strcmp(op,"point")){assert(sscanf(fx_line,"%*s %d %d",&fx_touch_x,&fx_touch_y)==2);fx_line[0]=0;return;}
  if(!strcmp(op,"nav")){assert(sscanf(fx_line,"%*s %lld",&value)==1);if(!fx_phase++)fx_buttons=(uint32_t)value;else fx_line[0]=0;return;}
  if(!strcmp(op,"check")){assert(sscanf(fx_line,"%*s %63s %15s %lld",key,cmp,&value)==3);fx_check(key,cmp,value);}
  else if(!strcmp(op,"equal")){assert(sscanf(fx_line,"%*s %63s %15s",key,cmp)==2);fx_check(key,"eq",fx_value(cmp));}
  else if(!strcmp(op,"remember")){assert(sscanf(fx_line,"%*s %63s",key)==1);unsigned i=0;for(;i<fx_remember_count&&strcmp(key,fx_remember_keys[i]);i++);assert(i<32);if(i==fx_remember_count)fx_remember_count++;strcpy(fx_remember_keys[i],key);fx_remember[i]=fx_value(key);}
  else if(!strcmp(op,"same")){assert(sscanf(fx_line,"%*s %63s",key)==1);unsigned i=0;for(;i<fx_remember_count&&strcmp(key,fx_remember_keys[i]);i++);assert(i<fx_remember_count);fx_check(key,"eq",fx_remember[i]);}
  else if(!strcmp(op,"set")){assert(sscanf(fx_line,"%*s %63s %lld",key,&value)==2);if(!strcmp(key,"signal"))fx_signal=(int)value;else if(!strcmp(key,"rf_failure"))fx_rf_failure=(int)value;else if(!strcmp(key,"kv_failure"))fx_kv_failure=(int)value;else if(!strcmp(key,"storage_failure"))fx_storage_failure=(int)value;else if(!strcmp(key,"format_failure"))fx_format_failure=(int)value;else if(!strcmp(key,"slow"))fx_slow=(int)value;else if(!strcmp(key,"jump"))fx_ticks+=(unsigned)value;else if(!strcmp(key,"poll_ms"))fx_poll_ms=(unsigned)value;else if(!strcmp(key,"no_radio"))fx_no_radio=value!=0;else assert(!"Unknown peripheral setting");}
  else if(!strcmp(op,"name")){char expected[100];assert(sscanf(fx_line,"%*s %99s",expected)==1);assert(!strcmp(editing.name,expected));fx_checks++;}
  else if(!strcmp(op,"snapshot")){assert(sscanf(fx_line,"%*s %63s",key)==1);fx_snapshot(key);}
  else if(!strcmp(op,"times"))fx_verify_times();
  else if(!strcmp(op,"finish")){assert(!signature_goal&&!signature_pending&&!temporal_exit_blocked());fx_done=true;return;}
  else {fprintf(stderr,"Unknown command line %u: %s",fx_lineno,fx_line);abort();}
  fx_line[0]=0;
 }
}
static rt_example fx_neural_example(unsigned identity,unsigned recording,unsigned kind){
 rt_segmenter segment={0};uint32_t power[128]={0},previous[128]={0},now=1000;
 for(unsigned i=0;i<4;i++){rt_frame f=rt_frame_make(power,previous,false,now,true);now+=100;rt_segment_observe(&segment,&f);}
 const unsigned envelope[]={100,70,48,31,21,11,5,1};
 for(unsigned i=0;i<8;i++){
  unsigned amplitude=(envelope[i]+(recording==2&&i>0?1u:0u))*(10+recording);memset(power,0,sizeof(power));power[15]=amplitude*10000;power[18]=amplitude*8000;
  if(i==2&&identity<2)power[identity?40:30]=amplitude*8000;
  if(i==2&&identity==2)power[30]=power[40]=amplitude*4000;
  rt_frame f=rt_frame_make(power,previous,true,now,true);now+=100;rt_segment_observe(&segment,&f);memcpy(previous,power,sizeof(power));
 }
 for(unsigned i=0;i<4;i++){memset(power,0,sizeof(power));rt_frame f=rt_frame_make(power,previous,false,now,true);now+=100;rt_segment_observe(&segment,&f);memcpy(previous,power,sizeof(power));}
 segment.event.id=recording+1;segment.event.kind=(uint8_t)kind;assert(rt_example_valid(&segment.event));return segment.event;
}
static void fx_seed_neural(void){
 static rt_library saved;saved.identity=rf_identity_default();saved.generation[0]=3;saved.generation[1]=5;
 for(unsigned i=0;i<2;i++){rt_label*l=&saved.labels[i];l->present=true;l->next_id=6;snprintf(l->name,sizeof(l->name),"Private RF %u",i);for(unsigned j=0;j<3;j++)l->examples[j]=fx_neural_example(i,j,RT_POSITIVE);l->examples[3]=fx_neural_example(2,3,RT_NEGATIVE);l->examples[4]=fx_neural_example(2,4,RT_NEGATIVE);assert(rt_label_valid(l));}
 for(unsigned bank=0;bank<2;bank++){strcpy(fx_files[bank].name,rt_file_name(bank));fx_files[bank].size=(uint32_t)rt_bank_encode(&saved,bank,fx_files[bank].bytes,sizeof(fx_files[bank].bytes));assert(fx_files[bank].size);fx_files[bank].revision=fx_revision;}
}
static bool fx_health(risc_runtime_health_v1*h){h->uptime_ms=fx_ticks;return !fx_done;}
static void fx_yield(uint32_t n){fx_ticks+=n;if(fx_storage_retained&&++fx_retained_yields==6)longjmp(fx_retained_jump,1);}
static bool fx_diag(const char*s){assert(s&&!strstr(s,"SECRET")&&!strstr(s,"private-label")&&!strstr(s,"Private RF")&&!strstr(s,"password")&&!strstr(s,"pairs=["));fx_diag_total++;fx_diag_captures+=strstr(s,"SDR capture rc=")!=NULL;fx_diag_stages+=strstr(s,"SDR stage=")!=NULL;fx_diag_details+=strstr(s,"SDR detail stage=")!=NULL;fx_diag_dumps+=strstr(s,"SDR dump clk=")!=NULL;if(fx_serial)rf_serial_line(s);fprintf(stderr,"%s\n",s);return true;}
static bool fx_launch(const char*s){fx_io();assert(!owned&&!running&&!strcmp(s,"springboard.elf"));fx_launches++;assert(fx_expected_launch&&fx_launches<=fx_expected_launch);if(fx_launch_refuse&&fx_launches==1)return false;fx_done=true;return true;}
static bool fx_info(void*c,risc_display_info_v1*s){(void)c;fx_io();*s=(risc_display_info_v1){.width=FX_W,.height=FX_H,.nominal_refresh_millihz=FX_FORMAT==RISC_DISPLAY_FORMAT_MONO1?1000:60000,.typical_present_latency_us=FX_FORMAT==RISC_DISPLAY_FORMAT_MONO1?900000:16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(FX_FORMAT),.flags=FX_FORMAT==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE:0};return true;}
static bool fx_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;fx_io();assert(!fx_frames&&f==FX_FORMAT);fx_frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=fx_raster.pixels,.width=FX_W,.height=FX_H,.stride_bytes=FX_STRIDE,.size_bytes=sizeof(fx_raster.pixels),.pixel_format=f};return true;}
static void fx_frame_release(void*c,risc_display_frame_v1 f){(void)c;fx_io();assert(fx_frames&&f==1);fx_frames=0;fx_guards();}
static bool fx_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)o;fx_io();assert(fx_frames&&f==1);for(size_t i=0;i<n;i++)assert(r[i].x+r[i].width<=FX_W&&r[i].y+r[i].height<=FX_H);fx_frames=0;*t=++fx_presents;fx_guards();fx_ticks+=(unsigned)fx_slow;return true;}
static bool fx_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;fx_io();assert(t);s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 fx_display_api={.api_version=1,.struct_size=sizeof(fx_display_api),.get_info=fx_info,.acquire=fx_frame,.release=fx_frame_release,.submit=fx_submit,.present_status=fx_present};
static void fx_touch_report(risc_touch_snapshot_v1*s){*s=(risc_touch_snapshot_v1){
#ifdef RF_RENDER_PAPER
 .width=480,.height=800,
#else
 .width=240,.height=240,
#endif
 .sequence=fx_polls};if(fx_touch_x>=0){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)fx_touch_x,.y=(uint16_t)fx_touch_y};}}
#ifdef RF_RENDER_WATCH_TOUCH
void hid_renderer_watch_report(risc_touch_snapshot_v1*s){fx_touch_report(s);}uint64_t hid_renderer_watch_millis(void){return fx_ticks;}
#endif
static uint64_t fx_sub(void*c){(void)c;fx_io();fx_subs++;
#ifdef RF_RENDER_WATCH_TOUCH
 return fx_watch->subscribe(fx_watch->context);
#else
 return 1;
#endif
}
static bool fx_unsub(void*c,uint64_t n){(void)c;fx_io();assert(n&&fx_subs);fx_subs--;
#ifdef RF_RENDER_WATCH_TOUCH
 return fx_watch->unsubscribe(fx_watch->context,n);
#else
 return true;
#endif
}
static bool fx_touch_poll(void*c,size_t n){(void)c;fx_io();assert(n==1);assert(++fx_polls<20000);fx_ticks+=fx_poll_ms;fx_script();
#ifdef RF_RENDER_WATCH_TOUCH
 return fx_watch->poll(fx_watch->context,n);
#else
 return true;
#endif
}
static int32_t fx_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;(void)e;fx_io();
#ifdef RF_RENDER_WATCH_TOUCH
 return fx_watch->next(fx_watch->context,n,e);
#else
 return 0;
#endif
}
static bool fx_touch_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;fx_io();
#ifdef RF_RENDER_WATCH_TOUCH
 return fx_watch->snapshot(fx_watch->context,s);
#else
 fx_touch_report(s);return true;
#endif
}
static const risc_touch_api_v1 fx_touch_api={1,sizeof(fx_touch_api),NULL,fx_sub,fx_unsub,fx_touch_poll,fx_next,fx_touch_snapshot};
static bool fx_battery(void*c,risc_battery_sample_v1*s){(void)c;fx_io();*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 fx_battery_api={1,sizeof(fx_battery_api),NULL,fx_battery};
static bool fx_rtc(void*c,twatch_rtc_time_v1*s){(void)c;fx_io();*s=(twatch_rtc_time_v1){2026,10,7,3,8,0,0};return true;}
static bool fx_rtc_write(void*c,const twatch_rtc_time_v1*s){(void)c;(void)s;assert(!"Unexpected RTC write");return false;}
static const twatch_rtc_api_v1 fx_rtc_api={.api_version=2,.struct_size=sizeof(fx_rtc_api),.read=fx_rtc,.write=fx_rtc_write};
static int32_t fx_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){(void)c;fx_io();*s=0;if(!strcmp(k,"quick_radio")&&getenv("RF_RENDER_POLICY")){if(!strcmp(getenv("RF_RENDER_POLICY"),"unread"))return RISC_KEY_VALUE_IO;assert(cap>=4);uint8_t policy[4]={0x51,1,4,0xa1};memcpy(b,policy,4);*s=4;return 0;}assert(!strncmp(k,"rf_",3)||!strcmp(k,"quick_radio")||!strncmp(k,"quick_",6));if(fx_kv_failure==1&&!strcmp(k,"rf_cfg"))return RISC_KEY_VALUE_IO;if(fx_kv_failure==2&&!strcmp(k,"rf_cfg")){assert(cap>=3);memset(b,0,3);*s=3;return 0;}for(unsigned i=0;i<32;i++)if(!strcmp(k,fx_cells[i].key)){*s=fx_cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,fx_cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fx_put(void*c,const char*k,const void*b,uint32_t n){(void)c;fx_io();fx_writes++;assert(!strncmp(k,"rf_",3)&&n<=2048&&strlen(k)<24);if(fx_kv_failure==3)return RISC_KEY_VALUE_IO;unsigned i;for(i=0;i<32&&fx_cells[i].key[0]&&strcmp(k,fx_cells[i].key);i++);assert(i<32);strcpy(fx_cells[i].key,k);memcpy(fx_cells[i].bytes,b,n);fx_cells[i].size=n;return 0;}
static const risc_key_value_v1 fx_legacy_kv={1,sizeof(fx_legacy_kv),NULL,fx_get,fx_put};
static const risc_key_value_v1 fx_kv={2,sizeof(fx_kv),NULL,fx_get,fx_put};
static int32_t fx_alarm_status(void*c,alarm_status_v1*s){(void)c;fx_io();*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t fx_alarm_step(void*c){(void)c;fx_io();return ALARM_OK;}
static int32_t fx_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;fx_io();return ALARM_OK;}
static int32_t fx_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;fx_io();*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 fx_alarm_api={1,sizeof(fx_alarm_api),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step};
static bool fx_nav(void*c,risc_input_navigation_frame_v1*s){(void)c;fx_io();*s=(risc_input_navigation_frame_v1){.buttons=fx_buttons,.pressed=fx_buttons};return true;}
static bool fx_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;fx_io();return true;}
static bool fx_reset(void*c){(void)c;fx_io();return true;}
static const risc_input_navigation_api_v1 fx_nav_api={1,sizeof(fx_nav_api),NULL,fx_nav,fx_foreground,fx_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &fx_nav_api;}void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;fx_io();}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(getenv("RF_RENDER_SLEEP")&&!running&&!owned&&!fx_frames&&!fx_subs);fx_sleeps++;fx_ticks+=2000;return 1;}
static bool fx_radio_suspend(void*c){(void)c;fx_suspend++;if(fx_radio_retained&&fx_retries){fx_retries--;return false;}fx_radio_retained=false;return true;}
static int fx_capture(void*c,uint32_t*p,uint32_t count){(void)c;fx_io();assert(count>=256&&count<=8192&&!(count&(count-1)));if(fx_legacy)assert(count==256);fx_count=count;if(fx_rf_failure){int failure=fx_rf_failure;fx_rf_failure=0;if(failure==RISC_RADIO_IQ_CLEANUP_RETAINED){fx_radio_retained=true;fx_retries=3;}return failure;}assert(fx_captures<8192);fx_times[fx_captures++]=burst_at;
 for(unsigned i=0;i<count;i++){double theta=6.283185307179586*(fx_signal<0?-20:20)*i/256.0;double amp=fx_signal==0?0:fx_signal==2?380:120;int a=(int)lround(amp*cos(theta)),b=(int)lround(amp*sin(theta));p[i]=((unsigned)a&1023u)|(((unsigned)b&1023u)<<10);}
 return RISC_RADIO_IQ_OK;
}
static bool fx_caps(void*c,risc_radio_iq_capabilities_v1*out){(void)c;fx_io();assert(out->struct_size==sizeof(*out));*out=(risc_radio_iq_capabilities_v1){.struct_size=sizeof(*out),.flags=7,.controls=511,.min_pairs=1,.max_pairs=8192,.center_min_hz=1841666667u,.center_max_hz=2790000000u,.sample_rates_hz={16000000,80000000},.bandwidths_hz={20000000,40000000},.gain_selector_max=127,.rf_gain_max=511,.bb_gain_max=127,.filter_mask=0x3f3f,.dc_max=511,.iq_correction_mask=0x3f1f,.automatic_value=UINT32_MAX,.sample_format=1,.component_bits=10,.component_full_scale=512,.defaults=RISC_RADIO_IQ_SETTINGS_DEFAULT};return true;}
static int fx_configured(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){assert(s&&s->struct_size==sizeof(*s)&&f&&f->struct_size==sizeof(*f));assert(s->center_hz==prefs.identity.lo_hz&&s->sample_rate_hz==prefs.identity.sample_rate_hz&&s->bandwidth_hz==prefs.identity.width_hz&&s->gain_selector==prefs.identity.raw_gain&&s->rf_gain==prefs.identity.rf_gain&&s->bb_gain==prefs.identity.bb_gain&&s->filter==prefs.identity.filter&&s->iq_correction==prefs.identity.iq_correction&&!memcmp(s->dc,prefs.identity.dc,sizeof(s->dc)));assert(n==prefs.fft_size);fx_settings=*s;int result=fx_capture(c,p,n);if(!result)*f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=7,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=n,.sample_format=1,.component_bits=10,.component_full_scale=512};if(fx_format_failure){f->pair_count--;fx_format_failure=0;}return result;}
static int fx_traced(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f,risc_radio_iq_trace_v1 trace,void*tc){assert(trace&&trace(tc,"host-coherent"));return fx_configured(c,p,n,s,f);}
static bool fx_radio_detail(void*c,risc_radio_iq_diagnostics_v1*out){(void)c;assert(out&&out->struct_size==sizeof(*out));*out=(risc_radio_iq_diagnostics_v1){.struct_size=sizeof(*out),.stage=RISC_RADIO_IQ_STAGE_DUMP,.result=RISC_RADIO_IQ_DUMP_TIMEOUT,.requested_pairs=256,.clock_mask=64,.dump_before=44,.dump_after=44,.elapsed_cycles=2400010,.cleanup_ok=1};return true;}
static int fx_legacy_traced(void*c,uint32_t*p,uint32_t n,risc_radio_iq_trace_v1 trace,void*tc){assert(trace&&trace(tc,"native-claim")&&trace(tc,"dump-start"));int rc=fx_capture(c,p,n);assert(trace(tc,"dump-stopped"));return rc;}
static risc_radio_iq_diagnostics_api_v1 fx_diagnostic_radio;
static risc_radio_iq_api_v1 fx_malformed;
static const risc_radio_iq_api_v1 fx_legacy_radio={1,sizeof(fx_legacy_radio),NULL,fx_capture,fx_radio_suspend};
static const risc_radio_iq_extended_api_v1 fx_radio={.base={{1,sizeof(fx_radio),NULL,fx_capture,fx_radio_suspend},NULL,NULL},.capabilities=fx_caps,.capture_configured=fx_configured,.capture_configured_traced=fx_traced};
static unsigned fx_file(const char*n){assert(!strncmp(n,"rf-",3));for(unsigned i=0;i<3;i++)if(!strcmp(n,fx_files[i].name))return i;for(unsigned i=0;i<3;i++)if(!fx_files[i].name[0]){assert(strlen(n)<32);strcpy(fx_files[i].name,n);return i;}assert(!"Too many RF files");return 0;}
static int32_t fx_stat(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;fx_io();unsigned b=fx_file(n);*s=fx_files[b].size;*r=*s?fx_files[b].revision:0;return *s?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t fx_read(void*c,const char*n,uint64_t expected,void*out,uint32_t cap,uint32_t*s,uint64_t*r){(void)c;fx_io();unsigned b=fx_file(n);*s=0;*r=0;if(expected!=fx_files[b].revision)return RISC_APP_DATA_STALE;assert(cap>=fx_files[b].size);memcpy(out,fx_files[b].bytes,fx_files[b].size);*s=fx_files[b].size;*r=expected;return 0;}
static int32_t fx_replace(void*c,const char*n,uint64_t expected,const void*in,uint32_t size){(void)c;fx_io();assert(!owned&&!running);unsigned b=fx_file(n);assert(expected==(fx_files[b].size?fx_files[b].revision:0)&&size<=RT_BANK_MAX);fx_app_writes++;if(fx_storage_failure==1)return RISC_APP_DATA_NO_SPACE;if(fx_storage_failure==2){fx_storage_retained=true;return RISC_APP_DATA_RETAINED;}memcpy(fx_files[b].bytes,in,size);fx_files[b].size=size;fx_files[b].revision=++fx_revision;if(fx_storage_failure==3){fx_storage_failure=0;return RISC_APP_DATA_COMMIT_UNKNOWN;}return 0;}
static const risc_app_data_v1 fx_data={1,sizeof(fx_data),NULL,fx_stat,fx_read,fx_replace};
static bool fx_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){fx_io();assert(g->struct_size==sizeof(*g));if(!strcmp(n,"display.output")&&v==1)g->api=&fx_display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&fx_touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&fx_battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&fx_rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&fx_legacy_kv;else if(!strcmp(n,"storage.key-value")&&v==2&&id==RF_STORAGE_INSTANCE)g->api=&fx_kv;else if(!strcmp(n,"storage.app-data")&&v==1&&id==RF_APP_DATA_INSTANCE){if(getenv("RF_RENDER_NO_STORAGE"))return false;g->api=&fx_data;}else if(!strcmp(n,"radio.iq")&&v==1){fx_radio_acquires++;if(fx_no_radio)return false;if(getenv("RF_RENDER_MALFORMED"))g->api=&fx_malformed;else if(getenv("RF_RENDER_DIAGNOSTICS"))g->api=&fx_diagnostic_radio;else g->api=fx_legacy?(const void*)&fx_legacy_radio:(const void*)&fx_radio;}else if(!strcmp(n,"alarm.service")&&v==1)g->api=&fx_alarm_api;else return false;fx_grants++;return true;}
static bool fx_release(risc_runtime_capability_v1*g){fx_io();assert(g->api&&fx_grants);if(getenv("RF_RENDER_RELEASE_RETRY")&&!fx_release_failures&&(g->api==&fx_radio||g->api==&fx_legacy_radio)){fx_release_failures++;return false;}g->api=NULL;fx_grants--;return true;}
static const risc_runtime_api_v1 fx_runtime={1,sizeof(fx_runtime),fx_health,fx_yield,fx_diag,fx_launch,fx_acquire,fx_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){static risc_runtime_api_v1 optional;if(v!=1)return NULL;if(fx_app_api_phase&&getenv("RF_RENDER_NO_DIAGNOSTIC")){optional=fx_runtime;optional.diagnostic=NULL;return &optional;}return &fx_runtime;}
int main(int argc,char**argv){
 assert(argc==3);fx_directory=argv[1];fx_commands=fopen(argv[2],"r");assert(fx_commands);memset(&fx_raster,0xa5,sizeof(fx_raster));fx_legacy=getenv("RF_RENDER_LEGACY")!=NULL;fx_no_radio=getenv("RF_RENDER_NO_RADIO")!=NULL;fx_launch_refuse=getenv("RF_RENDER_LAUNCH_REFUSE")!=NULL;fx_kv_failure=getenv("RF_RENDER_KV_FAILURE")?atoi(getenv("RF_RENDER_KV_FAILURE")):0;
if(getenv("RF_RENDER_EXPECT_LAUNCH"))fx_expected_launch=(unsigned)atoi(getenv("RF_RENDER_EXPECT_LAUNCH"));
 if(getenv("RF_RENDER_RF_FAILURE"))fx_rf_failure=atoi(getenv("RF_RENDER_RF_FAILURE"));
 if(getenv("RF_RENDER_MALFORMED")){fx_malformed=fx_legacy_radio;switch(atoi(getenv("RF_RENDER_MALFORMED"))){case 1:fx_malformed.struct_size=offsetof(risc_radio_iq_api_v1,suspend);break;case 2:fx_malformed.api_version=99;break;case 3:fx_malformed.capture_burst=NULL;break;case 4:fx_malformed.suspend=NULL;break;default:abort();}}
 if(getenv("RF_RENDER_DIAGNOSTICS")){fx_legacy=true;fx_diagnostic_radio=(risc_radio_iq_diagnostics_api_v1){{1,sizeof(fx_diagnostic_radio),NULL,fx_capture,fx_radio_suspend},fx_radio_detail,getenv("RF_RENDER_TRACED")?fx_legacy_traced:NULL};}
#ifdef RF_RENDER_RUNTIME_DIAGNOSTICS
 fx_serial=true;rf_serial_start();
#endif
#ifdef RF_RENDER_WATCH_TOUCH
 fx_watch=hid_watch_touch_start();assert(fx_watch);
#endif
 if(getenv("RF_RENDER_NEURAL"))fx_seed_neural();
 unsigned invocations=getenv("RF_RENDER_REENTRY")?2u:1u;for(unsigned invocation=0;invocation<invocations;invocation++){fx_app_api_phase=false;fx_done=false;fx_polls=fx_captures=fx_lineno=0;fx_line[0]=0;rewind(fx_commands);
 assert(app_module_init()==0);fx_corners();fx_app_api_phase=true;
 if(setjmp(fx_retained_jump)==0){app_main();assert(fx_done);fx_returned=true;app_module_fini();assert(!fx_grants&&!fx_frames&&!fx_subs&&!fx_radio_retained);}
 else {assert(fx_storage_retained&&fx_grants&&!running&&!owned&&fx_retained_yields==6);fprintf(stderr,"RF retained storage: no normal peripheral calls after uncertain cleanup\n");}
 fx_guards();assert(fx_checks);if(fx_storage_retained)break;}assert(fx_launches==fx_expected_launch);if(getenv("RF_RENDER_RELEASE_RETRY"))assert(fx_release_failures==1);if(fx_serial)rf_serial_finish(fx_returned,getenv("RF_RENDER_NO_DIAGNOSTIC")==NULL);
#ifdef RF_RENDER_WATCH_TOUCH
 if(fx_returned)hid_watch_touch_stop();
#endif
 assert(!fclose(fx_commands));printf("RF %s: checks=%u polls=%u captures=%u presents=%u kv-writes=%u app-writes=%u retained=%u\n",argv[2],fx_checks,fx_polls,fx_captures,fx_presents,fx_writes,fx_app_writes,fx_storage_retained);return 0;
}
