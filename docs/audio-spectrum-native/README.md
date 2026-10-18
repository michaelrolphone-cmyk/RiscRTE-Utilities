# Audio Spectrum native presentation

This is an in-progress, opt-in app migration. No released application or firmware
asset is replaced by this checkpoint.

Baseline Utilities: 12c7440c4c2fd9b6b78e10f11a983947c145b346.
Current shared System: 947895afdb2ecef59654402cf08fe7ecdf92362f.
The existing DSP/model/controller suite's retained SDK is System
fdc15395928a1fa8eee62a2823b3c8777dcf0fa0.

## Geometry checkpoint

`spectrum_native_layout.h` describes capability-pixel rectangles and checked
half-open targets. It does not allocate memory, call providers, or change model
state. The geometry test covers twelve profiles and 100,000 deterministic
positive dimension pairs, including extreme integer dimensions. This is an
arithmetic/bounds test, not a claim of usable UI on a one-pixel display.

The analysis model retains 228 spectrum samples, 166 waterfall frequency bins,
and the 228 by 166 analysis-history ring. History capacity counts analysis
updates, not a fixed elapsed-time duration. A partial history retains its empty
left prefix. Native drawing maps numeric values into display primitives; it does
not scale a completed legacy framebuffer. Cursor pixels and data-bin indexes
must derive independently from frequency.

## Qualification limits

The unchanged 26 DSP/model/controller fixtures passed normally and with
ASan/UBSan using the retained SDK. LeakSanitizer is disabled because this executor
reports ptrace incompatibility. Eight existing modal-policy cases passed with
the current full-feature System profile. These controls do not establish native
view completeness, current target ELF compatibility, or hardware usability.

Native view and target integration is being qualified separately. The original
profile remains the default. All capture, storage, shared-editor and retention
controllers remain required; a reduced renderer-only build is not a release
recipe.

## Native source checkpoint

`PORTABLE_SPECTRUM_NATIVE_UI` requires the existing native canvas, Nova primitives
and shared text-input selectors. No default profile is changed. The app-local
presenters cover the plot, waterfall, monitor, controls, labels, sound samples,
temporal events/examples and shared Contexts training. They rebuild bounded hit
targets from the current model without provider calls. Display size changes
presentation geometry, not DSP arrays, history capacity or storage formats.

`Apps/native/audio_spectrum.json` is a separate provisional 0.5.11-dev1 identity.
It retains the exact 21 capability/version entries from the frozen 0.5.10 payload.
The legacy `Apps/audio_spectrum.json` remains byte-identical. No immutable firmware
or release asset is replaced by this checkpoint.

The real-adapter host suite exercises shared editor accept/cancel/reopen, explicit
saves, absent/delayed host frames, audio progress during three-second close
latency, retained custody, repeated app entry, and the existing modal power/radio
policy. The native subpage fixture verifies nine pages across eight geometries,
pure current-model mapping, paint/input target equality and controller actions.
Plain, Nova/alarm and shared-editor selector-off preprocessing is token-identical
to the exact Utilities baseline.

### Reproduction

The pinned `.github/workflows/native-spectrum.yml` runs the host, geometry,
subpage, default-source parity, existing DSP/storage controls and full target
recipe. Its input roles are explicit: current Watch headers and the retained
6e82fdca generic navigation/sleep bridges are separate source pins. The host
runner can prepare its own recipe using `--watch-profile`; no local absolute
profile paths are required.

The target recipe uses the retained GCC 8.4.0+2021r2-patch5 toolchain and checks
source closure, all declared capabilities, narrow imports/exports and the exact
Runtime structural ELF loader. Newer GCC 15 was rejected after baseline format
errors. Retained `-lgcc` linkage passed the loader; the redundant forced
`--undefined=__divsf3` flag caused a baseline linker crash and is omitted, with
the actual import still resolved. These are recorded recipe adjustments, not a
claim of historical binary reproduction.

### Evidence boundaries

Initial host qualification completed 236 executions at 240×240 and 410×502.
The final matrix passed 61 cases per profile, normal and ASan/UBSan, at 240×240,
410×502, 502×410, 480×800 and 800×480: 610 executions. This includes 450 ms
application presentation and clean repeated entry. Normal and sanitized frame
bytes match. Both native CI attempts at source
`37fc4eebfded7879a7f442c387eae7b2613737df` passed:
[push](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39122467242)
and [pull request](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39122492547).
The default profile has separate exact-byte target comparison. Host frames do
not establish physical display readability, microphone behavior or hardware
sleep/USB behavior. Those remain physical qualification.

The first qualified raw native ELF is 417,216 bytes, SHA-256
`ce95cf81f59f6d256f80ef632531f8ef1fbbfc60046e56d173bae3793e4fb262`.
It has 151,846 executable-section bytes and 1,037,744 BSS bytes. Native presentation
adds 13,451 allocated-section bytes over the retained default build. These raw
ELFs include nonloaded metadata, so their file sizes are not directly comparable
to the compacted 0.5.10 firmware payload.

Production `ElfAdmission` at Runtime
`0aab2fd3b03b4190d638e23c79a465afeecf5dfb` accepts the ordinary app and rejects
missing imports, unsafe lifecycle state and driver-role admission. Normal and
ASan/UBSan execution passed all 64 data-placement alignment controls. The loader
requests 1,101,284 bytes of data allocation including its alignment reserve;
text plus data requests total 1,253,130 bytes. File size and the existing 8 MiB
structural allocation bound pass. Provider heap, stack, framebuffer and actual
free device memory are outside that structural check.

The standalone release recipe stages the complete source closure and evidence,
checks their hashes against the selected commit, and verifies immutable published
assets by downloading them again. A native catalog identity uses
`Apps/native/audio_spectrum.json`; the legacy catalog identity is not rebound.
