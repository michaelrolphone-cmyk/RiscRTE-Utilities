# RF Spectrogram shared name entry

The selected Watch and X4 RF profile is version 0.3.4. It opts into
`PORTABLE_RF_SHARED_TEXT_INPUT` and uses `ui.text-input@1` for frequency labels,
signature names, temporal-event labels and shared Contexts signal names.
The ordinary Waterfall sidecar/build remains 0.3.3 with the existing local
keyboard unless the deployment explicitly selects the new profile.

## Editing and ownership

Each request copies the existing name into a 17-byte buffer, retaining the
16-printable-ASCII-character limit. Done returns the copied name to the same
app editor only after the host has closed, released its grant and resumed the
app's input/display ownership. Done does not save a record or start learning.
Existing Save, Capture, Train, Cancel, storage Retry and Discard actions retain
their authority. Back cancels only the keyboard edit and restores the preceding
draft. Reopening starts from the latest accepted draft.

An explicit Home action closes the shared host first, cancels uncommitted name,
capture and learning state, stops RF and exits through normal cleanup. It never
requires a second Save/Cancel action and does not write pending edits as a side
effect of exit. Already saved data is preserved. Local Back and ordinary app
launch guards retain their existing semantics. Resident X4 Home resumes the
existing default host; Watch Home uses its existing springboard destination.
A refused launch stays paused and interactive for a fresh Home attempt.
Cancelled records are marked unread until an explicit Storage Retry reloads
them; Home refusal never triggers a Contexts checkpoint or re-saves a draft. An unconfirmed
host operation, release or adapter-resume retains the invocation and prevents
further RF app provider calls. An unconfirmed radio/storage cleanup does not
free live resources. Missing/unsupported cleanly released text services leave
the existing draft visible and allow retry.

The host owns keyboard drawing, hit targets, presentation and navigation.
RF skips its own renderer/contact handling during this ownership interval,
including a coalesced crown/touch event that opens the keyboard. Shared signal
navigation targets are refreshed from logical page state without waiting for a
display frame. App fonts and geometry outside the keyboard remain unchanged.

RF capture, temporal observation, shared fingerprint analysis and bounded neural
work continue according to the selected foreground capture policy while typing
or waiting for a clean keyboard close. Display latency does not gate this work.
Physical power/idle/alarm behavior is still owned by the device's existing
adapter profile; no radio policy, driver, training model or awake timeout is
changed by this feature.

## Source and device profiles

The same semantic version in the predecessor images did not identify the same
source or feature flags. Watch's preserved RF 0.3.3 component used Utilities
`fc0bdbbbc600ce891e60c44d091fce0586ade7d8`; X4's RF 0.3.3 component used
`c12781b2d4e08772d06078bfff7441276cb5a7a8`. Their device recipes preserve and audit
those profiles independently before adding the shared-input flags and grant.
The new Utilities source retains the X4 startup operation-scoped storage repair,
including `rf_app_data_scope.h` and the original event/fingerprint restoration
order. No unpublished SD persistence work is an input.

Audio Spectrum's separate `PORTABLE_SPECTRUM_SHARED_TEXT_INPUT` opt-in remains
unchanged. The shared Contexts-name include enables its existing logical input
geometry for either explicitly selected signal app; neither opt-out changes.

## Building and qualification

A generic development target can select the feature with:

```
python scripts/build_waterfall.py --system-apps SYSTEM \
  --system-source IMMUTABLE_SYSTEM_SHA --runtime-sdk RUNTIME/sdk/app \
  --shared-text-input
```

A selected System source must implement the shared text-input handoff and have
matching scene/text providers and deployment grants. Device integration must
use its own exact flags, native SDK, fonts, profile and composition recipe;
the generic command is not a device firmware release.

The RF shared-input workflow suite runs actual app/controller, adapter and
scene/text host source with deterministic peripherals. The independent safety
suite checks failure/retention and token parity against the published baseline.
Existing RF model, operation-scoped storage and startup/Contexts regressions
remain part of the source qualification. Full-cohort admission separately checks
the exact target ELFs and preserves each app inventory and flash reserve.
These checks do not execute Xtensa instructions or establish physical RF,
power-management, touchscreen or crown behavior. No device is flashed.

### Reproducing host qualification

The focused suite needs a C11 host compiler and Python 3. Its CI job pins both
the System client and the published Utilities comparison source:

```
python scripts/test_rf_shared_input_safety.py --system-apps SYSTEM \
  --baseline-utilities SPECTRUM_UTILITIES --output SAFETY_OUTPUT
python -m unittest discover -s tests -v
```

The full workflow suite additionally needs Pillow and fontTools. Pass the exact
device build receipts and published source checkouts rather than relying on the
local convenience defaults. Each receipt contains the deployment flags and
paths to the frozen Watch profile/preincluded headers:

```
python scripts/test_rf_shared_input.py --system-apps SYSTEM \
  --runtime WATCH_RUNTIME --x4-runtime X4_RUNTIME \
  --watch-recipe WATCH_TARGET/waterfall-build-record.json \
  --x4-recipe X4_TARGET/waterfall-build-record.json \
  --baseline-utilities SPECTRUM_UTILITIES \
  --watch-baseline WATCH_PACKAGED_UTILITIES \
  --x4-baseline X4_PACKAGED_UTILITIES --x4-host-system X4_PACKAGED_HOSTS \
  --output WORKFLOW_OUTPUT
```

The runner records exact source pins, source hashes, compiler commands, input
sequences and assertions. It compares 19 non-keyboard views to both the published
source baseline and each device's packaged predecessor, with identical normal
and sanitized pixels. Frames are losslessly compressed to PNG as each scenario
finishes. The local workflow runner disables LeakSanitizer for the restricted
executor and also checks resource accounting explicitly; AddressSanitizer and
UndefinedBehaviorSanitizer remain enabled. The independent CI safety job does
not disable LeakSanitizer.
