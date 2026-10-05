# Persisted alarm sound volume

The opt-in Points-enabled alarm service is version 0.4.0. It retains the
current non-modal Points start/warning/end cues and their three-edge ledger.
No Points codec, custom type, notification flag, duration, label or UI changes.

Alarms and Countdown read `alarm_volume` through one additional exact read-only
namespace-1 binding. The stored value is one byte from 0 through 100. A missing
value means 50%; it is never implicitly written. Invalid bytes or a read error
block output with the existing storage error rather than replacing the value.
The Alarms app provides the separate explicit Save/readback editor.

Alarm PCM uses a full-range32767 basis instead of the previous1600 limit, scaled
by the saved percentage:50% is16383 peak and100% is32767. After a successful
speaker open, a separate bounded phase explicitly calls `set_gain(100, 100)` so
the persistent device gain cannot attenuate a later Frequency Generator session.
Gain failure
enters the existing cleanup and retry path before a sample is written. Zero
skips the speaker entirely, while vibration remains independently controlled by
alert mode. The percentage describes linear digital amplitude, not measured
acoustic loudness. Physical loudness and amplifier behavior remain unqualified.

Points cues retain their existing short beep amplitude, one wrist tap and
non-modal completion. Since speaker gain survives close/open, every Points beep
explicitly restores unity gain before writing its original quiet PCM. Neither
Points nor Frequency Generator inherits a previous alarm volume. The output/control ABI remains unchanged.
Legacy builds without `ALARM_VOLUME_CONTROL` retain their former dependencies;
`build_points_service.py` explicitly enables the new profile. Deployments must
include all eight keys in `points-storage-policy.example.json`; a UI-only update
cannot supply the new backend gain behavior.

`test_alarm_volume.py` runs the real provider with default, persisted, zero,
range, malformed, read-failure, gain-failure and missing-gain-provider cases
under normal and ASan/UBSan compilation. It also runs the current Points cue
fixture against the new profile, retaining independent END/WARN and restart
no-duplicate-cue assertions. Existing legacy service/app fixtures run separately.

The copied status now adds `ALARM_STATE_CUE` without changing the104-byte status
layout or function table. It reserves output before the first hardware call and
through cleanup, separately from a visible alarm. Matched updated clients
preempt app audio and drain the short cue without replacing the frame. Old
clients reject the unknown state and perform bounded failure cleanup; deployments
must pair the updated client/source, including Clock.

## Integrated output custody

`test_points_audio_cues.py` links the actual service, current shared adapter and
an app-owned live audio session. Its24 normal/sanitized legacy/Nova profile
runs exercise due Points cues, preemption refusal, no overlaid frame, durable
completion exactly once and no automatic app-audio resume. It requires the
matched CUE-aware shared client; the shared client is pinned at `13f32d3e` in hosted CI.

`alarm_frequency_gain_test.c` links the actual service and Frequency Generator
as separate translation units with a persistent-gain speaker. After10% and90%
alarms, ACK and cleanup, the real Frequency default100% output remains32767 peak.
Observed alarm peaks are3276 and29490 respectively, with unity device gain.
No overlapping stream or leaked grant remains. New-phase acknowledgment, expiry,
stop-only, close failure and a read failure specifically at LOAD_VOLUME are
also covered. These are software/model observations, not acoustic measurements.
