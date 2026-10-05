#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#include "../Apps/ble_scanner.c"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
static unsigned input_mode,input_step;static bool back_enabled;
static uint32_t tick;static unsigned grants,claims,closes,sends;static bool close_bad,send_bad,release_bad,claim_bad;static int restore_result=1;
static uint8_t policy=3,queued[64];static size_t queued_size;static char drawn[4000];static jmp_buf retention;
static uint32_t millis(void){return tick;}
static int32_t screen(void){return 240;}
static bool poll_input(t5_app_input_t*i,uint32_t t){
 tick+=t;if(!input_mode)return false;input_step++;
 if(input_step==1){scan.count=1;strcpy(scan.devices[0].name,"Navigation fixture");i->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 if(input_step==2){assert(detail&&!back_enabled);i->buttons=T5_APP_BUTTON_BACK;return true;}
 assert(input_step==3&&!detail&&back_enabled);i->exit_requested=true;return true;
}
static void back(bool b){back_enabled=b;}
static bool swipe(t5_app_swipe_t*s){(void)s;return false;}
static void present(bool full){(void)full;}
static bool claim(void*c,uint64_t*out){(void)c;claims++;*out=1;return !claim_bad;}
static int32_t close_host(void*c,uint64_t t){(void)c;assert(t==1);closes++;return close_bad?-1:restore_result;}
static bool send_host(void*c,uint64_t t,uint8_t type,const uint8_t*p,size_t n){(void)c;assert(t==1&&type==1&&n==3u+p[2]);sends++;if(send_bad)return false;uint8_t ack[]={0x0e,4,1,p[0],p[1],0};memcpy(queued,ack,6);queued_size=6;return true;}
static int32_t next_host(void*c,uint64_t t,uint8_t*type,uint8_t*p,size_t cap,size_t*n){(void)c;assert(t==1&&cap>=1028);*n=queued_size;if(!*n)return 0;*type=4;memcpy(p,queued,*n);queued_size=0;return 1;}
static portable_bluetooth_host_v1 fake_host={{1,sizeof(fake_host),NULL,NULL,NULL,NULL,NULL},claim,send_host,next_host,close_host};
static int32_t get(void*c,const char*k,void*data,uint32_t cap,uint32_t*n){uint8_t*v=data;(void)c;assert(!strcmp(k,"quick_radio")&&cap==4);v[0]=0x51;v[1]=1;v[2]=policy;v[3]=(uint8_t)(policy^0xa5);*n=4;return 0;}
static int32_t put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;assert(!"Scanner must not write policy");return -1;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool acquire(const char*n,uint32_t v,uint64_t inst,risc_runtime_capability_v1*g){assert(v==1);if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(inst==1);g->api=&kv;}else{assert(!strcmp(n,"bluetooth.hci")&&inst==16);g->api=&fake_host;}grants++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api&&grants);if(release_bad)return false;grants--;return true;}
static bool diagnostic(const char*s){assert(s);return true;}
static void yield(uint32_t ms){(void)ms;longjmp(retention,1);}
static const t5_app_api_v1 fake_app={.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=screen,.screen_height=screen,.millis=millis,.poll=poll_input,.present=present,.set_back_exits_app=back,.take_touch_swipe=swipe};
static const risc_runtime_api_v1 fake_rt={.api_version=1,.struct_size=sizeof(fake_rt),.acquire=acquire,.release=release,.diagnostic=diagnostic,.yield_ms=yield};
const t5_app_api_v1*t5_app_get_api(uint32_t v){assert(v==1);return &fake_app;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return &fake_rt;}
bool portable_app_sleep_retained(void){return false;}
#ifndef BLE_RENDER
void portable_nova_begin(void){drawn[0]=0;}
void portable_nova_header(const char*s){strcat(drawn,s);strcat(drawn,"\n");}
void portable_nova_text(unsigned f,int x,int y,int w,const char*s,uint32_t color){(void)color;assert(f<=2&&x>=0&&y>=0&&x+w<=240&&y<196);assert(strlen(drawn)+strlen(s)+2<sizeof(drawn));strcat(drawn,s);strcat(drawn,"\n");}
void portable_nova_center(unsigned f,int x,int y,int w,const char*s,uint32_t c){portable_nova_text(f,x,y,w,s,c);}
void portable_nova_fill(int x,int y,int w,int h,uint32_t c){(void)c;assert(x>=0&&y>=0&&x+w<=240&&y+h<=240);}
void portable_nova_round(int x,int y,int w,int h,int radius,uint32_t c){(void)c;(void)radius;assert(x>=0&&y>=0&&x+w<=240&&y+h<=240);}
void portable_nova_button(int x,int y,int w,int h,const char*s,bool b){(void)b;assert(w>=44&&h>=44&&x>=0&&x+w<=240&&y>=0&&y+h<=240);strcat(drawn,s);strcat(drawn,"\n");}
bool portable_nova_hit(int x,int y,int l,int t,int w,int h){return x>=l&&y>=t&&x<l+w&&y<t+h;}
#else
static uint16_t pixels[240*240];
static struct {unsigned frame;void *pixels;size_t stride_bytes;} surface={1,pixels,480};
static bool list_mode;
static int width(void){return 240;}
static int height(void){return 240;}
static void clear_color(uint16_t c){for(unsigned i=0;i<240*240;i++)pixels[i]=c;}
#include "nova_ui.inc"
static void snapshot(const char *name){
 const char*dir=getenv("BLE_FRAME_DIR");if(!dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned i=0;i<240*240;i++){uint16_t v=pixels[i];uint8_t rgb[]={(uint8_t)((v>>11)*255/31),(uint8_t)(((v>>5)&63)*255/63),(uint8_t)((v&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}fclose(f);
}
#endif
static void reset(void){app=&fake_app;runtime=&fake_rt;scan=(ble_scan){0};grant=(risc_runtime_capability_v1){0};host=NULL;token=0;acquired=uncertain=detail=sensors=false;selected=scroll=detail_scroll=0;dirty=true;message=NULL;grants=claims=closes=sends=0;close_bad=send_bad=release_bad=claim_bad=false;restore_result=1;tick=0;policy=3;queued_size=0;input_mode=input_step=0;back_enabled=true;}
#ifdef BLE_RENDER
int main(void){
 reset();message="Enable Bluetooth in controls";draw();snapshot("empty");
 scan.count=8;for(unsigned i=0;i<8;i++){ble_device*d=&scan.devices[i];snprintf(d->name,32,"Sensor %u",i+1);d->rssi=-48-(int)i*6;d->bthome=true;d->bthome_version=2;d->seen=1000;d->measurement_seen=1000;d->service_count=1;d->services[0]=0xfcd2;}
 tick=2000;message="Passive scan - 15 seconds";draw();snapshot("results");
 detail=true;draw();snapshot("detail");detail_scroll=5;scan.devices[0].has_temperature=true;scan.devices[0].temperature=2250;scan.devices[0].has_humidity=true;scan.devices[0].humidity=5340;scan.devices[0].has_battery=true;scan.devices[0].battery=88;draw();snapshot("sensor");
 puts("BLE production Nova pixels rendered");
}
#else
int main(void){
 reset();draw();assert(strstr(drawn,"Tap Scan"));assert(!claims);
 policy=4;start_scan();assert(!claims&&!grants&&strstr(message,"Airplane"));policy=1;start_scan();assert(!claims&&strstr(message,"Enable Bluetooth"));policy=3;
 start_scan();assert(token&&active()&&grants==1);for(unsigned i=0;i<5;i++){tick+=20;pump();}assert(scan.phase==BLE_SCANNING&&sends==5);
 for(unsigned i=0;i<8;i++){scan.devices[i].rssi=-60;snprintf(scan.devices[i].name,32,"Sensor %u",i);}scan.count=8;move(4);assert(selected==4&&scroll==2);draw();assert(strstr(drawn,"Sensor 4"));detail=true;draw();assert(strstr(drawn,"Sensor 4"));move(30);draw();draw();assert(detail_scroll<=32);
 assert(portable_radio_suspend()&&!active()&&!token&&!grants&&closes==1);assert(portable_radio_suspend()&&closes==1);
 start_scan();assert(active());start_scan();assert(!active()&&!grants&&closes==2);
 start_scan();send_bad=true;pump();assert(scan.phase==BLE_ERROR&&!token&&!grants&&strstr(message,"Send"));send_bad=false;
 start_scan();for(unsigned i=0;i<5;i++)pump();tick+=BLE_SCAN_MS;pump();assert(scan.phase==BLE_COMPLETE&&!token&&!grants);
 claim_bad=true;start_scan();assert(!token&&!grants&&strstr(message,"busy"));claim_bad=false;
 fake_host.controls.struct_size=sizeof(portable_bluetooth_control_v1);start_scan();assert(!token&&!grants&&strstr(message,"update"));fake_host.controls.struct_size=sizeof(fake_host);
 start_scan();close_bad=true;unsigned prior=closes;if(!setjmp(retention)){stop();assert(!"Expected retained cleanup");}assert(uncertain&&token&&grants==1&&closes==prior+1);assert(!portable_radio_suspend()&&closes==prior+1);
 reset();start_scan();release_bad=true;if(!setjmp(retention)){stop();assert(!"Expected retained grant");}assert(uncertain&&!token&&grants==1);
 reset();start_scan();restore_result=0;start_scan();assert(!token&&!grants&&restore_failed&&strstr(message,"restore failed"));
 reset();start_scan();restore_result=0;for(unsigned i=0;i<5;i++)pump();tick+=BLE_SCAN_MS;pump();assert(!token&&!grants&&restore_failed&&strstr(message,"restore failed"));
 reset();app_main();assert(!claims&&!grants);
 reset();input_mode=1;app_main();assert(input_step==3&&back_enabled&&!claims&&!grants);
 puts("BLE real app: policy, results/details/scroll, stop/retry, no implicit RF and retained cleanup passed");
}

#endif
