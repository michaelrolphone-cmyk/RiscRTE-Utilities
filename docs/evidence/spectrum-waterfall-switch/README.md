# Spectrum / Waterfall transition investigation

This is regression coverage, not a production crash fix. No production code,
manifest, build feature selector, firmware version, or release is changed.

## Source-bound result

The current Audio Spectrum and RF Waterfall applications did not reproduce a
mode-switch crash in the covered host scenarios. Existing native tests switched
after only a short history; they did not require a full or wrapped ring.

The new native Audio cases seed a deterministic, nonuniform 228-by-166 numeric
history at ring heads 0 and 227. All subsequent transitions pass through real
touch dispatch, the production application, adapter, and shared providers.
Twenty-five SPEC/FALL transitions per run preserve active capture, the full ring,
and resource custody. Normal and 450 ms delayed-display profiles assert continued
microphone reads, no extra microphone open/close, no storage writes, no retained
invocation, guarded display stride, and clean teardown. The two seeds are fixture
boundaries, not a claim that 228 real audio transforms preceded the first switch.
Capture may advance a seeded head before the first FALL render. These checks do
not assert FFT progress on each switch, pixel-perfect ring ordering, or a display
latency bound; delayed captures use the existing application-frame gate.

The RF scenario fills/wraps its ring through 311 synthetic capture bursts instead
of injecting history. It checks repeated live transitions, then freezes capture
and verifies that further mode changes do not resume capture or advance history.
The focused watch run passes 59 assertions, in normal and ASan/UBSan modes.

## Local checks

- 12 pre-existing native baseline cases: three scenarios, two display sizes,
  normal and ASan/UBSan.
- 16 new full-history native cases: two ring boundaries, normal/delayed display,
  240x240 and 410x502, normal and ASan/UBSan.
- 32 pre-existing legacy renderer cases: sixteen scenes, normal and ASan/UBSan.
- Four RF actual-app/adapter runs: lifecycle and full-history switching, normal
  and ASan/UBSan, watch presentation.

Exact commits, source hashes, individual outcomes, and logs are alongside this
file. The new scenarios also run through the existing Native Audio Spectrum and
SDR Waterfall CI workflows; their wider profile matrices remain CI qualification,
not assumed local results.

## Reproduction commands

From this Utilities checkout, with the dependency directories named below:

```sh
python scripts/test_spectrum_native_ui.py \
  --system-apps ../native-system --runtime ../RiscRTE \
  --watch-profile ../RiscRTE-Watch --profile 240x240 --profile 410x502 \
  --scenario native-full-history-switch-0 \
  --scenario native-full-history-switch-227 \
  --scenario native-full-history-delayed-0 \
  --scenario native-full-history-delayed-227 \
  --output build/mode-switch-stress

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  python scripts/test_spectrum_renderer.py --system-apps ../legacy-system

python scripts/test_rf_application.py --system-apps ../legacy-system \
  --runtime ../RiscRTE --target watch \
  --scene lifecycle --scene full-history-switch
```

The native fixture and RF runner use ASan/UBSan with leak detection disabled, as
in their existing setup. LeakSanitizer is not supported under this executor's
ptrace environment. Explicit grant, frame, subscription, microphone and native
allocation assertions still check clean teardown; this is not a LeakSanitizer
pass.

## Remaining uncertainty

The original mode-switch report does not identify a device, installed firmware,
application binary, or attributable crash trace. Audio Spectrum's FALL tab and
the separate RF Waterfall application are distinct paths. A later RF capture
watchdog trace belongs to its separate runtime repair and is not substituted as
proof of this report's root cause.

Review found bounded cursor lookups and ring indices. A possible command-count
problem exists only in an optional deferred-raster configuration. The retained
Wave Audio Spectrum recipe at product f42a6e07133c599a22acfaa8b8a0672ff996dc1b
and this native qualification do not select PORTABLE_RASTER_SNAPSHOT, so that
hypothesis was rejected for this source-bound investigation rather than patched.

A reproducible production repair requires the exact affected app/build and
matching crash or watchdog evidence. Host success does not establish physical
microphone/RF behavior, panel timing, hardware acceptance, or a fix for the
historical report. No flash, merge, release, or hardware pass is claimed here.
