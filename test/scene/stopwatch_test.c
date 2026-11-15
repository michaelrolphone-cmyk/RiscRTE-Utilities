#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Apps/stopwatch_scene.c"
static risc_runtime_api_v1 runtime,replacement;
static bool replaced;
static risc_scene_api_v1 scene;
static risc_scene_lifecycle_api_v1 scene_life;
static unsigned configured;
static uint32_t snapshot_flags;
static int32_t configure_ui(void*c,uint64_t s,uint32_t f){(void)c;(void)s;(void)f;configured++;return 0;}
static risc_key_value_v1 store;
static risc_realtime_api_v1 realtime;
static twatch_rtc_api_v1 rtc;
static unsigned calls,releases,retained,writes,polls,updates,closes;
static uint32_t mono=100,epoch=1000;
static uint8_t data[2][20];static bool exists[2];
static bool put_fail,put_persist,get_fail,clock_fail,mono_fail,revoke,ui_absent,kv_absent,utc_absent,raw_absent,release_fail;
static int close_result;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!revoke?(replaced?&replacement:&runtime):NULL;}
static bool replacing_bind(risc_resident_client_v1 *c){(void)c;replacement=runtime;replaced=true;return false;}
static bool fake_retain(void){retained++;return true;}
static bool fake_ms(uint32_t *out){calls++;*out=mono;return !mono_fail;}
static void yield(uint32_t ms){mono+=ms;}
static int slot(const char *k){assert(!strcmp(k,"stopwatch")||!strcmp(k,"stopwatch_utc"));return !strcmp(k,"stopwatch_utc");}
static int32_t get(void *c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;calls++;int i=slot(k);*n=0;if(get_fail)return RISC_KEY_VALUE_IO;if(!exists[i])return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=20);memcpy(b,data[i],20);*n=20;return 0;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;calls++;writes++;assert(n==20);int i=slot(k);if(!put_fail||put_persist){memcpy(data[i],b,n);exists[i]=true;}return put_fail?RISC_KEY_VALUE_IO:0;}
static int32_t read_utc(void*c,risc_realtime_snapshot_v1*s){(void)c;calls++;if(clock_fail)return RISC_REALTIME_IO;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=946684800+epoch};return 0;}
static bool read_raw(void*c,twatch_rtc_time_v1*t){(void)c;calls++;*t=(twatch_rtc_time_v1){.year=2000,.month=1,.day=1,.second=(uint8_t)(epoch%60),.minute=(uint8_t)(epoch/60%60),.hour=(uint8_t)(epoch/3600%24)};return !clock_fail;}
static bool acq(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)v;assert(id==(!strcmp(n,"storage.key-value")?2u:0u));calls++;const void*p=NULL;if(!strcmp(n,"ui.scene")&&!ui_absent)p=&scene;if(!strcmp(n,"storage.key-value")&&!kv_absent)p=&store;if(!strcmp(n,"runtime.realtime")&&!utc_absent)p=&realtime;if(!strcmp(n,"rtc.clock")&&!raw_absent)p=&rtc;g->api=p;g->slot=p?1:0;g->generation=p?1:0;return p!=NULL;}
static bool rel(risc_runtime_capability_v1*g){calls++;releases++;if(!release_fail)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return !release_fail;}
static int32_t open_ui(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;assert(d->root==1&&n->depth==1);*s=1;calls++;return 0;}
static int32_t update(void*c,uint64_t s,const risc_scene_document_v1*d){(void)c;(void)s;assert(d->revision>document.revision);updates++;calls++;return 0;}
static int32_t next(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;(void)s;polls++;calls++;*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=document.revision,.sequence=polls};return 0;}
static int32_t close_ui(void*c,uint64_t s){(void)c;(void)s;closes++;calls++;return close_result;}
static int32_t snap_ui(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){(void)c;(void)s;*n=navigation;*f=snapshot_flags;return 0;}
static void setup(void){
 replaced=false;
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acq,.release=rel,.yield_ms=yield,.monotonic_ms=fake_ms,.retain_invocation=fake_retain};
 scene=(risc_scene_api_v1){.api_version=1,.struct_size=sizeof(scene),.open=open_ui,.update=update,.next=next,.snapshot=snap_ui,.close=close_ui};
 store=(risc_key_value_v1){.api_version=1,.struct_size=sizeof(store),.get=get,.put=put};realtime=(risc_realtime_api_v1){.api_version=1,.struct_size=sizeof(realtime),.read=read_utc};rtc=(twatch_rtc_api_v1){.api_version=2,.struct_size=sizeof(rtc),.read=read_raw};
 scene_life=(risc_scene_lifecycle_api_v1){.base=scene,.configure=configure_ui};scene_life.base.struct_size=sizeof(scene_life);configured=0;snapshot_flags=0;resident=(risc_scene_resident_v1){0};lifecycle=NULL;explicit_refresh=false;
 rt=&runtime;ui=&scene;kv=&store;utc=&realtime;raw=&rtc;utc_domain=true;terminal=loaded=uncertain=pending_pause=reset_armed=false;clock_state=(sw_clock){0};document=(risc_scene_document_v1){0};sequence=0;notice[0]=0;mono=100;epoch=1000;calls=releases=retained=writes=polls=updates=closes=0;put_fail=put_persist=get_fail=clock_fail=mono_fail=revoke=ui_absent=kv_absent=utc_absent=raw_absent=release_fail=false;close_result=0;memset(held,0,sizeof(held));memset(exists,0,sizeof(exists));
}
static void action(uint32_t id,uint32_t act){declare(&document);document.revision=1;risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.node=id,.action=act,.sequence=sequence+1,.document_revision=1};(void)handle(&e);}
int main(void){
 setup();sw_record old_raw={123000,0,false,false};sw_encode(&old_raw,data[0]);exists[0]=true;select_initial_domain();restore();assert(!utc_domain&&loaded&&clock_state.elapsed_ms==123000&&writes==0);
 setup();sw_record old_utc={456000,0,false,false};sw_utc_encode(&old_utc,data[1]);exists[1]=true;utc=NULL;select_initial_domain();restore();assert(utc_domain&&loaded&&clock_state.elapsed_ms==456000&&writes==0);
 setup();exists[0]=exists[1]=true;sw_encode(&old_raw,data[0]);sw_utc_encode(&old_utc,data[1]);select_initial_domain();assert(utc_domain);utc=NULL;select_initial_domain();assert(!utc_domain);
 setup();select_initial_domain();assert(utc_domain);utc=NULL;select_initial_domain();assert(!utc_domain);
 setup();exists[0]=true;get_fail=true;select_initial_domain();restore();assert(utc_domain&&!loaded&&writes==0);
 setup();exists[0]=true;memset(data[0],0,20);select_initial_domain();restore();assert(!utc_domain&&!loaded&&writes==0);
 setup();sw_record running_old={321000,1000,true,false};sw_utc_encode(&running_old,data[1]);exists[1]=true;mono_fail=true;restore();assert(!loaded&&clock_state.elapsed_ms==321000);tick();assert(!pending_pause);mono_fail=false;toggle();assert(writes==0);epoch+=2;restore();assert(clock_state.running&&clock_state.elapsed_ms==323000&&clock_state.approximate&&writes==0);

 setup();runtime.resident_shell=replacing_bind;app_main();assert(terminal&&polls==0&&closes==0&&releases==0&&writes==0&&calls==4);
 setup();restore();assert(loaded&&!clock_state.running&&writes==0);toggle();assert(clock_state.running&&writes==1);mono+=1234;epoch++;tick();toggle();assert(clock_state.elapsed_ms==1234&&!clock_state.running&&writes==2);restore();assert(clock_state.elapsed_ms==1234);reset();assert(reset_armed&&writes==2);reset();assert(clock_state.elapsed_ms==0&&writes==3);
 setup();restore();toggle();uint8_t saved[20];memcpy(saved,data[1],20);mono+=1000;epoch++;tick();assert(writes==1);action(6,DOMAIN);assert(!utc_domain&&!memcmp(saved,data[1],20));toggle();assert(clock_state.running&&exists[0]);toggle();action(6,DOMAIN);assert(utc_domain&&clock_state.running&&clock_state.approximate);
 setup();restore();put_fail=true;toggle();assert(uncertain&&!clock_state.running);unsigned w=writes;reset();assert(writes==w);action(6,DOMAIN);assert(utc_domain);put_fail=false;retry_save();assert(!uncertain&&clock_state.running);
 setup();restore();put_fail=put_persist=true;toggle();assert(!uncertain&&clock_state.running);assert(data[1][2]=='U');
 setup();utc_domain=false;restore();toggle();assert(data[0][2]=='0');mono+=2000;epoch+=2;toggle();assert(clock_state.elapsed_ms==2000);
 setup();restore();toggle();mono+=1000;clock_fail=true;tick();assert(!clock_state.running&&pending_pause);toggle();assert(!pending_pause&&!clock_state.clock_changed);assert(writes==2);
 setup();restore();toggle();mono+=1000;epoch+=20;tick();assert(pending_pause&&clock_state.clock_changed);
 setup();restore();clock_state.elapsed_ms=SW_MAX_MS;toggle();assert(!clock_state.running&&writes==0);
 setup();sw_record r={500,1000,true,false};sw_utc_encode(&r,data[1]);exists[1]=true;epoch=999;restore();assert(clock_state.clock_changed&&!clock_state.running);assert(writes==0);
 setup();sw_record p={321,0,false,false};sw_utc_encode(&p,data[1]);exists[1]=true;clock_fail=true;restore();assert(loaded&&clock_state.elapsed_ms==321&&!clock_state.clock_changed);
 setup();memset(data[1],0,20);exists[1]=true;restore();assert(!loaded&&writes==0);toggle();assert(writes==0);reset();reset();assert(loaded);
 setup();restore();toggle();mono=UINT32_MAX-50;clock_state.last_ms=mono;clock_state.ms_anchor=mono;mono=49;sw_tick(&clock_state,mono);assert(clock_state.elapsed_ms==100);
 setup();restore();declare(&document);document.revision=5;risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.node=3,.action=TOGGLE,.sequence=1,.document_revision=4};assert(!handle(&e)&&writes==0);e.document_revision=5;handle(&e);assert(writes==1);handle(&e);assert(writes==1);
 setup();app_main();assert(polls==1&&closes==1&&releases==4&&writes==0);
 setup();ui_absent=true;app_main();assert(polls==0&&releases==0);
 setup();kv_absent=utc_absent=raw_absent=true;app_main();assert(releases==1&&writes==0);
 setup();close_result=RISC_SCENE_AGAIN;app_main();assert(closes==250&&retained==1&&releases==0);
 setup();close_result=RISC_SCENE_RETAINED;app_main();assert(closes==1&&retained==1&&releases==0);
 setup();release_fail=true;app_main();assert(releases==1&&retained==1);
 setup();revoke=true;unsigned c=calls;tick();assert(terminal&&calls==c);
 setup();restore();toggle();mono+=1000;epoch+=20;tick();assert(pending_pause);declare(&document);document.revision=1;risc_scene_event_v1 leave={.struct_size=sizeof(leave),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=1,.sequence=1};assert(!handle(&leave)&&pending_pause);
 setup();sw_record prior={321000,0,false,false};sw_utc_encode(&prior,data[1]);exists[1]=true;mono_fail=true;restore();assert(!loaded&&clock_state.elapsed_ms==321000);tick();assert(!pending_pause);mono_fail=false;toggle();assert(writes==0);restore();toggle();assert(clock_state.elapsed_ms==321000&&writes==1);
 setup();ui=&scene_life.base;lifecycle=&scene_life;restore();toggle();mono+=1000;epoch+=20;tick();assert(pending_pause);declare(&document);document.revision=1;risc_scene_event_v1 reject={.struct_size=sizeof(reject),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=1,.sequence=1};assert(!handle(&reject)&&configured==1&&explicit_refresh);
 puts("Stopwatch shared scene: 31 behavioral/lifecycle scenarios passed");return 0;
}
