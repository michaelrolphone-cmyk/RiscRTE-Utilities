/* Focused lifecycle and channel declaration checks; no device or scene fixture. */
#include <assert.h>
#include "../../Apps/obd_dashboard/app.c"
static bool owned=true,open_failure,close_failure,reload_on_close;
static uint32_t radio_state,opens,closes,retains,radio_calls;
static uint64_t radio_token;
static void yield_ms(uint32_t ms){(void)ms;assert(owned);}
static bool retained(void){retains++;owned=false;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.yield_ms=yield_ms,.retain_invocation=retained};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return owned&&v==1?&runtime:NULL;}
static bool bopen(void*c,uint64_t*t){(void)c;assert(owned&&!radio_token);radio_calls++;opens++;*t=radio_token=77;radio_state=RISC_SERIAL_SCANNING;return !open_failure;}
static bool bpoll(void*c,uint64_t t,uint32_t n){(void)c;assert(owned&&t==radio_token&&n<=16);radio_calls++;return true;}
static bool bstatus(void*c,uint64_t t,risc_serial_status_v1*s){(void)c;assert(owned&&t==radio_token);radio_calls++;*s=(risc_serial_status_v1){.struct_size=sizeof(*s),.state=radio_state,.peer_count=1,.flags=reload_on_close&&!radio_token?RISC_SERIAL_RELOAD_REQUIRED:0};return true;}
static bool bpeer(void*c,uint64_t t,uint32_t i,risc_serial_peer_v1*p){(void)c;assert(owned&&t==77&&!i);radio_calls++;*p=(risc_serial_peer_v1){.struct_size=sizeof(*p),.address={1,2,3,4,5,6},.connectable=1};return true;}
static bool bconnect(void*c,uint64_t t,const risc_serial_peer_v1*p){(void)c;assert(owned&&t==77&&p->address[0]==1);radio_calls++;radio_state=RISC_SERIAL_DISCONNECTED;return false;}
static bool bclose(void*c,uint64_t t){(void)c;assert(owned&&t==radio_token);radio_calls++;closes++;if(close_failure)return false;radio_token=0;radio_state=RISC_SERIAL_OFF;return true;}
static bool bchannel(void*c,uint64_t t,uint32_t i,risc_serial_channel_v1*p){(void)c;assert(owned&&t==77&&i<2);*p=(risc_serial_channel_v1){.struct_size=sizeof(*p),.tx_handle=0x12,.rx_handle=(uint16_t)(0x15+i*3),.service="6e400001-b5a3-f393-e0a9-e50e24dcca9e",.tx_uuid="6e400002-b5a3-f393-e0a9-e50e24dcca9e"};snprintf(p->rx_uuid,sizeof(p->rx_uuid),"12345678-1234-1234-1234-123456789%03u",i);return true;}
static const risc_bluetooth_serial_v1 ble={.api_version=1,.struct_size=sizeof(ble),.open=bopen,.poll=bpoll,.status=bstatus,.peer=bpeer,.connect=bconnect,.close=bclose,.channel=bchannel};
static void init_app(obd_app*a){memset(a,0,sizeof(*a));owned=true;open_failure=close_failure=reload_on_close=false;opens=closes=retains=radio_calls=0;radio_token=0;radio_state=0;a->rt=&runtime;a->ble=&ble;a->have_peer=a->reconnecting=true;a->selected_peer=(risc_serial_peer_v1){.struct_size=sizeof(a->selected_peer),.address={1,2,3,4,5,6}};db_init(&a->data,0);}
int main(int argc,char**argv){
 (void)argv;
 obd_app a;
 if(argc>1){
  init_app(&a);a.token=radio_token=77;a.screen=CHANNELS;a.status.channel_count=2;declare(&a);
  const risc_scene_node_v1*n0=NULL,*n1=NULL;
  for(unsigned i=0;i<a.candidate.node_count;i++){const risc_scene_node_v1*n=&a.candidate.nodes[i];if(n->action==ACT_CHANNEL)n0=n;if(n->action==ACT_CHANNEL+1)n1=n;}
  assert(n0&&n1);printf("Channel 0 visible: [%s] [%s]; RX: %s\nChannel 1 visible: [%s] [%s]; RX: %s\n",n0->label,n0->text,a.channel_rows[0].rx_uuid,n1->label,n1->text,a.channel_rows[1].rx_uuid);fflush(stdout);
  assert(strcmp(n0->label,n1->label)||strcmp(n0->text,n1->text));
  assert(!strncmp(n0->text,"TX 0012 / RX 0015",17));assert(!strncmp(n1->text,"TX 0012 / RX 0018",17));
  return 0;
 }

 for(unsigned mode=0;mode<2;mode++){
  init_app(&a);open_failure=mode!=0;
  for(unsigned i=0;i<6;i++){a.now=i*6000;assert(service_link(&a));}
  assert(opens==3&&closes==3&&a.reconnect_attempts==3&&!a.reconnecting&&!a.token&&a.screen==NOTICE);
 }
 puts("Failed open and rejected connect each stop after exactly three clean attempts PASS");
 init_app(&a);a.token=radio_token=77;radio_state=RISC_SERIAL_SCANNING;action(&a,ACT_DISCONNECT,0);assert(!a.reconnecting&&!a.token&&a.screen==INTRO);
 for(unsigned i=0;i<6;i++){a.now=i*6000;assert(service_link(&a));}assert(!opens&&closes==1);
 puts("Cancel during reconnect closes once and prevents reopening PASS");
 init_app(&a);open_failure=true;reload_on_close=true;assert(service_link(&a));assert(a.reload_required&&!a.reconnecting&&!a.token);
 for(unsigned i=0;i<6;i++){a.now=i*6000;assert(service_link(&a));}assert(opens==1&&closes==1);
 puts("Reload-required close suppresses retries PASS");
 init_app(&a);open_failure=close_failure=true;assert(!service_link(&a));assert(a.retained&&!owned&&a.token&&radio_token&&closes==3&&retains==1);
 puts("Failed-open cleanup uncertainty retains invocation and lease after three close attempts PASS");
 return 0;
}
