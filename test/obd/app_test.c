/* Runs the actual app_main and native scene implementation. Only the board
 * and serial capability are fixtures; every UI action is a DOWN/UP touch. */
#define main scene_baseline_main
#define risc_runtime_get_api scene_test_runtime_stub
#include "host_test.c"
#undef risc_runtime_get_api
#undef main
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t);
#include "../../Apps/obd_dashboard/app.c"
static uint32_t radio_state,radio_polls,radio_tx,radio_rx,close_count,grant_count,released_count,retains;
static char response[256];static size_t response_at,response_size;
static uint64_t radio_token;
static unsigned scenario,step,seen_publications,shared_calls,live_polls,paused_frames,paused_tx;
static bool requested_policy;
static uint32_t step_at;
static const char *frames;
static bool missing_radio,close_failure,lost_in_read,test_reopen,stationary,overheat,force_link_drop;
static unsigned clear_writes,reconnects;
static risc_scene_telemetry_api_v1 legacy_scene;
#include "storage_fixture.inc"
int telemetry_test_target(unsigned,int,int*,int*);
static bool bopen(void*ctx,uint64_t*out){(void)ctx;assert(!radio_token);radio_token=44;*out=44;radio_state=RISC_SERIAL_SCANNING;response_at=response_size=0;return true;}
static bool bpoll(void*ctx,uint64_t t,uint32_t n){(void)ctx;assert(native_alive&&t==44&&radio_token&&n<=16);radio_polls++;
 if(force_link_drop){radio_state=RISC_SERIAL_DISCONNECTED;force_link_drop=false;}
 if(radio_state==RISC_SERIAL_CONNECTING)radio_state=RISC_SERIAL_CHANNELS;
 else if(radio_state==RISC_SERIAL_SUBSCRIBING)radio_state=RISC_SERIAL_READY;return true;}
static bool bstatus(void*ctx,uint64_t t,risc_serial_status_v1*s){(void)ctx;assert(native_alive&&t==radio_token);*s=(risc_serial_status_v1){.struct_size=sizeof(*s),.state=radio_state,.peer_count=1,.channel_count=1,.rx_bytes=radio_rx,.tx_bytes=radio_tx,.generation=1};strcpy(s->message,"Fixture BLE adapter");return true;}
static bool bpeer(void*ctx,uint64_t t,uint32_t i,risc_serial_peer_v1*p){(void)ctx;assert(native_alive&&t==44&&i==0);*p=(risc_serial_peer_v1){.struct_size=sizeof(*p),.address={1,2,3,4,5,6},.connectable=1,.rssi=-48,.name="OBDII"};return true;}
static bool bconnect(void*ctx,uint64_t t,const risc_serial_peer_v1*p){(void)ctx;assert(native_alive&&t==44&&p->address[0]==1);radio_state=RISC_SERIAL_CONNECTING;reconnects++;return true;}
static bool bchannel(void*ctx,uint64_t t,uint32_t i,risc_serial_channel_v1*c){(void)ctx;assert(native_alive&&t==44&&i==0);*c=(risc_serial_channel_v1){.struct_size=sizeof(*c),.flags=RISC_SERIAL_KNOWN_UART,.tx_handle=0x12,.rx_handle=0x15,.service="fff0",.tx_uuid="fff2",.rx_uuid="fff1"};return true;}
static bool bselect(void*ctx,uint64_t t,uint32_t i){(void)ctx;assert(native_alive&&t==44&&i==0);radio_state=RISC_SERIAL_SUBSCRIBING;return true;}
static int32_t bwrite(void*ctx,uint64_t t,const uint8_t*b,uint32_t n){
 (void)ctx;assert(native_alive&&t==44&&radio_token&&n<=20&&radio_state==RISC_SERIAL_READY);assert(response_at==response_size);
 char q[21];memcpy(q,b,n);q[n]=0;radio_tx+=n;response_at=0;
 if(!strcmp(q,"ATZ\r"))strcpy(response,"ELM327 v1.5\r>");
 else if(!strcmp(q,"ATDPN\r"))strcpy(response,"A6\r>");
 else if(!strcmp(q,"ATE0\r")||!strcmp(q,"ATL0\r")||!strcmp(q,"ATS1\r")||!strcmp(q,"ATH1\r")||!strcmp(q,"ATSP0\r"))strcpy(response,"OK\r>");
 else if(!strcmp(q,"0902\r"))strcpy(response,"7E8 10 14 49 02 01 31 4D 38\r7E8 21 47 44 4D 39 41 58 4B\r7E8 22 50 30 34 32 37 38 38\r>");
 else if(!strcmp(q,"ATRV\r"))strcpy(response,"14.1V\r>");
 else if(!strcmp(q,"03\r"))strcpy(response,"7E8 06 43 02 04 20 01 28\r>");
 else if(!strcmp(q,"07\r"))strcpy(response,"7E8 04 47 01 01 71\r>");
 else if(!strcmp(q,"0A\r"))strcpy(response,"7E8 02 4A 00\r>");
 else if(!strcmp(q,"04\r")){assert(stationary&&app.elm.clear_status==OBD_DIAG_LOADING);clear_writes++;strcpy(response,"7E8 01 44\r>");}
 else if(!strcmp(q,"020200\r"))strcpy(response,"7E8 05 42 02 00 04 20\r>");
 else if(q[0]=='0'&&q[1]=='2'){unsigned pid;assert(sscanf(q+2,"%02x",&pid)==1);unsigned v=pid==0x0c?9800:pid==0x0d?88:pid==5?131:pid==6?131:pid==7?133:80;
 if(pid==0x0c)snprintf(response,sizeof(response),"7E8 05 42 %02X 00 %02X %02X\r>",pid,v>>8,v&255);else snprintf(response,sizeof(response),"7E8 04 42 %02X 00 %02X\r>",pid,v);}
 else {
  assert(n==5&&q[0]=='0'&&q[1]=='1'&&q[4]=='\r');unsigned pid;assert(sscanf(q+2,"%02x",&pid)==1);unsigned bytes=4,value=0;
  if(pid==0||pid==32||pid==64)strcpy(response,"FF FF FF FF");
  else {
   int i=-1;for(unsigned j=0;j<OBD_METRICS;j++)if(pid==obd_specs[j].pid)i=(int)j;assert(i>=0);
   bytes=obd_specs[i].bytes;
   switch(pid){case 0x0c:value=stationary?0:2400*4;break;case 0x0d:value=stationary?0:88;break;case 0x05:value=overheat?150:130;break;case 0x04:value=100;break;case 0x11:value=42;break;case 0x42:value=14100;break;case 0x06:value=130;break;case 0x07:value=128;break;case 0x10:value=350;break;case 0x1f:value=1234;break;case 0x51:case 0x1c:value=1;break;case 0x01:value=0x82070000;break;default:value=100;break;}
   if(bytes==1)snprintf(response,sizeof(response),"%02X",value);else if(bytes==2)snprintf(response,sizeof(response),"%02X %02X",value>>8,value&255);else snprintf(response,sizeof(response),"%02X %02X %02X %02X",value>>24,(value>>16)&255,(value>>8)&255,value&255);
  }
  char data[32];strcpy(data,response);snprintf(response,sizeof(response),"7E8 %02X 41 %02X %s\r>",bytes+2,pid,data);
 }
 if(scenario==1&&q[0]=='0'&&q[2]=='0'&&q[3]=='0')response[0]=0; /* ECU search timeout */
 response_size=strlen(response);return (int32_t)n;
}
static int32_t bread(void*ctx,uint64_t t,uint8_t*b,uint32_t n){(void)ctx;assert(native_alive&&t==44&&radio_token);
 if(lost_in_read){native_alive=false;return -1;}
 if(n>7)n=7;if(n>response_size-response_at)n=(uint32_t)(response_size-response_at);memcpy(b,response+response_at,n);response_at+=n;radio_rx+=n;return (int32_t)n;}
static bool bclose(void*ctx,uint64_t t){(void)ctx;assert(native_alive&&t==radio_token&&t==44);close_count++;if(close_failure)return false;radio_token=0;radio_state=0;return true;}
static const risc_bluetooth_serial_v1 serial_api={1,sizeof(serial_api),NULL,bopen,bpoll,bstatus,bpeer,bconnect,bchannel,bselect,bwrite,bread,bclose};
static bool cap_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(native_alive&&v==1&&!id);const void* a;
 if(!strcmp(n,"ui.scene")){if(scenario==7){memcpy(&legacy_scene,api,sizeof(legacy_scene));((risc_scene_api_v1*)&legacy_scene)->struct_size=sizeof(risc_scene_reading_links_api_v1);a=&legacy_scene;}else a=api;}else if(!strcmp(n,"bluetooth.serial")){if(missing_radio)return false;a=&serial_api;}else if(!strcmp(n,"storage.volume"))a=&fs_api;else return false;
 g->api=a;g->slot=++grant_count;g->generation=1;return true;}
static bool cap_release(risc_runtime_capability_v1*g){assert(native_alive&&!radio_token&&g->slot);released_count++;return true;}
static bool clock_now(uint32_t*out){assert(native_alive);*out=(uint32_t)ms;return true;}
static bool retain_rt(void){retains++;native_alive=false;return true;}
static bool diagnostic(const char*s){assert(strstr(s,"OBD/dashboard"));return true;}
static void swipe(int direction){event_at=0;event_count=3;int y=direction>0?650:200,end=direction>0?200:650;
 events[0]=(risc_touch_event_v1){++seq,ms,1,0,240,(uint16_t)y};events[1]=(risc_touch_event_v1){++seq,ms+30,2,0,240,(uint16_t)(y+end)/2};events[2]=(risc_touch_event_v1){++seq,ms+60,3,0,240,(uint16_t)end};}
static void click(unsigned id,int val){
 int x=0,y=0,rc=telemetry_test_target(id,val,&x,&y);
 if(rc==2||rc==-1){swipe(rc==2?1:-1);step_at=(uint32_t)ms;return;}
 if(rc!=1){fprintf(stderr,"Missing target action=%u value=%d screen=%u step=%u revision=%u\n",id,val,app.screen,step,app.revision);assert(0);}
 fprintf(stderr,"TAP step=%u action=%u value=%d x=%d y=%d screen=%u\n",step,id,val,x,y,app.screen);touch_tap((unsigned)x,(unsigned)y);step++;step_at=(uint32_t)ms;
}
static void capture(const char*name){
 if(!frames||!submits)return;char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",frames,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P4\n480 800\n");
 for(unsigned i=0;i<480*800/8;i++){uint8_t b=pixels[16+i];assert(fwrite(&b,1,1,f)==1);}fclose(f);
}
static void yield_rt(uint32_t delay){
 assert(native_alive);ms+=delay;if(ms>=240000){fprintf(stderr,"STALLED step=%u screen=%u state=%u dirty=%u pending=%u\n",step,app.screen,app.elm.state,app.dirty,event_count-event_at);assert(0);}
 if(!app.session||presenting||held||event_at<event_count)return;
 risc_scene_navigation_v1 path={.struct_size=sizeof(path)};uint32_t flags=0;assert(!api->snapshot(NULL,app.session,&path,&flags));if(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return;
 if(ms-step_at<300)return;
 if(scenario==7){assert(app.upgrade_required&&!radio_token);capture("update-firmware");if(step==0)click(ACT_EXIT,0);return;}
 if(test_reopen){assert(app.data.cfg.imperial==1&&app.data.cfg.update_ms==2000&&app.data.cfg.dash_count==7);capture("restored-settings");app.done=true;return;}
 if(step==0&&app.screen==INTRO){capture("intro");click(ACT_SCAN,0);return;}
 if(missing_radio){if(app.screen==NOTICE){capture("missing-provider");app.done=true;}return;}
 if(step==1&&app.screen==PEERS){capture("devices");click(ACT_PEER,0);return;}
 if(step==2&&app.screen==SELECTED){capture("selected");click(ACT_CONNECT,0);return;}
 if(scenario==1){if(app.screen==NOTICE){assert(!radio_token&&close_count);capture("ecu-timeout");app.done=true;}return;}
 if(scenario==2&&step==3&&app.elm.state==OBD_LIVE){close_failure=true;nav_pressed=RISC_NAV_HOME;step++;return;}
 if(scenario==3&&step==3&&app.elm.state==OBD_LIVE){lost_in_read=true;step++;return;}
 if(scenario==5){
  if(step==3&&app.elm.state==OBD_LIVE){event_at=0;event_count=3;events[0]=(risc_touch_event_v1){++seq,ms,1,0,240,1};events[1]=(risc_touch_event_v1){++seq,ms+20,2,0,240,200};events[2]=(risc_touch_event_v1){++seq,ms+40,3,0,240,220};step++;step_at=(uint32_t)ms;}
  else if(step==4&&app.screen==INTRO){assert(shared_calls&&live_polls&&!radio_token);capture("shared-return");app.done=true;}return;
 }
 if(app.screen==CONNECTING){capture("connecting");return;}
 if(scenario>=6){
  if(step==3){if(app.elm.state!=OBD_LIVE||!obd_fresh(&app.elm,OBD_VOLTS,app.now)||!obd_fresh(&app.elm,OBD_RPM,app.now)||ms<20000)return;
   if(scenario==6){overheat=true;step++;return;}
   if(scenario==8){stationary=true;click(ACT_TAB,2);return;}
   if(scenario==11){force_link_drop=true;step++;return;}
   click(ACT_SETTINGS,0);return;
  }
  if(scenario==6){
   if(step==4&&app.screen==ALERT_MODAL){assert(app.data.alert.metric==OBD_COOLANT);capture("alert-hot");click(ACT_MUTE,0);}
   else if(step==5&&app.screen==DASHBOARD){assert(app.data.muted[OBD_COOLANT]);capture("muted-hot-tiles");click(ACT_SETTINGS,0);}
   else if(step==6&&app.screen==SETTINGS)click(ACT_ALERTS,0);
   else if(step==7&&app.screen==ALERTS){assert(app.data.alert_count==1);capture("alert-history");click(ACT_ALERT_ITEM,0);}
   else if(step==8&&app.screen==DETAIL){capture("alert-graph");app.done=true;}return;
  }
  if(scenario==8){
   if(step==4&&app.screen==FAULTS&&obd_fresh(&app.elm,OBD_SPEED,app.now)&&app.elm.samples[OBD_SPEED].milli==0&&app.elm.samples[OBD_RPM].milli==0)click(ACT_CLEAR_ASK,0);
   else if(step==5&&app.screen==CONFIRM){capture("clear-confirmed");click(ACT_CLEAR,0);}
   else if(step==6&&app.screen==FAULTS&&app.elm.clear_status==OBD_DIAG_VALID){assert(clear_writes==1);capture("clear-acknowledged");app.done=true;}return;
  }
  if(scenario==9){
   if(step==4&&app.screen==SETTINGS)click(ACT_UNITS,1);
   else if(step==5&&sf_replacements){sf_short=true;click(ACT_UPDATE,1);}
   else if(step==6&&!app.save_config&&app.storage_error){sf_short=false;char path[1024];sf_path(DB_SETTINGS_PATH,path);settings_record rec;FILE*f=fopen(path,"rb");assert(f&&fread(&rec,1,sizeof(rec),f)==sizeof(rec));fclose(f);assert(rec.config.imperial==1&&rec.config.update_ms==1000);capture("settings-short-write");click(ACT_LOG_TRIP,1);}
   else if(step==7&&app.log_file){sf_short=true;step++;}
   else if(step==8&&!app.log_file){assert(app.storage_error);sf_short=false;capture("trip-write-error");app.done=true;}return;
  }
  if(scenario==10){if(step==4&&app.screen==SETTINGS){sf_retain_on_write=true;click(ACT_UNITS,1);}return;}
  if(scenario==11){if(step==4&&app.screen==DASHBOARD&&app.elm.state==OBD_LIVE&&reconnects==2){assert(close_count==1&&!app.reconnecting);capture("reconnected-selected-peer");app.done=true;}return;}
 }
 if(step==3){if(app.elm.state!=OBD_LIVE||!obd_fresh(&app.elm,OBD_VOLTS,app.now)||!obd_fresh(&app.elm,OBD_RPM,app.now)||ms<20000||app.elm.vin_status!=OBD_DIAG_VALID)return;capture("live");seen_publications=submits;click(ACT_PID+OBD_COOLANT,0);return;}

 switch(step){
 case 4:if(app.screen==DETAIL){capture("detail");click(ACT_DETAIL_SPAN,1);}break;
 case 5:if(app.detail_span==1){capture("detail-2min");click(ACT_LIMIT_HIGH,31);}break; /* 110 C */
 case 6:assert(app.data.cfg.limits[OBD_COOLANT].high==110000);click(ACT_BACK,0);break;
 case 7:if(app.screen==DASHBOARD)click(ACT_TAB,1);break;
 case 8:if(app.screen==GRAPH){capture("graph");click(ACT_GRAPH_PAUSE,0);}break;
 case 9:if(app.data.paused){capture("graph-frozen");if(ms-step_at<3000)return;click(ACT_GRAPH_ADD,0);}break;
 case 10:if(app.screen==GRAPH_PICK){capture("graph-picker");click(ACT_GRAPH_SENSOR+OBD_LOAD,0);}break;
 case 11:assert(app.data.cfg.graph_count==2);click(ACT_GRAPH_SENSOR+OBD_SPEED,1);break;
 case 12:assert(app.data.cfg.graph_count==3);click(ACT_BACK,0);break;
 case 13:if(app.screen==GRAPH){capture("graph-selected");click(ACT_GRAPH_SPAN,1);}break;
 case 14:assert(app.graph_span==1);click(ACT_TAB,2);break;
 case 15:if(app.screen==FAULTS){capture("faults");click(ACT_DTC,0);}break;
 case 16:if(app.screen==DTC_DETAIL&&app.elm.freeze_status==OBD_DIAG_VALID){capture("dtc");swipe(1);step++;step_at=(uint32_t)ms;}break;
 case 17:capture("freeze-frame");click(ACT_BACK,0);break;
 case 18:if(app.screen==FAULTS)click(ACT_RESCAN+1000,1);break;
 case 19:assert(app.codes_tab==1);capture("pending-codes");click(ACT_READINESS,0);break;
 case 20:if(app.screen==READINESS){capture("readiness");click(ACT_BACK,0);}break;
 case 21:if(app.screen==FAULTS)click(ACT_CLEAR_ASK,0);break;
 case 22:if(app.screen==CONFIRM){capture("clear-confirmation");click(ACT_KEEP,0);}break;
 case 23:assert(!app.elm.clear_armed);click(ACT_TAB,3);break;
 case 24:if(app.screen==INFO&&obd_fresh(&app.elm,OBD_DISTANCE_CLEAR,app.now)){capture("info");click(ACT_LOG,0);}break;
 case 25:if(app.screen==ADLOG){capture("adapter-log");click(ACT_BACK,0);}break;
 case 26:if(app.screen==INFO)click(ACT_SETTINGS,0);break;
 case 27:if(app.screen==SETTINGS){capture("settings");click(ACT_UNITS,1);}break;
 case 28:assert(app.imperial);click(ACT_UPDATE,1);break;
 case 29:assert(app.data.cfg.update_ms==2000);click(ACT_LOG_TRIP,1);break;
 case 30:if(app.log_file){capture("recording");if(ms-step_at<3000)return;click(ACT_LOG_TRIP,0);}break;
 case 31:assert(!app.log_file&&app.log_bytes>100);click(ACT_BACK,0);break;
 case 32:if(app.screen==INFO)click(ACT_TAB,0);break;
 case 33:if(app.screen==DASHBOARD){capture("imperial-live");click(ACT_CUSTOM,0);}break;
 case 34:if(app.screen==CUSTOM){capture("customize");click(ACT_ON_DASH+OBD_INTAKE,0);}break;
 case 35:assert(app.data.cfg.dash_count==7);click(ACT_BACK,0);break;
 case 36:if(app.screen==DASHBOARD){capture("seven-tiles");click(ACT_SETTINGS,0);}break;
 case 37:if(app.screen==SETTINGS)click(ACT_POLL_PAUSE,0);break;
 case 38:if(app.elm.paused){if(!paused_frames){paused_frames=submits;paused_tx=radio_tx;}if(ms-step_at<5000)return;assert(radio_tx==paused_tx);capture("paused");click(ACT_POLL_PAUSE,0);}break;
 case 39:assert(!app.elm.paused);click(ACT_DISCONNECT_ASK,0);break;
 case 40:if(app.screen==CONFIRM){capture("disconnect-confirmation");click(ACT_DISCONNECT,0);}break;
 case 41:if(app.screen==INTRO){assert(close_count&&!radio_token);capture("disconnected");nav_pressed=RISC_NAV_HOME;step++;}break;
 }
}
static int32_t shell_checkpoint(uint64_t token,const risc_resident_request_v1*r,risc_resident_reply_v1*out){
 assert(token==9&&native_alive);out->flags=0;
 if(r->reason==RISC_RESIDENT_CHECKPOINT_POLL){
  if(radio_token){assert((r->flags&(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY))==(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY));live_polls++;}
  else if(!requested_policy){out->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;requested_policy=true;}
 }else {assert(!radio_token&&!app.session&&!presenting&&!subscribed);assert(r->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS||r->reason==RISC_RESIDENT_CHECKPOINT_POLICY);shared_calls++;}
 return RISC_RESIDENT_OK;
}
static bool resident_bind(risc_resident_client_v1*out){if(scenario!=5)return false;*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=9,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=shell_checkpoint};return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.yield_ms=yield_rt,.diagnostic=diagnostic,.acquire=cap_acquire,.release=cap_release,.retain_invocation=retain_rt,.monotonic_ms=clock_now,.resident_shell=resident_bind};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return native_alive&&v==1?&runtime:NULL;}
int main(int argc,char**argv){
 if(argc>1)scenario=(unsigned)atoi(argv[1]);if(argc>2)frames=argv[2];missing_radio=scenario==4;
 w=480;h=800;format=1;presentation_delay=100;poll_step=5;expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};
 driver=t5_driver_get(2);api=driver->capability;risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 sf_init();assert(driver->start(deps,5));app_main();
 if(scenario==2){assert(retains==1&&!released_count&&radio_token);puts("uncertain BLE close retains entire invocation PASS");return 0;}
 if(scenario==10){assert(sf_retain&&retains==1&&!released_count);puts("SD custody lost during write: retain without further I/O PASS");return 0;}
 if(scenario==3){assert(!native_alive&&!released_count&&app.retained);puts("native custody lost during serial read: no cleanup I/O PASS");return 0;}
 assert(native_alive&&!radio_token&&!presenting&&!held&&!subscribed&&grant_count==released_count);
 for(unsigned i=1;i<8;i++)assert(!sf_handles[i]);
 if(!scenario){assert(sf_replacements&&sf_writes&&sf_syncs);test_reopen=true;app_main();assert(grant_count==released_count&&sf_reads);}
 assert(driver->quiesce());driver->stop();
 /* Diagnostic endpoint: fixture counts, not device timing. */
 fprintf(stderr,"END scenario=%u step=%u screen=%u time=%llu rev=%u frames=%u tx=%u rx=%u msg=%s\n",scenario,step,app.screen,(unsigned long long)ms,app.revision,submits,radio_tx,radio_rx,app.elm.message);
 if(scenario==5)assert(shared_calls&&live_polls);
 if(!scenario)assert(step==42&&radio_tx&&radio_rx&&radio_polls>100&&submits>seen_publications);
 printf("actual app + native scene physical-touch simulation scenario=%u actions=%u frames=%u radio-polls=%u PASS\n",scenario,step,submits,radio_polls);
}
