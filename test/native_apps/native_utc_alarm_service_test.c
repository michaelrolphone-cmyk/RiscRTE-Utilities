#define ALARM_SERVICE_TAGGED_V2
#define ALARM_NATIVE_UTC
#define ALARM_VISUAL_ONLY
#define ALARM_DND_CONTROL
#define POINTS_IN_TIME_SERVICE
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../Services/alarm_service/service.c"
static uint8_t blobs[9][64];static uint32_t sizes[9];
static uint64_t ms;static int64_t epoch;static unsigned get_count,put_count,reads,clocks;
static int read_result,get_result,put_result;static bool valid=true,forbid,put_commits=true;
static unsigned malformed;
static const alarm_service_v1 *client;static const risc_driver_v2 *provider;
static const char *const keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,
    ALARM_TIMER_OCCURRENCE_KEY,POINTS_CONFIG_KEY,POINTS_OCCURRENCE_KEY,ALARM_DND_KEY,"time_zone"};
static int index_key(const char *key){for(int i=0;i<9;i++)if(!strcmp(keys[i],key))return i;assert(!"legacy/unbound key access");return 0;}
static int32_t get_blob(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){assert(c==blobs&&!forbid);get_count++;if(get_result)return get_result;int i=index_key(k);*n=0;if(!sizes[i])return -1;if(cap<sizes[i])return -2;memcpy(b,blobs[i],sizes[i]);*n=sizes[i];return 0;}
static int32_t put_blob(void*c,const char*k,const void*b,uint32_t n){assert(c==blobs&&!forbid);int i=index_key(k);assert(i==3||i==4||i==6);put_count++;if(put_result==RISC_BOUND_KEY_VALUE_CONTEXT)return put_result;if(put_commits){memcpy(blobs[i],b,n);sizes[i]=n;}return put_result;}
static uint64_t mono(void*c){assert(c==&ms&&!forbid);clocks++;return ms;}
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){assert(c==&epoch&&!forbid&&out->struct_size==sizeof(*out));reads++;if(read_result)return read_result;
 *out=(risc_realtime_snapshot_v1){sizeof(*out),valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,valid?epoch+(int64_t)(ms/1000):0,0,0,ms*1000,ms*1000};
 if(malformed==1)out->nanoseconds=1000000000u;
 if(malformed==2)out->reserved=1;
 if(malformed==3)out->validity=2;
 if(malformed==4)out->struct_size=0;
 if(malformed==5){out->monotonic_before_us=1;out->monotonic_after_us=0;}return 0;}
static const risc_bound_key_value_v1 bound={1,sizeof(bound),blobs,get_blob,put_blob};
static const risc_platform_clock_api_v1 clock_table={1,sizeof(clock_table),&ms,mono,NULL};
static const risc_platform_realtime_api_v1 real_table={1,sizeof(real_table),&epoch,native_read};
static const risc_provider_dependency_v1 deps[]={{"storage.key-value.bound",1,&bound},{"platform.clock",1,&clock_table},{"platform.realtime",1,&real_table}};
static uint32_t date(unsigned y,unsigned m,unsigned d,unsigned h,unsigned min,unsigned sec){uint32_t value;assert(alarm_calendar_seconds(y,m,d,h,min,sec,&value));return value;}
static void boot(bool clear){if(provider)assert(provider->quiesce());if(clear){memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;}read_result=get_result=put_result=0;valid=put_commits=true;forbid=false;malformed=0;
 provider=t5_driver_get(2);client=provider->capability;assert(provider->start(deps,3));get_count=put_count=reads=clocks=0;}
static alarm_status_v1 snapshot(void){unsigned before=get_count+put_count+reads+clocks;alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==0);assert(before==get_count+put_count+reads+clocks);return s;}
static void tick(void){client->step(NULL);ms++;assert(snapshot().mode==ALARM_MODE_VISUAL);}
static void until(unsigned state){for(unsigned i=0;i<200;i++){if(snapshot().state==state)return;tick();}fprintf(stderr,"state=%u wanted=%u error=%d phase=%d\n",snapshot().state,state,error,phase);assert(0);}
static void playing(void){until(ALARM_STATE_ALERT);for(unsigned i=0;i<40&&phase!=PLAYING;i++)tick();assert(phase==PLAYING);}
static void save(points_config c){assert(points_config_valid(&c));points_config_encode(&c,blobs[5]);sizes[5]=64;}
static points_config schedule(unsigned hour,unsigned minute,unsigned mode){points_config c={.revision=1,.created=(uint32_t)(epoch-946684800)-3600};c.points[0]=(points_item){POINTS_BREAK,1,(uint8_t)mode,127,(uint8_t)hour,(uint8_t)minute,30,1,1};return c;}
static void zone(const char*id){assert(portable_timezone_find(id,strlen(id)+1)>=0);memset(blobs[8],0,64);blobs[8][0]='T';blobs[8][1]='Z';blobs[8][2]=1;memcpy(blobs[8]+4,id,strlen(id));uint8_t sum=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)sum^=blobs[8][i];blobs[8][3]=sum;sizes[8]=44;}
static alarm_sleep_v1 plan(void){alarm_sleep_v1 p={.struct_size=sizeof(p)};for(unsigned i=0;i<200;i++){int r=client->prepare_sleep(NULL,&p);if(!r)return p;assert(r==ALARM_PENDING);tick();}assert(0);return p;}
static void ack(void){alarm_token_v1 t=snapshot().occurrence;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(client->acknowledge(NULL,&t)==ALARM_OK);}
static void clean_schedule(void){save((points_config){.revision=1});}
static void pure_projection(void){uint32_t raw;int64_t unix_time;assert(points_utc_from_unix(INT32_MAX,&raw)&&raw==ALARM_RTC_MAX);assert(points_utc_to_unix(raw,&unix_time)&&unix_time==INT32_MAX);assert(!points_utc_from_unix(INT64_C(2147483648),&raw)&&!points_utc_to_unix(ALARM_RTC_MAX+1,&unix_time));assert(portable_timezone_count()==419);portable_timezone_rule rule;assert(portable_timezone_resolve("America/Denver",15,&rule)==0);
 points_config c={.revision=1};c.points[0]=(points_item){POINTS_BREAK,1,3,127,2,30,15,1,1};points_event e;uint32_t flags=0;
 assert(!points_utc_event_for_day(&rule,&c,0,date(2026,3,8,0,0,0)/86400+1,0,&e,&flags)&&flags==POINTS_FLAG_GAP);
 c.points[0].hour=1;c.points[0].minute=30;flags=0;
 assert(!points_utc_event_for_day(&rule,&c,0,date(2026,11,1,0,0,0)/86400+1,0,&e,&flags)&&flags==POINTS_FLAG_FOLD);
 c.points[0].minute=55;uint32_t day=date(2026,3,8,0,0,0)/86400+1;
 assert(points_utc_event_for_day(&rule,&c,0,day,0,&e,NULL));uint32_t start=e.deadline;
 assert(points_utc_event_for_day(&rule,&c,0,day,1,&e,NULL)&&e.deadline==start+900);
 portable_timezone_civil end;assert(portable_timezone_utc_to_local(&rule,INT64_C(946684800)+e.deadline,&end,NULL)==0&&end.hour==3&&end.minute==10);
 points_projection projection;assert(points_utc_project(&rule,&c,start-1,&projection)&&projection.count==4&&projection.next[0].deadline==start);
 c.points[0].hour=3;c.points[0].minute=14;assert(portable_timezone_resolve("UTC",4,&rule)==0);flags=0;
 assert(!points_utc_event_for_day(&rule,&c,0,date(2038,1,19,0,0,0)/86400+1,0,&e,&flags)&&flags==POINTS_FLAG_RANGE);
}
static void cold_and_bounds(void){epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);valid=false;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&put_count==0);alarm_sleep_v1 p={.struct_size=sizeof(p)};assert(client->prepare_sleep(NULL,&p)==ALARM_RTC);
 valid=true;clean_schedule();assert(client->refresh(NULL)==ALARM_PENDING);until(ALARM_STATE_READY);assert(plan().deadline==0);
 for(unsigned i=1;i<=5;i++){boot(true);clean_schedule();malformed=i;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!put_count);}
 const int64_t bad[]={946684799,2147483648,INT64_MAX,-1};for(unsigned i=0;i<4;i++){boot(true);clean_schedule();epoch=bad[i];until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC&&!put_count);}
 boot(true);clean_schedule();epoch=INT32_MAX;until(ALARM_STATE_READY);assert(snapshot().rtc_seconds==ALARM_RTC_MAX&&plan().deadline==0);
 epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);zone("America/Denver");blobs[8][43]=1;until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE&&!put_count);
}
static void alerts_and_zone(void){for(unsigned mode=1;mode<=3;mode++){epoch=INT64_C(946684800)+date(2026,10,5,18,0,0);boot(true);zone("America/Denver");save(schedule(12,0,mode));playing();alarm_token_v1 old=snapshot().occurrence;assert(points_occ.mode==mode&&points_occ.timezone_index==timezone_index);
 // A zone edit and reboot preserve the already durable absolute occurrence.
 zone("Asia/Tokyo");boot(false);playing();assert(snapshot().occurrence.deadline==old.deadline&&snapshot().occurrence.generation==old.generation+1);assert(client->acknowledge(NULL,&old)==ALARM_STALE);ack();
 unsigned generation=points_occ.generation;boot(false);until(ALARM_STATE_READY);assert(points_occ.generation==generation&&!put_count);assert(plan().deadline>snapshot().rtc_seconds);
 }
 epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);save(schedule(12,0,3));playing();unsigned pc=put_count;for(unsigned i=0;i<10;i++)tick();assert(put_count==pc);ms+=ALARM_INVOCATION_MS;until(ALARM_STATE_READY);assert(points_occ.state==ALARM_OCC_ACKED);assert(plan().deadline==date(2026,10,5,12,27,0));
 // Light-return reconciliation and fresh deep-wake replay keep elapsed deadlines.
 ms=27*60*1000;until(ALARM_STATE_ALERT);playing();assert(points_occ.edge==POINTS_EDGE_WARNING);ack();assert(plan().deadline==date(2026,10,5,12,30,0));ms=30*60*1000;boot(false);playing();assert(points_occ.edge==POINTS_EDGE_END);ack();
 // Late startup compacts without surfacing an old alert.
 boot(true);save(schedule(12,0,3));ms=61000;until(ALARM_STATE_READY);assert(!active&&points_occ.delivered[0]&1);
 // Change the zone for future starts: no stale sleeping deadline survives.
 boot(true);epoch=INT64_C(946684800)+date(2026,10,5,10,0,0);save(schedule(12,0,3));assert(plan().deadline==date(2026,10,5,12,0,0));zone("America/Denver");client->refresh(NULL);until(ALARM_STATE_READY);assert(plan().deadline==date(2026,10,5,18,0,0));
}
static void ordinary_and_failures(void){for(unsigned kind=1;kind<=2;kind++){epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);clean_schedule();uint32_t now=(uint32_t)(epoch-946684800);alarm_config c={1,now,now-10,kind==2?10:0,(uint8_t)kind,1};alarm_config_encode(&c,blobs[kind-1]);sizes[kind-1]=32;playing();assert(snapshot().occurrence.kind==kind);ack();assert(occurrences[kind-1].state==ALARM_OCC_ACKED);}
 boot(true);save(schedule(12,0,3));put_commits=false;put_result=RISC_BOUND_KEY_VALUE_IO;until(ALARM_STATE_BLOCKED);assert(error==ALARM_STORAGE&&!active);put_commits=true;put_result=0;client->refresh(NULL);playing();
 // Foreground stop is storage/time free and cannot acknowledge by itself.
 unsigned before=get_count+put_count+reads+clocks;forbid=true;assert(client->stop_only(NULL)==ALARM_PENDING);assert(client->stop_only(NULL)==ALARM_PENDING);assert(client->stop_only(NULL)==ALARM_OK);assert(before==get_count+put_count+reads+clocks);assert(snapshot().error==ALARM_FOREGROUND);assert(client->step(NULL)==ALARM_FOREGROUND);forbid=false;ack();
 // IO-after-commit is reconciled by exact bytes; write success alone is unused.
 boot(true);save(schedule(12,0,3));put_result=RISC_BOUND_KEY_VALUE_IO;playing();assert(put_count);put_result=0;ack();
 // DND, cancellation before activation, and backward awake time remain guarded.
 boot(true);save(schedule(12,0,3));playing();blobs[7][0]=1;sizes[7]=1;for(unsigned i=0;i<20;i++)tick();assert(points_occ.silenced&&active);ack();
 boot(true);points_config c=schedule(12,0,3);save(c);while(phase!=START_AUDIO)tick();c.revision++;c.points[0].enabled=0;save(c);tick();until(ALARM_STATE_READY);assert(!active);
 boot(true);clean_schedule();until(ALARM_STATE_READY);epoch-=100;ms+=1000;until(ALARM_STATE_BLOCKED);assert(error==ALARM_RTC);client->refresh(NULL);until(ALARM_STATE_READY);
}
static void retention(unsigned route){pid_t child=fork();assert(child>=0);if(child==0){epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);save(schedule(12,0,3));if(route==3){clean_schedule();alarm_sleep_v1 ticket=plan();read_result=RISC_REALTIME_CONTEXT;assert(alarm_service_resume(client,&ticket)==ALARM_RETAINED);}else if(route==0)read_result=RISC_REALTIME_CONTEXT;else if(route==1)get_result=RISC_BOUND_KEY_VALUE_CONTEXT;else put_result=RISC_BOUND_KEY_VALUE_CONTEXT;
 until(ALARM_STATE_BLOCKED);assert(error==ALARM_RETAINED);unsigned before=get_count+put_count+reads+clocks;forbid=true;alarm_status_v1 s=snapshot();assert(s.output_uncertain&&s.error==ALARM_RETAINED);alarm_sleep_v1 p={.struct_size=sizeof(p)};alarm_token_v1 t=s.occurrence;
 for(unsigned i=0;i<3;i++){assert(alarm_service_resume(client,&p)==ALARM_RETAINED);assert(client->step(NULL)==ALARM_RETAINED);assert(client->refresh(NULL)==ALARM_RETAINED);assert(client->acknowledge(NULL,&t)==ALARM_RETAINED);assert(client->prepare_sleep(NULL,&p)==ALARM_RETAINED);assert(client->stop_only(NULL)==ALARM_RETAINED);assert(!provider->quiesce());provider->stop();assert(!provider->start(deps,3));snapshot();}
 assert(before==get_count+put_count+reads+clocks);_exit(0);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}
static void checked_native_resume(void) {
 epoch=INT64_C(946684800)+date(2026,10,5,12,0,0);boot(true);clean_schedule();
 alarm_sleep_v1 ticket=plan(),wrong=ticket;unsigned before=reads;
 wrong.snapshot++;assert(alarm_service_resume(client,&wrong)==ALARM_STALE&&reads==before);
 epoch+=3;ms+=300000;assert(alarm_service_resume(client,&ticket)==ALARM_OK&&reads==before+1);
 assert(alarm_service_resume(client,&ticket)==ALARM_STALE);until(ALARM_STATE_READY);
 ticket=plan();client->refresh(NULL);before=reads;
 assert(alarm_service_resume(client,&ticket)==ALARM_STALE&&reads==before);until(ALARM_STATE_READY);
 ticket=plan();valid=false;assert(alarm_service_resume(client,&ticket)==ALARM_RTC);
 assert(alarm_service_resume(client,&ticket)==ALARM_STALE);valid=true;client->refresh(NULL);until(ALARM_STATE_READY);
 ticket=plan();epoch-=10;assert(alarm_service_resume(client,&ticket)==ALARM_RTC);
}
int main(void){pure_projection();cold_and_bounds();checked_native_resume();alerts_and_zone();ordinary_and_failures();for(unsigned i=0;i<4;i++)retention(i);assert(provider->quiesce());puts("Native UTC visual: timezone, DST, 2038, replay, sleep, foreground and terminal custody passed");return 0;}
