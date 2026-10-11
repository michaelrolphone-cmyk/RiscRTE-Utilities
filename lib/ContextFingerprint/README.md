# Temporal room and event fingerprints

Shared implementation for `Apps/audio_spectrum.c`, `Apps/waterfall.c`
(`rf_application.inc`, both Watch and X4 presentations), and
`Services/contexts/service.c`. The board repositories are
`RiscRTE-T-Watch-S3` and `RiskRTE-XTEINK-X4-PRO`. Their historical release
recipes are not new temporal firmware releases.

## Interfaces and behavior

`cf_extract_iq(work, config, identity, iq, count, rate, fft, origin, span, out)`
extracts one continuous IQ window. Streaming callers use `cf_iq` or `cf_pcm`,
`cf_spectrum`, and `cf_take`. All work memory is caller owned. Use module BSS
in PSRAM; do not put banks or pipelines on a small task stack. IQ is the
existing packed signed 10-bit I/Q format; PCM is signed 16-bit mono.

The amplitude envelope is averaged to approximately 1 kHz. Burst thresholds
use the preceding ten seconds' rolling quantized median, a 1.8 ratio, and a
small additive floor. The extractor records burst start, duration, peak, and
successive onset intervals. Exact sparse 1 ms histogram bins cover 0–10 s.
The 256 occupied-bin limit is explicit: overflow invalidates temporal
evidence. It never aliases unrelated intervals. Only the latest 64 completed
burst records are retained. Noise warmup is 100 ms for continuous input.

Existing canonical FFTs remain unchanged. Mean power, power variance and
five local maxima (frequency and amplitude) are combined with envelope mean,
variance, duty cycle, and burst rate. Peaks are ordered by frequency to avoid
rank swaps. RF defaults to 5 s windows; audio to 1.5 s. Event windows follow
the existing onset/end segmenter, identically in the trainer and service.
Room and event pipelines/fusion state are independent.

`cf_match_profiles` computes one-dimensional Earth Mover's Distance in
milliseconds and normalized Euclidean feature distance. Relative amplitude
scales and a two-FFT-bin frequency scale avoid combining Hz and power in
unscaled units. Timing receives 70% of the combined distance when available.
A profile trained with temporal evidence cannot match a live vector missing
that evidence. Confidence is a similarity score `1/(1+4*distance)`, **not a
calibrated probability**. Defaults require 0.75 confidence and a 0.08 lead
over the runner-up. Unknown, ambiguous and explicitly taught nonmatch events
do not produce labels. The Contexts source screen also supports timing-only
matching.

Enabled, fresh sources contribute weighted confidence. Re-enabled sources
start at one tenth weight and ramp over ten windows. A profile missing a
fresh enabled source is penalized rather than winning by omission. Source
weights decay after fourteen days; only a real UTC clock enables aging.
Automatic room updates require 0.95 fused confidence and agreement from each
updated source. Each fresh window is consumed at most once. Manual room/event
captures confirm labels. Updates blend 95% old and 5% new. Events never
self-label for training. Deleting an individual example invalidates its
aggregate because an EMA cannot subtract a historical example; record again
to rebuild that aggregate.

## Current RF sampling limit

The S3 provider returns independent coherent dumps: 256–8192 IQ pairs at
16 or 80 MHz. At 16 MHz, even 8192 pairs cover only 512 microseconds. They
are not seconds of continuous IQ. Foreground and service integrations use
`cf_iq_snapshot`: each dump supplies a timestamped mean magnitude. It retains
the measured sampling resolution and coverage. Temporal intervals shorter
than four observation spacings are excluded; observations farther than
250 ms apart cannot produce a temporal histogram. Capture gaps invalidate
temporal evidence. This supports sufficiently slow, repeatedly observed RF
activity; it does **not** establish 100 ms Wi-Fi beacon timing from a 100 ms
polling loop. The continuous IQ API is ready for a future continuous provider.
Different source identities, receiver settings and coordinate spans cannot
be silently mixed. Audio uses real continuous PCM.

## Persistence and integration

The optional `ContextFingerprintService.h` CFP1 suffix leaves the existing
Contexts ABI prefix unchanged and rejects unrelated suffixes. It exposes
configuration, status, manual confirmation, import/export and generation
acknowledgment. The service holds copied data; app owners checkpoint it after
capture quiescence. Source-local slot mapping survives cross-source fusion.

`context-fingerprints.cfp` is a separate, CRC-checked, little-endian schema-1
file in the existing Audio@2 or RF@3 `storage.app-data` namespace. Old records
are preserved; they contain no raw timing and cannot be converted into new
temporal profiles. Record new examples. There are 24 aggregate label slots
(including nonmatches); imports exceeding combined capacity fail atomically.
The maximum fingerprint record is 60 kB. The existing namespace permits four
files and 128 KiB total: fully populated legacy banks plus new fingerprints
can exceed that quota. Save failure retains an unsaved draft and does not
claim success; reduce old examples or provision a larger future storage ABI.

Background clients need explicit app-data grants for the enabled source
namespaces, in addition to existing Contexts grants. `rtc.clock@2` is optional
for age decay; older adapters without the clock helper still build with aging
disabled. `context_sources` in shared KV@1 stores Audio=1/RF=2;
`context_timing` stores full=0/timing-only=1. The RF-only service never requests
audio. Do not add an audio dependency to X4.

Tested source combinations:

- Watch client: System Apps `1027b44b309521aea5835d6fb63014c56a16023d`.
- X4 foreground RF adapter: System Apps
  `12fefac58600ed805722ff873ecf8855b06d4db7`.
- X4's existing full Contexts release recipe names System Apps
  `45215c6c57ca8e48a424efd7e37e827205696a18`, which was unavailable from GitHub
  during this work. Full X4 Contexts packaging remains blocked on recovering
  that source or selecting and validating a replacement. The RF app and
  RF-only service are separately buildable. No board release pins are changed.

## Validation

Run `scripts/test_context_fingerprints.py --drivers PATH --runtime PATH` for
normal and ASan/UBSan extraction, equal-spectrum/different-timing separation,
unknown, RF polling-alias rejection, negative examples, disagreement, source
ramping/aging, storage corruption, real service event windows and restart.
`test_contexts_service.py`, `test_contexts_app.py`,
`test_spectrum_temporal_renderer.py`, and `test_rf_application.py` retain the
existing production controller/adapter and lifecycle checks. System Apps'
`test_context_client.py` checks quiescent checkpointing and retained storage.

With `NATIVE_APP_CC` set to GCC8.4 2021r2-patch5, build using
`build_contexts_service.py` (full and `--profile rf-only`),
`build_fingerprint_watch.py`, and `build_waterfall.py` with the explicit System
source above. The latter supports Watch and paper presentations. The bounded
ELF normalizer removes only trailing, all-zero R_XTENSA_NONE linker padding;
the original runtime validator still checks every output.

These are deterministic host and target-build checks, not measured room
accuracy or on-device latency/power qualification. Before release, replay
held-out captures collected on different days, measure wrong-room/event and
unknown rates, tune distance/thresholds, and check PSRAM and live microphone
drain headroom on both devices. Do not report synthetic-test separation as
field accuracy.
