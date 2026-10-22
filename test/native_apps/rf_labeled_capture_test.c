/* Labeled capture through the production RF controller, using the existing
 * deterministic IQ/storage fixture. This is not an observed RF recording,
 * native UI/Contexts integration, target ELF, or physical-device trial. */
#define main rf_shared_safety_main
#include "rf_shared_input_safety_test.c"
#undef main

static unsigned input_amplitude = 256;
static risc_radio_iq_extended_api_v1 labeled_radio;
static int labeled_burst(void *c,uint32_t *pairs,uint32_t n,const risc_radio_iq_settings_v1 *settings,risc_radio_iq_format_v1 *format) {
    int rc=configured_burst(c,pairs,n,settings,format);
    unsigned positive=input_amplitude,negative=1024u-positive;
    for(unsigned i=0;i<n;i++)pairs[i]=i%4==0?positive:i%4==1?positive<<10:i%4==2?negative:negative<<10;
    return rc;
}
static void capture_setup(void) {
    direct_setup();signature_load();
    waterfall_preferences=(risc_runtime_capability_v1){.struct_size=sizeof(waterfall_preferences)};
    assert(acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&waterfall_preferences));
    preferences=waterfall_preferences.api;preferences_acquired=true;
    input_amplitude=256;
}
static void use_labeled_provider(void) {
    /* Same existing host provider, with an explicitly variable synthetic IQ
     * amplitude. Acquisition policy remains covered by the original suite. */
    labeled_radio=radio;labeled_radio.capture_configured=labeled_burst;
    waterfall_radio=&labeled_radio.base.base;
}
static void observe_bursts(unsigned n) {
    for (unsigned i = 0; i < n; ++i) { tick += 100; capture(); }
}
static void same_signature(const rf_signature *a,const rf_signature *b) {
    uint8_t aa[RF_SIGNATURE_RECORD_SIZE],bb[RF_SIGNATURE_RECORD_SIZE];
    assert(rf_signature_encode(a,aa)&&rf_signature_encode(b,bb));
    assert(!memcmp(aa,bb,sizeof(aa))); /* Compare encoding, not C padding. */
}
static void same_feature(const cf_feature *a,const cf_feature *b) {
    uint8_t aa[4096]={0},bb[4096]={0};
    cf_writer aw={aa,0,sizeof(aa),true},bw={bb,0,sizeof(bb),true};
    cf_write_feature(&aw,a);cf_write_feature(&bw,b);
    assert(aw.ok&&bw.ok&&aw.at==bw.at&&!memcmp(aa,bb,aw.at));
}
static void labeled_signature_capture(unsigned kind) {
    capture_setup();
    if(kind==RF_SIGNATURE_EVENT){toggle();assert(running);use_labeled_provider();observe_bursts(1);}
    signature_open(-1, kind);
    strcpy(editing.name, "  Lab RF  ");
    signature_capture_begin();
    assert(signature_goal == 64 && running && !strcmp(editing.name, "Lab RF"));
    assert(!signatures[0].kind && !puts_count);
    use_labeled_provider();
    observe_bursts(16);
    assert(signature_capture.frames == 16 && !signatures[0].kind);
    stop();
    unsigned before = captures;
    /* Stop preserves the partial sample; the original loop suite separately
     * exercises no-capture behavior while stopped. */
    assert(!running);
    assert(signature_capture.frames == 16 && captures == before);
    toggle();
    assert(running);
    observe_bursts(48);
    assert(!signature_goal && signatures[0].frames == 64 && !signature_pending);
    assert(signatures[0].kind==kind && !strcmp(signatures[0].name, "Lab RF") && rf_signature_valid(&signatures[0]));
    rf_signature first = signatures[0];
    if(kind==RF_SIGNATURE_EVENT){if(!running)toggle();use_labeled_provider();observe_bursts(1);}
    signature_open(0, RF_SIGNATURE_ROOM);
    signature_capture_begin();
    assert(signature_goal==64);
    /* Storage custody can reacquire the provider during save/resume. */
    use_labeled_provider();
    input_amplitude=128;
    observe_bursts(1);
    uint32_t second_power[RF_SIGNATURE_BANDS];
    memcpy(second_power,signature_audio.power,sizeof(second_power));
    uint32_t first_power[RF_SIGNATURE_BANDS];rf_signature_mean(&first,first_power);
    assert(memcmp(first_power,second_power,sizeof(first_power)));
    observe_bursts(62);
    assert(signature_capture.frames == 63 && signatures[0].frames == 64);
    fail_put = true;
    observe_bursts(1);
    assert(!signature_goal && signatures[0].frames == 128 && signature_pending == 1);
    rf_signature accumulated = signatures[0];
    assert(!signature_exit_ready());
    for (unsigned band = 0; band < RF_SIGNATURE_BANDS; ++band)
        assert(accumulated.sums[band] == first.sums[band] + (uint64_t)second_power[band]*64u);
    signature_save();
    assert(signature_pending == 1); same_signature(&signatures[0], &accumulated);
    fail_put = false;
    signature_save();
    assert(!signature_pending); same_signature(&signatures[0], &accumulated);
    stop();
    signature_restore();
    assert(!signature_errors && signatures[0].frames == 128);
    same_signature(&signatures[0], &accumulated);
    signature_open(0, RF_SIGNATURE_ROOM);
    prefs.identity.raw_gain++;
    signature_capture_begin();
    assert(!signature_goal && signatures[0].frames == 128);
    prefs.identity = accumulated.identity;
    stop();
}

static void shared_named_capture(void) {
    capture_setup();shared_kind=CF_ROOM;shared_selected=-1;
    strcpy(shared_name,"Shared RF");set_page(PAGE_SHARED_EDIT);
    shared_tap(120,135);assert(cfa_learning_kind==CF_ROOM&&running);
    use_labeled_provider();observe_bursts(310);
    assert(cfa_learn_ready()&&cfa_learning_samples>=3);
    assert(cf_profile_find(&cfa_bank,CF_ROOM,"Shared RF")<0);
    assert(cfa_learn_save());
    int slot=cf_profile_find(&cfa_bank,CF_ROOM,"Shared RF");assert(slot>=0);
    cf_feature first=cfa_bank.profiles[slot].feature[CFA_SOURCE];
    unsigned generation=cfa_bank.generation;uint64_t revision=cfa_revision;
    shared_selected=slot;strcpy(shared_name,"Shared RF");set_page(PAGE_SHARED_EDIT);
    shared_tap(120,135);input_amplitude=128;observe_bursts(310);
    assert(cfa_learn_ready());
    cf_feature expected=first;
    assert(cf_blend(&expected,&cfa_learning.feature[CFA_SOURCE],.05f));
    deny_data=true;
    assert(!cfa_learn_save()&&cfa_learning_saved&&cfa_changed);
    assert(cfa_bank.generation==generation+1&&cfa_revision==revision);
    same_feature(&cfa_bank.profiles[slot].feature[CFA_SOURCE],&expected);
    assert(!cfa_learn_save());
    assert(cfa_bank.generation==generation+1&&cfa_revision==revision);
    deny_data=false;assert(cfa_learn_save());
    assert(cfa_bank.generation==generation+1&&cfa_revision==revision+1);
    stop();cfa_restore();
    assert(cfa_loaded&&!cfa_failed&&!cfa_changed);
    slot=cf_profile_find(&cfa_bank,CF_ROOM,"Shared RF");assert(slot>=0);
    assert(cfa_bank.generation==generation+1);
    same_feature(&cfa_bank.profiles[slot].feature[CFA_SOURCE],&expected);
}

#include "rf_capture_identity_guard_test.inc"

int main(void) {
    labeled_signature_capture(RF_SIGNATURE_ROOM);
    labeled_signature_capture(RF_SIGNATURE_EVENT);
    shared_named_capture();
    identity_guard_tests();
    puts("RF labeled room/event capture: name trimming, 64-frame capture, pause/resume, same-label 128-frame accumulation, failed-save retry without duplicate accumulation, legacy/CFP durable reload, shared 5% blending with retry deduplication, and receiver mismatch passed; synthetic host only.");
    return 0;
}
