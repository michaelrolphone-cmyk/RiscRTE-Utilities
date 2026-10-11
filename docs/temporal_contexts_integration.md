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

## One library in three applications

Contexts, SDR and Audio Spectrogram use the same named room/event profiles.
Open **Controls > Samples / Events** (or tap the Monitor room or
**Learn** button) in a signal app to open its shared library. **+ New** accepts a
custom name; selecting an existing profile offers another training sample.
Room training requires 30 seconds and at least three windows; event training
requires three separated takes. **Save** verifies persistent storage and imports
the source record into the service immediately. **Tools** preserves access to
the earlier spectrum captures, event examples and frequency-label workflows.

Each room/event keeps independent RF and audio observations. Profiles join by
kind and exact name, never by coincident slot numbers. A room trained only in
Audio appears in SDR with **Add RF sample**; it is not treated as an RF match.
The Watch supports both sources. X4's supplied profile exposes RF only.
Foreground plots and Monitor show the current shared room/event label and
confidence; missing or insufficient evidence remains **Unknown**.

Room profiles also retain all 128 unfiltered canonical spectral band powers.
**Raw** shows the original spectrum. **Auto** subtracts the confidently matched
room. **Manual** uses the room chosen with **Use room background**. Selection
persists across reopening. Subtraction affects displayed captures only;
classification and training always receive raw observations. Unknown temporal
rooms do not fall back to a guessed spectral room. RF receiver identity must
match, and an audio background is never applied to RF. Earlier raw room spectra
remain usable by exact name; schema-1 temporal profiles remain readable and gain
a full background spectrum after fresh training. New writes use schema 2.

## Background detection switch

Watch and X4 Quick Actions expose **Context detection On / Off**, using the same
persisted preference as Contexts. Its absent/default state is Off. A toggle
pauses background capture, verifies the write, and invalidates the cached
policy. A failed save reports an unconfirmed state rather than a false success.

SDR and Spectrogram still analyze the explicit foreground capture while they
are running. Explicit Contexts training temporarily enables its capture client
without writing the background preference; Save/Cancel ends that temporary
session. X4 preserves an in-progress session through Light suspend/resume.

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

## Shared-library and Quick Actions verification

The follow-up uses Audio Spectrogram 0.5.1, SDR/Contexts/service 0.3.1, Watch
clock 0.11.1 and X4 Home 0.4.1. Both target builds and the service profiles pass
GCC 8.4.0 compilation, import/export checks and the native ELF validator.

Targeted host checks (normal and ASan/UBSan) cover:

- `test_shared_signal_library.py`: actual PCM controller training, names across
  independent slots, save/reopen and uncertain commits, persisted background
  selection, unknown handling and subtraction without mutation of raw data.
- `test_context_fingerprints.py`: full-capacity schema-2 storage, schema-1 reads,
  source/identity guards, and service import merging independent RF/audio data.
- `test_contexts_app.py`: the Contexts Save/Cancel/retry paths leave the master
  detection preference unchanged during explicit learning.
- System `test_context_quick_toggle.py`: gestures, confirmed persistence, missing
  and corrupt settings, failed writes, and background versus foreground policy.
- X4 `test_shared_quick_tiles.py` and `test_context_clock_rendezvous.py`: button
  layout, current Home ownership boundaries and temporary learning on resume.
- Existing actual-app temporal raster journeys and RF lifecycle checks preserve
  event examples, pending-save/retained cleanup and display bounds.

These are source/app/service updates. The earlier Watch 1.0.22 and X4 0.1.56 full
images predate this follow-up and do not contain these changes. No replacement
full image is assembled here. Device accuracy still requires labeled captures.
