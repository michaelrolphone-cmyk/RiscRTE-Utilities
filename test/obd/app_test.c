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
static bool missing_radio,close_failure,lost_in_read;
static bool bopen(void*ctx,uint64_t*out){(void)ctx;assert(!radio_token);radio_token=44;*out=44;radio_state=RISC_SERIAL_SCANNING;response_at=response_size=0;return true;}
static bool bpoll(void*ctx,uint64_t t,uint32_t n){(void)ctx;assert(native_alive&&t==44&&radio_token&&n<=16);radio_polls++;
 if(radio_state==RISC_SERIAL_CONNECTING)radio_state=RISC_SERIAL_CHANNELS;
 else if(radio_state==RISC_SERIAL_SUBSCRIBING)radio_state=RISC_SERIAL_READY;return true;}
static bool bstatus(void*ctx,uint64_t t,risc_serial_status_v1*s){(void)ctx;assert(native_alive&&t==radio_token);*s=(risc_serial_status_v1){.struct_size=sizeof(*s),.state=radio_state,.peer_count=1,.channel_count=1,.rx_bytes=radio_rx,.tx_bytes=radio_tx,.generation=1};strcpy(s->message,"Fixture BLE adapter");return true;}
static bool bpeer(void*ctx,uint64_t t,uint32_t i,risc_serial_peer_v1*p){(void)ctx;assert(native_alive&&t==44&&i==0);*p=(risc_serial_peer_v1){.struct_size=sizeof(*p),.address={1,2,3,4,5,6},.connectable=1,.rssi=-48,.name="OBDII"};return true;}
static bool bconnect(void*ctx,uint64_t t,const risc_serial_peer_v1*p){(void)ctx;assert(native_alive&&t==44&&p->address[0]==1);radio_state=RISC_SERIAL_CONNECTING;return true;}
static bool bchannel(void*ctx,uint64_t t,uint32_t i,risc_serial_channel_v1*c){(void)ctx;assert(native_alive&&t==44&&i==0);*c=(risc_serial_channel_v1){.struct_size=sizeof(*c),.flags=RISC_SERIAL_KNOWN_UART,.tx_handle=0x12,.rx_handle=0x15,.service="fff0",.tx_uuid="fff2",.rx_uuid="fff1"};return true;}
static bool bselect(void*ctx,uint64_t t,uint32_t i){(void)ctx;assert(native_alive&&t==44&&i==0);radio_state=RISC_SERIAL_SUBSCRIBING;return true;}
static int32_t bwrite(void*ctx,uint64_t t,const uint8_t*b,uint32_t n){
 (void)ctx;assert(native_alive&&t==44&&radio_token&&n<=20&&radio_state==RISC_SERIAL_READY);assert(response_at==response_size);
 char q[21];memcpy(q,b,n);q[n]=0;radio_tx+=n;response_at=0;
 if(!strcmp(q,"ATZ\r"))strcpy(response,"ELM327 v1.5\r>");
 else if(!strcmp(q,"ATDPN\r"))strcpy(response,"A6\r>");
 else if(!strcmp(q,"ATE0\r")||!strcmp(q,"ATL0\r")||!strcmp(q,"ATS1\r")||!strcmp(q,"ATH1\r")||!strcmp(q,"ATSP0\r"))strcpy(response,"OK\r>");
 else {
  assert(n==5&&q[0]=='0'&&q[1]=='1'&&q[4]=='\r');unsigned pid;assert(sscanf(q+2,"%02x",&pid)==1);unsigned bytes=4,value=0;
  if(pid==0||pid==32||pid==64)strcpy(response,"FF FF FF FF");
  else {
   int i=-1;for(unsigned j=0;j<OBD_METRICS;j++)if(pid==obd_specs[j].pid)i=(int)j;assert(i>=0);
   bytes=obd_specs[i].bytes;
   switch(pid){case 0x0c:value=6896;break;case 0x0d:value=88;break;case 0x05:value=130;break;case 0x04:value=100;break;case 0x11:value=42;break;case 0x42:value=14100;break;case 0x06:value=130;break;case 0x07:value=128;break;case 0x10:value=350;break;case 0x1f:value=1234;break;case 0x01:value=0;break;default:value=100;break;}
   if(bytes==1)snprintf(response,sizeof(response),"%02X",value);else if(bytes==2)snprintf(response,sizeof(response),"%02X %02X",value>>8,value&255);else snprintf(response,sizeof(response),"%02X 00 00 00",value);
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
 if(!strcmp(n,"ui.scene"))a=api;else if(!strcmp(n,"bluetooth.serial")){if(missing_radio)return false;a=&serial_api;}else return false;
 g->api=a;g->slot=++grant_count;g->generation=1;return true;}
static bool cap_release(risc_runtime_capability_v1*g){assert(native_alive&&!radio_token&&g->slot);released_count++;return true;}
static bool clock_now(uint32_t*out){assert(native_alive);*out=(uint32_t)ms;return true;}
static bool retain_rt(void){retains++;native_alive=false;return true;}
static bool diagnostic(const char*s){assert(strstr(s,"OBD/dashboard"));return true;}
static unsigned row_height(unsigned kind){switch(kind){case RISC_COMPONENT_CARD:return 144;case RISC_COMPONENT_SEGMENTS:return 76;case RISC_COMPONENT_PROGRESS:return 176;case RISC_COMPONENT_STATS:return 112;case RISC_SCENE_TEXT_NODE:return 112;case RISC_COMPONENT_EMPTY:return 280;default:return 84;}}
static void click(unsigned id,int val){
 unsigned y=120;bool found=false;
 for(unsigned i=0;i<app.document.node_count;i++){
  risc_scene_node_v1*n=&app.document.nodes[i];unsigned height=row_height(n->kind);
  if(n->action==id){unsigned x=160;
   if(n->kind==RISC_COMPONENT_SEGMENTS)x=32+(val*(416/(n->maximum+1)))+416/(n->maximum+1)/2;
   if(n->kind==RISC_COMPONENT_CHIP){x=id==ACT_DISCONNECT?280:100;y=628;height=60;}
   assert(y+height/2<740);touch_tap(x,y+height/2);found=true;break;
  }y+=height;
 }assert(found);step++;step_at=(uint32_t)ms;
}
static void capture(const char*name){
 if(!frames||!submits)return;char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",frames,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P4\n480 800\n");
 for(unsigned i=0;i<480*800/8;i++){uint8_t b=pixels[16+i];assert(fwrite(&b,1,1,f)==1);}fclose(f);
}
static void yield_rt(uint32_t delay){
 assert(native_alive);ms+=delay;assert(ms<120000);
 if(!app.session||presenting||held||event_at<event_count)return;
 risc_scene_navigation_v1 path={.struct_size=sizeof(path)};uint32_t flags=0;assert(!api->snapshot(NULL,app.session,&path,&flags));if(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return;
 if(ms-step_at<250)return;
 if(step==0&&app.screen==INTRO){capture("intro");click(ACT_SCAN,0);return;}
 if(missing_radio){if(app.screen==NOTICE){capture("missing-provider");app.done=true;}return;}
 if(step==1&&app.screen==PEERS&&app.status.state==RISC_SERIAL_SCANNING&&app.document.node_count>2){capture("devices");click(ACT_PEER,0);return;}
 if(scenario==1){if(app.screen==NOTICE){assert(!radio_token&&close_count);capture("ecu-timeout");app.done=true;}return;}
 if(scenario==2&&step==2&&app.elm.state==OBD_LIVE){close_failure=true;nav_pressed=RISC_NAV_HOME;step++;return;}
 if(scenario==3&&step==2&&app.elm.state==OBD_LIVE){lost_in_read=true;step++;return;}
 if(scenario==5){
  if(step==2&&app.elm.state==OBD_LIVE){event_at=0;event_count=3;events[0]=(risc_touch_event_v1){++seq,ms,1,0,240,1};events[1]=(risc_touch_event_v1){++seq,ms+20,2,0,240,200};events[2]=(risc_touch_event_v1){++seq,ms+40,3,0,240,220};step++;step_at=(uint32_t)ms;}
  else if(step==3&&app.screen==INTRO){assert(shared_calls&&live_polls&&!radio_token);capture("shared-return");app.done=true;}
  return;
 }
 if(step==2&&app.screen==DASHBOARD&&app.elm.state==OBD_LIVE&&obd_fresh(&app.elm,OBD_VOLTS,app.now)&&obd_fresh(&app.elm,OBD_RPM,app.now)){
  /* Wait for the actual model to reach the real framebuffer publication. */
  if(app.document.nodes[1].kind!=RISC_COMPONENT_STATS||strcmp(app.document.nodes[1].label,"1724")||strcmp(app.document.nodes[2].text,"14.1")||!strcmp(app.document.nodes[3].label,"--")||!strcmp(app.document.nodes[3].text,"--"))return;
  capture("drive");seen_publications=submits;click(ACT_TAB,1);return;
 }
 if(step==3&&app.tab==1&&app.document.screen_key==32){capture("engine");click(ACT_TAB,2);return;}
 if(step==4&&app.tab==2&&app.document.screen_key==33){capture("more");click(ACT_TAB,0);return;}
 if(step==5&&app.tab==0&&app.document.screen_key==31){click(ACT_PAUSE,0);return;}
 if(step==6&&app.elm.paused&&app.document.nodes[1].kind==RISC_COMPONENT_STATS&&!strcmp(app.document.nodes[1].label,"--")){
  capture("paused");if(!paused_frames){paused_frames=submits;paused_tx=radio_tx;}if(ms-step_at<3000)return;
  assert(submits==paused_frames&&radio_tx==paused_tx);
  /* Passive paused data must not be repainted indefinitely. */
  click(ACT_PAUSE,0);return;
 }
 if(step==7&&!app.elm.paused&&app.document.nodes[1].kind==RISC_COMPONENT_STATS&&strcmp(app.document.nodes[1].label,"--")){
  click(ACT_DISCONNECT,0);return;
 }
 if(step==8&&app.screen==INTRO){assert(close_count&&radio_token==0);capture("disconnected");nav_pressed=RISC_NAV_HOME;step++;}
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
 assert(driver->start(deps,5));app_main();
 if(scenario==2){assert(retains==1&&!released_count&&radio_token);puts("uncertain BLE close retains entire invocation PASS");return 0;}
 if(scenario==3){assert(!native_alive&&!released_count&&app.retained);puts("native custody lost during serial read: no cleanup I/O PASS");return 0;}
 assert(native_alive&&!radio_token&&!presenting&&!held&&!subscribed&&grant_count==released_count);assert(driver->quiesce());driver->stop();
 /* Diagnostic endpoint: fixture counts, not device timing. */
 fprintf(stderr,"END scenario=%u step=%u screen=%u time=%llu rev=%u frames=%u tx=%u rx=%u msg=%s\n",scenario,step,app.screen,(unsigned long long)ms,app.revision,submits,radio_tx,radio_rx,app.elm.message);
 if(scenario==5)assert(shared_calls&&live_polls);
 if(!scenario)assert(step==9&&radio_tx&&radio_rx&&radio_polls>100&&submits>seen_publications);
 printf("actual app + native scene physical-touch simulation scenario=%u actions=%u frames=%u radio-polls=%u PASS\n",scenario,step,submits,radio_polls);
}
