/* Included by rf_labeled_capture_test.c. All observations are synthetic. */
static uint8_t guard_file_snapshot[sizeof(files)],guard_cell_snapshot[sizeof(cells)];
static cf_bank guard_bank_snapshot;
static unsigned guard_puts,guard_writes;
static void guard_snapshot(void) {
    memcpy(guard_file_snapshot,files,sizeof(files));
    memcpy(guard_cell_snapshot,cells,sizeof(cells));
    guard_bank_snapshot=cfa_bank;guard_puts=puts_count;guard_writes=data_writes;
}
static void guard_no_persisted_mutation(void) {
    assert(puts_count==guard_puts&&data_writes==guard_writes);
    assert(!memcmp(files,guard_file_snapshot,sizeof(files)));
    assert(!memcmp(cells,guard_cell_snapshot,sizeof(cells)));
    /* Byte comparison here detects mutation of one snapshotted object, not
     * semantic equality between separately initialized padded C structs. */
    assert(!memcmp(&cfa_bank,&guard_bank_snapshot,sizeof(cfa_bank)));
}
static void guard_set_omitted(unsigned field) {
    if(field==0)prefs.identity.lo_mode=1;
    else if(field==1)prefs.identity.settings_id=1;
    else prefs.identity.settings_flags=6;
    assert(rf_identity_valid(&prefs.identity));
    assert(!cfa_capture_identity_eligible(&prefs.identity));
    uint8_t encoded[RF_PREFERENCES_SIZE],again[RF_PREFERENCES_SIZE];
    rf_preferences decoded={0};
    assert(rf_preferences_encode(&prefs,encoded));
    assert(rf_preferences_decode(&decoded,encoded,sizeof(encoded)));
    assert(rf_identity_equal(&decoded.identity,&prefs.identity));
    assert(rf_preferences_encode(&decoded,again)&&!memcmp(encoded,again,sizeof(encoded)));
    prefs=decoded;
    rf_signature legacy={.kind=RF_SIGNATURE_ROOM,.identity=prefs.identity,.frames=1};
    strcpy(legacy.name,"Legacy metadata");legacy.sums[0]=123;
    uint8_t record[RF_SIGNATURE_RECORD_SIZE];rf_signature loaded={0};
    assert(rf_signature_encode(&legacy,record)&&rf_signature_decode(&loaded,record,sizeof(record)));
    same_signature(&legacy,&loaded);assert(rf_identity_equal(&loaded.identity,&prefs.identity));
}
static void guard_seed_saved(void) {
    capture_setup();signature_open(-1,RF_SIGNATURE_ROOM);strcpy(editing.name,"Guard RF");
    signature_capture_begin();assert(signature_goal==64);use_labeled_provider();observe_bursts(64);
    assert(signatures[0].frames==64&&!signature_pending&&!cfa_changed);
    assert(cf_profile_find(&cfa_bank,CF_ROOM,"Guard RF")>=0);
    stop();
}
static void guard_all_omitted_fields(void) {
    for(unsigned field=0;field<3;field++) {
        guard_seed_saved();rf_capture_identity original=prefs.identity;
        assert(cfa_capture_identity_eligible(&original));assert(cfa_identity()==2931434081u);
        guard_snapshot();guard_set_omitted(field);
        /* Historical hash/codecs intentionally stay byte-compatible. The
         * prospective eligibility guard, not a new hash, closes this path. */
        assert(cfa_identity()==2931434081u&&!rf_identity_equal(&original,&prefs.identity));
        cfa_begin();assert(!cfa_identity_current());
        signature_open(-1,RF_SIGNATURE_ROOM);strcpy(editing.name,"Rejected");
        signature_capture_begin();assert(!signature_goal&&!running);
        shared_kind=CF_ROOM;shared_selected=-1;strcpy(shared_name,"Rejected");set_page(PAGE_SHARED_EDIT);
        shared_tap(120,135);assert(!cfa_learning_kind&&!running);
        event_edit_slot=0;event_arm(RT_POSITIVE);assert(!event_armed);
        unsigned old_captures=captures;toggle();capture();assert(!running&&captures==old_captures);
        assert(toast_message&&strstr(toast_message,"RF"));
        uint32_t pairs[256]={0};risc_radio_iq_envelope_v1 envelope={.timestamp_us=1000000};
        cfa_iq(pairs,256,80000000,1000);cfa_envelope(&envelope);cfa_observe();
        cf_feature feature={.flags=CF_SPECTRAL,.identity=cfa_identity(),.windows=1};
        cfa_learn_observe(&feature,false);cfa_segment(1000000,false);cfa_event_boundary(true,true,true);
        assert(!cfa_confirm(0,"Guard RF",CF_ROOM));assert(!cfa_save_event(0,"Guard RF",true));
        assert(!cfa_learn_save());assert(!cfa_publish_saved(0));
        assert(!cfa_filter_select(1,"Guard RF"));
        signature_mode=2;strcpy(cfa_manual_name,"Guard RF");manual_room=0;
        assert(cfa_background()==NULL); /* Includes legacy fallback. */
        cfa_apply();assert(room_tracker.selected<0&&event_slot<0&&!event_match.running);
        assert(!strcmp(cfa_name(CF_ROOM,"fallback"),"RF SETTINGS UNSUPPORTED"));
        cfa_shared_names();cfa_forget(0);cfa_rename_event(0,"Rejected");
        cfa_save();signature_save();event_retry();event_save_capture();
        guard_no_persisted_mutation();
        /* Full-identity legacy records remain recoverable, not rewritten. */
        signature_restore();assert(signatures[0].frames==64);
        assert(rf_identity_equal(&signatures[0].identity,&original));
        guard_no_persisted_mutation();
    }
}
static void guard_partial_capture_transition(void) {
    capture_setup();signature_open(-1,RF_SIGNATURE_ROOM);strcpy(editing.name,"Partial");
    signature_capture_begin();use_labeled_provider();observe_bursts(16);
    rf_capture_identity original=prefs.identity;rf_signature draft=signature_capture;
    guard_snapshot();prefs.identity.settings_id=1;capture();
    assert(!running&&signature_goal==64);same_signature(&draft,&signature_capture);
    assert(cfa_room.slot<0&&cfa_event.slot<0&&!cfa_fusion.have[CFA_SOURCE]);
    prefs.identity=original;toggle();assert(!running);
    signature_commit_capture();same_signature(&draft,&signature_capture);guard_no_persisted_mutation();
    bool requested=false;set_page(PAGE_SIGNATURES);signature_tap(170,125,&requested);
    assert(!signature_goal&&!signature_capture.frames);
    signature_open(-1,RF_SIGNATURE_ROOM);strcpy(editing.name,"Fresh");signature_capture_begin();
    assert(signature_goal==64&&!signature_capture.frames&&running);
    use_labeled_provider();observe_bursts(64);assert(signatures[0].frames==64&&!signature_pending);
    stop();
}
static void guard_shared_transition(void) {
    capture_setup();shared_kind=CF_ROOM;shared_selected=-1;strcpy(shared_name,"Partial shared");
    set_page(PAGE_SHARED_EDIT);shared_tap(120,135);use_labeled_provider();observe_bursts(310);
    assert(cfa_learn_ready()&&!cfa_learning_saved);unsigned samples=cfa_learning_samples;
    cf_feature draft=cfa_learning.feature[CFA_SOURCE];rf_capture_identity original=prefs.identity;
    guard_snapshot();prefs.identity.lo_mode=1;capture();assert(!running);
    assert(cfa_learning_samples==samples);same_feature(&draft,&cfa_learning.feature[CFA_SOURCE]);
    prefs.identity=original;assert(cfa_background()==NULL);assert(!cfa_learn_save());
    toggle();assert(!running);guard_no_persisted_mutation();
    set_page(PAGE_SHARED_CAPTURE);shared_tap(120,212);assert(!cfa_learning_kind&&!cfa_learning_samples);
    shared_tap(120,135);assert(cfa_learning_kind==CF_ROOM&&running&&!cfa_learning_samples);
    use_labeled_provider();observe_bursts(310);assert(cfa_learn_ready());assert(cfa_learn_save());
    int slot=cf_profile_find(&cfa_bank,CF_ROOM,"Partial shared");assert(slot>=0);
    assert(cfa_bank.generation==1);stop();
}
static void guard_pending_signature_retry(void) {
    capture_setup();signature_open(-1,RF_SIGNATURE_ROOM);strcpy(editing.name,"Pending");
    signature_capture_begin();use_labeled_provider();fail_put=true;observe_bursts(64);
    assert(signature_pending==1&&signatures[0].frames==64&&cfa_changed);
    rf_signature pending=signatures[0];rf_capture_identity original=prefs.identity;
    guard_snapshot();prefs.identity.settings_flags=6;signature_save();guard_no_persisted_mutation();
    same_signature(&pending,&signatures[0]);assert(signature_pending==1);
    prefs.identity=original;fail_put=false;signature_save();
    assert(!signature_pending&&!cfa_changed);same_signature(&pending,&signatures[0]);
    assert(cfa_bank.generation==guard_bank_snapshot.generation);
    stop();signature_restore();same_signature(&pending,&signatures[0]);
}
static void guard_pending_shared_retry(void) {
    capture_setup();shared_kind=CF_ROOM;shared_selected=-1;strcpy(shared_name,"Pending shared");
    set_page(PAGE_SHARED_EDIT);shared_tap(120,135);use_labeled_provider();observe_bursts(310);
    deny_data=true;assert(!cfa_learn_save()&&cfa_learning_saved&&cfa_changed);
    rf_capture_identity original=prefs.identity;unsigned generation=cfa_bank.generation;
    guard_snapshot();prefs.identity.settings_id=1;assert(!cfa_learn_save());cfa_save();guard_no_persisted_mutation();
    prefs.identity=original;deny_data=false;assert(cfa_learn_save());
    assert(!cfa_changed&&!cfa_learning_kind&&cfa_bank.generation==generation);
    assert(!memcmp(&cfa_bank,&guard_bank_snapshot,sizeof(cfa_bank)));stop();
}
static void guard_idle_return_and_supported_identity(void) {
    guard_seed_saved();rf_capture_identity original=prefs.identity;
    signature_mode=2;strcpy(cfa_manual_name,"Guard RF");assert(cfa_background()!=NULL);
    guard_snapshot();prefs.identity.settings_id=1;assert(cfa_background()==NULL);
    prefs.identity=original;assert(cfa_background()==NULL);
    assert(cfa_prepare_identity());assert(cfa_background()!=NULL);guard_no_persisted_mutation();
    prefs.identity.raw_gain++;assert(cfa_capture_identity_eligible(&prefs.identity));
    assert(cfa_prepare_identity()&&cfa_identity()!=2931434081u&&cfa_background()==NULL);
    guard_no_persisted_mutation();
}
static void guard_receiver_veto(void) {
    capture_setup();shared_kind=CF_ROOM;shared_selected=-1;strcpy(shared_name,"Receiver veto");
    set_page(PAGE_SHARED_EDIT);shared_tap(120,135);use_labeled_provider();
    receiver_caps.controls|=RISC_RADIO_IQ_CONTROL_GAIN;receiver_caps.gain_selector_max=127;
    rf_capture_identity original=prefs.identity;unsigned pending=pending_save;
    guard_snapshot();assert(!receiver_change(3,1));
    assert(rf_identity_equal(&prefs.identity,&original)&&pending_save==pending);guard_no_persisted_mutation();
    observe_bursts(310);deny_data=true;assert(!cfa_learn_save());
    cfa_learning_kind=0; /* Isolate the dirty-bank veto from active-learning veto. */
    guard_snapshot();assert(!receiver_change(3,1));
    assert(rf_identity_equal(&prefs.identity,&original)&&pending_save==pending);guard_no_persisted_mutation();
    deny_data=false;cfa_save();assert(!cfa_changed);cfa_learning_saved=false;
    receiver_extended=true;receiver_caps.controls|=RISC_RADIO_IQ_CONTROL_GAIN;receiver_caps.gain_selector_max=127;
    assert(receiver_change(3,1));assert(prefs.identity.raw_gain==original.raw_gain+1);stop();
}
static void identity_guard_tests(void) {
    rf_capture_identity identity=rf_identity_default();
    assert(cfa_capture_identity_eligible(&identity));
    for(unsigned flags=0;flags<7;flags++){identity.settings_flags=flags;assert(rf_identity_valid(&identity)&&!cfa_capture_identity_eligible(&identity));}
    identity=rf_identity_default();identity.settings_id=UINT32_MAX;
    assert(rf_identity_valid(&identity)&&!cfa_capture_identity_eligible(&identity));
    identity=rf_identity_default();identity.lo_mode=UINT32_MAX;
    assert(rf_identity_valid(&identity)&&!cfa_capture_identity_eligible(&identity));
    guard_all_omitted_fields();
    guard_partial_capture_transition();
    guard_shared_transition();
    guard_pending_signature_retry();
    guard_pending_shared_retry();
    guard_idle_return_and_supported_identity();
    guard_receiver_veto();
    puts("RF omitted lo_mode/settings_id/settings_flags: explicit rejection, unchanged persisted preferences/signatures/CFP, ingest/train/save/match/background/bridge guards passed.");
}
