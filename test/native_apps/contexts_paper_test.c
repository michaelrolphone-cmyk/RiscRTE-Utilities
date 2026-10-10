/* Actual Contexts app + shared X4 adapter, fonts/icons and frame custody. */
#define main contexts_toolbar_fixture_main
#include CONTEXTS_TOOLBAR_FIXTURE
#undef main
#include <setjmp.h>
static unsigned cp_writes,cp_requests,cp_pause_count,cp_launches,cp_pending,cp_submitted,cp_latency;
static bool cp_unknown,cp_read_fail,cp_pause_fail,cp_no_service,cp_launch_fail,cp_flip;
static int cp_unread=-1;
static uint8_t cp_cells[9][64],cp_queued[48000];static uint32_t cp_sizes[9];
static char cp_destination[32];
static bool cp_run_loop,cp_loop_draft;static unsigned cp_loop_start,cp_home_events;
static bool cp_navigation_poll(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};
 if(cp_run_loop){unsigned elapsed=ticks-cp_loop_start;assert(elapsed<6000);
  unsigned threshold=cp_loop_draft?(cp_home_events?1800:1100):700;
  if(elapsed>=threshold&&cp_home_events<(cp_loop_draft?2u:1u)){out->buttons=out->pressed=RISC_NAV_HOME;cp_home_events++;}}
 return true;
}
static bool cp_foreground(void *c,const risc_input_foreground_v1 *f,size_t n){(void)c;(void)f;io();assert(n<=1);return true;}
static bool cp_nav_reset(void *c){(void)c;io();return true;}
static const risc_input_navigation_api_v1 cp_navigation={1,sizeof(cp_navigation),NULL,cp_navigation_poll,cp_foreground,cp_nav_reset};
static bool cp_snapshot(void *c,risc_touch_snapshot_v1 *out){
 fx_snapshot(c,out);if(!cp_run_loop||!cp_loop_draft)return true;unsigned elapsed=ticks-cp_loop_start;int x=0,y=0;
 if(elapsed>=150&&elapsed<260){x=350;y=744;}
 if(elapsed>=450&&elapsed<560){x=150;y=185;}
 if(elapsed>=750&&elapsed<860){x=150;y=270;}
 if(elapsed>=1400&&elapsed<1510){x=350;y=744;}
 if(elapsed>=2150&&elapsed<2260){x=100;y=744;}
 if(x){out->contact_count=1;out->contacts[0].id=1;out->contacts[0].x=(uint16_t)(cp_flip?479-x:x);out->contacts[0].y=(uint16_t)(cp_flip?799-y:y);}
 return true;
}
static risc_touch_api_v1 cp_touch;
static contexts_status_v1 cp_live;
static contexts_model_details_v1 cp_models[2];
static int cp_key(const char *key){if(!strcmp(key,PORTABLE_CONTEXT_ENABLED_KEY))return 8;if(!strncmp(key,"ctx_p",5)&&key[5]>='0'&&key[5]<='7'&&!key[6])return key[5]-'0';return -1;}
static int32_t cp_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){
 int i=cp_key(key);if(i<0){
  if(cp_flip&&!strcmp(key,PORTABLE_READER_FLIP_KEY)){const uint8_t r[]={0x52,1,1,0xa4};assert(cap>=4);memcpy(out,r,4);*size=4;return RISC_KEY_VALUE_OK;}
  return fx_kv_get(c,key,out,cap,size);
 }
 io();*size=0;if(i==cp_unread||cp_read_fail)return RISC_KEY_VALUE_IO;
 if(!cp_sizes[i])return RISC_KEY_VALUE_NOT_FOUND;
 assert(cap>=cp_sizes[i]);*size=cp_sizes[i];memcpy(out,cp_cells[i],*size);return RISC_KEY_VALUE_OK;
}
static int32_t cp_put(void *c,const char *key,const void *value,uint32_t size){int i=cp_key(key);if(i<0)return fx_kv_put(c,key,value,size);io();assert(size<=64);cp_writes++;memcpy(cp_cells[i],value,size);cp_sizes[i]=size;if(cp_unknown)cp_read_fail=true;return cp_unknown?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static bool cp_status(void *c,contexts_status_v1 *out){(void)c;io();*out=cp_live;return true;}
static bool cp_step(void *c,const contexts_policy_v1 *p){(void)c;(void)p;io();return true;}
static bool cp_pause(void *c){(void)c;io();cp_pause_count++;return !cp_pause_fail;}
static bool cp_mask(void *c,uint32_t mask){(void)c;io();assert(mask==CONTEXTS_ALL);cp_requests++;cp_live.export_pending=CONTEXTS_RADIO;return true;}
static bool cp_begin(void *c,uint32_t mask){(void)c;(void)mask;io();return true;}
static bool cp_record(void *c,uint32_t a,uint32_t b,uint32_t d,const void *e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;io();return true;}
static bool cp_finish(void *c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;io();return true;}
static int32_t cp_label(void *c,uint32_t source,uint32_t slot,contexts_label_v1 *out){(void)c;io();if(source==CONTEXTS_AUDIO)return CONTEXTS_EXPORT_UNSUPPORTED;if(slot>=8)return 0;*out=(contexts_label_v1){.source=source,.slot=slot,.kind=1};snprintf(out->name,sizeof(out->name),"Studio %u",slot+1);return 1;}
static bool cp_claim(void *c,uint32_t a,uint32_t b,const char *name,uint32_t d){(void)c;(void)a;(void)b;(void)name;(void)d;io();return true;}
static bool cp_result(void *c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;io();return true;}
static bool cp_capture(void *c){(void)c;io();return true;}
static bool cp_details(void *c,uint32_t source,contexts_model_details_v1 *out){(void)c;io();*out=cp_models[source==CONTEXTS_RADIO?1:0];return true;}
static const contexts_service_v1 cp_service={.api_version=1,.struct_size=sizeof(cp_service),.status=cp_status,.step=cp_step,.pause=cp_pause,.request_export=cp_mask,.begin_export=cp_begin,.export_record=cp_record,.finish_export=cp_finish,.label=cp_label,.claim_preset=cp_claim,.preset_result=cp_result,.capture_audio=cp_capture,.model_details=cp_details};
static const risc_key_value_v1 cp_kv={1,sizeof(cp_kv),NULL,cp_get,cp_put};
static alarm_service_descriptor_v2 cp_alarm={.base={2,sizeof(cp_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_refresh,fx_alarm_ack,fx_alarm_prepare,fx_alarm_stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=0};
static risc_display_output_api_v1 cp_display;
static bool cp_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool cp_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){assert(!cp_pending);bool ok=fx_submit(c,f,r,n,o,token);cp_pending=*token;cp_submitted=ticks;memcpy(cp_queued,pixels,sizeof(cp_queued));return ok;}
static bool cp_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){(void)c;io();assert(cp_pending==token&&!memcmp(pixels,cp_queued,sizeof(cp_queued)));out->state=ticks-cp_submitted<cp_latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)cp_pending=0;return true;}
static bool cp_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 const void *api=NULL;
 if(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)){assert(version==1&&!instance);if(cp_no_service)return false;api=&cp_service;}
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==2&&!instance);api=&cp_alarm;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(version==1&&instance==1);api=&cp_kv;}
 else if(!strcmp(name,"display.output"))api=&cp_display;
 else if(!strcmp(name,"input.touch.raw"))api=&cp_touch;
 else if(!strcmp(name,"input.navigation"))api=&cp_navigation;
 else return fx_acquire(name,version,instance,out);
 io();++acquires;++live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=api};return true;
}
static bool cp_launch(const char *destination){io();assert(!cp_pending);snprintf(cp_destination,sizeof(cp_destination),"%s",destination);cp_launches++;return !cp_launch_fail;}
static unsigned cp_sleep_calls;
int portable_app_idle_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a){(void)r;(void)d;(void)b;(void)a;io();assert(!contexts_client.api&&!cp_pending);cp_sleep_calls++;return 1;}
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a){return portable_app_idle_sleep(r,d,b,a);}
#include "../../Apps/contexts.c"
static void cp_draw(const char *directory,const char *name){
 ctx_dirty=true;ctx_draw();assert(portable_paper_frame_drain());ctxp_prepare();
 char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",directory,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));assert(!fclose(f));
}
static void cp_input(int x,int y,bool began,bool released,bool moved){
 paper_scroll_sample=(portable_touch_sample){.valid=true,.down=!released,.began=began,.released=released,.tap_eligible=true,.x=x,.y=y,.moved=moved,.release_position_valid=true};paper_scroll_sampled_at=ticks+=20;
 t5_app_input_t input={0};ctx_paper_input(&input);if(input.tapped)ctx_paper_tap(input.touch_x,input.touch_y);if(input.buttons&T5_APP_BUTTON_BACK)ctx_back();
}
static void cp_tap(int x,int y){cp_input(x,y,true,false,false);cp_input(x,y,false,true,false);}
int main(int argc,char **argv){
 assert(argc==3);cp_flip=!strcmp(argv[2],"flip");cp_display=fx_display;cp_display.get_info=cp_info;cp_display.submit=cp_submit;cp_display.present_status=cp_present;
 cp_touch=fx_touch;cp_touch.snapshot=cp_snapshot;fx_runtime.acquire=cp_acquire;fx_runtime.request_launch=cp_launch;
 assert(app_module_init()==0);ctx_app=t5_app_get_api(1);ctx_paper=paper_presentation_get();assert(ctx_paper&&width()==480&&height()==800);ctx_paper_reset();ctx_runtime=portable_app_custody_runtime();ctx_service=portable_contexts_service();assert(ctx_service);
 cp_live=(contexts_status_v1){.struct_size=sizeof(cp_live),.state=CONTEXTS_LIVE,.audio={.source=CONTEXTS_AUDIO,.model_state=CONTEXTS_MODEL_UNAVAILABLE},.radio={.source=CONTEXTS_RADIO,.model_state=CONTEXTS_MODEL_READY,.signatures_ready=true,.current=true,.room_valid=true,.room_name="Studio 1",.event_valid=true,.event_name="Burst"}};
 cp_models[0]=(contexts_model_details_v1){.struct_size=sizeof(cp_models[0]),.source=CONTEXTS_AUDIO,.temporal_state=CONTEXTS_IMPORT_UNAVAILABLE,.neural_state=CONTEXTS_IMPORT_UNAVAILABLE};cp_models[1]=(contexts_model_details_v1){.struct_size=sizeof(cp_models[1]),.source=CONTEXTS_RADIO,.temporal_state=CONTEXTS_IMPORT_READY,.neural_state=CONTEXTS_IMPORT_MISSING,.event_engine=CONTEXTS_EVENT_TEMPORAL};
 ctx_page=CT_HOME;ctx_message="";ctx_load();ctx_read_status();
 cp_live.radio.age_ms+=50;cp_live.radio.samples++;cp_models[1].event_age_ms+=50;assert(!ctx_read_status());
 assert(!cp_writes&&ctx_enabled_valid&&!ctx_enabled);cp_draw(argv[1],"01-home");
 cp_tap(200,195);assert(ctx_enabled&&cp_writes==1);cp_draw(argv[1],"02-monitor-on");
 cp_tap(200,295);assert(ctx_page==CT_MODELS&&ctx_model_source==1);cp_draw(argv[1],"03-radio-models");
 cp_models[1].neural_state=CONTEXTS_IMPORT_FAILED;cp_models[1].neural_error=CONTEXTS_IMPORT_STALE;assert(ctx_read_status());cp_draw(argv[1],"04-stale-model");
 ctx_back();cp_draw(argv[1],"05-home");cp_tap(200,426);assert(ctx_page==CT_MODELS&&!ctx_model_source);cp_draw(argv[1],"06-audio-unavailable");
 ctx_back();cp_draw(argv[1],"07-home");cp_tap(350,744);assert(ctx_page==CT_PRESETS);cp_draw(argv[1],"08-presets");
 cp_input(200,580,true,false,false);cp_input(200,200,false,false,true);cp_input(200,190,false,true,true);assert(portable_scroll_offset(&ctxp_scroll)>0&&ctx_page==CT_PRESETS&&!ctx_draft_dirty);ctxp_scroll.velocity_q8=0;cp_draw(argv[1],"09-presets-scrolled");
 ctxp_scroll.position_q8=0;cp_draw(argv[1],"10-presets-top");cp_tap(150,185);assert(ctx_page==CT_ROOMS&&ctx_room_count==8);cp_draw(argv[1],"11-rooms");
 cp_tap(150,185);assert(ctx_page==CT_EDIT&&ctx_draft.source==CONTEXTS_RADIO&&ctx_draft_dirty);cp_draw(argv[1],"12-edit");
 assert(!ctx_paper_field_available(0)&&!ctx_paper_field_available(2)&&!ctx_paper_field_available(3)&&!ctx_paper_field_available(4)&&!ctx_paper_field_available(6)&&!ctx_paper_field_available(7));assert(ctx_paper_field_available(1)&&ctx_paper_field_available(5));
 ctx_field=1;ctx_page=CT_FIELD;cp_draw(argv[1],"13-idle");cp_tap(200,270);cp_tap(420,380);assert(ctx_draft.actions==PORTABLE_CONTEXT_IDLE);cp_draw(argv[1],"14-idle-changed");
 ctx_field=3;cp_draw(argv[1],"15-unavailable-action");unsigned old=ctx_draft.brightness;cp_tap(420,380);assert(ctx_draft.brightness==old&&!(ctx_draft.actions&PORTABLE_CONTEXT_BRIGHTNESS));
 ctx_page=CT_EDIT;cp_draw(argv[1],"16-edit");cp_tap(150,270);assert(ctx_draft.enabled);cp_draw(argv[1],"17-enabled");cp_tap(100,744);assert(ctx_page==CT_PRESETS&&!ctx_draft_dirty&&cp_writes==2);cp_draw(argv[1],"18-saved");
 ctx_open_preset(0);ctx_field=1;ctx_change(1);assert(!portable_app_before_launch("default.elf")&&ctx_page==CT_DISCARD);cp_draw(argv[1],"19-home-draft");cp_tap(350,744);assert(ctx_page==CT_EDIT&&ctx_draft_dirty&&!cp_launches);cp_draw(argv[1],"20-draft-preserved");
 cp_unknown=true;ctx_save();assert(ctx_save_uncertain);old=ctx_draft.idle_ms;ctx_change(1);assert(ctx_draft.idle_ms==old);cp_draw(argv[1],"21-unconfirmed-save");cp_unknown=cp_read_fail=false;ctx_save();assert(!ctx_save_uncertain&&ctx_page==CT_PRESETS&&cp_writes==3);
 cp_unread=1;ctx_load();ctx_open_preset(1);assert(ctx_page==CT_PRESETS&&!strcmp(ctx_message,"UNREAD PRESET / TAP RETRY"));cp_draw(argv[1],"22-unread");cp_unread=-1;
 /* Submitted frames are immutable; contacts keep completed row identities. */
 cp_latency=160;ctx_page=CT_HOME;ctx_dirty=true;ctx_draw();assert(cp_pending);unsigned before=presents;ctx_page=CT_PRESETS;ctx_dirty=true;ctx_draw();assert(presents==before&&ctx_dirty);cp_tap(150,185);assert(ctx_page==CT_PRESETS&&!ctx_draft_dirty);assert(portable_paper_frame_drain());ctx_draw();assert(portable_paper_frame_drain());ctxp_prepare();cp_latency=0;
 /* Quick Controls cancels a held contact and keeps a local draft intact. */
 ctx_open_preset(0);ctx_field=1;ctx_change(1);cp_draw(argv[1],"23-draft-quick");old=ctx_draft.idle_ms;cp_input(150,270,true,false,false);quick_modal=true;cp_input(150,270,false,true,false);assert(ctx_draft.enabled&&ctx_draft.idle_ms==old);quick_modal=false;
 /* Actual automatic Light releases Contexts; app reacquires before reading. */
 assert(portable_paper_frame_drain());automatic_idle=true;assert(idle_sleep());assert(cp_sleep_calls==1&&!contexts_client.api);ctx_read_status();assert(contexts_client.api&&ctx_draft.idle_ms==old&&ctx_draft_dirty);cp_draw(argv[1],"24-light-draft");
 ctx_draft_dirty=false;ctx_page=CT_HOME;assert(contexts_suspend());cp_no_service=true;ctx_read_status();assert(!ctx_status_valid);cp_draw(argv[1],"25-service-unavailable");cp_no_service=false;ctx_read_status();
 ctx_load_models();assert(cp_requests==1&&ctx_exit&&!strcmp(cp_destination,"default.elf"));ctx_exit=false;assert(!contexts_client.api);
 ctx_read_status();ctx_page=CT_HOME;cp_launch_fail=true;ctx_back();assert(!ctx_exit&&!strcmp(cp_destination,"springboard.elf"));cp_launch_fail=false;ctx_back();assert(ctx_exit);
 app_module_fini();assert(!retained&&!live&&!subscriptions&&!frames&&!cp_pending);
 /* Run the actual foreground loop with physical snapshots and Home routing. */
 for(unsigned pass=0;pass<2;pass++){
  cp_run_loop=true;cp_loop_draft=pass!=0;cp_home_events=0;cp_loop_start=ticks;cp_latency=50;
  unsigned saved_writes=cp_writes,saved_launches=cp_launches;
  assert(app_module_init()==0);app_main();assert(cp_launches==saved_launches+1&&!strcmp(cp_destination,"default.elf"));
  assert(!ctx_draft_dirty&&cp_writes==saved_writes&&cp_home_events==(pass?2u:1u));
  app_module_fini();assert(!retained&&!live&&!subscriptions&&!frames&&!cp_pending);cp_run_loop=false;
 }
 printf("Contexts paper %s: production raster, capabilities, models, draft/save/discard, async identity, modal interruption, Light resume, launch and cleanup PASS\n",argv[2]);
}
