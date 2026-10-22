# Labeled acoustic capture qualification

Task: Capture labeled acoustic samples (d2c44a6f-fc0a-44af-89c5-8bac3e3f9bba, T027).
Observed 2026-10-21 UTC. This is bounded source/host qualification, not firmware or physical acceptance.

## Result

Capture and accumulation already exist. No capture, neural, Contexts, storage or RF implementation was duplicated or changed. A test-only increment adds real-PCM repeated-label capture and save-retry checks, preserving every pre-existing assertion. It passes on the observed Utilities base and on the delivered Utilities source when compiled with the existing Spectrum test header pin.

The three existing paths are distinct:
- Legacy spectral samples collect 100 canonical 512-sample frames, nominally 3.2 seconds at 16 kHz. Repeated samples retain exact power sums/frame counts and a frame-weighted arithmetic mean, not an average of dB values. Capacity, unread slots, cancellation, frozen/pause behavior and uncertain-write retry are covered.
- Temporal events retain separate positive/nonmatch examples per named label, rather than collapsing event sequences into a mean.
- Shared Contexts room/event profiles collect room windows over at least 30 seconds or at least three event captures. Same-session windows are averaged; a later explicit session adapts the existing profile with the already-implemented 5% blend. The new tests verify that behavior without changing training policy.

## Exact delivered provenance

Read-only decoding of the existing Watch 1.1.46 store yielded 125 files. Store SHA-256: e50d26000dc0366be61abbf480cd39ee98323ea5ef489536baf0cf7c4097b126 (5,570,560 bytes).

- Audio Spectrum 0.5.8 ELF: 230,644 bytes, SHA-256 914eb41db2494643a903f2af55a100f11f9866c6f668ff835602588ef17d0a31.
- Manifest: 1,567 bytes, SHA-256 3307b5a79070317954ae6c4012df8213a7371f897bf1f793b4270ed5675caaba.
- Manifest and boot grants include audio.input@1, app-data namespace 2, legacy KV@2 namespace 7 and shared Contexts fingerprint access.
- The inherited replay recipe exactly matches this ELF size/hash. It identifies Utilities ca7fe82f43adc4ad8a8fa477a5336c8f5c693770, System 96742a20af660808cae6cd3a36eadd12a83a460f, Watch 6e82fdcaec0b73ed700d2104cfb4d94f011c19d4 and Runtime d493ca3ede561d410a858440ee2db9073c963426. It enables PORTABLE_CONTEXTS_CLIENT, the Watch ContextsFaces/alarm profile, shared text input, custody, the production adapter and other Watch flags. It does not enable PORTABLE_SPECTRUM_NATIVE_UI.

The 1.1.46 default-build source lock is not the producer source for this inherited Spectrum payload. No target-byte rebuild was performed. The decoded ELF/manifest and original recipe are retained in delivered-spectrum.json.

Observed Utilities base: 6b2047500905f8f45087cb49989103a926fc5c63. This is a locally observed checkpoint, not a new assertion about the latest remote main. The legacy capture/store, temporal controller, shared library and fingerprint files are byte-identical to delivered ca7fe82; audio_spectrum.c itself differs for newer native UI. Hash comparisons and the full app diff are retained.

## New test coverage

Only test/native_apps/shared_signal_library_test.c changes:
- Collect another real-PCM room session under the existing Office label, with a different signal.
- Verify unchanged saved bank before Save, same label/slot, the existing exact 5% power/background blend, one generation/window increment, and unchanged other profiles.
- Inject full storage; repeat failed Save and verify no duplicate blend/count or durable revision change.
- Retry with an uncertain commit; verify exact readback resolves it without a second logical accumulation.
- Decode/reopen the saved profile and verify persisted identity/count/power.
- Retain all original room/event PCM, named profile, background/raw-invariance, persistence and cleanup assertions.

## Executed checks

All listed passing host checks use deterministic provider fixtures. Normal/sanitizer counts are executions, not physical trials. LeakSanitizer was disabled by the caller; it did not pass.

1. Observed-base full Spectrum runner: 26 fixtures in normal and ASan/UBSan modes, 52 executions passed. Log: observed-base-spectrum.log.
2. Original shared signal baseline: two executions passed. Log: observed-base-shared-library.log.
3. Expanded shared signal fixture: normal and ASan/UBSan passed on observed base and delivered Utilities, four executions. Logs: expanded-shared-library.log and delivered-expanded-shared-library.log.
4. Independent reviewer repeated the expanded observed-base pair successfully. Log: independent-shared-capture-rerun.log.
5. Delivered Utilities focused legacy capture: six fixtures/twelve executions passed, covering exact arithmetic/codec, room matching after accumulation, label controller save/retry/cancel/capacity, temporal examples, interrupted live learning and slow-display cadence. Log: delivered-focused-capture.log; reproducible runner: run_delivered_capture.py.
6. Storage lifecycle: observed-base Utilities with System f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a passed twice. Delivered Utilities with frozen System 96742a20 passed twice. These separate fixtures explicitly model Contexts hooks and single-slot app/shared-data custody. Logs: observed-base-storage.log and delivered-storage.log.
7. Python aggregate: 269 tests passed in 100.666 seconds. Log: python-final.log. Tag/publication-looking lines are mocked tests, not publication.
8. git diff --check passed.

The full Spectrum, expanded shared and focused capture fixtures use System header pin fdc15395928a1fa8eee62a2823b3c8777dcf0fa0, matching the existing Spectrum test workflow. They test the specified Utilities source, not the entire released System/Watch composition.

Important preserved failure: the independent attempt to compile the generic shared capture fixture against frozen System 96742a20 failed because that fixture does not supply PORTABLE_CONTEXTS_CHECK_LIVE, portable_contexts_callback or PORTABLE_CONTEXTS_LIVE required by PortableFingerprintClock.h. Log: independent-frozen-shared-capture-rerun.log. This is a fixture/composition limitation, not a demonstrated firmware failure. It was not hidden, worked around or called a full frozen-composition pass.

Rerun expanded pair from the observed-base checkout:

    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python scripts/test_shared_signal_library.py --system-apps ../../acoustic-neural/RiscRTE-System-Apps

Other original commands:

    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python scripts/test_audio_spectrum.py --system-apps ../../acoustic-neural/RiscRTE-System-Apps
    python scripts/test_spectrum_storage_lifecycle.py --system-apps ../../acoustic-neural/system-current
    python -m unittest discover -s tests -v

## Capture-duration copy reconciliation

Apps/spectrum_signature_app.inc:146 still describes 64 room frames or a current 32 ms event frame. docs/apps/audio_spectrum.md:130–132 repeats that old description. Actual signature_capture_begin sets needed/goal to 100 for both. The shipped non-native 0.5.8 recipe therefore retains stale explanatory text.

The newer intended native composition already renders the correct “100 FRAMES / 3.20 SEC” in Apps/spectrum_native_subpages.inc:88. audio_spectrum.c:525–527 intercepts draw through spectrum_native_draw, and native subpages handle PAGE_SIGNATURE_EDIT at line 270, bypassing the stale renderer. Its build script enables PORTABLE_SPECTRUM_NATIVE_UI. No current native-copy repair is needed. The historical legacy copy is recorded, not altered, and frozen released bytes are untouched.

## Remaining qualification and boundaries

- T040 owns unpublished commit 601fa56324e1a1c0c4b4f6d51b13b6a8c6ae54e2, guarding neural checkpointing during shared capture. It was not copied, modified or incorporated here. Its integration/publication remains separately held; these checks do not prove that the known neural-promotion collision is resolved in either baseline.
- Neither Xtensa compiler name is installed. No target build, exact-byte rebuild, remote CI, full deployed native/Contexts composition or physical microphone/storage/display run occurred.
- Exact-source integration followed by on-device named capture, same-label addition, pause/cancel, durable relaunch, failed-save recovery and no sample loss remains open. No immediate device action is requested.
- X4's RF-only profile is not acoustic support. No X4 acoustic claim is made.
- No push, PR, merge, tag, release, catalog, product-pin, target flash, radio or device action occurred. Existing T040 and other workspaces remain unchanged.

The task should remain awaiting qualification, not completed. This test-only patch is unpublished; the inherited capture implementation's published status is distinct.

Local patch commit: 117bc544e70d2a08f6fcd7532de1289e50497e34 on tasker/labeled-acoustic-qualification-20261021.

The existing frozen storage fixture was inspected for a composed capture rerun. It owns a separate single-slot native_data backend with a 32-byte disk and its own custody callbacks; the capture fixture owns four 72 KB banks and different acquisition callbacks. Reusing it would require broader multi-namespace/Context backend redesign. No no-op lifetime guards were substituted.
