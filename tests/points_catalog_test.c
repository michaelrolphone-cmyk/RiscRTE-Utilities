#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned calls,fail_call;
static void *checked_alloc(size_t n){++calls;return calls==fail_call?NULL:malloc(n);}
#define POINTS_CATALOG_ALLOC checked_alloc
#define POINTS_CATALOG_FREE free
#include "PointsCatalog.h"

static void repair_hash(uint8_t *p,uint32_t n){alarm_write32(p+n-4,points_catalog_hash(p,n-4));}
static void unchanged_invalid(points_catalog *current,uint8_t *p,uint32_t n) {
    uint32_t rev=current->revision,count=current->event_count;points_catalog_item *events=current->events;
    assert(points_catalog_decode(current,p,n)==POINTS_CATALOG_INVALID);
    assert(current->revision==rev&&current->event_count==count&&current->events==events);
}
int main(void) {
    points_config old=points_default_config();points_meta meta=points_default_meta();
    old.revision=41;old.created=123456;old.points[1]=(points_item){0};
    old.points[7]=(points_item){.kind=POINTS_CUSTOM_2,.enabled=1,.weekdays=65,.hour=9,.minute=7,
        .duration_minutes=45,.notify_end=1,.warn3=1};
    memcpy(meta.custom[0].name,"  commute ",11);
    points_catalog c={0};
    assert(points_catalog_migrate(&c,&old,&meta,POINTS_TIME_RAW_RTC)==POINTS_CATALOG_OK);
    assert(c.event_count==7&&c.type_count==7&&c.revision==41&&c.next_event_id==9);
    assert(!points_catalog_find_event(&c,2));
    const points_catalog_item *last=points_catalog_find_event(&c,8);
    assert(last&&last->type_id==7&&last->created==123456&&last->revision==41&&last->weekdays==65);
    assert(last->duration_minutes==45&&last->notify_end&&last->warn3);
    assert(!strcmp(points_catalog_find_type(&c,6)->name,"  commute "));
    points_catalog_type custom={.color=0xABCDEF,.duration_minutes=22,.mode=ALARM_MODE_VIBRATE,
        .flags=POINTS_TYPE_DURATION|POINTS_TYPE_NOTIFY_END|POINTS_TYPE_WARN3};
    strcpy(custom.name,"Exercise");uint32_t type=0;
    assert(points_catalog_save_type(&c,&custom,&type)==POINTS_CATALOG_OK&&type==8);
    points_catalog_item event={.enabled=1,.weekdays=42,.hour=7,.minute=5,.created=200000};
    assert(points_catalog_apply_type(&event,&c,type)==POINTS_CATALOG_OK);
    assert(event.type_id==8&&event.duration_minutes==22&&event.notify_end&&event.warn3&&event.mode==1);
    uint32_t first=0;
    assert(points_catalog_add_event(&c,&event,&first)==POINTS_CATALOG_OK&&first==9);
    for(unsigned i=0;i<1500;i++){uint32_t id;event.hour=(uint8_t)(i%24);
        assert(points_catalog_add_event(&c,&event,&id)==POINTS_CATALOG_OK&&id==10+i);}
    assert(c.event_count==1508&&points_catalog_find_event(&c,1509));
    for(unsigned i=0;i<40;i++) {snprintf(custom.name,sizeof(custom.name),"Custom type %u",i);uint32_t id;
        assert(points_catalog_save_type(&c,&custom,&id)==POINTS_CATALOG_OK&&id==9+i);}
    assert(c.type_count==48);uint32_t size=0;
    assert(points_catalog_size(c.event_count,c.type_count,&size)&&size>48000&&size<65536);
    uint8_t *bytes=malloc(size),*bad=malloc(size);assert(bytes&&bad);uint32_t used=0;
    assert(points_catalog_encode(&c,bytes,size,&used)==POINTS_CATALOG_OK&&used==size);
    points_catalog decoded={0};assert(points_catalog_decode(&decoded,bytes,size)==POINTS_CATALOG_OK);
    assert(decoded.event_count==c.event_count&&decoded.type_count==48&&decoded.next_event_id==1510);
    uint8_t *roundtrip=malloc(size);assert(roundtrip);
    assert(points_catalog_encode(&decoded,roundtrip,size,&used)==POINTS_CATALOG_OK&&!memcmp(bytes,roundtrip,size));
    /* A saved edit changes only that event's scheduling boundary. */
    points_catalog_item edited=*points_catalog_find_event(&decoded,first);
    uint32_t other_rev=points_catalog_find_event(&decoded,10)->revision;
    edited.minute=12;edited.created=200500;
    assert(points_catalog_update_event(&decoded,&edited)==POINTS_CATALOG_OK);
    assert(points_catalog_find_event(&decoded,first)->revision==2);
    assert(points_catalog_find_event(&decoded,10)->revision==other_rev);
    assert(points_catalog_find_event(&decoded,8)->created==123456);
    assert(points_catalog_delete_type(&decoded,8)==POINTS_CATALOG_IN_USE);
    custom=*points_catalog_find_type(&decoded,8);strcpy(custom.name,"Training");
    assert(points_catalog_save_type(&decoded,&custom,&type)==POINTS_CATALOG_OK&&type==8);
    assert(points_catalog_find_event(&decoded,first)->revision==2);
    custom.flags=0;custom.duration_minutes=0;
    assert(points_catalog_save_type(&decoded,&custom,&type)==POINTS_CATALOG_IN_USE);
    assert(points_catalog_delete_event(&decoded,first)==POINTS_CATALOG_OK);
    assert(!points_catalog_find_event(&decoded,first));
    assert(points_catalog_find_event(&decoded,10)->revision==other_rev);
    uint32_t added;assert(points_catalog_add_event(&decoded,&event,&added)==POINTS_CATALOG_OK&&added==1510);
    assert(points_catalog_delete_type(&decoded,48)==POINTS_CATALOG_OK);
    custom.id=0;strcpy(custom.name,"training");
    assert(points_catalog_save_type(&decoded,&custom,&type)==POINTS_CATALOG_INVALID);
    /* Corruption, canonical reserved bytes and malicious allocation counts. */
    for(uint32_t i=0;i<size;i+=997){memcpy(bad,bytes,size);bad[i]^=0x80;unchanged_invalid(&decoded,bad,size);}
    memcpy(bad,bytes,size);alarm_write32(bad+20,UINT32_MAX);repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    memcpy(bad,bytes,size);bad[36]=1;repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    memcpy(bad,bytes,size);alarm_write32(bad+28,2);repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    memcpy(bad,bytes,size);alarm_write32(bad+POINTS_CATALOG_HEADER_BYTES+4,99);repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    memcpy(bad,bytes,size);bad[POINTS_CATALOG_HEADER_BYTES+25]=1;repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    memcpy(bad,bytes,size);alarm_write32(bad+POINTS_CATALOG_HEADER_BYTES+32,1);repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    uint32_t types_at=POINTS_CATALOG_HEADER_BYTES+c.event_count*POINTS_CATALOG_EVENT_BYTES;
    memcpy(bad,bytes,size);bad[types_at+63]=1;repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    memcpy(bad,bytes,size);bad[types_at+43]='x';repair_hash(bad,size);unchanged_invalid(&decoded,bad,size);
    unchanged_invalid(&decoded,bytes,size-1);
    assert(!points_catalog_size(UINT32_MAX,UINT32_MAX,&used));
    /* Allocation refusal must leave the old catalog and every ID untouched. */
    for(unsigned which=1;which<=2;which++) {
        points_catalog_item *old_ptr=decoded.events;uint32_t revision=decoded.revision;
        fail_call=calls+which;
        assert(points_catalog_decode(&decoded,bytes,size)==POINTS_CATALOG_MEMORY);
        assert(decoded.events==old_ptr&&decoded.revision==revision);fail_call=0;
    }
    uint32_t next=decoded.next_event_id,revision=decoded.revision;fail_call=calls+1;
    assert(points_catalog_add_event(&decoded,&event,&added)==POINTS_CATALOG_MEMORY&&!added);
    assert(decoded.next_event_id==next&&decoded.revision==revision);fail_call=0;
    decoded.next_event_id=UINT32_MAX;
    assert(points_catalog_add_event(&decoded,&event,&added)==POINTS_CATALOG_EXHAUSTED);
    decoded.next_event_id=next;decoded.revision=UINT32_MAX;
    assert(points_catalog_delete_event(&decoded,10)==POINTS_CATALOG_EXHAUSTED);
    decoded.revision=revision;
    points_catalog_dispose(&decoded);points_catalog_dispose(&c);
    free(bytes);free(bad);free(roundtrip);
    puts("Points catalog: 1508 events, 48 types, stable IDs, legacy migration, corruption and allocation refusal PASS");
    return 0;
}
