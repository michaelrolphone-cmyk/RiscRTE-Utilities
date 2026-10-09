#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool terminal;
static unsigned allocations,frees,calls;
static void *tracked_alloc(size_t n){assert(!terminal);++allocations;return malloc(n);}
static void tracked_free(void *p){assert(!terminal);if(p)++frees;free(p);}
#define POINTS_CATALOG_ALLOC tracked_alloc
#define POINTS_CATALOG_FREE tracked_free
#include "PointsCatalogStorage.h"
#include "PointsCatalogLedgerStorage.h"
static int32_t result;
static void *borrowed;
static int32_t stat_(void *c,const char *name,uint32_t *size,uint64_t *revision){
    (void)c;(void)name;assert(!terminal);++calls;*size=128;*revision=1;return 0;
}
static int32_t read_(void *c,const char *name,uint64_t expected,void *data,uint32_t capacity,uint32_t *used,uint64_t *revision){
    (void)c;(void)name;(void)expected;(void)capacity;assert(!terminal);++calls;
    borrowed=data;memset(data,0x5a,128);*used=0;*revision=0;
    terminal=result==RISC_APP_DATA_RETAINED||result==RISC_APP_DATA_CONTEXT;return result;
}
static int32_t replace_(void *c,const char *name,uint64_t expected,const void *data,uint32_t size){
    (void)c;(void)name;(void)expected;(void)data;(void)size;assert(!terminal);++calls;return RISC_APP_DATA_IO;
}
static const risc_app_data_v1 api={1,sizeof(api),NULL,stat_,read_,replace_};
static void check_catalog(int32_t status,bool resolving){
    points_catalog_storage s={0};s.loaded=true;s.uncertain=resolving;
    if(resolving){s.pending=tracked_alloc(12);s.pending_size=12;}
    result=status;unsigned before_frees=frees;
    int32_t r=resolving?points_catalog_storage_resolve(&s,&api):points_catalog_storage_load(&s,&api,0);
    assert(r==status&&s.retained&&s.retained_read==borrowed&&frees==before_frees);
    unsigned before_calls=calls,before_allocations=allocations;
    points_catalog_storage snapshot=s;
    points_catalog_storage_dispose(&s);assert(!memcmp(&snapshot,&s,sizeof(s)));
    assert(points_catalog_storage_load(&s,&api,0)==RISC_APP_DATA_RETAINED);
    uint8_t *out;uint32_t n;uint64_t rev;
    assert(points_catalog_read_document(&s,&api,&out,&n,&rev)==RISC_APP_DATA_RETAINED);
    assert(frees==before_frees&&calls==before_calls&&allocations==before_allocations);
    /* Simulate the host destroying an invocation after its native owner is gone. */
    terminal=false;free(s.retained_read);s.retained_read=NULL;s.retained=false;points_catalog_storage_dispose(&s);
}
static void check_ledger(int32_t status,bool resolving){
    points_catalog_ledger_storage s={0};points_catalog_ledger ledger={0};s.uncertain=resolving;
    if(resolving){s.pending=tracked_alloc(12);s.pending_size=12;}
    result=status;unsigned before_frees=frees;
    assert(points_catalog_ledger_load(&s,&api,&ledger,0)==status);
    assert(s.retained&&s.retained_read==borrowed&&frees==before_frees);
    unsigned before_calls=calls,before_allocations=allocations;points_catalog_ledger_storage snapshot=s;
    points_catalog_ledger_storage_dispose(&s);assert(!memcmp(&snapshot,&s,sizeof(s)));
    assert(points_catalog_ledger_load(&s,&api,&ledger,0)==RISC_APP_DATA_RETAINED);
    assert(points_catalog_ledger_write(&s,&api,&ledger)==RISC_APP_DATA_RETAINED);
    assert(frees==before_frees&&calls==before_calls&&allocations==before_allocations);
    terminal=false;free(s.retained_read);s.retained_read=NULL;s.retained=false;points_catalog_ledger_storage_dispose(&s);
}
int main(void){
    for(unsigned context=0;context<2;context++)for(unsigned resolving=0;resolving<2;resolving++){
        int32_t status=context?RISC_APP_DATA_CONTEXT:RISC_APP_DATA_RETAINED;
        check_catalog(status,resolving);check_ledger(status,resolving);
    }
    /* Recoverable read failures still release their temporary allocation. */
    points_catalog_storage s={0};result=RISC_APP_DATA_IO;unsigned before=frees;
    assert(points_catalog_storage_load(&s,&api,0)==result&&!s.retained&&s.retained_read==NULL&&frees==before+1);
    points_catalog_ledger_storage l={0};points_catalog_ledger ledger={0};before=frees;
    assert(points_catalog_ledger_load(&l,&api,&ledger,0)==result&&!l.retained&&l.retained_read==NULL&&frees==before+1);
    puts("Catalog and ledger read custody: 8 terminal load/resolve cases and recoverable errors PASS");
}
