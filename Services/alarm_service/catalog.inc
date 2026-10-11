/* Selected storage-scaled implementation. Included after common service state
 * and helpers; the unselected service keeps its original source path. */
#ifdef ALARM_NATIVE_UTC
#define CATALOG_DOMAIN POINTS_TIME_NATIVE_UTC
#define CATALOG_RULE &timezone_rule,
#else
#define CATALOG_DOMAIN POINTS_TIME_RAW_RTC
#define CATALOG_RULE
#endif
static bool catalog_storage_result(int32_t result) {
    if(result==RISC_APP_DATA_CONTEXT||result==RISC_APP_DATA_RETAINED||catalog_input.retained||ledger_store.retained)custody_retained=true;
    return result==RISC_APP_DATA_OK;
}
static bool read_legacy_points(points_config *c,points_meta *m,points_ledger *l) {
    uint8_t b[POINTS_RECORD_SIZE];uint32_t n=0;int32_t r=storage_get(kv->context,POINTS_CONFIG_KEY,b,sizeof(b),&n);
    if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND)*c=points_default_config();
    else if(r!=RISC_BOUND_KEY_VALUE_OK||!points_config_decode(c,b,n))return false;
    if(m){n=0;r=storage_get(kv->context,POINTS_META_KEY,b,sizeof(b),&n);
        if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND)*m=points_default_meta();
        else if(r!=RISC_BOUND_KEY_VALUE_OK||!points_meta_decode(m,b,n))return false;}
    if(l){n=0;r=storage_get(kv->context,POINTS_OCCURRENCE_KEY,b,sizeof(b),&n);
        if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND)*l=(points_ledger){0};
        else if(r!=RISC_BOUND_KEY_VALUE_OK||!points_ledger_decode(l,b,n))return false;}
    return true;
}
static bool read_points_config(void) {
    bool had_file=catalog_input.file_revision!=0;
    int32_t r=points_catalog_storage_load(&catalog_input,app_data,CATALOG_DOMAIN);
    if(r==RISC_APP_DATA_NOT_FOUND){points_config old;points_meta meta;
        if(had_file)return false;
        if(!read_legacy_points(&old,&meta,NULL))return false;
        if(points_catalog_migrate(&catalog_input.saved,&old,&meta,CATALOG_DOMAIN))return false;
        catalog_input.loaded=true;}
    else if(!catalog_storage_result(r))return false;
    return points_catalog_clone(&points_staging,&catalog_input.saved)==POINTS_CATALOG_OK;
}
static bool read_points_occ(void) {
    int32_t r=points_catalog_ledger_load(&ledger_store,app_data,&points_staged_occ,CATALOG_DOMAIN);
    if(r==RISC_APP_DATA_NOT_FOUND) {
        /* A disappeared committed file is a rollback, not permission to replay. */
        if(points_occ.generation||ledger_store.file_revision||ledger_store.uncertain)return false;
        points_config old;points_ledger legacy;
        if(!read_legacy_points(&old,NULL,&legacy))return false;
        return points_catalog_ledger_migrate(&points_staged_occ,&points_staging,&old,&legacy)==POINTS_CATALOG_OK;
    }
    return catalog_storage_result(r);
}
static bool catalog_recorded_point(const points_catalog *c,const points_catalog_ledger *l,points_catalog_event *e) {
#ifdef ALARM_NATIVE_UTC
    const portable_timezone_entry *entry=portable_timezone_get(l->timezone_index);portable_timezone_rule rule;
    return entry&&portable_timezone_resolve(entry->id,strlen(entry->id)+1,&rule)==PORTABLE_TIMEZONE_OK&&
        points_catalog_event_for_day(&rule,c,l->event_id,l->parent_day,l->edge,e,NULL);
#else
    return points_catalog_event_for_day(c,l->event_id,l->parent_day,l->edge,e,NULL);
#endif
}
static bool reconcile_points(void) {
    if(points_configured.revision&&points_staging.revision<points_configured.revision)return false;
    if(points_staging.revision==points_configured.revision) {
        uint32_t a,b,used;
        if(!points_catalog_size(points_staging.event_count,points_staging.type_count,&a)||
           !points_catalog_size(points_configured.event_count,points_configured.type_count,&b)||a!=b)return false;
        uint8_t *x=POINTS_CATALOG_ALLOC(a),*y=POINTS_CATALOG_ALLOC(b);bool same=false;
        if(x&&y&&!points_catalog_encode(&points_staging,x,a,&used)&&!points_catalog_encode(&points_configured,y,b,&used))same=!memcmp(x,y,a);
        POINTS_CATALOG_FREE(x);POINTS_CATALOG_FREE(y);if(!same)return false;
    }
    if(points_staged_occ.revision>points_staging.revision||!points_catalog_ledger_advances(&points_staged_occ,&points_occ))return false;
    if(points_staged_occ.revision==points_staging.revision&&points_staged_occ.count!=points_staging.event_count)return false;
    for(uint32_t i=0;i<points_staged_occ.count;i++) {
        const points_catalog_cursor *cursor=&points_staged_occ.entries[i];
        const points_catalog_item *item=points_catalog_find_event(&points_staging,cursor->id);
        if(item&&cursor->revision>item->revision)return false;
        if(points_staged_occ.revision==points_staging.revision&&(!item||item->revision!=cursor->revision))return false;
    }
    for(uint32_t i=0;i<points_occ.count;i++) {
        const points_catalog_cursor *old=&points_occ.entries[i],*next=points_catalog_cursor_find(&points_staged_occ,old->id);
        const points_catalog_item *item=points_catalog_find_event(&points_staging,old->id);
        if((next&&next->revision<old->revision)||(item&&item->revision==old->revision&&(!next||next->revision!=old->revision)))return false;
    }
    points_catalog_ledger next={0};
    if(points_catalog_ledger_reconcile(&next,&points_staged_occ,&points_staging))return false;
    if(next.state) {
        points_catalog_event e;const points_catalog_item *item=points_catalog_find_event(&points_staging,next.event_id);
        if(!catalog_recorded_point(&points_staging,&next,&e)||e.deadline!=next.deadline||
           (item->mode&&next.mode!=item->mode)){points_catalog_ledger_dispose(&next);return false;}
    }
    points_catalog_dispose(&points_configured);points_configured=points_staging;memset(&points_staging,0,sizeof(points_staging));
    points_catalog_ledger_dispose(&points_occ);points_occ=points_staged_occ;memset(&points_staged_occ,0,sizeof(points_staged_occ));
    points_catalog_ledger_dispose(&points_scan);points_scan=next;return true;
}
static void prepare_point(const points_catalog_event *e,bool replay) {
    selected=2;
    if(points_catalog_ledger_clone(&points_desired,&points_scan)){fail(ALARM_STORAGE);return;}
    if(points_desired.generation==UINT32_MAX){fail(ALARM_EXHAUSTED);return;}
#ifdef ALARM_NATIVE_UTC
    if(!replay)points_desired.timezone_index=(uint16_t)timezone_index;
#endif
    ++points_desired.generation;points_desired.event_id=e->event_id;points_desired.event_revision=e->revision;
    points_desired.parent_day=e->parent_day;points_desired.edge=e->edge;points_desired.deadline=e->deadline;
    points_desired.recovery_until=e->deadline+ALARM_RECOVERY_SECONDS;
    points_desired.state=seconds>=points_desired.recovery_until?ALARM_OCC_EXPIRED:ALARM_OCC_PENDING;
    points_desired.mode=replay?points_scan.mode:e->mode?e->mode:(uint8_t)staged_mode;
    points_desired.silenced=replay?points_scan.silenced:0;
#ifdef ALARM_DND_CONTROL
    points_desired.silenced|=staged_dnd;
#endif
    if(!points_catalog_ledger_mark(&points_desired,e->event_id,e->parent_day,e->edge)){fail(ALARM_STORAGE);return;}
    desired=(alarm_occurrence){.revision=e->revision,.deadline=e->deadline,.generation=points_desired.generation,
        .recovery_until=points_desired.recovery_until,.kind=POINTS_KIND_BASE,.state=points_desired.state,
        .mode=points_desired.mode,.silenced=points_desired.silenced};
    persistence_pending=true;phase=WRITE_OCC;
}
static int32_t catalog_evaluate(int candidate,bool *handled) {
    *handled=false;points_catalog_event event={0};bool have=false,replay=false;
    if(points_scan.state==ALARM_OCC_PENDING) {
        have=catalog_recorded_point(&points_configured,&points_scan,&event)&&event.deadline<=seconds;replay=true;
    } else {
        bool compacted=ledger_store.missing||points_occ.revision!=points_configured.revision;
        for(uint32_t i=0;i<points_configured.event_count;i++)for(unsigned edge=0;edge<3;edge++) {
            points_catalog_event expired;
            if(points_catalog_latest_for_edge(CATALOG_RULE &points_configured,&points_scan,seconds,points_configured.events[i].id,edge,&expired)&&
               seconds>=expired.deadline+ALARM_RECOVERY_SECONDS) {
                if(!points_catalog_ledger_mark(&points_scan,expired.event_id,expired.parent_day,edge))return fail(ALARM_STORAGE);
                compacted=true;
            }
        }
        if(compacted) {
            if(points_scan.generation==UINT32_MAX)return fail(ALARM_EXHAUSTED);
            ++points_scan.generation;points_catalog_ledger_clear_active(&points_scan);
            if(points_catalog_ledger_clone(&points_desired,&points_scan))return fail(ALARM_STORAGE);
            desired=(alarm_occurrence){0};selected=2;persistence_pending=true;phase=WRITE_OCC;*handled=true;return ALARM_OK;
        }
        have=points_catalog_due(CATALOG_RULE &points_configured,&points_scan,seconds,&event);
    }
    if(have&&(candidate<0||event.deadline<config[candidate].deadline)){prepare_point(&event,replay);*handled=true;}
    return error?error:ALARM_OK;
}
static bool catalog_current_unchanged(void) {
    uint32_t size=0;uint64_t rev=0;
    int32_t r=app_data->stat(app_data->context,POINTS_CATALOG_FILE,&size,&rev);
    if(r==RISC_APP_DATA_NOT_FOUND&&catalog_input.missing) {
        points_config old;points_meta meta;points_catalog latest={0};
        if(!read_legacy_points(&old,&meta,NULL)||points_catalog_migrate(&latest,&old,&meta,CATALOG_DOMAIN))return false;
        const points_catalog_item *e=points_catalog_find_event(&latest,points_desired.event_id);
        const points_catalog_item *saved=points_catalog_find_event(&points_configured,points_desired.event_id);
        bool same=e&&saved&&e->revision==saved->revision&&e->created==saved->created;
        points_catalog_dispose(&latest);return same;
    }
    if(!catalog_storage_result(r)){fail(ALARM_STORAGE);return false;}
    return rev==catalog_input.file_revision;
}
/* Final cancellation check shares a serialized phase with first output. Legacy
 * migration records are read-only in selected products; a newly saved catalog
 * or a replaced catalog token always forces full reconciliation first. */
static bool catalog_token_unchanged(void) {
    uint32_t size=0;uint64_t rev=0;int32_t r=app_data->stat(app_data->context,POINTS_CATALOG_FILE,&size,&rev);
    if(r==RISC_APP_DATA_NOT_FOUND&&catalog_input.missing)return true;
    if(!catalog_storage_result(r)){fail(ALARM_STORAGE);return false;}
    return rev==catalog_input.file_revision;
}
static int32_t catalog_sleep_deadline(uint32_t *deadline) {
    uint32_t day;if(!points_catalog_local_day(CATALOG_RULE seconds,&day))return ALARM_RTC;
    const points_catalog_ledger *l=&points_scan;
    if(l->state==ALARM_OCC_PENDING&&l->deadline>seconds&&(!*deadline||l->deadline<*deadline))*deadline=l->deadline;
    for(uint32_t i=0;i<points_configured.event_count;i++)for(unsigned edge=0;edge<3;edge++) {
        const points_catalog_item *item=&points_configured.events[i];if(!points_catalog_notice_enabled(item,edge))continue;
        uint32_t first=day>1?day-1:day;
        for(unsigned offset=0;offset<=8;offset++){uint32_t parent=first+offset;points_catalog_event e;
            if(!points_catalog_ledger_handled(l,item->id,parent,edge)&&
               points_catalog_event_for_day(CATALOG_RULE &points_configured,item->id,parent,edge,&e,NULL)&&
               e.deadline>seconds&&(!*deadline||e.deadline<*deadline))*deadline=e.deadline;}}
    return ALARM_OK;
}
static void catalog_dispose(void) {
    points_catalog_dispose(&points_configured);points_catalog_dispose(&points_staging);
    points_catalog_ledger_dispose(&points_occ);points_catalog_ledger_dispose(&points_staged_occ);
    points_catalog_ledger_dispose(&points_desired);points_catalog_ledger_dispose(&points_scan);
    points_catalog_storage_dispose(&catalog_input);points_catalog_ledger_storage_dispose(&ledger_store);
}
