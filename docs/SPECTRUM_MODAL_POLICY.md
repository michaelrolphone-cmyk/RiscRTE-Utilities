# Audio Spectrum shared-editor policy

`PORTABLE_TEXT_INPUT_POLICY` is an optional companion to
`PORTABLE_SPECTRUM_SHARED_TEXT_INPUT`; the selected Watch successor enables both.
It uses the same System policy bridge as Timecard and RF. The selected System
source is `fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0`.

The exact selected Spectrum 0.5.4 source left its ordinary adapter poll loop
while editing. Against its pre-migration editor, 65 simulated idle seconds
changed from one idle sleep and 13 battery samples to none. Active microphone
capture correctly inhibited ordinary sleep, but its battery and background
service progression was also lost. This change restores those policy steps
without resuming the ordinary app input consumer.

- The shared host remains the sole touch/navigation/USB input consumer.
- Routine battery work retains input continuity; a possible idle sleep fully
  pauses and detaches the host first. Draft, cursor and editor ownership survive.
- Pause mode is latched across AGAIN. A BUSY pause needs a fresh snapshot.
- Failed resume retains custody; policy is never dispatched a second time.
- App-owned microphone capture, spectral analysis, temporal learning and shared
  signal learning continue during ordinary editing and delayed host close.
- Done changes only the draft. Existing Save/Train actions still own persistence.
- Session restrictions are preserved: AUDIO_SESSION excludes background Contexts
  audio and ordinary Contexts automatic rules; eligible RF/background policy
  steps still run. Counter probes establish calls, not actual sensor activity.
- No X4 Spectrum payload is selected. The known future rules-enabled resident
  Contexts-message limitation in the generic bridge is therefore unreachable
  here; it is not repaired by this app-only change.

## Reproduction

Run `scripts/test_spectrum_policy.py` with explicit System, Watch Runtime,
Watch profile and output directories. The profile is checked in under
`test/spectrum_shared_input/`. It runs the real app, adapter, text host, scene
host and profile provider with deterministic physical-service fixtures.

The full `scripts/test_spectrum_shared_input.py --extra-define
PORTABLE_TEXT_INPUT_POLICY` suite additionally exercises all four editors,
explicit save/training, Home, unavailable/older hosts, retained custody,
delayed presentation and capture progress, plus unchanged non-keyboard pixels.
Both suites run normal and ASan/UBSan variants. LeakSanitizer is disabled in
this ptrace-restricted executor; clean exits assert zero live grants, frames,
subscriptions and allocations. Controller safety and optional-flag token parity
remain in `scripts/test_spectrum_shared_input_safety.py`.

Target size, stack reports, decoded metadata parity, complete cohort admission,
public source identities and product recipes are separate qualification gates.
These host tests are not physical microphone, PMU, sleep/wake or device tests.
