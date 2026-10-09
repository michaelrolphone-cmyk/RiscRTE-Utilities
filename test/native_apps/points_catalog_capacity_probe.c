#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Track requested live bytes, excluding allocator headers and test file copies.
 * Production catalog allocations and actual Runtime backend snapshots share
 * one measured budget. The original service/test sources are included intact. */
typedef union { max_align_t alignment; struct { size_t bytes; unsigned kind; } value; } allocation_header;
static size_t live[3], peak[3], total_peak, allocation_count[3], budget;
static void *capacity_allocate(size_t n, unsigned kind) {
    if (kind && budget && live[1]+live[2]+n>budget) return NULL;
    allocation_header *h=malloc(sizeof(*h)+n);if(!h)return NULL;
    h->value.bytes=n;h->value.kind=kind;live[kind]+=n;allocation_count[kind]++;
    if(live[kind]>peak[kind])peak[kind]=live[kind];
    if(live[1]+live[2]>total_peak)total_peak=live[1]+live[2];
    return h+1;
}
static void capacity_free(void *p) {
    if(!p)return;
    allocation_header *h=(allocation_header*)p-1;
    assert(live[h->value.kind]>=h->value.bytes);live[h->value.kind]-=h->value.bytes;free(h);
}
static void *capacity_malloc(size_t n,const char *caller) {
    return capacity_allocate(n,!strcmp(caller,"catalog_alloc")?1:0);
}
void *capacity_backend_allocate(size_t n){return capacity_allocate(n,2);}
void capacity_backend_free(void *p){capacity_free(p);}
static uint64_t host_ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
#define malloc(n) capacity_malloc((n),__func__)
#define free(p) capacity_free(p)
#define main original_service_fixture_main
#include "points_catalog_service_test.c"
#undef main
#undef malloc
#undef free

extern void capacity_backend_start(const char *root);
extern int32_t capacity_backend_stat(void*,const char*,uint32_t*,uint64_t*);
extern int32_t capacity_backend_read(void*,const char*,uint64_t,void*,uint32_t,uint32_t*,uint64_t*);
extern int32_t capacity_backend_replace(void*,const char*,uint64_t,const void*,uint32_t);
extern unsigned long capacity_backend_cooperations(void);
static const risc_bound_app_data_v1 real_data={1,sizeof(real_data),NULL,capacity_backend_stat,capacity_backend_read,capacity_backend_replace};
static void reset_measure(void){for(unsigned i=0;i<3;i++){peak[i]=live[i];allocation_count[i]=0;}total_peak=live[1]+live[2];}
static const char *phase_name(phase_t p){switch(p){case EVALUATE:return "EVALUATE";case LOAD_POINTS_CFG:return "LOAD_POINTS_CFG";case LOAD_POINTS_OCC:return "LOAD_POINTS_OCC";case VERIFY_OCC:return "VERIFY_OCC";case WRITE_OCC:return "WRITE_OCC";default:return "other";}}
static void emit(const char *operation,unsigned count,uint64_t elapsed,uint64_t maximum,unsigned calls,phase_t slowest,unsigned long cooperations){
    printf("{\"operation\":\"%s\",\"events\":%u,\"elapsed_ns\":%llu,\"max_call_ns\":%llu,\"calls\":%u,\"slowest_phase\":\"%s\",\"catalog_live\":%zu,\"catalog_peak\":%zu,\"backend_peak\":%zu,\"combined_peak\":%zu,\"catalog_allocations\":%zu,\"backend_allocations\":%zu,\"backend_cooperations\":%lu,\"error\":%d}\n",
      operation,count,(unsigned long long)elapsed,(unsigned long long)maximum,calls,phase_name(slowest),live[1],peak[1],peak[2],total_peak,allocation_count[1],allocation_count[2],cooperations,error);
}
static void run_idle(const char *name,unsigned count){
    reset_measure();uint64_t start=host_ns(),maximum=0;unsigned calls=0;phase_t slowest=phase;unsigned long coop=capacity_backend_cooperations();
    do {phase_t before=phase;uint64_t at=host_ns();int32_t r=client->step(NULL);uint64_t elapsed=host_ns()-at;
        if(elapsed>maximum){maximum=elapsed;slowest=before;}++calls;++ms;
        if(r<0||phase==BLOCKED)break;
        assert(calls<10000);
    }while(phase!=IDLE);
    emit(name,count,host_ns()-start,maximum,calls,slowest,capacity_backend_cooperations()-coop);
}
int main(int argc,char **argv){
    assert(argc>=3);unsigned count=(unsigned)strtoul(argv[1],NULL,10);
    boot(true);points_catalog c=make_catalog(count);
    for(unsigned i=0;i<count;i++){c.events[i].hour=13;c.events[i].minute=i%60;c.events[i].duration_minutes=30;c.events[i].notify_end=1;c.events[i].warn3=1;}
    assert(points_catalog_valid(&c));save_catalog(&c);points_catalog_dispose(&c);assert(provider->quiesce());
    capacity_backend_start(argv[2]);assert(!capacity_backend_replace(NULL,POINTS_CATALOG_FILE,0,files[0].data,files[0].size));
    unsigned catalog_bytes=files[0].size;capacity_free(files[0].data);files[0]=(file){0};
    risc_provider_dependency_v1 real_deps[sizeof(deps)/sizeof(deps[0])];memcpy(real_deps,deps,sizeof(deps));real_deps[1].api=&real_data;
    assert(provider->start(real_deps,sizeof(real_deps)/sizeof(real_deps[0])));assert(!live[1]&&!live[2]);
    if(argc>3)budget=strtoul(argv[3],NULL,10);
    printf("{\"catalog_file_bytes\":%u,\"events\":%u,\"item_size\":%zu,\"type_size\":%zu,\"cursor_size\":%zu,\"budget\":%zu}\n",catalog_bytes,count,sizeof(points_catalog_item),sizeof(points_catalog_type),sizeof(points_catalog_cursor),budget);
    run_idle("cold",count);
    if(phase==BLOCKED){assert(!active&&!opens&&!effects);budget=0;assert(provider->quiesce());assert(!live[1]&&!live[2]);return 0;}
    assert(points_occ.count==count&&points_configured.event_count==count&&!active);
    for(unsigned i=0;i<3;i++){assert(client->refresh(NULL)==ALARM_PENDING);run_idle("warm",count);assert(phase==IDLE);}
    reset_measure();uint64_t at=host_ns();points_catalog_projection p={.struct_size=sizeof(p)};
    unsigned long coop=capacity_backend_cooperations();unsigned before_io=io;
    for(unsigned i=0;i<10000;i++)assert(!points_service_project(client,&p));
    emit("copied_projection_10000",count,host_ns()-at,0,10000,phase,capacity_backend_cooperations()-coop);assert(io==before_io&&!allocation_count[1]&&!allocation_count[2]);
    reset_measure();at=host_ns();for(unsigned i=0;i<3;i++)assert(points_catalog_project(CATALOG_RULE &points_configured,seconds,&p));
    emit("compute_projection_3",count,host_ns()-at,0,3,phase,0);
    alarm_sleep_v1 plan={.struct_size=sizeof(plan)};assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING);run_idle("sleep_reconcile",count);
    reset_measure();at=host_ns();assert(!client->prepare_sleep(NULL,&plan));uint64_t elapsed=host_ns()-at;assert(plan.deadline>seconds);
    emit("sleep_deadline",count,elapsed,elapsed,1,phase,0);
    ms+=10u*86400u*1000u+12u*3600u*1000u;assert(client->refresh(NULL)==ALARM_PENDING);run_idle("outage_compaction",count);assert(phase==IDLE&&!active);
    assert(provider->quiesce());assert(!live[1]&&!live[2]);return 0;
}
