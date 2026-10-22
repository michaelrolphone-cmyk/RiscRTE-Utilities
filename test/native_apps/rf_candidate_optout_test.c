/* Genuine text-input-off production RF guard coverage. Synthetic IQ/storage only.
 * The existing shared-input fixture cannot be used: it forces the option on.
 * Callback harness below is deliberately independent of that fixture. */
#ifdef PORTABLE_RF_SHARED_TEXT_INPUT
#error This fixture requires the production local-keyboard path
#endif
#define PORTABLE_CONTEXTS_LIVE() true
#define PORTABLE_CONTEXTS_CHECK_LIVE() do{}while(0)
#define portable_contexts_callback(value) (value)
#include "../../Apps/waterfall.c"
#include <assert.h>
#include <stdlib.h>
static t5_app_api_v1 api;
static risc_runtime_api_v1 rt;
static uint32_t tick;
static unsigned captures,radio_suspends,puts_count,store_gets;
static unsigned data_acquires,data_releases,data_stats,data_writes,io_calls;
static bool fail_put,deny_data,radio_grant,store_live,policy_live,data_live;
static bool radio_failure,radio_custody,storage_release_failure,storage_custody;
static struct {char key[16];uint8_t bytes[2048];uint32_t size;} cells[24];
static struct {char name[32];uint8_t bytes[72000];uint32_t size;uint64_t revision;} files[4];
static void require_live(void){assert(!radio_custody&&!storage_custody);io_calls++;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return &rt;}
const t5_app_api_v1 *t5_app_get_api(uint32_t v){assert(v==1);return &api;}
static int32_t get_value(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){
 require_live();*n=0;
 if(c==(void*)1){assert(policy_live&&!strcmp(k,"quick_radio")&&cap==4);uint8_t bytes[]={0x51,1,3,3^0xa5};memcpy(out,bytes,4);*n=4;return 0;}
 assert(store_live);store_gets++;for(unsigned i=0;i<24;i++)if(!strcmp(cells[i].key,k)){*n=cells[i].size;if(*n>cap)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,cells[i].bytes,*n);return 0;}return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t put_value(void*c,const char*k,const void*b,uint32_t n){
 (void)c;require_live();assert(store_live&&strlen(k)<16&&n<=2048);puts_count++;if(fail_put)return RISC_KEY_VALUE_IO;
 unsigned i=0;for(;i<24&&cells[i].key[0]&&strcmp(cells[i].key,k);i++);assert(i<24);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;
}
static const risc_key_value_v1 kv={2,sizeof(kv),NULL,get_value,put_value},policy={1,sizeof(policy),(void*)1,get_value,put_value};
static unsigned file_index(const char*n){for(unsigned i=0;i<4;i++)if(!strcmp(files[i].name,n))return i;for(unsigned i=0;i<4;i++)if(!files[i].name[0]){assert(strlen(n)<32);strcpy(files[i].name,n);return i;}abort();}
static int32_t stat_data(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;require_live();assert(data_live);data_stats++;unsigned i=file_index(n);*s=files[i].size;*r=*s?files[i].revision:0;return *s?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t read_data(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*s,uint64_t*r){(void)c;require_live();assert(data_live);unsigned i=file_index(n);*s=*r=0;if(expected!=files[i].revision)return RISC_APP_DATA_STALE;assert(cap>=files[i].size);*s=files[i].size;*r=expected;memcpy(b,files[i].bytes,*s);return 0;}
static int32_t replace_data(void*c,const char*n,uint64_t expected,const void*b,uint32_t z){(void)c;require_live();assert(data_live&&!owned&&!running);unsigned i=file_index(n);assert(z<=sizeof(files[i].bytes)&&expected==(files[i].size?files[i].revision:0));memcpy(files[i].bytes,b,z);files[i].size=z;files[i].revision++;data_writes++;return 0;}
static const risc_app_data_v1 data_api={1,sizeof(data_api),NULL,stat_data,read_data,replace_data};
static bool clean_radio(void*c){(void)c;assert(!storage_custody&&radio_grant);io_calls++;radio_suspends++;radio_custody=radio_failure;return !radio_failure;}
static int legacy_burst(void*c,uint32_t*p,uint32_t n){(void)c;require_live();assert(radio_grant&&n==256);for(unsigned i=0;i<n;i++)p[i]=(i%4==0)?256u:(i%4==1)?(256u<<10):(i%4==2)?768u:(768u<<10);captures++;return 0;}
static bool radio_caps(void*c,risc_radio_iq_capabilities_v1*out){(void)c;require_live();assert(radio_grant);*out=(risc_radio_iq_capabilities_v1){.struct_size=sizeof(*out),.min_pairs=1,.max_pairs=8192};return true;}
static int configured_burst(void*c,uint32_t*p,uint32_t n,const risc_radio_iq_settings_v1*s,risc_radio_iq_format_v1*f){
 int rc=legacy_burst(c,p,n);*f=(risc_radio_iq_format_v1){.struct_size=sizeof(*f),.flags=RISC_RADIO_IQ_FLAG_COHERENT_BURST,.center_hz=s->center_hz,.sample_rate_hz=s->sample_rate_hz,.bandwidth_hz=s->bandwidth_hz,.pair_count=n,.sample_format=RISC_RADIO_IQ_FORMAT_S10_I0_Q10,.component_bits=10,.component_full_scale=512};return rc;
}
static const risc_radio_iq_extended_api_v1 radio={.base={.base={1,sizeof(radio),NULL,legacy_burst,clean_radio}},.capabilities=radio_caps,.capture_configured=configured_burst};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 require_live();assert(g->struct_size==sizeof(*g));
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){if(v==1){assert(i==1&&!policy_live);policy_live=true;g->api=&policy;}else{assert(v==2&&i==RF_STORAGE_INSTANCE&&!store_live);store_live=true;g->api=&kv;}}
 else if(!strcmp(n,RISC_APP_DATA_CAPABILITY)){assert(v==1&&i==RF_APP_DATA_INSTANCE&&!data_live);if(deny_data)return false;data_acquires++;data_live=true;g->api=&data_api;}
 else if(!strcmp(n,"radio.iq")){assert(v==1&&!i&&!radio_grant);radio_grant=true;g->api=&radio;}
 else return false;
 g->slot=1;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){
 require_live();assert(g->api);
 if(g->api==&kv){assert(store_live);if(storage_release_failure){storage_custody=true;return false;}store_live=false;}
 else if(g->api==&policy){assert(policy_live);policy_live=false;}
 else if(g->api==&data_api){assert(data_live);data_live=false;data_releases++;}
 else{assert(g->api==&radio&&radio_grant&&!running&&!owned);radio_grant=false;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static uint32_t millis(void){require_live();return tick;}
static void set_back(bool exits){(void)exits;require_live();}
static int32_t screen_size(void){return 240;}
static void direct_setup(void){
 tick=1000;captures=radio_suspends=puts_count=store_gets=data_acquires=data_releases=data_stats=data_writes=io_calls=0;
 fail_put=deny_data=radio_grant=store_live=policy_live=data_live=radio_failure=radio_custody=storage_release_failure=storage_custody=false;
 memset(cells,0,sizeof(cells));memset(files,0,sizeof(files));
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};
 api=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(api),.millis=millis,.set_back_exits_app=set_back,.screen_width=screen_size,.screen_height=screen_size};
 app=waterfall_api=&api;runtime=waterfall_runtime=&rt;rf_bulk=&rf_resident_bulk;memset(rf_bulk,0,sizeof(*rf_bulk));
 storage=NULL;waterfall_radio=NULL;preferences=NULL;radio_acquired=preferences_acquired=store_acquired=false;
 waterfall_uncertain=uncertain=running=owned=waterfall_enabled=capture_requested=false;paper=NULL;ui_height=240;
 shared_keyboard=false;pending_save=load_errors=0;edit_slot=undo_slot=-1;page=PAGE_MAIN;toast_message=NULL;
 restore();signature_restore();event_restore();cfa_restore();configure_dsp();build_palette();set_page(PAGE_MAIN);
 assert(!data_live&&data_acquires==data_releases);
 /* Compile and exercise the actual local keyboard, not an undefined-macro claim. */
 void (*keyboard_draw)(void)=draw_keyboard;assert(keyboard_draw);
}
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
    direct_setup();
    strcpy(editing.name,"Local");keyboard_begin();assert(page==PAGE_KEYBOARD);
    keyboard_activate(0);assert(strcmp(editing.name,"Local"));
    keyboard_cancel();assert(page==PAGE_LABEL&&!strcmp(editing.name,"Local"));
    labeled_signature_capture(RF_SIGNATURE_ROOM);
    labeled_signature_capture(RF_SIGNATURE_EVENT);
    shared_named_capture();
    identity_guard_tests();
    puts("RF genuine text-input-off labeled room/event capture: name trimming, 64-frame capture, pause/resume, same-label 128-frame accumulation, failed-save retry without duplicate accumulation, legacy/CFP durable reload, shared 5% blending with retry deduplication, and receiver mismatch passed; synthetic host only.");
    return 0;
}
