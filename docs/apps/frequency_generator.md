# Frequency Generator 0.1.0

Shared Utilities application `frequency_generator.elf`. This is a development
app for the existing `audio.output@1` provider and portable foreground adapter.
It does not add a speaker driver, task, firmware UI or network service.

## Controls and limits

- Explicit Start produces a sine wave; it never starts when opened or resumed.
- 20–7000 Hz at 16,000 signed 16-bit mono PCM frames/second. The selected speaker
  provider mirrors mono into both hardware slots.
- Select 10, 100 or 1000 Hz adjustment steps, then use Less/More. Left/right
  navigation adjusts the selected step; Confirm toggles Start/Stop.
- Level starts at 5%, adjustable 0–25% of digital full scale. This is digital
  amplitude, not calibrated acoustic loudness. Keep the speaker away from ears.
- A 256-entry interpolated phase accumulator preserves phase across edits;
  amplitude changes ramp by at most128 full-scale units per sample.
- A session automatically stops after60 seconds. Start is required again.
- Touch Back, crown Back, Stop, idle sleep, foreground failure and alarm
  preemption all close the application's stream before return or handoff.

Settings are invocation-local; no data is stored and no network is used.
The UI requires at least240×240 and uses the existing portable touch and alarm
overlay. The Watch deployment adds its existing rotation, crown navigation and
hybrid idle-sleep modules; the standalone portable ELF does not claim those
board-local features. Capture is not a feature of this app.

## Ownership and alarm coexistence

The app acquires only its explicitly authorized `audio.output@1` grant and never
changes the provider's shared gain. Its own PCM scaling cannot reduce a later
alarm's volume. A failed open may own partial hardware state and must be closed.
Only the app's own attempted stream is closed; after successful suspension the
alarm may safely use the same speaker. A failed close causes terminal retention,
with no repeated close, service/storage work, grant release or app handoff.

Use the audio-enabled portable adapter and Runtime0.1.12 or newer reviewed
equivalent. Healthy I2S activity must preserve alarm storage while any live stream
still blocks sleep/unload. Before any existing or newly activated alarm reaches
its output phase the adapter suspends this app's audio. Playback stays stopped
after the modal is dismissed or the watch wakes. Package minimum versions alone
are not a replacement for exact deployment pins and cross-layer admission tests.

## Verification

`python scripts/test_frequency_generator.py --system-apps <source>` exercises
actual app code with deterministic provider fakes in normal and ASan/UBSan modes.
It covers no implicit output, Start/Back, partial-open cleanup, write/close/release
failure, invalid/denied APIs, zero level, frequency clamps, phase continuity,
20,000 randomized core blocks, preemption and native-retained sleep.

Target ELFs also require the common Audio Tools build, import/export and actual
loader checks. Host tests and target builds do not qualify physical loudness,
distortion, clock accuracy, DMA underruns, alarm timing or hardware cleanup.
