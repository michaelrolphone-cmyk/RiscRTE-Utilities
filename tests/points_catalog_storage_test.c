#include <assert.h>
#include <stdio.h>
#include "PointsCatalogStorage.h"
enum { NORMAL, FAIL_BEFORE, FAIL_AFTER, RETAIN_AFTER };
static uint8_t *disk;
static uint32_t disk_size,capacity=65536,reads,stats,writes,mode;
static uint64_t disk_revision;
static bool read_error,stat_retained;
static int32_t fs_stat(void *ctx,const char *name,uint32_t *size,uint64_t *revision) {
    (void)ctx;assert(!strcmp(name,POINTS_CATALOG_FILE));++stats;*size=0;*revision=0;
    if(stat_retained)return RISC_APP_DATA_RETAINED;
    if(!disk)return RISC_APP_DATA_NOT_FOUND;
    *size=disk_size;*revision=disk_revision;return RISC_APP_DATA_OK;
}
static int32_t fs_read(void *ctx,const char *name,uint64_t expected,void *bytes,uint32_t cap,uint32_t *size,uint64_t *revision) {
    (void)ctx;assert(!strcmp(name,POINTS_CATALOG_FILE));++reads;*size=0;*revision=0;
    if(read_error)return RISC_APP_DATA_IO;
    if(!disk)return RISC_APP_DATA_NOT_FOUND;
    if(expected!=disk_revision)return RISC_APP_DATA_STALE;
    if(cap<disk_size){*size=disk_size;return RISC_APP_DATA_BUFFER_SMALL;}
    memcpy(bytes,disk,disk_size);*size=disk_size;*revision=disk_revision;return RISC_APP_DATA_OK;
}
static int32_t fs_replace(void *ctx,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
    (void)ctx;assert(!strcmp(name,POINTS_CATALOG_FILE));++writes;
    if(expected!=(disk?disk_revision:0))return RISC_APP_DATA_STALE;
    if(size>capacity)return RISC_APP_DATA_NO_SPACE;
    if(mode==FAIL_BEFORE)return RISC_APP_DATA_IO;
    uint8_t *next=malloc(size);assert(next);memcpy(next,bytes,size);free(disk);disk=next;disk_size=size;++disk_revision;
    return mode==FAIL_AFTER?RISC_APP_DATA_COMMIT_UNKNOWN:mode==RETAIN_AFTER?RISC_APP_DATA_RETAINED:RISC_APP_DATA_OK;
}
static const risc_app_data_v1 api={1,sizeof(api),NULL,fs_stat,fs_read,fs_replace};
static points_catalog draft_from(const points_catalog_storage *s) {
    points_catalog c={0};assert(points_catalog_clone(&c,&s->saved)==0);
    points_catalog_item e={.enabled=1,.weekdays=62,.hour=9,.minute=5,.type_id=1,.created=100000};uint32_t id;
    assert(points_catalog_add_event(&c,&e,&id)==0);return c;
}
int main(void) {
    points_catalog_storage s={0};points_config old=points_default_config();points_meta meta=points_default_meta();
    assert(points_catalog_storage_load(&s,&api,0)==RISC_APP_DATA_NOT_FOUND&&s.missing&&!s.loaded&&!writes);
    assert(points_catalog_storage_migrate(&s,&old,&meta,0)==0&&s.loaded&&!writes);
    /* Migration is explicit and preserves the source until atomic publication. */
    assert(points_catalog_storage_save(&s,&api,&s.saved)==0&&s.saved.event_count==7&&writes==1);
    assert(!s.uncertain&&!s.missing&&s.file_revision==disk_revision);
    points_catalog d=draft_from(&s);assert(points_catalog_storage_save(&s,&api,&d)==0);points_catalog_dispose(&d);
    d=draft_from(&s);assert(points_catalog_storage_save(&s,&api,&d)==0);points_catalog_dispose(&d);
    assert(s.saved.event_count==9);
    /* A full backend rejects a tenth entry without changing committed data. */
    capacity=disk_size;d=draft_from(&s);uint32_t old_size=disk_size,old_revision=s.saved.revision;
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_NO_SPACE&&!s.uncertain);
    assert(disk_size==old_size&&s.saved.revision==old_revision&&s.saved.event_count==9);
    points_catalog_dispose(&d);capacity=65536;
    /* Before-rename failure observes the old complete catalog, never retries. */
    mode=FAIL_BEFORE;d=draft_from(&s);unsigned before=writes;
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_STALE&&writes==before+1);
    assert(s.saved.event_count==9&&!s.uncertain);points_catalog_dispose(&d);
    /* Ambiguous commit resolves from bytes and does not duplicate the event. */
    mode=FAIL_AFTER;d=draft_from(&s);before=writes;
    assert(points_catalog_storage_save(&s,&api,&d)==0&&writes==before+1);
    assert(s.saved.event_count==10&&!s.uncertain);points_catalog_dispose(&d);
    /* A post-commit read fault keeps editing locked until a read-only retry. */
    mode=NORMAL;d=draft_from(&s);read_error=true;before=writes;
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_IO&&s.uncertain);
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_COMMIT_UNKNOWN&&writes==before+1);
    read_error=false;assert(points_catalog_storage_resolve(&s,&api)==0&&writes==before+1&&s.saved.event_count==11);
    points_catalog_dispose(&d);
    /* Another namespace invalidating the token cannot overwrite a file. */
    ++disk_revision;d=draft_from(&s);before=writes;
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_STALE&&!s.uncertain&&writes==before+1);
    assert(s.saved.event_count==11);points_catalog_dispose(&d);
    assert(points_catalog_storage_load(&s,&api,0)==0);
    /* Catalog corruption disables edits and never creates defaults. */
    disk[0]^=1;before=writes;
    assert(points_catalog_storage_load(&s,&api,0)==RISC_APP_DATA_IO&&!s.loaded&&!s.missing&&writes==before);
    assert(points_catalog_storage_migrate(&s,&old,&meta,0)==RISC_APP_DATA_INVALID);
    disk[0]^=1;assert(points_catalog_storage_load(&s,&api,0)==0);
    assert(points_catalog_storage_load(&s,&api,1)==RISC_APP_DATA_INVALID&&!s.loaded);
    assert(points_catalog_storage_load(&s,&api,0)==0);
    /* True retained cleanup is terminal; retries make no further native call. */
    d=draft_from(&s);mode=RETAIN_AFTER;
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_RETAINED&&s.retained);
    unsigned calls=stats+reads+writes;
    assert(points_catalog_storage_resolve(&s,&api)==RISC_APP_DATA_RETAINED);
    assert(points_catalog_storage_load(&s,&api,0)==RISC_APP_DATA_RETAINED);
    assert(points_catalog_storage_save(&s,&api,&d)==RISC_APP_DATA_RETAINED);
    assert(stats+reads+writes==calls);points_catalog_dispose(&d);points_catalog_storage_dispose(&s);
    /* A fresh invocation reloads the exact complete committed data. */
    mode=NORMAL;assert(points_catalog_storage_load(&s,&api,0)==0&&s.saved.event_count==12);
    stat_retained=true;assert(points_catalog_storage_load(&s,&api,0)==RISC_APP_DATA_RETAINED);
    calls=stats+reads+writes;assert(points_catalog_storage_load(&s,&api,0)==RISC_APP_DATA_RETAINED&&stats+reads+writes==calls);
    points_catalog_storage_dispose(&s);free(disk);
    puts("Points file writer: storage-full, pre/post commit failure, stale revision, reset and terminal retention PASS");
    return 0;
}
