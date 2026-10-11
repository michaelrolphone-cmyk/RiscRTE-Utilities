#define PORTABLE_APP_OWNS_TOUCH_CHROME
#define PORTABLE_RETURN_APP "springboard.elf"
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_APP_SLEEP_LOCAL
#include "../Apps/ble_scanner.c"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
static unsigned input_mode,input_step,launches;static bool back_enabled;
static uint32_t tick;static unsigned grants,claims,closes,sends;static bool close_bad,send_bad,release_bad,claim_bad;static int restore_result=1;
static bool allow_write,write_bad,enable_bad,status_bad,rollback_bad;static unsigned controls,writes;static uint8_t enabled;
static uint8_t policy=3;static size_t queued_size;static char drawn[4000];static jmp_buf retention;
static uint32_t millis(void){return tick;}
static int32_t screen(void){return 240;}
static bool poll_input(t5_app_input_t*i,uint32_t t){
 tick+=t;if(!input_mode)return false;input_step++;
 if(input_step==1){scan.count=1;strcpy(scan.devices[0].name,"Navigation fixture");i->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 if(input_step==2){assert(detail&&!back_enabled);i->buttons=T5_APP_BUTTON_BACK;return true;}
 assert(input_step==3&&!detail&&!back_enabled);i->buttons=T5_APP_BUTTON_BACK;return true;
}
static void back(bool b){back_enabled=b;}
static bool contact(t5_app_contact_t*s){*s=(t5_app_contact_t){0};return true;}
static void present(bool full){(void)full;}
static bool set_enabled(void*c,bool on){(void)c;controls++;if((on&&enable_bad)||(!on&&rollback_bad))return false;enabled=on;return true;}
static bool get_status(void*c,uint8_t*out){(void)c;*out=enabled;return !status_bad;}
static portable_bluetooth_control_v1 control={.api_version=1,.struct_size=sizeof(control),.set_enabled=set_enabled,.status=get_status};
static unsigned provider_phase,provider_polls,provider_count;static ble_device provider_devices[BLE_MAX_DEVICES];
static bool provider_restore;static uint8_t name_record[40];static char name_key_saved[16];static bool name_present,name_write_bad;
static bool claim(void*c,uint64_t*out){(void)c;claims++;*out=1;provider_polls=provider_count=0;provider_phase=BLE_STARTING;provider_restore=false;return !claim_bad;}
static bool close_host(void*c,uint64_t t){(void)c;assert(t==1);closes++;if(close_bad)return false;provider_restore=restore_result==0;return true;}
static bool poll_host(void*c,uint64_t t,uint32_t n){(void)c;assert(t==1&&n==6);sends++;if(send_bad){provider_phase=BLE_ERROR;return false;}if(++provider_polls>=5)provider_phase=BLE_SCANNING;if(tick>=15000)provider_phase=BLE_COMPLETE;return true;}
static bool host_status(void*c,risc_ble_sensor_status_v1*out){(void)c;*out=(risc_ble_sensor_status_v1){.struct_size=sizeof(*out),.state=provider_phase,.count=provider_count,.restore_failed=provider_restore};if(send_bad)strcpy(out->error,"Send failed");return true;}
static bool host_device(void*c,uint32_t i,ble_device*out){(void)c;if(i>=provider_count)return false;*out=provider_devices[i];return true;}
static risc_bluetooth_sensors_v1 fake_host={1,sizeof(fake_host),NULL,claim,poll_host,host_status,host_device,close_host};
static int32_t get(void*c,const char*k,void*data,uint32_t cap,uint32_t*n){uint8_t*v=data;(void)c;if(!strcmp(k,"quick_radio")){assert(cap==4);v[0]=0x51;v[1]=1;v[2]=policy;v[3]=(uint8_t)(policy^0xa5);*n=4;return 0;}assert(cap==40);if(!name_present||strcmp(k,name_key_saved))return -1;memcpy(data,name_record,40);*n=40;return 0;}
static int32_t put(void*c,const char*k,const void*v,uint32_t n){(void)c;if(!strcmp(k,"quick_radio")){assert(allow_write&&n==4);writes++;if(write_bad)return RISC_KEY_VALUE_IO;policy=((const uint8_t*)v)[2];return RISC_KEY_VALUE_OK;}assert(n==40);if(name_write_bad)return -5;strcpy(name_key_saved,k);memcpy(name_record,v,40);name_present=true;return 0;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool acquire(const char*n,uint32_t v,uint64_t inst,risc_runtime_capability_v1*g){assert(v==1);if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(inst==1);g->api=&kv;}else if(!strcmp(n,"bluetooth.hci")){assert(inst==16);g->api=&control;}else{assert(!strcmp(n,RISC_BLUETOOTH_SENSORS_CAPABILITY)&&inst==0);g->api=&fake_host;}grants++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api&&grants);if(release_bad)return false;grants--;return true;}
static bool launch(const char*s){assert(!strcmp(s,"springboard.elf")&&!token&&!acquired);launches++;return true;}
static bool diagnostic(const char*s){assert(s);return true;}
static void yield(uint32_t ms){(void)ms;longjmp(retention,1);}
static const t5_app_api_v1 fake_app={.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=screen,.screen_height=screen,.millis=millis,.poll=poll_input,.present=present,.set_back_exits_app=back,.touch_contact=contact};
static const risc_runtime_api_v1 fake_rt={.api_version=1,.struct_size=sizeof(fake_rt),.acquire=acquire,.release=release,.request_launch=launch,.diagnostic=diagnostic,.yield_ms=yield};
const t5_app_api_v1*t5_app_get_api(uint32_t v){assert(v==1);return &fake_app;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return &fake_rt;}
bool portable_app_sleep_retained(void){return false;}
#ifndef BLE_RENDER
void portable_nova_begin(void){drawn[0]=0;}
void portable_nova_header(const char*s){strcat(drawn,s);strcat(drawn,"\n");}
void portable_nova_text(unsigned f,int x,int y,int w,const char*s,uint32_t color){(void)color;assert(f<=2&&x>=0&&y>=0&&x+w<=240&&y<240);assert(strlen(drawn)+strlen(s)+2<sizeof(drawn));strcat(drawn,s);strcat(drawn,"\n");}
void portable_nova_center(unsigned f,int x,int y,int w,const char*s,uint32_t c){portable_nova_text(f,x,y,w,s,c);}
void portable_nova_fill(int x,int y,int w,int h,uint32_t c){(void)c;assert(x>=0&&y>=0&&x+w<=240&&y+h<=240);}
void portable_nova_round(int x,int y,int w,int h,int radius,uint32_t c){(void)c;(void)radius;assert(x>=0&&y>=0&&x+w<=240&&y+h<=240);}
void portable_nova_button(int x,int y,int w,int h,const char*s,bool b){(void)b;assert(w>=44&&h>=44&&x>=0&&x+w<=240&&y>=0&&y+h<=240);strcat(drawn,s);strcat(drawn,"\n");}
bool portable_nova_hit(int x,int y,int l,int t,int w,int h){return x>=l&&y>=t&&x<l+w&&y<t+h;}
#else
static uint16_t pixels[240*240];
static struct {unsigned frame;void *pixels;size_t stride_bytes;} surface={1,pixels,480};
static bool list_mode;
static unsigned surface_format=RISC_DISPLAY_FORMAT_RGB565;
static void np_pixel(int x,int y,uint32_t rgb,uint8_t alpha){(void)x;(void)y;(void)rgb;(void)alpha;assert(!"mono in Watch fixture");}
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
static void reset(void){paper=NULL;paper_enable_needed=false;allow_write=write_bad=enable_bad=status_bad=rollback_bad=false;controls=writes=0;enabled=0;app=&fake_app;runtime=&fake_rt;scan=(ble_scan){0};grant=(risc_runtime_capability_v1){0};host=NULL;token=0;acquired=uncertain=detail=sensors=naming=name_save_failed=false;selected=scroll=detail_scroll=detail_index=0;memset(aliases,0,sizeof(aliases));memset(alias_state,0,sizeof(alias_state));dirty=true;message=NULL;grants=claims=closes=sends=0;close_bad=send_bad=release_bad=claim_bad=false;restore_result=1;tick=0;policy=3;queued_size=0;input_mode=input_step=launches=0;back_enabled=true;provider_count=provider_polls=0;provider_phase=BLE_IDLE;provider_restore=false;name_present=name_write_bad=false;}
#ifndef BLE_RENDER
static springboard_contact paper_contact;
static int32_t paper_width(void){return 480;}
static int32_t paper_height(void){return 800;}
static void paper_begin(void){drawn[0]=0;}
static void paper_text(int x,int y,int w,const char*s,unsigned scale,bool heading,bool black){
 (void)scale;(void)heading;(void)black;assert(x>=0&&y>=0&&x+w<=480&&y<800);
 assert(strlen(drawn)+strlen(s)+2<sizeof(drawn));strcat(drawn,s);strcat(drawn,"\n");
}
static int paper_measure(const char*s,bool h){(void)h;return (int)strlen(s)*10;}
static void paper_get_contact(springboard_contact*c){*c=paper_contact;}
static void paper_fill(int x,int y,int w,int h,bool black){(void)black;assert(x>=0&&y>=0&&x+w<=480&&y+h<=800);}
static bool paper_icon(int x,int y,const char*s,uint8_t size,bool black){(void)x;(void)y;(void)s;(void)size;(void)black;return true;}
static const paper_presentation test_paper={.struct_size=sizeof(test_paper),.begin=paper_begin,.text=paper_text,.measure=paper_measure,.contact=paper_get_contact};
static void paper_tap(int x,int y){t5_app_input_t i={0};paper_contact=(springboard_contact){.down=true,.x=x,.y=y};assert(paper_input(&i));paper_contact.down=false;assert(paper_input(&i));}
static void ordering_regression(bool use_paper){
 reset();t5_app_api_v1 paper_app=fake_app;
 if(use_paper){paper=&test_paper;paper_app.screen_width=paper_width;paper_app.screen_height=paper_height;paper_app.fill_rect=paper_fill;paper_app.draw_icon=paper_icon;app=&paper_app;}
 start_scan();provider_count=8;
 for(unsigned i=0;i<8;i++)provider_devices[i]=(ble_device){.address={(uint8_t)i},.address_type=1,.rssi=-50,.bthome=i!=0,.bthome_version=2};
 provider_devices[0].service_count=1;provider_devices[0].services[0]=0x180f; /* Service-only is not BTHome. */
 provider_devices[2].encrypted=true;
 pump();selected=6;detail_index=6;detail=true;
 assert(ble_alias_save(&kv,1,provider_devices[7].address,"Kitchen sensor"));alias_state[7]=0;
 pump();unsigned list[BLE_MAX_DEVICES];assert(indexes(list)==8&&list[0]==7&&list[selected]==6&&detail_index==6);
 assert(scroll==(use_paper?2u:5u));
 strcpy(aliases[3],"Desk");assert(indexes(list)==8&&list[0]==3&&list[1]==7&&list[2]==0&&list[3]==1);aliases[3][0]=0;
 draw();assert(!strstr(drawn,"Kitchen sensor")); /* Reordering must not change open details. */
 detail=false;sensors=true;assert(indexes(list)==7&&list[0]==7&&list[1]==1&&list[2]==2);
 scroll=selected=0;draw();assert(strstr(drawn,"Kitchen sensor"));assert(strstr(drawn,"Sensors")||strstr(drawn,"SENSORS"));
 if(use_paper){paper_tap(100,150);assert(detail&&detail_index==7);paper_tap(100,730);assert(naming&&!active());paper_tap(140,240);assert(!strcmp(editing,"Kitchen sensorb"));paper_tap(360,600);assert(!naming&&!strcmp(aliases[7],"Kitchen sensorb"));}
 else {detail=true;detail_index=7;begin_name();name_key(2);save_name();assert(!naming&&!strcmp(aliases[7],"Kitchen sensorb"));}
 assert(ble_alias_load(&kv,1,provider_devices[7].address,editing)==1&&!strcmp(editing,"Kitchen sensorb"));
 scan.devices[7].reading_count=3;scan.devices[7].readings[0]=(risc_ble_reading_v1){RISC_TELEMETRY_TEMPERATURE_CENTIC,-123};scan.devices[7].readings[1]=(risc_ble_reading_v1){RISC_TELEMETRY_TEMPERATURE_CENTIC,2250};scan.devices[7].readings[2]=(risc_ble_reading_v1){RISC_TELEMETRY_VOLTAGE_MV,2990};
 detail_scroll=use_paper?0:5;draw();assert(strstr(drawn,"Temperature -1.23 C")&&strstr(drawn,"Temperature 22.50 C")&&strstr(drawn,"Voltage 2990 mV"));
 assert(navigate_back()&&!detail);start_scan();provider_count=1;provider_devices[0]=(ble_device){.address={7},.address_type=1,.bthome=true};pump();assert(scan.count==1&&indexes(list)==1&&!strcmp(aliases[0],"Kitchen sensorb"));stop();
 assert(!grants);paper=NULL;
}
#endif
#ifdef BLE_RENDER
int main(void){
 reset();message="Enable Bluetooth in controls";draw();snapshot("empty");
 scan.count=8;for(unsigned i=0;i<8;i++){ble_device*d=&scan.devices[i];snprintf(d->name,32,"Sensor %u",i+1);d->rssi=-48-(int)i*6;d->bthome=true;d->bthome_version=2;d->seen=1000;d->measurement_seen=1000;d->service_count=1;d->services[0]=0xfcd2;}
 tick=2000;message="Passive scan - 15 seconds";draw();snapshot("results");
 detail=true;draw();snapshot("detail");detail_scroll=5;scan.devices[0].has_temperature=true;scan.devices[0].temperature=2250;scan.devices[0].has_humidity=true;scan.devices[0].humidity=5340;scan.devices[0].has_battery=true;scan.devices[0].battery=88;scan.devices[0].reading_count=3;scan.devices[0].readings[0]=(risc_ble_reading_v1){RISC_TELEMETRY_TEMPERATURE_CENTIC,2250};scan.devices[0].readings[1]=(risc_ble_reading_v1){RISC_TELEMETRY_HUMIDITY_CENTIPERCENT,5340};scan.devices[0].readings[2]=(risc_ble_reading_v1){RISC_TELEMETRY_BATTERY_PERCENT,88};draw();snapshot("sensor");begin_name();strcpy(editing,"Kitchen sensor");draw();snapshot("naming");
 puts("BLE production Nova pixels rendered");
}
#else
int main(void){
 reset();draw();assert(strstr(drawn,"Tap Scan"));assert(!claims);
 policy=4;start_scan();assert(!claims&&!grants&&strstr(message,"Airplane"));policy=1;start_scan();assert(!claims&&strstr(message,"Enable Bluetooth"));policy=3;
 start_scan();assert(token&&active()&&grants==1);for(unsigned i=0;i<5;i++){tick+=20;pump();}assert(scan.phase==BLE_SCANNING&&sends==5);
 for(unsigned i=0;i<8;i++){scan.devices[i].rssi=-60;snprintf(scan.devices[i].name,32,"Sensor %u",i);}scan.count=8;move(4);assert(selected==4&&scroll==2);draw();assert(strstr(drawn,"Sensor 4"));detail=true;detail_index=4;draw();assert(strstr(drawn,"Sensor 4"));move(30);draw();draw();assert(detail_scroll<=32);
 assert(portable_radio_suspend()&&!active()&&!token&&!grants&&closes==1);assert(portable_radio_suspend()&&closes==1);
 start_scan();assert(active());start_scan();assert(!active()&&!grants&&closes==2);
 start_scan();send_bad=true;pump();assert(scan.phase==BLE_ERROR&&!token&&!grants&&strstr(message,"Send"));send_bad=false;
 start_scan();for(unsigned i=0;i<5;i++)pump();tick+=15000;pump();assert(scan.phase==BLE_COMPLETE&&!token&&!grants);
 claim_bad=true;start_scan();assert(!token&&!grants&&strstr(message,"busy"));claim_bad=false;
 fake_host.struct_size=1;start_scan();assert(!token&&!grants&&strstr(message,"update"));fake_host.struct_size=sizeof(fake_host);
 start_scan();close_bad=true;unsigned prior=closes;if(!setjmp(retention)){stop();assert(!"Expected retained cleanup");}assert(uncertain&&token&&grants==1&&closes==prior+1);assert(!portable_radio_suspend()&&closes==prior+1);
 reset();start_scan();release_bad=true;if(!setjmp(retention)){stop();assert(!"Expected retained grant");}assert(uncertain&&!token&&grants==1);
 reset();start_scan();restore_result=0;start_scan();assert(!token&&!grants&&restore_failed&&strstr(message,"restore failed"));
 reset();start_scan();restore_result=0;for(unsigned i=0;i<5;i++)pump();tick+=15000;pump();assert(!token&&!grants&&restore_failed&&strstr(message,"restore failed"));
 reset();app_main();assert(!claims&&!grants);
 reset();input_mode=1;app_main();assert(input_step==3&&back_enabled&&launches==1&&!claims&&!grants);
 reset();scan.count=3;for(unsigned i=0;i<3;i++){scan.devices[i].address[0]=(uint8_t)i;scan.devices[i].bthome=true;}detail=true;detail_index=2;
 begin_name();assert(naming);strcpy(editing,"Kitchen");save_name();assert(!naming&&!strcmp(aliases[2],"Kitchen")&&name_present);
 unsigned list[BLE_MAX_DEVICES];assert(indexes(list)==3&&list[0]==2&&selected==0);assert(detail_index==2);
 memset(aliases,0,sizeof(aliases));load_alias(2);assert(!strcmp(aliases[2],"Kitchen"));
 begin_name();strcpy(editing,"Canceled");assert(navigate_back()&&!naming&&!strcmp(aliases[2],"Kitchen"));
 begin_name();strcpy(editing,"Failed");name_write_bad=true;save_name();assert(naming&&name_save_failed&&!strcmp(aliases[2],"Kitchen"));name_write_bad=false;
 editing[0]=0;save_name();assert(!naming&&!aliases[2][0]);memset(aliases,0,sizeof(aliases));load_alias(2);assert(!aliases[2][0]);
 begin_name();key_page=2;name_key(1);assert(editing[0]=='a');name_key(PWK_DELETE);assert(!editing[0]);name_key(PWK_PAGE);assert(key_page==0);draw();assert(strstr(drawn,"SENSOR NAME"));
 naming=false;detail=false;sensors=true;scan.devices[1].bthome=false;assert(indexes(list)==2);
 reset();start_scan();for(unsigned i=0;i<5;i++)pump();provider_count=2;provider_devices[0]=(ble_device){.address={1},.address_type=1};provider_devices[1]=(ble_device){.address={2},.address_type=1};
 assert(ble_alias_save(&kv,1,provider_devices[1].address,"Named"));pump();assert(indexes(list)==2&&list[0]==1&&!strcmp(aliases[1],"Named"));stop();
 static const paper_presentation paper_stub={.struct_size=sizeof(paper_stub)};
 reset();paper=&paper_stub;policy=1;allow_write=true;paper_enable();assert(policy==3&&enabled==1&&controls==1&&writes==1&&!grants&&!claims&&!sends&&!paper_enable_needed);
 reset();paper=&paper_stub;policy=PORTABLE_RADIO_AIRPLANE;allow_write=true;paper_enable();assert(policy==4&&!controls&&!writes&&!grants&&!claims);
 reset();paper=&paper_stub;policy=PORTABLE_RADIO_WIFI|PORTABLE_RADIO_PREVIOUS_BLUETOOTH;allow_write=true;paper_enable();assert(policy==19&&enabled==1&&!grants);
 reset();paper=&paper_stub;policy=1;allow_write=true;write_bad=true;paper_enable();assert(policy==1&&!enabled&&controls==2&&writes==2&&!grants&&!claims);
 reset();paper=&paper_stub;policy=1;allow_write=true;enable_bad=true;paper_enable();assert(policy==1&&!enabled&&controls==2&&!writes&&!grants);
 reset();paper=&paper_stub;policy=1;allow_write=true;status_bad=true;paper_enable();assert(!controls&&!writes&&!grants);
 reset();paper=&paper_stub;policy=1;allow_write=true;enable_bad=rollback_bad=true;if(!setjmp(retention)){paper_enable();assert(!"Expected enable rollback retention");}assert(uncertain&&grants==2&&!claims);
 reset();paper=&paper_stub;policy=1;allow_write=true;release_bad=true;if(!setjmp(retention)){paper_enable();assert(!"Expected enable grant retention");}assert(uncertain&&grants==2&&!claims);
 ordering_regression(false);ordering_regression(true);
 puts("BLE real app: policy, results/details/scroll, stop/retry, no implicit RF and retained cleanup passed");
}

#endif
