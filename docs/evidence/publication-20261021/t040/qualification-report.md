# T040 acoustic neural matcher: current-source reconciliation

Observed on 2026-10-21. This is a source and host-test assessment, not device qualification.

## Outcome

The requested acoustic neural matcher already exists in current Utilities source. Reconciliation found one remaining integration defect: idle neural promotion could checkpoint and close the microphone during the newer shared room/event capture. A narrow local audio-only guard fixes that defect without reimplementing the matcher. The base is current main `6b2047500905f8f45087cb49989103a926fc5c63`.

The implementation came through [Utilities PR37](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/37), merged on 2026-10-07. Its final source checkpoint is `47f311513a36048d7262ba12012654ad7c4c1696`. The neural core, cache codec and three pure neural test sources are byte-for-byte identical between that checkpoint and current main. The existing Contexts inference upgrades and subsequent native UI qualification remain present. Only the audio app’s shared-capture idle guard was changed; the neural architecture, training, codec, Contexts algorithms and storage callbacks remain unchanged.

## Approved scope and mapping

- Small on-watch training for user-labeled acoustic examples: `Apps/spectrum_neural.h`, with eight bounded label slots and positive/negative examples. Its existing 80-input, 16-hidden-unit architecture is an implementation fact, not a newly imposed requirement. The complete trainer occupies 16,920 bytes in the host fixture.
- Interruptible idle work: one training example or one bounded validation tick per `sn_tick` call. `event_neural_tick` defers to recording, analysis, pending storage writes, room capture, input and audible foreground activity. Temporal matching remains available while training is incomplete.
- Held-out calibration: newest positive and newest negative per eligible label are excluded from training and normalization. Candidate promotion requires confusing-negative coverage and improvement without regression on both held-out recordings and a second check of all saved examples of eligible labels. The latter excludes each queried recording from its own reference match. The documentation was corrected to state that exact scope.
- Unknown/fallback behavior: only plausible ambiguous or confusing-negative temporal results may be refined. Untrained plausible rivals, unknown-region inputs, insufficient score/margin, and non-improving candidates keep the existing temporal result.
- Persistence: `spectrum-neural.snn` is an optional 6,436-byte derived checkpoint, saved once on promotion, with CRC, dimensions, finite bounds, held-out identity and complete collection generation/CRC validation. Source examples are unchanged. Full, stale, uncertain, corrupt and retained storage paths retain their explicit semantics.
- Contexts: current `Services/contexts/temporal_models.h` and `temporal_inference.h` import and use the existing owner’s canonical library/checkpoint. Contexts does not become a second trainer. Its later fingerprint integration and RF-only profile guards are preserved.
- Boundary: no speaker recognition, transcription, command execution, new radio-signature work or automatic sample creation was added.

The later explicit queue-selection message supersedes the earlier decision to defer neural work. The complete original research proposal was not available in this execution; the preserved task scope and verified later selection were used. The missing proposal was not treated as evidence of missing implementation.

## Preserved external work

- [PR50](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/50) documents bounded Contexts temporal/neural imports and source recovery. The PR remains open; current source was inspected directly rather than assuming PR merge status establishes absence. Its current descendants preserve the inference logic and add fingerprint handling and explicit RF-only guards.
- [PR82](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/82) is merged and qualifies Contexts native geometry/text lifecycle. Its execution is finished; no active duplicate neural implementation execution was found in the board’s 406-record execution inventory.
- [PR68](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/68) remains open, but its `5d8b116` transaction repair is present in current main. `rf_app_data_scope.h`, the storage regression and runner are identical to that repair. The temporal include differs only by the later native-custody retained guard. No repair was copied over current source or redone.

## Narrow repair and reproduction

The existing neural idle guard checked legacy event/room captures but omitted `cfa_learning_kind`, which the newer shared capture UI owns. On unmodified main, the new real-app fixture produced 1,839 neural ticks, one checkpoint write and one microphone close while a shared room capture was still active. Its invariant assertion failed with exit 134. This is a simulated production-controller reproduction, not the proven cause of any historical hardware capture complaint.

The local fix adds an audio-local accessor for the existing shared learning state and an early return in `event_neural_tick`. Shared room/event capture, paused capture and pending/failed Save keep that state nonzero. Successful Save, explicit Cancel, or confirmed Discard clear it through existing lifecycle paths, allowing normal neural learning and one-time checkpointing to resume.

The new matrix covers both room and event captures; quiet active and paused states; preparing and genuinely trained, pre-promotion candidates; plain Cancel; failed Save followed by retry; and failed Save followed by confirmed Discard. It verifies no neural progress/checkpoint microphone closure while capture owns priority, progress after release, and unchanged authoritative event banks. No shared/RF production file changed. Two independent source reviews found no blocking defect in the narrow fix.

## Verification

All executed checks below passed unless explicitly marked as the expected baseline failure. Normal and ASan/UBSan fixture counts are executions, not independent physical trials. LeakSanitizer was disabled by the caller.

- Before the production change: full Spectrum runner, 26 fixtures in two modes, 52 executions passed (`evidence/spectrum-baseline-suite.log`).
- Baseline reproduction: newly added real-app shared-capture invariant failed on unmodified production, exit 134. It reported neural ticks `0 -> 1839`, checkpoint writes `0 -> 1`, microphone closes `0 -> 1` (`evidence/neural-shared-baseline.log`).
- Final full Spectrum runner: all 52 normal/sanitizer executions passed, including the final six-case shared-capture fixture (`evidence/spectrum-final-suite.log`). Command: `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 900 python scripts/test_audio_spectrum.py --system-apps ../RiscRTE-System-Apps`.
- Final focused six-case shared-capture fixture separately compiled with `-std=c11 -O1 -g -Wall -Wextra -Werror`, then ran normal and ASan/UBSan under a 120-second limit. Both passed (`evidence/neural-shared-all-normal-final.log`, `evidence/neural-shared-all-sanitized-final.log`). Sanitizer flags included `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -no-pie`.
- Unchanged Contexts production: ten normal/sanitizer executions passed. Command: `timeout 600 python scripts/test_contexts_service.py --drivers ../RiscRTE-Drivers --runtime ../RiscRTE --system-apps ../system-current` (`evidence/contexts-suite.log`).
- Contexts public contract check and four focused Python contract tests passed (`evidence/contexts-contract.log`).
- Storage lifecycle: two normal/sanitizer executions passed before and after the fix. Final command: `timeout 180 python scripts/test_spectrum_storage_lifecycle.py --system-apps ../system-current` (`evidence/storage-final-suite.log`).
- Shared-input safety: two normal/sanitizer executions passed. Command: `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 300 python scripts/test_spectrum_shared_input_safety.py --system-apps ../system-current` (`evidence/shared-input-final.log`, `evidence/shared-input-evidence.json`). No baseline parity option was used because the two audio production files intentionally changed.
- Full Python aggregate: 269 tests passed in 136.780 seconds. Command: `python -m unittest discover -s tests -v` (`evidence/python-final-suite.log`). Publication/tag-looking lines in that log are mocked test output, not remote publication.
- `git diff --check` passed; final checkout is clean. Local commit: `601fa56324e1a1c0c4b4f6d51b13b6a8c6ae54e2`, branch `tasker/t040-neural-reconcile-20261021`.

Patch: `T040-shared-capture-neural-guard.patch`, SHA-256 `2c26b8f015172185cb3e05924ff9aa0f356f1fc0d36cd8c1e4bf5e3b99aab6ae`. Draft publication text: `T040-draft-PR.md`. Neither has been published.

Exact dependency pins used:
- Legacy Spectrum headers: System Apps `fdc15395928a1fa8eee62a2823b3c8777dcf0fa0`.
- Contexts/storage headers: System Apps `f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a`.
- Contexts Drivers: `aac1cb342696e96345d4546e2c69023834c6c8d4`.
- Contexts Runtime: `3b90cd11cf0ff4e8116bb54d2d9eb9c7c67a148b`.

The Spectrum suite uses its workflow’s exact legacy header pin. The storage fixture used the newer Contexts header pin, not PR68’s original System pin. All repository checkouts were isolated; the T039 and Spectrum repair branches were not modified.

## Remaining qualification

The generated-PCM result of 4/4 held-out decisions versus 2/4 temporal-only decisions, 10/10 stored examples and nine fresh synthetic positive/unknown recordings is regression evidence, not measured acoustic accuracy.

No target firmware rebuild, exact currently installed product composition verification, physical microphone trial, on-watch training-time/heap/energy measurement or hardware acceptance occurred. Those remain needed before calling the task physically qualified. A qualification run should identify the exact Watch build first, use genuine taught positives and confusing nonmatches, verify unknown/fallback behavior and interruption/relaunch persistence, and measure the device’s resource cost. X4’s RF-only profile must not be described as acoustic support.

The local patch and draft-PR text are preparation only. No branch was pushed; no PR, release, product pin, catalog, frozen artifact or hardware state was changed. The app manifest remains unchanged because no replacement package was built or published; any later package delivery must coordinate its new version with the canonical owner.
