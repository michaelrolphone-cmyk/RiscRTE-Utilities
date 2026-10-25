/* Actual app + System adapter; returning-Light hardware helper is modeled.
 * Existing controller/adapter scenarios compile unchanged without this opt-in. */
#define TEST_FOREGROUND_ALARM
#define main inherited_adapter_main
#include "native_utc_alarm_adapter_test.c"
#undef main
#include "PortableForegroundIdle.h"
static unsigned sleeps,resumes,prepares,snapshot=1,case_id;
static bool planning;static unsigned preserved_draft[3];
static alarm_sleep_v1 expected_ticket;
static int32_t fg_status(void*c,alarm_status_v1*s){int rc=test_status(c,s);s->snapshot=snapshot;s->rtc_seconds=(uint32_t)native_epoch;return rc;}
static int32_t fg_prepare(void*c,alarm_sleep_v1*t){(void)c;assert(!retained_flag);prepares++;if(!planning){planning=true;snapshot++;return ALARM_PENDING;}planning=false;*t=(alarm_sleep_v1){sizeof(*t),snapshot,(uint32_t)native_epoch,(uint32_t)native_epoch+10};expected_ticket=*t;return ALARM_OK;}
static int32_t fg_resume(void*c,const alarm_sleep_v1*t){(void)c;assert(!retained_flag&&!memcmp(t,&expected_ticket,sizeof(*t)));resumes++;return ALARM_OK;}
uint32_t portable_foreground_idle_timeout_ms(void){return sleeps<3 && (case_id==0?editing:writer.uncertain)?40:0;}
int portable_foreground_idle_configure(const risc_runtime_api_v1*r){assert(r&&r->api_version==1);return 0;}
void portable_foreground_idle_timeout_changed(uint32_t ms){(void)ms;assert(0);}
int portable_app_foreground_idle_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){(void)r;(void)d;(void)b;assert(!"Alarm profile must use ticket-aware helper");return -2;}
int portable_app_foreground_idle_sleep_alarm(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const portable_foreground_alarm_hooks*h){
 assert(r&&r->api_version==1&&d&&b&&h&&h->prepare&&h->resume&&!frames&&!subs&&!retained_flag);
 assert(case_id==0?editing:writer.uncertain);
 if(!sleeps&&case_id==0){
#if DAILY_ALARM_KIND == 1
  portable_timezone_civil local={0};assert(portable_timezone_utc_to_local(&native_zone,native_epoch,&local,NULL)==PORTABLE_TIMEZONE_OK);
  unsigned initial_hour=(local.hour+(local.minute==59?1u:0u))%24u;
  assert(values[0]==(initial_hour+1u)%24u&&values[1]==(local.minute+1u)%60u);
#else
  assert(values[0]==1&&values[1]==5&&values[2]==0);
#endif
  printf("Draft after single hour increment: %u:%u:%u; editing=%u writes=%u\n",values[0],values[1],values[2],editing,puts_count);
 }
 if(!sleeps)memcpy(preserved_draft,values,sizeof(preserved_draft));else assert(!memcmp(preserved_draft,values,sizeof(preserved_draft)));
 unsigned draft[3];memcpy(draft,values,sizeof(draft));alarm_writer before=writer;bool before_edit=editing;unsigned writes=puts_count;
 if(case_id==1){assert(!portable_app_before_launch("default.elf"));assert(!launches);}
 uint32_t duration=0;assert(h->prepare(h->context,&duration)==1&&duration==9000);
 ticks+=100;assert(h->resume(h->context)==1);sleeps++;
 assert(before_edit==editing&&!memcmp(draft,values,sizeof(draft))&&!memcmp(&before,&writer,sizeof(writer))&&writes==puts_count);
 return 1;
}
int main(int argc,char**argv){assert(argc==3);directory=argv[1];case_id=(unsigned)atoi(argv[2]);assert(case_id<2);paper_profile=false;memset(pixels,0xa5,sizeof(pixels));stop_poll=500;home_at=400;
 test_alarm.base.status=fg_status;test_alarm.base.prepare_sleep=fg_prepare;test_alarm.features=ALARM_DESCRIPTOR_RESUME_SLEEP;test_alarm.resume_sleep=fg_resume;
 if(case_id==0)touch(20,0,0,50,45);else{kv_io=true;start(20);raw_home_at=60;start(180);}
 assert(!app_module_init());app_main();assert(sleeps==3&&resumes==3&&prepares==6&&!retained_flag&&!reader_live);
 if(!case_id){assert(editing&&!puts_count&&!memcmp(preserved_draft,values,sizeof(preserved_draft)));}else{assert(!writer.uncertain&&puts_count==2);}
 assert(launches==1);app_module_fini();assert(!fixture_grants&&!frames&&!subs);
 printf("PASS alarm app kind=%u case=%u: three modeled returning-Light cycles preserve draft/uncertain-save, exact tickets, Home guard and clean teardown\n",DAILY_ALARM_KIND,case_id);
}
