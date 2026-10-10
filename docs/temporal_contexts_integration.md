# Temporal Contexts integration

This successor integrates the temporal RF/audio fingerprints into the Contexts
editor, background service, foreground spectrum applications and device shell.
It is a development build. Compilation, host tests and package admission do not
measure accuracy in physical rooms.

## Detection and training

RF uses a dedicated native IQ worker and an additive driver interface. Records
carry actual capture timestamps, observed duration, sequence and discontinuity
flags. The application drains a bounded ring and does not manufacture a 1 kHz
timeline from occasional FFT frames. Audio uses independent 1 ms PCM envelope
chunks. The original FFT paths remain available.

Both pipelines keep a rolling noise estimate, detect bursts and compare their
repetition intervals using a sparse, bounded 1 ms histogram over 0–10 seconds.
Matching combines histogram Earth Mover's Distance with normalized envelope and
spectral features. Ambiguous, insufficient or distant observations are unknown.
The two sources have independent enable state, warmup, aging and weights.

Room learning collects 30 seconds and at least three windows. Event learning
collects three separate takes, with 500 ms quiet boundaries and a 10 second
maximum event. Save explicitly commits the staged profile; Cancel discards it.
High-confidence adaptation blends 95% previous profile with 5% fresh data.
Repeated status polling cannot train the same observation repeatedly.

Open Contexts, enable the available sources, add a place or event, collect the
requested samples and Save. Add further samples at different times. The Watch
supports RF and audio; X4 uses RF because its supplied hardware has no microphone.
Timing-only matching is selectable. Names and learned profiles are shared with
the RF/audio applications and Watch clock labels.

## Rules, actions and storage

The editor supports prioritized rules with hold times, room/event conditions,
battery/time/charging conditions, supported device settings, and up to three
workflow steps (installed app, timer or dismissible message). Event edges run
once. The service retains the workflow cursor across app handoff. Radio and
storage ownership is released before conflicting actions.

Profiles use the source's existing app-data namespace (2 audio, 3 RF), scoped to
`context-fingerprints.cfp`. Rules use namespace 4, scoped to `context-rules.ctx`.
The new generic `storage.shared-data@1` grant authorizes only the named file.
It does not expose unrelated files or relax ordinary private namespace ownership.
Writes validate checksum and generation, refresh global revision tokens, and
resolve ambiguous commits by exact readback. Corrupt or unread data is preserved.

Watch eligible foreground applications and the default clock use the new shared
adapter. Bluetooth HID, firmware/network operations and explicit RF/audio
foreground capture keep their existing resource exclusion behavior. X4 Home
runs the shared rules; its Contexts and RF applications are resident clients.
Other retained X4 foreground binaries are unchanged and do not run this new
adapter; workflows resume at Home. Classification also pauses during sleep and
incompatible radio ownership. This is not always-on sensing during those states.

## Reproducible builds and evidence

`scripts/build_temporal_watch.py` builds the Watch app cohort. `clock.elf` is a
small compatibility entry point whose return invokes the configured default
clock, avoiding a second copy of all clock assets in the fixed boot partition.
`scripts/build_temporal_x4.py` builds the current X4 resident clients from explicit
System/runtime/display/alarm SDK inputs. Historical pinned builders are unchanged.
Both record actual compiler commands, dependency hashes, target exports/imports
and loader/compaction checks.

`scripts/package_temporal_cohort.py` pairs exact native firmware and boot store,
adds explicit file grants, checks production Runtime admission and native ELF
compatibility, constructs deterministic SPIFFS with free-block reserve, verifies
readback and bank metadata, and preserves all unrelated baseline bytes.
Outputs are full-initial 16 MiB images, not data-preserving update packages.

Core/service/rule tests: `scripts/test_context_fingerprints.py` with `--drivers`
and `--runtime` inputs; repeat with `SANITIZE=1`. Runtime tests cover IQ lifecycle
and real shared-file grant enforcement. System's `context_rule_storage_test.c`
covers stale global revisions, concurrent generations, corrupt data, retained
cleanup and ambiguous commits. No device has been flashed by these build steps.

Hardware qualification remains: collect labeled rooms and events on separate
days, include unknown rooms and background negatives, record confusion/unknown
rates and event false alarms, and adjust confidence thresholds from held-out
captures. Synthetic tests establish behavior, not field accuracy.
