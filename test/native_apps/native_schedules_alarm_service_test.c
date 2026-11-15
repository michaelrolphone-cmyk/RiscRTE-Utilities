#define ALARM_SERVICE_TAGGED_V2
#define ALARM_NATIVE_UTC
#define ALARM_VISUAL_ONLY
#define ALARM_DND_CONTROL
#define ALARM_NATIVE_SCHEDULES_ONLY
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../Services/alarm_service/service.c"
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
static uint8_t blobs[10][EDIT_BYTES];static uint32_t sizes[10];
#define TEST_KEY_COUNT 10
#else
static uint8_t blobs[9][64];static uint32_t sizes[9];
#define TEST_KEY_COUNT 9
#endif
static uint64_t ms;static int64_t epoch;static unsigned get_count,put_count,reads,clocks;
static int read_result,get_result,put_result;static bool valid=true,forbid,put_commits=true;
static unsigned malformed;
static const alarm_service_v1 *client;static const risc_driver_v2 *provider;
static const char *const keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,
    ALARM_TIMER_OCCURRENCE_KEY,"points_utc_cfg","points_utc_occ",ALARM_DND_KEY,"time_zone"
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
,ALARM_CLOCK_EDIT_KEY
#endif
};
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
static void clock_edit_test_cut(unsigned stage);
#endif
static int index_key(const char *key){for(int i=0;i<TEST_KEY_COUNT;i++)if(!strcmp(keys[i],key))return i;assert(!"legacy/unbound key access");return 0;}
static int32_t get_blob(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){assert(c==blobs&&!forbid);assert(strcmp(k,"points_utc_cfg")&&strcmp(k,"points_utc_occ")&&strcmp(k,"time_zone"));get_count++;if(get_result){if(get_result==99||get_result==-99)forbid=true;return get_result;}int i=index_key(k);*n=0;if(!sizes[i])return -1;if(cap<sizes[i])return -2;memcpy(b,blobs[i],sizes[i]);*n=sizes[i];
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
if(i==9)clock_edit_test_cut(2);
#endif
return 0;}
static int32_t put_blob(void*c,const char*k,const void*b,uint32_t n){assert(c==blobs&&!forbid);int i=index_key(k);assert(i==3||i==4
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
||i==9
#endif
);put_count++;
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
if(i==9)clock_edit_test_cut(0);
#endif
if(put_result==RISC_BOUND_KEY_VALUE_CONTEXT)return put_result;
if(put_commits){memcpy(blobs[i],b,n);sizes[i]=n;
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
if(i==9)clock_edit_test_cut(1);
#endif
}if(put_result==99||put_result==-99||put_result==RISC_BOUND_KEY_VALUE_NOT_FOUND||put_result==RISC_BOUND_KEY_VALUE_BUFFER_SMALL)forbid=true;return put_result;}
static uint64_t mono(void*c){assert(c==&ms&&!forbid);clocks++;return ms;}
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){assert(c==&epoch&&!forbid&&out->struct_size==sizeof(*out));reads++;if(read_result){if(read_result==99||read_result==-99)forbid=true;return read_result;}
 *out=(risc_realtime_snapshot_v1){sizeof(*out),valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,valid?epoch+(int64_t)(ms/1000):0,0,0,ms*1000,ms*1000};
 if(malformed==1)out->nanoseconds=1000000000u;
 if(malformed==2)out->reserved=1;
 if(malformed==3)out->validity=2;
 if(malformed==4)out->struct_size=0;
 if(malformed==5){out->monotonic_before_us=1;out->monotonic_after_us=0;}return 0;}
static const risc_bound_key_value_v1 bound={
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
2,
#else
1,
#endif
sizeof(bound),blobs,get_blob,put_blob};
static const risc_platform_clock_api_v1 clock_table={1,sizeof(clock_table),&ms,mono,NULL};
static const risc_platform_realtime_api_v1 real_table={1,sizeof(real_table),&epoch,native_read};
static const risc_provider_dependency_v1 deps[]={{"storage.key-value.bound",
#ifdef ALARM_SERVICE_CLOCK_EDIT_V2
2,
#else
1,
#endif
&bound},{"platform.clock",1,&clock_table},{"platform.realtime",1,&real_table}};
static uint32_t date(unsigned y,unsigned m,unsigned d,unsigned h,unsigned min,unsigned sec){uint32_t value;assert(alarm_calendar_seconds(y,m,d,h,min,sec,&value));return value;}
static void boot(bool clear){if(provider)assert(provider->quiesce());if(clear){memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;}read_result=get_result=put_result=0;valid=put_commits=true;forbid=false;malformed=0;
 provider=t5_driver_get(2);client=provider->capability;assert(provider->start(deps,3));get_count=put_count=reads=clocks=0;}
static alarm_status_v1 snapshot(void){unsigned before=get_count+put_count+reads+clocks;alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==0);assert(before==get_count+put_count+reads+clocks);return s;}
static void tick(void){client->step(NULL);ms++;assert(snapshot().mode==ALARM_MODE_VISUAL);}
static void until(unsigned state){for(unsigned i=0;i<200;i++){if(snapshot().state==state)return;tick();}fprintf(stderr,"state=%u wanted=%u error=%d phase=%d\n",snapshot().state,state,error,phase);assert(0);}
static void playing(void){until(ALARM_STATE_ALERT);for(unsigned i=0;i<40&&phase!=PLAYING;i++)tick();assert(phase==PLAYING);}

static alarm_sleep_v1 plan(void){alarm_sleep_v1 p={.struct_size=sizeof(p)};for(unsigned i=0;i<200;i++){int r=client->prepare_sleep(NULL,&p);if(!r)return p;assert(r==ALARM_PENDING);tick();}assert(0);return p;}
static void ack(void){alarm_token_v1 t=snapshot().occurrence;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(client->acknowledge(NULL,&t)==ALARM_OK);}
static void setup(void){epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);}
static void schedule(unsigned kind,unsigned ahead){uint32_t now=(uint32_t)(epoch-946684800);alarm_config c={1,now+ahead,now-10,kind==2?ahead+10:0,(uint8_t)kind,1};assert(alarm_config_valid(&c));alarm_config_encode(&c,blobs[kind-1]);sizes[kind-1]=32;}
static void unselected_points(void){
 for(unsigned present=0;present<2;present++){
  setup();if(present){memset(blobs[5],0x91,64);memset(blobs[6],0x38,64);sizes[5]=sizes[6]=64;}
  uint8_t saved[2][64];memcpy(saved,blobs+5,sizeof(saved));until(ALARM_STATE_READY);
  assert(plan().deadline==0&&!put_count);ms=86400000;assert(alarm_service_resume(client,&(alarm_sleep_v1){.struct_size=sizeof(alarm_sleep_v1)})==ALARM_STALE);
  client->refresh(NULL);until(ALARM_STATE_READY);assert(plan().deadline==0&&!put_count);assert(!memcmp(saved,blobs+5,sizeof(saved)));
 }
}
static void due_and_resume(void){
 for(unsigned kind=1;kind<=2;kind++){
  setup();schedule(kind,10);alarm_sleep_v1 ticket=plan(),bad=ticket;assert(ticket.deadline==date(2026,10,5,12,0,10));
  unsigned before=reads;bad.snapshot++;assert(alarm_service_resume(client,&bad)==ALARM_STALE&&reads==before);
  ms=10000;assert(alarm_service_resume(client,&ticket)==ALARM_OK);assert(alarm_service_resume(client,&ticket)==ALARM_STALE);playing();assert(snapshot().occurrence.kind==kind&&put_count==1);
  alarm_token_v1 old=snapshot().occurrence;boot(false);playing();assert(snapshot().occurrence.generation==old.generation+1);assert(client->acknowledge(NULL,&old)==ALARM_STALE);ack();
  unsigned pc=put_count;boot(false);until(ALARM_STATE_READY);assert(!put_count&&plan().deadline==0&&pc);assert(occurrences[kind-1].state==ALARM_OCC_ACKED);
 }
 setup();schedule(2,10);alarm_sleep_v1 ticket=plan();alarm_config cancelled={2,0, date(2026,10,5,12,0,0),0,2,0};alarm_config_encode(&cancelled,blobs[1]);client->refresh(NULL);assert(alarm_service_resume(client,&ticket)==ALARM_STALE);until(ALARM_STATE_READY);assert(plan().deadline==0);
}
static void errors_and_dnd(void){
 setup();valid=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!put_count);
 for(unsigned i=1;i<=5;i++){setup();malformed=i;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!put_count);}
 setup();schedule(1,0);put_result=RISC_BOUND_KEY_VALUE_IO;put_commits=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE&&!active);
 put_result=0;put_commits=true;client->refresh(NULL);playing();ack();
 setup();schedule(2,0);put_result=RISC_BOUND_KEY_VALUE_IO;playing();assert(put_count);put_result=0;ack();
 setup();schedule(1,0);playing();blobs[7][0]=1;sizes[7]=1;for(unsigned i=0;i<20;i++)tick();assert(active&&occurrences[0].silenced);ack();
 setup();until(ALARM_STATE_READY);epoch-=100;ms+=1000;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);
}
static void retention(unsigned route,int injected){pid_t child=fork();assert(child>=0);if(!child){setup();schedule(1,0);
 if(route==3){boot(true);alarm_sleep_v1 p=plan();read_result=injected?injected:RISC_REALTIME_CONTEXT;assert(alarm_service_resume(client,&p)==ALARM_RETAINED);}else if(route==0)read_result=injected?injected:RISC_REALTIME_CONTEXT;else if(route==1)get_result=injected?injected:RISC_BOUND_KEY_VALUE_CONTEXT;else put_result=injected?injected:RISC_BOUND_KEY_VALUE_CONTEXT;
 until(ALARM_STATE_BLOCKED);assert(error==ALARM_RETAINED);unsigned before=get_count+put_count+reads+clocks;forbid=true;alarm_token_v1 t=snapshot().occurrence;alarm_sleep_v1 p={.struct_size=sizeof(p)};
 for(unsigned i=0;i<3;i++){assert(client->step(NULL)==ALARM_RETAINED);assert(client->refresh(NULL)==ALARM_RETAINED);assert(client->prepare_sleep(NULL,&p)==ALARM_RETAINED);assert(alarm_service_resume(client,&p)==ALARM_RETAINED);assert(client->acknowledge(NULL,&t)==ALARM_RETAINED);assert(client->stop_only(NULL)==ALARM_RETAINED);assert(!provider->quiesce());provider->stop();assert(!provider->start(deps,3));snapshot();}
 assert(before==get_count+put_count+reads+clocks);_exit(0);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}
static void typed_refusals(void){
 const int clean[]={RISC_BOUND_KEY_VALUE_BUSY,RISC_BOUND_KEY_VALUE_INVALID};
 for(unsigned i=0;i<2;i++){
  setup();schedule(1,0);while(phase!=WRITE_OCC)tick();unsigned before=get_count;put_result=clean[i];put_commits=false;tick();
  assert(error==ALARM_STORAGE&&!custody_retained&&!active&&get_count==before&&put_count==1);
  put_result=0;put_commits=true;client->refresh(NULL);playing();ack();
  setup();get_result=clean[i];until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE&&!custody_retained&&!put_count);
 }
 for(unsigned i=0;i<2;i++){setup();read_result=i?RISC_REALTIME_IO:RISC_REALTIME_INVALID;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!custody_retained&&!put_count);}
}
int main(void){unselected_points();due_and_resume();errors_and_dnd();typed_refusals();for(unsigned i=0;i<4;i++){retention(i,0);retention(i,-99);retention(i,99);}retention(2,RISC_BOUND_KEY_VALUE_NOT_FOUND);retention(2,RISC_BOUND_KEY_VALUE_BUFFER_SMALL);assert(provider->quiesce());puts("Schedules-only API2 visual: absent/populated Points untouched; six-key policy; due/replay/ACK/cancel/ticket/UNSET/DND/retained boundaries PASS; no hardware wake integration");return 0;}
