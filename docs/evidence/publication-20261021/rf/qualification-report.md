# Labeled RF capture: prospective identity safety repair

Scope: task e42e6e9e-55db-4753-b64e-46a00b9ebbd8, T033. This is local host qualification of an observed source base, not completion of the RF feature, a target build, historical-data repair, publication, or hardware acceptance.

## Existing delivered capability

The existing RF controller captures named room/event signatures over 64 synthetic-tested transforms, accumulates additional same-label compatible samples, retains paused partial work, and retries pending saves without duplicate accumulation. Its full legacy identity codec already retains receiver metadata. Shared ContextFingerprint room/event learning is also present; same-name shared additions blend at 5%, with a separate flag preventing duplicate blending on save retry. No new RF observations were gathered.

The locally available, unchanged T-Watch 1.1.46 store has SHA-256 e50d26000dc0366be61abbf480cd39ee98323ea5ef489536baf0cf7c4097b126. Read-only extraction found Waterfall 0.3.8, 243016 bytes, SHA-256 2ea64e7e52452ffa5a94cf31a70f3d8ac951da272a1e8c730c48322196c4a479. This exactly matches the original launch-brightness handoff (product 1.1.39, inherited by 1.1.46). The older shared-app-crashes replay recipe describes Waterfall 0.3.7 with SHA-256 62c25516e32c7c5e3ff878dd90b52df6a2987fd97cdbfe27ce55390f8c7df86e; it is not the final 0.3.8 byte recipe. The delivered provider is s3-radio-iq 0.3.0, status experimental-unpublished, driver ELF SHA-256 e42951a6122ee5e43e7167ebdd0066febeb590a40229c269687706223e54a44e.

Original pinned producer System 24270c36cd4bc1edb7bb2e8d860501eaa5105d43 and earlier RF Utilities d987d2bbfeb02a516dbf5182b599e25346a7c5b3 are absent from inspected local Git object stores. Original receipts and final ELF hashes establish artifact provenance only, not an exact-source rebuild, current-main acceptance, or hardware behavior.

## Reproduced defect

Apps/rf_identity.h documents the complete receiver compatibility identity and says settings_id distinguishes provider/calibration revisions. lo_mode, settings_id and settings_flags are persisted and participate in full legacy equality. The CFP hash projects only the preceding twelve 32-bit fields.

On observed Utilities base 6b2047500905f8f45087cb49989103a926fc5c63, default identity and accepted preference round-trip settings_id=1 are unequal in the full legacy codec but both yield shared hash 2931434081. A retained negative probe asserts distinct hashes and aborts. This is a documented semantic discriminator being lost, not a random hash collision.

## Conservative prospective behavior

- Admit shared RF work only when omitted metadata is exactly historical defaults: lo_mode=0, settings_id=0, settings_flags=7. Other valid included settings remain available.
- Keep the default projection/hash, storage schemas, saved bytes and full-identity codecs unchanged. Do not relabel or migrate old CFP records.
- Guard capture start/ingestion, shared training/confirmation, matching/background use, persistence and Contexts bridge entrypoints. Unsupported capture returns an explicit error rather than falling back to legacy-only completion.
- Bind active shared evidence to the full receiver identity. An incompatible interval invalidates live fusion/matches/background evidence. Partial unconfirmed samples remain visible but require explicit cancellation, even after original settings return. Already-confirmed pending saves stay recoverable and can retry under the original identity without duplicate blending.

A hash extension only for non-default metadata would not restore provenance of old CFP files: their hash omitted that information, so historical non-default captures cannot be distinguished from default captures. This patch prevents future incompatible shared updates; it does not certify existing CFP provenance.

## Qualification boundaries

Host inputs are explicitly synthetic IQ and modeled Runtime/radio/storage callbacks. No RF/environmental samples were fabricated as observations. The controller fixture does not include the native renderer or a live Contexts service and does not qualify the complete product macro composition. Exact command, source/dependency hash, compiler and binary receipts accompany the local evidence. LeakSanitizer is disabled in this executor; AddressSanitizer and UndefinedBehaviorSanitizer are otherwise enabled in the sanitized runs.

The source composition is observed Utilities 6b204750 plus this patch with host System headers f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a. No claim of latest-main or target source equivalence is made. No target compiler was found. Publication and physical qualification remain outstanding and outside this local deliverable.

## Verified local results

- Focused labeled capture/guard: 4/4 ordinary and continuous host configurations, normal plus combined ASan/UBSan. Each executable records 43 actual project dependencies; 461 snapshotted source/header inputs remained unchanged during qualification.
- Existing RF shared-input/custody safety: 9/9 normal, ASan and UBSan executions across ordinary watch, continuous/resident-home and app-data scope fixtures.
- Existing RF models: 6/6 normal and combined ASan/UBSan executions covering DSP, full-identity codecs and accumulation, temporal/neural models.
- Shared audio controller safety: 2/2 normal and combined ASan/UBSan. No audio capture behavior change is intended.
- Diff whitespace check passes.

The focused fixture tests each omitted field, all unsupported settings_flags values, preserved non-default preference/signature codecs, the unchanged historical default hash, start-time rejection, shared ingress/training/save/match/background/bridge rejection, no persistent bank/file mutation, partial signature and shared-learning interruption, explicit cancel/fresh start, original-identity pending-save retry without reblend, supported included-field changes, and receiver-UI pre-mutation veto. Shared bridge rejection is tested at controller entrypoints, not as a live Contexts-service integration run.

Test-only changes are explicit: the fixture composes the existing controller safety fixture, acquires the existing preference-policy grant, supplies deterministic IQ amplitude variants, compares serialized signatures/features rather than C-struct padding, and explicitly restarts continuous capture after the existing custody suspension clears capture intent. It does not weaken production assertions or simulate a successful hardware recording. Both room and event capture accumulation plus the shared retry path use production controller functions.

The original collision probe and all failed fixture-development attempts are retained in the evidence bundle. A first SPIFFS decoder rejected the store size; read-only extraction succeeded using the original Watch decoder that explicitly supports that exact size. Neither decoder nor frozen bytes were altered.

## Reproduction

From the isolated Utilities checkout:

    python3 scripts/test_rf_labeled_capture.py --system-apps ../../acoustic-neural/system-current --output ../evidence/recheck
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 scripts/test_rf_shared_input_safety.py --system-apps ../../acoustic-neural/system-current --output ../evidence/recheck-safety
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 scripts/test_rf_models.py
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 scripts/test_spectrum_shared_input_safety.py --system-apps ../../acoustic-neural/system-current

Do not mix these receipts with other source compositions or another worker's qualification. In particular, this branch does not contain the separate unpublished T040 neural-repair commit.

Independent review examined all six production diffs and both capture/guard fixtures, found no further issue within this host-controller scope, and reran the four-mode focused matrix successfully. Its separate parent-independent-rerun/qualification.json records source_hashes_unchanged=true. The receiver-UI veto is in Apps/rf_controls.inc.
