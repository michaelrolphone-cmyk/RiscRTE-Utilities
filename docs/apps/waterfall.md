# Waterfall: RF spectrum, history and learned events 0.2.6

Waterfall uses receive-only complex IQ bursts through the generic `radio.iq@1`
capability. It provides the Spectrum application's SPEC, FALL and MONITOR views,
labels, room/background profiles, event examples and bounded neural refinement.
Watch and X4 use the same RF models and controls. X4 selects the shared portrait
MONO1 paper adapter, retained waterfall history, native paper text and a slower
redraw cadence; these adaptations do not remove RF features.

## Views and controls

SPEC displays the shifted complex spectrum around the receiver's nominal center
frequency. FALL retains a horizontal history with actual capture timestamps.
Both sidebands and the center/DC bin are kept. Freeze stops acquisition while
retaining the view. MONITOR displays saved frequency labels, room matches,
learned event matches and unclassified RF activity.

Drag the cursor to an absolute RF frequency, then tap its caption to create a
label. Eight labels support 16-character names, all 95 printable ASCII keyboard
characters, eight colors, a frequency/bin tolerance, rename, delete and Undo.

The settings list has the same 13 controls as Audio Spectrum:

- LOW and HIGH: absolute RF plot bounds inside the captured band
- X SCALE and Y SCALE: logarithmic/linear frequency and dB/linear amplitude
- LABELS and COLORS: label visibility and five palettes
- GAIN: display gain from -24 to +60 dB, independent of receiver gain codes
- WINDOW: rectangular, Hann, Hamming, Blackman and flat-top
- FFT SIZE: 256, 512, 1024, 2048, 4096 or 8192 complex samples
- FLOOR: -90 to -20 dBFS display/detection threshold
- STORAGE: load unresolved records or retry pending saves
- SAMPLES: room and snapshot background profiles
- EVENTS: positive/nonmatch examples, review and recognition

The Receiver page independently exposes center, nominal sample rate, receive
width, raw gain selector, RF/BB gain, I/Q filter codes, four DC values and IQ
amplitude/phase correction, plus defaults, saved-event receiver restore and
format details. AUTO remains explicit for controls that support it. Changing
receiver settings stops capture; Start applies the new configuration. A legacy
provider remains usable at its fixed defaults and 256 samples, with unavailable
controls reported in the UI.

The extended 0.2.0 ESP32-S3 provider reports its actual supported bounds:
1841.666667–2790 MHz nominal center, 16/80 MSa/s nominal sample rate and 20/40 MHz
receive width. These are register-defined operating settings, not a claim of
calibrated tuning, analog bandwidth or sensitivity over that range. Receiver
codes are not dB or dBm. The display uses normalized digital dBFS.

## Coherent capture and compatible observations

Every selected FFT consumes one complete coherent burst of that exact size.
The app never joins independent bursts to fabricate a longer FFT or pads one to
claim better resolution. The provider leases the native PHY/SRAM resource and
restores it after each capture; retaining a capability grant does not keep RF on.

Detection uses an independent, fixed canonical transform: the first 256 complex
samples, Hann window and 128 power bands spanning both sidebands. Display FFT,
window, palette and gain changes therefore do not rewrite the learned meaning.
A capture identity binds nominal center/rate/width, receiver controls, sample
format and algorithm version. AUTO/manual policy is part of that identity;
changing calibration readback is diagnostic data, not a new identity. Saved
examples from a different receiver identity remain viewable and are not matched
against incompatible input. The Receiver page can restore their settings.

Each independent burst is a sparse observation and records a gap before it.
Event duration is elapsed time between timestamped observations, not measured
continuous transmitter occupancy. Slow paper updates preserve actual time gaps.
The codec rejects unrepresentable intervals or timer-wrap clips instead of
inventing missing observations. No speech, protocol or transmitter identity is
inferred from a generic RF match.

## Rooms, examples and storage

Eight sample slots hold room profiles or snapshots. Room Capture averages 64
observations; Add accumulates another 64 with count-weighted power. Pause, resume
and cancel preserve the current saved profile. RAW, AUTO and MANUAL subtraction
are separate choices. The automatic room tracker uses its own quiet/background
history and does not turn a transient event into the room profile.

Eight event labels each hold six positive or nonmatch examples of up to 64
observations. Recording first waits for observed quiet, then new activity.
Review includes the whole captured window, clipping indication, cancellation,
explicit save and example/label deletion. Pending, unreadable, full, stale,
conflicting or uncertain storage retains the existing collection and the edit
needed to retry or discard it. A failed read never authorizes replacing saved
data with defaults.

Pending preference/label writes expose Retry and Discard in STORAGE. Discard
requires a second explicit tap and reloads authoritative records without
writing replacements; a failed reload stays read-only until retried.

Neural refinement uses bounded incremental work. It requires enough positive
and nonmatch examples across at least two labels. Candidate promotion requires
held-out improvement and no full-set regressions; edits invalidate the cached
model. It is a local classifier of the user's examples, not a pre-trained RF
protocol decoder.

RF storage is separate from Audio Spectrum. The app uses `rf_cfg`, `rf_l*` and
RF profile keys in its private KV API2 namespace, plus `rf-events-a.rft`,
`rf-events-b.rft` and `rf-neural.rnn` in its private AppData namespace. Two maximum
61,252-byte banks and the 6,500-byte neural checkpoint consume 129,004 bytes of
the 131,072-byte quota. Watch's future profile uses KV instance13 and AppData3;
the reusable app defaults to KV0/AppData3. Shared read-only radio policy stays in
KV1. Spectrum remains AppData2 and Timecard AppData1.

The generic catalog sidecar keeps six required capabilities and marks the
battery indicator and navigation optional. Target packages include battery,
shared radio-policy KV1, and generic navigation on paper. Loaded RF working state uses bounded module BSS, placed in PSRAM by the
supported Runtime ELF loader; the optional PSRAM allocator is also supported.
There is no internal-heap fallback for the large working block.

## Lifecycle and diagnostics

The app owns nested Back, unsaved/capture prompts and root return. The target
build uses `RF_RETURN_APP`, so an adapter cannot intercept a paper corner tap
and launch before those guards. Physical navigation aliases are normalized to
one action. X4 raw portrait touch uses the shared paper contact provider; it is
not rotated a second time.

X4 Home and QuickActions builds opt into the shared pre-launch guard. Pending
windows, unsaved names and unconfirmed saves veto a handoff while leaving the
RF UI interactive. Clean Home goes to the explicit default app; a refused
launch can be retried. Opening/dismissing paper QuickActions preserves the
foreground edit and stops RF without automatic resume. The paper sheet's
radio tiles retain the capability-selected availability of the shared client.

Airplane mode and unread radio policy fail closed. Missing grants and ordinary
capture failures allow explicit Start retry. Idle sleep or alarm suspension
stops capture; wake does not silently restart it. Unconfirmed radio cleanup
retains the invocation and retries without display, storage or other provider
I/O. Unconfirmed storage cleanup likewise keeps its context alive.

The first capture and changed failure outcomes use the existing Runtime serial
diagnostic path. Stage markers precede risky operations; subsequent successful
captures stay quiet. A Runtime without the optional logger remains usable.

## Build and verification

The standalone builder requires a clean exact System Apps source and Xtensa
GCC8.4.0. Defaults are pinned in `scripts/build_waterfall.py`; an explicit
immutable integration SHA can be supplied with `--system-source`.

```
python scripts/build_waterfall.py --system-apps SYSTEM
python scripts/build_waterfall.py --system-apps PAPER_SYSTEM --presentation paper --home-app default.elf --quick-actions
python scripts/test_rf_models.py
python scripts/test_rf_application.py --system-apps SYSTEM --watch-quick-system SYSTEM --x4-system PAPER_SYSTEM --watch WATCH --runtime RUNTIME
```

Both targets retain ELF, deployment manifest, source/configuration/hash record
and notices. The real loader's structural validator checks the final ELF.
Production host fixtures exercise DSP/room/event/neural models, actual Watch
and paper adapters, Watch touch and Runtime diagnostics, plus the real Runtime
AppData transactional backend under quota, stale and uncertain-commit faults.
Its fault injector intercepts both ordinary and fortified libc reads, preserving
fortify overflow checks; GCC11/glibc2.35 and GCC14/glibc2.41 are covered.
These are software checks; the new extended captures and X4 RF receive path
still require physical qualification. The hardware-accepted Watch1.0.7 release
retains its earlier app/provider bytes independently of this next increment.

## Contexts development integration

This source version can opt into the awake Contexts lifecycle. A pending export reads saved preferences and signatures through this app's own storage grant before opening UI or capture; ordinary foreground launches retain their existing behavior. The shared background cleanup fence pauses Contexts before BLE and stops on unconfirmed cleanup. This is source development, not a new installed Watch release. See [Contexts service](../CONTEXTS_SERVICE.md).
