#define PORTABLE_APP_OWNS_TOUCH_CHROME
#define PORTABLE_RETURN_APP "springboard.elf"
#define PORTABLE_ALARM_CLIENT
#include "../Apps/ble_hid_app.inc"
#include <assert.h>
#include <stdlib.h>
static unsigned grants,raw_subs,opens,closes,releases,confirms,forgets,key_reports,mouse_reports,writes,launches,yields;
static unsigned close_fail,grant_fail,unsub_fail;static bool open_bad,confirm_bad,report_bad,poll_bad,status_bad,pair_choice,confirm_choice,storage_bad,raw_fault,raw_gap,back_enabled;
static bool deny_hid,deny_raw,deny_battery,fail_sub,fail_snapshot;
static unsigned input_step,input_mode;static uint32_t tick;static uint64_t raw_seq,drained;static uint8_t policy=3,battery_value,last_mod,last_key,last_mouse;
typedef struct {uint8_t kind,mod,key,buttons;int8_t dx,dy,wheel;} report_record;static report_record reports[256];static unsigned report_count;
static uint8_t saved[40];static uint32_t saved_size;static bool have_token;static risc_touch_snapshot_v1 sample;
static risc_bluetooth_hid_status_v1 fake_status;static char drawn[5000],diagnostics[8192];static unsigned diagnostic_count;
static uint32_t millis(void){return tick;}
static int32_t screen(void){return 240;}
static bool poll_input(t5_app_input_t*i,uint32_t ms){tick+=ms;if(input_mode){input_step++;i->buttons=T5_APP_BUTTON_BACK;return true;}return false;}
static void back_enabled_fn(bool b){back_enabled=b;}
static void present(bool b){(void)b;}
static bool contact(t5_app_contact_t*c){*c=(t5_app_contact_t){0};return true;}
static bool hid_open(void*c,const char*n,bool p,uint64_t*t){(void)c;assert(n&&strlen(n)<=20&&!have_token);opens++;pair_choice=p;*t=7;have_token=true;return !open_bad;}
static bool hid_poll(void*c,uint64_t t,uint32_t n){(void)c;assert(t==7&&have_token&&n==8);return !poll_bad;}
static bool hid_status(void*c,uint64_t t,risc_bluetooth_hid_status_v1*s){(void)c;assert(s->struct_size==sizeof(*s)&&t==(have_token?7u:0u));if(status_bad)return false;*s=fake_status;if(!have_token)s->state=RISC_HID_OFF;return true;}
static bool hid_confirm(void*c,uint64_t t,bool a){(void)c;assert(t==7&&have_token);confirms++;confirm_choice=a;return !confirm_bad;}
static bool hid_keyboard(void*c,uint64_t t,uint8_t m,const uint8_t*k){(void)c;assert(t==7&&have_token);assert(report_count<256);reports[report_count++]=(report_record){.kind=0,.mod=m,.key=k[0]};key_reports++;last_mod=m;last_key=k[0];return !report_bad;}
static bool hid_mouse(void*c,uint64_t t,uint8_t b,int8_t x,int8_t y,int8_t w){(void)c;assert(t==7&&have_token);assert(report_count<256);reports[report_count++]=(report_record){.kind=1,.buttons=b,.dx=x,.dy=y,.wheel=w};mouse_reports++;last_mouse=b;return !report_bad;}
static bool hid_release(void*c,uint64_t t){(void)c;assert(t==7&&have_token);releases++;last_mod=last_key=last_mouse=0;return !report_bad;}
static bool hid_close(void*c,uint64_t t){(void)c;assert(t==7&&have_token);closes++;if(close_fail){close_fail--;return false;}have_token=false;return true;}
static bool hid_forget(void*c){(void)c;assert(!have_token);forgets++;return true;}
static bool hid_battery(void*c,uint64_t t,uint8_t p){(void)c;assert(t==7&&have_token);battery_value=p;return true;}
static risc_bluetooth_hid_v1 fake_hid={1,sizeof(fake_hid),NULL,hid_open,hid_poll,hid_status,hid_confirm,hid_keyboard,hid_mouse,hid_release,hid_close,hid_forget,hid_battery};
static uint64_t sub(void*c){(void)c;if(fail_sub)return 0;raw_subs++;return 33;}
static bool unsub(void*c,uint64_t t){(void)c;assert(t==33&&raw_subs);if(unsub_fail){unsub_fail--;return false;}raw_subs--;return true;}
static bool touch_poll(void*c,size_t n){(void)c;assert(n==1);return true;}
static int32_t touch_next(void*c,uint64_t t,risc_touch_event_v1*e){(void)c;assert(t==33);if(raw_fault)return -2;if(raw_gap){raw_gap=false;drained=raw_seq;return -1;}if(drained==raw_seq)return 0;*e=(risc_touch_event_v1){.sequence=++drained};return 1;}
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;if(fail_snapshot)return false;*s=sample;s->sequence=raw_seq;return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,sub,unsub,touch_poll,touch_next,snapshot};
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;if(!strcmp(k,"quick_radio")){assert(cap==4);uint8_t v[]={0x51,1,policy,(uint8_t)(policy^0xa5)};memcpy(b,v,4);*n=4;return 0;}assert(!strcmp(k,HID_BUTTONS_KEY)&&cap==40);if(!saved_size)return RISC_KEY_VALUE_NOT_FOUND;memcpy(b,saved,saved_size);*n=saved_size;return 0;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!strcmp(k,HID_BUTTONS_KEY)&&n==40);writes++;if(storage_bad)return RISC_KEY_VALUE_IO;memcpy(saved,b,40);saved_size=40;return 0;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool battery_read(void*c,risc_battery_sample_v1*s){(void)c;s->percent=73;s->flags=0;return true;}
static const risc_battery_gauge_api_v1 gauge={1,sizeof(gauge),NULL,battery_read};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1);if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(id==1||id==11);g->api=&kv;}else if(!strcmp(n,"bluetooth.hid")){assert(id==0);if(deny_hid)return false;g->api=&fake_hid;}else if(!strcmp(n,"input.touch.raw")){assert(id==6);if(deny_raw)return false;g->api=&touch_api;}else{assert(!strcmp(n,"board.battery")&&id==0);if(deny_battery)return false;g->api=&gauge;}grants++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(grants&&g->api);if(grant_fail){grant_fail--;return false;}g->api=NULL;grants--;return true;}
static bool launch(const char*n){assert(!strcmp(n,"springboard.elf")&&!grants&&!token&&!subscription);launches++;return true;}
static void yield(uint32_t ms){tick+=ms;assert(++yields<20);}
static bool diag(const char*s){assert(s&&!strstr(s,"000042")&&!strstr(s,"pairing_number"));assert(strlen(diagnostics)+strlen(s)+2<sizeof(diagnostics));strcat(diagnostics,s);strcat(diagnostics,"\n");diagnostic_count++;return true;}
static const t5_app_api_v1 fake_app={.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=screen,.screen_height=screen,.poll=poll_input,.millis=millis,.present=present,.set_back_exits_app=back_enabled_fn,.touch_contact=contact};
static const risc_runtime_api_v1 fake_rt={.api_version=1,.struct_size=sizeof(fake_rt),.acquire=acquire,.release=release,.yield_ms=yield,.diagnostic=diag,.request_launch=launch};
const t5_app_api_v1 *t5_app_get_api(uint32_t v){assert(v==1);return &fake_app;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return &fake_rt;}
bool portable_app_sleep_retained(void){return false;}
#ifndef HID_RENDER
void portable_nova_begin(void){drawn[0]=0;}
static void text_capture(const char*s){assert(strlen(drawn)+strlen(s)+2<sizeof(drawn));strcat(drawn,s);strcat(drawn,"\n");}
void portable_nova_header(const char*s){text_capture(s);}
void portable_nova_text(unsigned f,int x,int y,int w,const char*s,uint32_t c){(void)c;assert(f<=2&&x>=0&&y>=0&&x+w<=240&&y<240);text_capture(s);}
void portable_nova_center(unsigned f,int x,int y,int w,const char*s,uint32_t c){portable_nova_text(f,x,y,w,s,c);}
void portable_nova_round(int x,int y,int w,int h,int r,uint32_t c){(void)c;(void)r;assert(x>=0&&y>=0&&x+w<=240&&y+h<=240);}
void portable_nova_button(int x,int y,int w,int h,const char*s,bool b){(void)b;assert(w>=44&&h>=44&&x>=0&&y>=0&&x+w<=240&&y+h<=240);text_capture(s);}
bool portable_nova_hit(int x,int y,int l,int t,int w,int h){return x>=l&&y>=t&&x<l+w&&y<t+h;}
#else
static uint16_t pixels[240*240];static struct {unsigned frame;void*pixels;size_t stride_bytes;} surface={1,pixels,480};static bool list_mode;
static int width(void){return 240;}static int height(void){return 240;}static void clear_color(uint16_t c){for(unsigned i=0;i<240*240;i++)pixels[i]=c;}
#include "nova_ui.inc"
static void capture(const char*n){const char*dir=getenv("HID_FRAME_DIR");assert(dir);char p[512];snprintf(p,sizeof(p),"%s/%s.ppm",dir,n);FILE*f=fopen(p,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){uint16_t v=pixels[i];uint8_t rgb[]={(uint8_t)((v>>11)*255/31),(uint8_t)(((v>>5)&63)*255/63),(uint8_t)((v&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));}
#endif
static void reset(void){assert(!grants&&!have_token&&!raw_subs);diagnostics[0]=0;diagnostic_count=0;pair_trace_down=pair_trace_cancelled=pair_trace_gap=false;app=&fake_app;runtime=&fake_rt;hid=NULL;raw=NULL;hid_acquired=raw_acquired=held_acquired=closing=had_ready=close_error=save_failed=pair_armed=pair_down=pair_sent=false;token=subscription=sequence=0;held_button=-1;button_gate=true;view=HID_VIEW_MAIN;message=NULL;dirty=true;hid_defaults(assignments);hid_gesture_reset(&gesture,true);opens=closes=releases=confirms=forgets=key_reports=mouse_reports=writes=launches=yields=report_count=0;deny_hid=deny_raw=deny_battery=fail_sub=fail_snapshot=open_bad=confirm_bad=report_bad=poll_bad=status_bad=storage_bad=raw_fault=raw_gap=false;close_fail=grant_fail=unsub_fail=0;tick=raw_seq=drained=ui_block_until=0;sample=(risc_touch_snapshot_v1){.width=240,.height=240};fake_status=(risc_bluetooth_hid_status_v1){.struct_size=sizeof(fake_status),.state=RISC_HID_READY,.flags=RISC_HID_KEYBOARD_READY|RISC_HID_MOUSE_READY|RISC_HID_ENCRYPTED|RISC_HID_AUTHENTICATED,.connection_generation=1};policy=3;input_mode=input_step=0;}
#ifndef HID_RENDER
static void step(unsigned n,int x,int y){sample.contact_count=(uint8_t)n;sample.contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};sample.contacts[1]=(risc_touch_contact_v1){.id=2,.x=170,.y=120};raw_seq++;tick+=20;pump();}
int main(void){
 reset();draw();assert(!opens);policy=4;start_session(true);assert(!opens&&!grants&&strstr(message,"Airplane"));policy=1;start_session(true);assert(!opens&&strstr(message,"Enable Bluetooth"));policy=3;
 deny_hid=true;start_session(true);assert(!opens&&!grants);deny_hid=false;
 fake_hid.struct_size=0;start_session(true);assert(!opens&&!grants);fake_hid.struct_size=sizeof(fake_hid);
 deny_raw=true;start_session(true);assert(!opens&&!grants);deny_raw=false;
 fail_sub=true;start_session(true);assert(!opens&&!grants&&!raw_subs);fail_sub=false;
 fail_snapshot=true;start_session(true);assert(!opens&&!grants&&!raw_subs);fail_snapshot=false;
 deny_battery=true;start_session(true);assert(battery_value==255);stop_session("Test");deny_battery=false;
 start_session(true);assert(token&&subscription&&grants==2&&pair_choice&&battery_value==73);step(0,0,0);assert(had_ready);
 if(HID_TOUCHPAD){step(1,100,120);step(0,0,0);assert(mouse_reports==2&&reports[0].buttons==1&&reports[1].buttons==0&&last_mouse==0);step(2,100,120);step(0,0,0);assert(mouse_reports==4&&reports[2].buttons==2&&reports[3].buttons==0&&last_mouse==0);step(1,100,120);step(1,120,120);step(0,0,0);assert(mouse_reports==5);}
 else{step(1,30,160);assert(key_reports==1&&last_mod==1&&last_key==6);tick+=380;step(1,30,160);assert(key_reports==1&&held_button==-1&&last_mod==0);step(0,0,0);step(1,30,160);for(unsigned i=0;i<21;i++)step(1,30,160);assert(key_reports==3&&last_mod==1);step(0,0,0);assert(last_mod==0&&key_reports==4);step(1,30,60);step(1,30,90);assert(key_reports==4);step(0,0,0);}
 if(!HID_TOUCHPAD){
  unsigned before=key_reports;fake_status.flags&=~RISC_HID_KEYBOARD_READY;step(0,0,0);step(1,30,100);step(0,0,0);assert(key_reports==before);
  assignments[0]=(hid_assignment){.kind=HID_ACTION_MOUSE,.buttons=2};step(1,30,100);assert(last_mouse==2);step(0,0,0);assert(!last_mouse);
  assignments[0]=(hid_assignment){.kind=HID_ACTION_WHEEL,.wheel=-3};step(1,30,100);assert(reports[report_count-1].wheel==-3);unsigned once=mouse_reports;for(unsigned i=0;i<30;i++)step(1,30,100);assert(mouse_reports==once);step(0,0,0);
  assignments[0]=(hid_assignment){.kind=HID_ACTION_MOVE,.dx=-15};step(1,30,100);assert(reports[report_count-1].dx==-15);step(0,0,0);
  hid_defaults(assignments);fake_status.flags|=RISC_HID_KEYBOARD_READY;step(0,0,0);step(1,30,100);assert(last_key==75);sample.contacts[0].id=2;raw_seq++;tick+=20;pump();assert(!last_key&&held_button==-1);step(0,0,0);
 }
 unsigned r=releases;raw_gap=true;step(1,100,120);assert(releases==r+1);step(0,0,0);
 fake_status.state=RISC_HID_ADVERTISING;step(0,0,0);assert(strstr(message,"Disconnected")&&!last_mod);fake_status.state=RISC_HID_PAIR_CONFIRM;fake_status.pairing_number=42;step(0,0,0);draw();assert(strstr(drawn,"000042"));tap(180,220);assert(confirms==0);step(1,180,220);step(0,0,0);assert(confirms==1&&confirm_choice);step(1,30,220);step(0,0,0);assert(confirms==1);stop_session("Next pairing");start_session(true);step(1,180,220);step(0,0,0);assert(confirms==1);step(1,30,220);step(0,0,0);assert(!token&&!grants&&!raw_subs&&confirms==2&&!confirm_choice);
 start_session(false);assert(!pair_choice);close_fail=2;assert(!close_session()&&token&&hid_acquired&&!portable_radio_services_safe());cleanup_wait();assert(!token&&!grants&&!raw_subs&&yields==1);
 open_bad=true;start_session(true);assert(!token&&!grants&&!raw_subs);open_bad=false;
 start_session(true);unsub_fail=2;stop_session("Stopped");assert(!grants&&!raw_subs);
 start_session(true);step(0,0,0);report_bad=true;step(1,100,120);step(0,0,0);if(token)stop_session("Done");assert(!grants);report_bad=false;
 start_session(true);poll_bad=true;step(0,0,0);assert(!grants&&strstr(message,"Bluetooth error"));poll_bad=false;
 start_session(true);fake_status.state=RISC_HID_PAIR_CONFIRM;fake_status.pairing_number=1000000;step(0,0,0);assert(!grants&&strstr(message,"Invalid pairing"));fake_status.pairing_number=42;
 start_session(true);step(0,0,0);fake_status.state=RISC_HID_CONNECTED;step(0,0,0);assert(!grants&&strstr(message,"timed out"));fake_status.state=RISC_HID_READY;
 start_session(true);raw_fault=true;step(0,0,0);assert(!grants&&strstr(message,"Touch error"));raw_fault=false;
 start_session(true);grant_fail=2;stop_session("Stopped");assert(!grants);
 forget();assert(forgets==1&&!grants);selected=2;draft=(hid_assignment){.kind=HID_ACTION_KEY,.key=40,.modifiers=15};storage_bad=true;assert(!save_draft()&&assignments[2].key==6&&save_failed);storage_bad=false;assert(save_draft()&&assignments[2].key==40&&!save_failed);hid_defaults(assignments);if(!HID_TOUCHPAD){load_assignments();assert(assignments[2].key==40&&assignments[2].modifiers==15);}
 for(unsigned k=0;k<4;k++){for(unsigned i=0;i<100;i++){change_value(1);assert(hid_assignment_valid(&draft));change_value(-1);assert(hid_assignment_valid(&draft));}change_kind();assert(hid_assignment_valid(&draft));}
 view=HID_VIEW_EDIT;draw();view=HID_VIEW_MODIFIERS;for(unsigned i=0;i<8;i++)tap(20+(int)(i%2)*116,70+(int)(i/2)*46);assert(draft.modifiers==255);draft.modifiers=0;draw();assert(back()&&view==HID_VIEW_EDIT);assert(back()&&view==HID_VIEW_MAIN);
 /* Pairing logging follows decisions, remains bounded while held, reports
  * signed driver errors once, and rearms across stopped/restarted sessions. */
 reset();start_session(true);fake_status.state=RISC_HID_PAIR_CONFIRM;fake_status.pairing_number=42;
 step(0,0,0);step(1,180,220);unsigned held_logs=diagnostic_count;
 for(unsigned i=0;i<20;i++)step(1,180,220);
 assert(diagnostic_count==held_logs);step(0,0,0);
 assert(confirms==1&&strstr(diagnostics,"contact target=accept x=180 y=220")&&strstr(diagnostics,"action=accept requested")&&strstr(diagnostics,"action=accept result=ok"));
 stop_session("Next pairing");start_session(true);step(0,0,0);step(1,30,220);step(0,0,0);
 assert(confirms==2&&!confirm_choice&&strstr(diagnostics,"contact target=reject")&&strstr(diagnostics,"action=reject result=ok"));
 reset();start_session(true);step(0,0,0);fake_status.error=-9;step(0,0,0);unsigned error_logs=diagnostic_count;
 for(unsigned i=0;i<20;i++)step(0,0,0);
 assert(diagnostic_count==error_logs&&strstr(diagnostics,"error=-9"));stop_session("Done");
 assert(!strcmp(message,"Done")&&strstr(diagnostics,"HID stop reason=Done")&&strstr(diagnostics,"HID closed status=1 state=0")&&strstr(diagnostics,"error=-9"));
 reset();start_session(true);step(0,0,0);fake_status.state=RISC_HID_FAULT;fake_status.error=12;poll_bad=true;step(0,0,0);
 assert(!grants&&!token&&!strcmp(message,"Bluetooth error - reconnect"));
 assert(strstr(diagnostics,"HID stop reason=Bluetooth error - reconnect")&&strstr(diagnostics,"HID suspend status=1 state=6")&&strstr(diagnostics,"HID closed status=1 state=0"));
 reset();start_session(true);fake_status.state=RISC_HID_PAIR_CONFIRM;fake_status.pairing_number=42;confirm_bad=true;
 step(0,0,0);step(1,180,220);step(0,0,0);
 assert(confirms==1&&!token&&!grants&&strstr(diagnostics,"action=accept result=failed"));
 reset();pair_trace_down=pair_trace_cancelled=pair_trace_gap=true;app_main();assert(!grants&&!opens&&back_enabled&&!pair_trace_down&&!pair_trace_cancelled&&!pair_trace_gap);reset();input_mode=1;app_main();assert(launches==1&&input_step==1&&back_enabled&&!grants);
 puts("HID application: scoped grants, touch, secure pairing, holds, interruptions, close retries, persistence and navigation passed");
}
#else
int main(void){reset();draw();capture("idle");start_session(true);status=fake_status;message="Connected securely";draw();capture("connected");status.state=RISC_HID_PAIR_CONFIRM;status.pairing_number=42;draw();capture("pairing");stop_session("Disconnected - waiting for host");draw();capture("disconnected");view=HID_VIEW_SETTINGS;draw();capture("settings");view=HID_VIEW_FORGET;draw();capture("forget");view=HID_VIEW_EDIT;selected=2;draft=assignments[2];draw();capture("editor");save_failed=true;draw();capture("save-failed");view=HID_VIEW_MODIFIERS;draw();capture("modifiers");view=HID_VIEW_MAIN;message="Turn off Airplane mode";draw();capture("airplane");puts("HID production Nova screenshots captured");}
#endif
