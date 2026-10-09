#define POINTS_CATALOG_SERVICE
#define POINTS_IN_TIME_SERVICE
#define ALARM_DND_CONTROL
#ifdef ALARM_NATIVE_UTC
#define ALARM_SERVICE_TAGGED_V2
#define ALARM_VISUAL_ONLY
#else
#define ALARM_VOLUME_CONTROL
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
static bool allocation_denied;
static void *catalog_alloc(size_t n){return allocation_denied?NULL:malloc(n);}
#define POINTS_CATALOG_ALLOC catalog_alloc
#define POINTS_CATALOG_FREE free
#include "../../Services/alarm_service/service.c"
typedef struct {uint8_t *data;uint32_t size;uint64_t revision;} file;
static file files[2];static uint8_t blobs[12][64];static uint32_t sizes[12];
static uint64_t ms;static uint32_t base;static unsigned io,put_count,opens,writes,effects,closes,stops,silences;
static int replace_result,read_result,stat_result,kv_result;static bool persist_replace=true,close_failed;
static const alarm_service_v1 *client;static const risc_driver_v2 *provider;
static unsigned key_index(const char *key) {const char *keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,
    ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,POINTS_CONFIG_KEY,POINTS_OCCURRENCE_KEY,"alarm_volume","alert_dnd",POINTS_META_KEY,"time_zone","unused"};
    for(unsigned i=0;i<12;i++)if(!strcmp(key,keys[i]))return i;
    assert(0);return 0;}
static int32_t get(void *c,const char *k,void *b,uint32_t cap,uint32_t *n) {(void)c;io++;*n=0;
    if(kv_result)return kv_result;
    unsigned i=key_index(k);if(!sizes[i])return RISC_BOUND_KEY_VALUE_NOT_FOUND;
    assert(cap>=sizes[i]);memcpy(b,blobs[i],sizes[i]);*n=sizes[i];return 0;}
static int32_t put(void *c,const char *k,const void *b,uint32_t n) {(void)c;io++;unsigned i=key_index(k);
    assert(i==3||i==4);put_count++;memcpy(blobs[i],b,n);sizes[i]=n;return 0;}
static unsigned replacements;
static unsigned file_index(const char *name){if(!strcmp(name,POINTS_CATALOG_FILE))return 0;assert(!strcmp(name,POINTS_LEDGER_FILE));return 1;}
static int32_t fstat_(void *c,const char *name,uint32_t *size,uint64_t *revision){(void)c;io++;*size=0;*revision=0;
    if(stat_result)return stat_result;
    file *f=&files[file_index(name)];if(!f->data)return RISC_APP_DATA_NOT_FOUND;
    *size=f->size;*revision=f->revision;return 0;}
static int32_t fread_(void *c,const char *name,uint64_t revision,void *out,uint32_t cap,uint32_t *used,uint64_t *actual){(void)c;io++;*used=0;*actual=0;
    if(read_result)return read_result;
    file *f=&files[file_index(name)];if(revision!=f->revision)return RISC_APP_DATA_STALE;
    assert(f->data&&cap>=f->size);memcpy(out,f->data,f->size);*used=f->size;*actual=f->revision;return 0;}
static int32_t freplace_(void *c,const char *name,uint64_t revision,const void *b,uint32_t n){(void)c;io++;assert(file_index(name)==1);replacements++;
    file *f=&files[1];if(revision!=f->revision)return RISC_APP_DATA_STALE;
    if(persist_replace){uint8_t *copy=malloc(n);assert(copy);memcpy(copy,b,n);free(f->data);f->data=copy;f->size=n;f->revision++;}
    return replace_result;}
static uint64_t monotonic(void *c){(void)c;io++;return ms;}
#ifdef ALARM_NATIVE_UTC
static int32_t realtime_read(void *c,risc_realtime_snapshot_v1 *out){(void)c;io++;*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),
    .validity=RISC_REALTIME_VALID,.epoch_seconds=INT64_C(946684800)+base+ms/1000,.monotonic_before_us=ms*1000,.monotonic_after_us=ms*1000};return 0;}
static const risc_platform_realtime_api_v1 time_api={1,sizeof(time_api),NULL,realtime_read};
#else
static bool rtc_read(void *c,twatch_rtc_time_v1 *out){(void)c;io++;return points_calendar(base+(uint32_t)(ms/1000),out);}
static bool open_(void *c,uint32_t hz,uint8_t channels){(void)c;io++;assert(hz==8000&&channels==1);opens++;return true;}
static bool write_(void *c,const int16_t *p,size_t n){(void)c;io++;assert(p&&n==256);writes++;return true;}
static bool gain_(void *c,uint16_t gain,uint16_t max){(void)c;io++;assert(gain==100&&max==100);return true;}
static bool silence_(void *c){(void)c;io++;silences++;return true;}
static bool close_(void *c){(void)c;io++;closes++;return !close_failed;}
static bool effect_(void *c,uint8_t effect){(void)c;io++;assert(effect==47);effects++;return true;}
static bool stop_(void *c){(void)c;io++;stops++;return true;}
static const twatch_rtc_api_v1 time_api={2,sizeof(time_api),NULL,rtc_read,NULL,NULL,NULL};
static const twatch_audio_out_api_v1 audio_api={1,sizeof(audio_api),NULL,open_,write_,gain_,silence_,close_};
static const twatch_haptic_api_v1 haptic_api={1,sizeof(haptic_api),NULL,effect_,stop_};
#endif
static const risc_bound_key_value_v1 key_api={1,sizeof(key_api),NULL,get,put};
static const risc_bound_app_data_v1 data_api={1,sizeof(data_api),NULL,fstat_,fread_,freplace_};
static const risc_platform_clock_api_v1 clock_fixture={1,sizeof(clock_fixture),NULL,monotonic,NULL};
static const risc_provider_dependency_v1 deps[]={
    {"storage.key-value.bound",1,&key_api},{"storage.app-data.bound",1,&data_api},{"platform.clock",1,&clock_fixture},
#ifdef ALARM_NATIVE_UTC
    {"platform.realtime",1,&time_api}
#else
    {"rtc.clock",2,&time_api},{"audio.output",1,&audio_api},{"haptic.effect",1,&haptic_api}
#endif
};
static uint32_t noon(void){uint32_t n;
#ifdef ALARM_NATIVE_UTC
    assert(alarm_calendar_seconds(2026,10,5,12,0,0,&n));
#else
    twatch_rtc_time_v1 t={2026,10,5,1,12,0,0};portable_time_candidate c[2];assert(portable_time_inverse(&t,c)==1);
    assert(alarm_calendar_seconds(c[0].rtc.year,c[0].rtc.month,c[0].rtc.day,c[0].rtc.hour,c[0].rtc.minute,0,&n));
#endif
    return n;}
static void save_catalog(const points_catalog *c){uint32_t n,used;assert(points_catalog_size(c->event_count,c->type_count,&n));
    uint8_t *b=malloc(n);assert(b&&!points_catalog_encode(c,b,n,&used));free(files[0].data);files[0].data=b;files[0].size=n;files[0].revision++;}
static points_catalog make_catalog(unsigned count){points_config old={.revision=1};points_meta meta=points_default_meta();points_catalog c={0};
    assert(!points_catalog_migrate(&c,&old,&meta,CATALOG_DOMAIN));
    for(unsigned i=0;i<5;i++){points_catalog_type t={.color=0x123456,.mode=3,.flags=POINTS_TYPE_DURATION};snprintf(t.name,sizeof(t.name),"Type %u",i);uint32_t id;assert(!points_catalog_save_type(&c,&t,&id));}
    for(unsigned i=0;i<count;i++){points_catalog_item e={.type_id=8+i%5,.created=noon()-3600,.mode=3,.enabled=1,.weekdays=127,.hour=12,.minute=i%60};uint32_t id;assert(!points_catalog_add_event(&c,&e,&id));}
    return c;}
static void boot(bool clear){if(provider)assert(provider->quiesce());if(clear){for(unsigned i=0;i<2;i++){free(files[i].data);files[i]=(file){0};}
    memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;base=noon();}
    replace_result=read_result=stat_result=kv_result=0;persist_replace=true;close_failed=false;allocation_denied=false;replacements=0;
    opens=writes=effects=closes=stops=silences=put_count=0;provider=t5_driver_get(2);client=provider->capability;assert(provider->start(deps,sizeof(deps)/sizeof(deps[0])));}
static alarm_status_v1 snapshot(void){alarm_status_v1 s={.struct_size=sizeof(s)};assert(!client->status(NULL,&s));return s;}
static void tick(bool ack){(void)client->step(NULL);
#ifdef ALARM_NATIVE_UTC
    if(ack&&active&&phase==PLAYING){alarm_token_v1 t=snapshot().occurrence;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);}
#else
    (void)ack;
#endif
    ms++;}
static void idle(void){for(unsigned i=0;i<10000;i++){tick(true);if(phase==IDLE)return;if(phase==BLOCKED)break;}
    fprintf(stderr,"phase %d error %d active %u state %u generation %u\n",phase,error,active,points_occ.state,points_occ.generation);assert(0);}
static void settle(uint32_t id){for(unsigned i=0;i<10000;i++){tick(true);if(phase==IDLE&&points_occ.event_id==id&&points_occ.state==ALARM_OCC_ACKED)return;if(phase==BLOCKED)break;}
    fprintf(stderr,"settle %u phase %d error %d active %u got %u state %u\n",id,phase,error,active,points_occ.event_id,points_occ.state);assert(0);}
static alarm_sleep_v1 sleep_plan(void){alarm_sleep_v1 p={.struct_size=sizeof(p)};for(unsigned i=0;i<300;i++){int r=client->prepare_sleep(NULL,&p);if(!r)return p;assert(r==ALARM_PENDING);tick(true);}assert(0);return p;}
static void test_scaled(void){boot(true);points_catalog c=make_catalog(20);save_catalog(&c);uint32_t first=c.events[0].id,last=c.events[19].id;settle(first);
    assert(points_occ.count==20&&points_configured.type_count==12&&!put_count&&!sizes[6]);
#ifndef ALARM_NATIVE_UTC
    assert(opens==1&&writes==1&&effects==1&&closes&&stops&&silences);
#endif
    points_catalog_projection p={.struct_size=sizeof(p)};alarm_service_v1 invalid=*client;invalid.struct_size=sizeof(invalid);
    assert(points_service_project(&invalid,&p)==ALARM_INVALID);
    unsigned calls=io;assert(!points_service_project(client,&p)&&io==calls&&p.count==4&&p.previous.event_id==first&&p.next[0].event_id==c.events[1].id);
    assert(p.valid_until==p.next[3].deadline&&p.next[0].label[0]);p.next[0].label[0]='!';assert(!points_service_project(client,&p)&&p.next[0].label[0]!='!');
    uint32_t generation=points_occ.generation;boot(false);idle();assert(points_occ.generation==generation&&!opens&&!effects);
    alarm_sleep_v1 plan=sleep_plan();assert(plan.deadline==noon()+60);alarm_sleep_v1 wrong=plan;wrong.deadline++;assert(resume_sleep(NULL,&wrong)==ALARM_STALE);
    /* Refused sleep leaves the ticket and ledger intact. Explicit successful light return reanchors. */
    assert(points_occ.generation==generation);ms=19u*60u*1000u;assert(!resume_sleep(NULL,&plan));settle(last);
    const points_catalog_cursor *before=points_catalog_cursor_find(&points_occ,last);assert(before&&before->delivered==1);uint32_t day=before->day;
    points_catalog_item edit=c.events[1];edit.minute=25;edit.created=noon()+19*60;assert(!points_catalog_update_event(&c,&edit));save_catalog(&c);assert(client->refresh(NULL)==ALARM_PENDING);idle();
    before=points_catalog_cursor_find(&points_occ,last);assert(before&&before->day==day&&before->delivered==1);assert(!points_catalog_cursor_find(&points_occ,edit.id)->day);
    assert(sleep_plan().deadline==noon()+25*60);
    assert(!points_catalog_delete_event(&c,first));save_catalog(&c);assert(client->refresh(NULL)==ALARM_PENDING);idle();
    before=points_catalog_cursor_find(&points_occ,last);assert(before&&before->day==day&&before->delivered==1&&!points_catalog_cursor_find(&points_occ,first));
    points_catalog_dispose(&c);
}
static void test_migration(void){boot(true);points_config old={.revision=7,.created=noon()-3600};old.points[7]=(points_item){POINTS_CUSTOM_2,1,3,127,12,0,30,1,1};
    points_config_encode(&old,blobs[5]);sizes[5]=64;points_meta m=points_default_meta();points_catalog c={0};assert(!points_catalog_migrate(&c,&old,&m,CATALOG_DOMAIN));save_catalog(&c);
    uint32_t day;assert(points_catalog_local_day(
#ifdef ALARM_NATIVE_UTC
        &(portable_timezone_rule){0},
#endif
        noon(),&day));
    /* For UTC the all-zero rule is UTC; raw policy maps the same civil day. */
    points_ledger legacy={.revision=7,.generation=12,.deadline=noon(),.recovery_until=noon()+ALARM_RECOVERY_SECONDS,.slot=7,.edge=0,.state=ALARM_OCC_PENDING,.mode=3};
    assert(points_ledger_mark(&legacy,7,day,0));points_ledger_encode(&legacy,blobs[6]);sizes[6]=64;uint8_t saved[64];memcpy(saved,blobs[6],64);
    settle(8);assert(points_occ.generation>12&&!memcmp(saved,blobs[6],64)&&!put_count);assert(sleep_plan().deadline==noon()+27*60);
    boot(false);idle();assert(!opens&&!effects);points_catalog_dispose(&c);
}
static void test_virtual_legacy_metadata(void){
    boot(true);points_config old={.revision=3,.created=noon()-3600};old.points[7]=(points_item){POINTS_CUSTOM_2,1,3,127,12,0,0,0,0};
    points_config_encode(&old,blobs[5]);sizes[5]=64;points_meta meta=points_default_meta();memcpy(meta.custom[1].name,"Edited Wake",12);meta.custom[1].color=2;
    points_meta_encode(&meta,blobs[9]);sizes[9]=64;settle(8);assert(!files[0].data&&files[1].data&&!put_count);
    points_catalog_projection p={.struct_size=sizeof(p)};assert(!points_service_project(client,&p)&&p.has_previous&&p.previous.event_id==8&&
        !strcmp(p.previous.label,"Edited Wake")&&p.previous.color==0xff3d71u);
    boot(false);idle();assert(!opens&&!effects&&!files[0].data);
}
static void test_uncertain(void){boot(true);points_catalog c=make_catalog(1);save_catalog(&c);replace_result=RISC_APP_DATA_COMMIT_UNKNOWN;settle(c.events[0].id);assert(!ledger_store.uncertain);
    boot(false);idle();assert(!opens&&!effects);points_catalog_dispose(&c);
    boot(true);c=make_catalog(1);save_catalog(&c);for(unsigned i=0;i<1000&&phase!=WRITE_OCC;i++)tick(false);
    assert(phase==WRITE_OCC);
    assert(!points_occ.generation);replace_result=RISC_APP_DATA_NO_SPACE;persist_replace=false;tick(false);assert(phase==BLOCKED&&!points_occ.generation&&!opens&&!effects&&!ledger_store.uncertain);
    replace_result=0;persist_replace=true;assert(client->refresh(NULL)==ALARM_PENDING);settle(c.events[0].id);
    /* A later unknown write that did not persist must not modify committed cursors. */
    ms=86400u*1000u;assert(client->refresh(NULL)==ALARM_PENDING);for(unsigned i=0;i<1000&&phase!=WRITE_OCC;i++)tick(false);
    assert(phase==WRITE_OCC);
    uint32_t generation=points_occ.generation,day=points_occ.entries[0].day;assert(points_desired.entries!=points_occ.entries&&points_scan.entries!=points_occ.entries);
    replace_result=RISC_APP_DATA_COMMIT_UNKNOWN;persist_replace=false;tick(false);tick(false);assert(phase==BLOCKED&&points_occ.generation==generation&&points_occ.entries[0].day==day);
    replace_result=0;persist_replace=true;assert(client->refresh(NULL)==ALARM_PENDING);settle(c.events[0].id);assert(points_occ.entries[0].day==day+1);points_catalog_dispose(&c);
}
static void test_arbitration_and_mute(void){
    boot(true);points_catalog c=make_catalog(1);save_catalog(&c);
    for(unsigned i=0;i<2;i++){alarm_config a={.revision=1,.created=noon()-60,.deadline=noon(),.kind=(uint8_t)(i+1),.enabled=1};
        if(i)a.duration=60;
        alarm_config_encode(&a,blobs[i]);sizes[i]=ALARM_RECORD_SIZE;}
    for(unsigned which=0;which<2;which++){
        for(unsigned i=0;i<2000&&!(active&&selected==which&&phase==PLAYING);i++)tick(false);
        assert(active&&selected==which&&phase==PLAYING);alarm_token_v1 t=snapshot().occurrence;
        assert(client->acknowledge(NULL,&t)==ALARM_PENDING);
        for(unsigned i=0;i<2000&&occurrences[which].state!=ALARM_OCC_ACKED;i++)tick(false);
        assert(occurrences[which].state==ALARM_OCC_ACKED);
    }
    settle(c.events[0].id);points_catalog_dispose(&c);
    boot(true);c=make_catalog(1);save_catalog(&c);blobs[8][0]=1;sizes[8]=1;settle(c.events[0].id);
    assert(points_occ.silenced&&!opens&&!effects);blobs[8][0]=0;assert(client->refresh(NULL)==ALARM_PENDING);idle();assert(!opens&&!effects);points_catalog_dispose(&c);
#ifndef ALARM_NATIVE_UTC
    boot(true);c=make_catalog(1);save_catalog(&c);blobs[7][0]=0;sizes[7]=1;settle(c.events[0].id);assert(!opens&&!writes&&effects==1);points_catalog_dispose(&c);
#endif
}
static void test_refusal_and_compaction(void){
    boot(true);points_catalog c=make_catalog(20);save_catalog(&c);
    ms=10u*86400u*1000u+3600u*1000u;idle();assert(replacements==1&&points_occ.count==20&&points_occ.state==0&&!opens&&!effects);
    uint32_t generation=points_occ.generation,day=points_occ.entries[0].day;
    ms+=86400u*1000u;assert(client->refresh(NULL)==ALARM_PENDING);
    for(unsigned i=0;i<1000&&phase!=WRITE_OCC;i++)tick(false);
    assert(phase==WRITE_OCC);
    allocation_denied=true;tick(false);assert(phase==BLOCKED&&points_occ.generation==generation&&points_occ.entries[0].day==day);allocation_denied=false;
    assert(client->refresh(NULL)==ALARM_PENDING);idle();assert(points_occ.generation==generation+1&&points_occ.entries[0].day==day+1);points_catalog_dispose(&c);
    boot(true);c=make_catalog(1);save_catalog(&c);persist_replace=false;replace_result=RISC_APP_DATA_COMMIT_UNKNOWN;
    for(unsigned i=0;i<1000&&phase!=BLOCKED;i++)tick(false);
    assert(phase==BLOCKED&&!points_occ.generation&&!ledger_store.uncertain);
    persist_replace=true;replace_result=0;assert(client->refresh(NULL)==ALARM_PENDING);settle(c.events[0].id);points_catalog_dispose(&c);
}
static void test_corrupt_storage(void){
    boot(true);points_catalog c=make_catalog(1);save_catalog(&c);settle(c.events[0].id);uint32_t generation=points_occ.generation;
    unsigned old_opens=opens,old_effects=effects;files[1].data[files[1].size-1]^=1;assert(client->refresh(NULL)==ALARM_PENDING);
    for(unsigned i=0;i<1000&&phase!=BLOCKED;i++)tick(false);
    assert(phase==BLOCKED&&error==ALARM_STORAGE&&points_occ.generation==generation&&opens==old_opens&&effects==old_effects);
    points_catalog_projection p={.struct_size=sizeof(p)};assert(points_service_project(client,&p)==ALARM_STORAGE);
    files[1].data[files[1].size-1]^=1;assert(client->refresh(NULL)==ALARM_PENDING);idle();assert(points_occ.generation==generation);
    files[0].data[files[0].size-1]^=1;assert(client->refresh(NULL)==ALARM_PENDING);
    for(unsigned i=0;i<1000&&phase!=BLOCKED;i++)tick(false);
    assert(phase==BLOCKED&&error==ALARM_STORAGE&&points_occ.generation==generation);files[0].data[files[0].size-1]^=1;points_catalog_dispose(&c);
}
static void test_cleanup(void){
#ifndef ALARM_NATIVE_UTC
    boot(true);points_catalog c=make_catalog(1);save_catalog(&c);for(unsigned i=0;i<1000&&phase!=PLAYING;i++)tick(false);
    assert(active&&phase==PLAYING&&opens&&effects);
    close_failed=true;ms+=400;for(unsigned i=0;i<20&&phase!=BLOCKED;i++)tick(false);
    assert(phase==BLOCKED&&audio_uncertain&&stops&&silences&&closes);
    close_failed=false;for(unsigned i=0;i<5;i++)if(client->stop_only(NULL)==ALARM_OK)break;
    assert(!audio_uncertain&&!haptic_uncertain);boot(false);settle(c.events[0].id);points_catalog_dispose(&c);
#endif
}
static void test_retained(int mode){boot(true);points_catalog c=make_catalog(1);save_catalog(&c);points_catalog_dispose(&c);
    if(mode==1){for(unsigned i=0;i<1000&&phase!=START_AUDIO;i++)tick(false);
    assert(phase==START_AUDIO);kv_result=RISC_BOUND_KEY_VALUE_CONTEXT;}
    else if(mode>=3){for(unsigned i=0;i<1000&&phase!=WRITE_OCC;i++)tick(false);
        assert(phase==WRITE_OCC);if(mode==3)replace_result=RISC_APP_DATA_RETAINED;
        else {tick(false);assert(phase==VERIFY_OCC);read_result=RISC_APP_DATA_RETAINED;}}
    else {for(unsigned i=0;i<1000&&phase!=LOAD_POINTS_CFG;i++)tick(false);
    assert(phase==LOAD_POINTS_CFG);stat_result=mode==2?RISC_APP_DATA_CONTEXT:RISC_APP_DATA_RETAINED;}
    assert(client->step(NULL)==ALARM_RETAINED&&custody_retained);unsigned before=io;alarm_sleep_v1 sleep={.struct_size=sizeof(sleep)};alarm_token_v1 token_={0};
    assert(client->step(NULL)==ALARM_RETAINED&&client->refresh(NULL)==ALARM_RETAINED&&client->prepare_sleep(NULL,&sleep)==ALARM_RETAINED&&
        client->stop_only(NULL)==ALARM_RETAINED&&client->acknowledge(NULL,&token_)==ALARM_RETAINED&&resume_sleep(NULL,&sleep)==ALARM_RETAINED);
    points_catalog_projection p={.struct_size=sizeof(p)};assert(points_service_project(client,&p)==ALARM_RETAINED);assert(snapshot().error==ALARM_RETAINED);
    assert(!provider->quiesce()&&!provider->start(deps,sizeof(deps)/sizeof(deps[0])));provider->stop();assert(io==before);
}
int main(int argc,char **argv){test_scaled();test_migration();test_virtual_legacy_metadata();test_uncertain();test_arbitration_and_mute();test_refusal_and_compaction();test_corrupt_storage();test_cleanup();test_retained(argc>1?atoi(argv[1]):0);
    puts("Catalog service: >8 events/>2 types, migration, restart/light wake, edit isolation, projection, unknown/refused writes, cleanup and terminal retention PASS");return 0;}
