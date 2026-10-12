/* Real catalog service against typed no-I/O native storage refusal boundaries. */
#define ALARM_STORAGE_BUSY_RETRY
#define ALARM_NATIVE_UTC
#define main original_points_main
#include "points_catalog_service_test.c"
#undef main
static void reach(phase_t wanted) {for(unsigned i=0;i<1000&&phase!=wanted&&phase!=BLOCKED;i++)tick(false);assert(phase==wanted);}
static void held_phase(phase_t expected) {
    alarm_status_v1 before=snapshot();unsigned old_puts=put_count;
    for(unsigned i=0;i<20;i++) {assert(client->step(NULL)==ALARM_PENDING);assert(phase==expected&&!error);}
    assert(!put_count||put_count==old_puts);assert(snapshot().occurrence.generation==before.occurrence.generation);
}
int main(void) {
    const phase_t kv_phases[]={LOAD_ALARM,LOAD_TIMER,LOAD_MODE,LOAD_DND,LOAD_ALARM_OCC,LOAD_TIMER_OCC,LOAD_POINTS_CFG,LOAD_POINTS_OCC,LOAD_ZONE};
    for(unsigned i=0;i<sizeof(kv_phases)/sizeof(kv_phases[0]);i++) {
        boot(true);reach(kv_phases[i]);kv_result=RISC_BOUND_KEY_VALUE_BUSY;held_phase(kv_phases[i]);
        kv_result=0;idle();assert(!error&&!opens&&!effects);
    }
    for(unsigned phase_index=0;phase_index<2;phase_index++) {
        phase_t expected=phase_index?LOAD_POINTS_OCC:LOAD_POINTS_CFG;
        boot(true);reach(expected);stat_result=RISC_APP_DATA_BUSY;held_phase(expected);stat_result=0;idle();
    }
    boot(true);reach(WRITE_OCC);replace_result=RISC_APP_DATA_BUSY;persist_replace=false;
    held_phase(WRITE_OCC);assert(!ledger_store.uncertain&&!ledger_store.pending&&!files[1].data);
    replace_result=0;persist_replace=true;tick(false);assert(phase==VERIFY_OCC&&ledger_store.uncertain);
    unsigned accepted=replacements;stat_result=RISC_APP_DATA_BUSY;held_phase(VERIFY_OCC);
    assert(ledger_store.uncertain&&replacements==accepted);stat_result=0;
    read_result=RISC_APP_DATA_BUSY;held_phase(VERIFY_OCC);assert(ledger_store.uncertain&&replacements==accepted);
    read_result=0;idle();assert(!ledger_store.uncertain);
    boot(false);reach(LOAD_POINTS_OCC);read_result=RISC_APP_DATA_BUSY;held_phase(LOAD_POINTS_OCC);read_result=0;idle();
    /* Fresh catalog reads include a complete real stat then a busy read. */
    boot(true);points_catalog c=make_catalog(1);save_catalog(&c);reach(LOAD_POINTS_CFG);
    read_result=RISC_APP_DATA_BUSY;held_phase(LOAD_POINTS_CFG);read_result=0;idle();points_catalog_dispose(&c);
    /* Cancellation preflights must keep the verified occurrence/generation. */
    for(unsigned p=0;p<2;p++) {
        boot(true);c=make_catalog(1);save_catalog(&c);reach(p?START_AUDIO:CATALOG_PREFLIGHT);
        assert(active&&points_occ.generation);unsigned generation=points_occ.generation;accepted=replacements;
        stat_result=RISC_APP_DATA_BUSY;held_phase(p?START_AUDIO:CATALOG_PREFLIGHT);
        assert(active&&points_occ.generation==generation&&replacements==accepted);
        stat_result=0;settle(c.events[0].id);assert(points_occ.generation==generation);points_catalog_dispose(&c);
    }
    /* A real storage error is still a visible block, even following BUSY. */
    boot(true);kv_result=RISC_BOUND_KEY_VALUE_BUSY;held_phase(LOAD_ALARM);kv_result=RISC_BOUND_KEY_VALUE_IO;tick(false);
    assert(phase==BLOCKED&&error==ALARM_STORAGE);kv_result=0;for(unsigned i=0;i<20;i++)tick(false);assert(phase==BLOCKED);
    boot(true);reach(LOAD_POINTS_CFG);stat_result=RISC_APP_DATA_BUSY;held_phase(LOAD_POINTS_CFG);stat_result=RISC_APP_DATA_IO;tick(false);assert(phase==BLOCKED&&error==ALARM_STORAGE);
    boot(true);reach(WRITE_OCC);replace_result=RISC_APP_DATA_COMMIT_UNKNOWN;persist_replace=false;tick(false);
    assert(phase==VERIFY_OCC&&ledger_store.uncertain);tick(false);assert(phase==BLOCKED&&error==ALARM_STORAGE);
    boot(true);reach(WRITE_OCC);replace_result=RISC_APP_DATA_COMMIT_UNKNOWN;persist_replace=true;tick(false);
    accepted=replacements;stat_result=RISC_APP_DATA_BUSY;held_phase(VERIFY_OCC);assert(replacements==accepted);
    stat_result=0;idle();assert(!ledger_store.uncertain);
    assert(provider->quiesce());for(unsigned i=0;i<2;i++)free(files[i].data);
    puts("Alarm startup: KV/catalog/ledger BUSY phases defer without false errors or write uncertainty; genuine IO/unknown commits retain normal failure and readback rules PASS");return 0;
}
