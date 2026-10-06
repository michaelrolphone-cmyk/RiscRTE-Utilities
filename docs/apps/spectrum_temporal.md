# Spectrum 0.4.2: live monitoring and simpler sound learning

The `storage.app-data@1` backend is pinned to published Runtime 0.1.32 commit
`da92aef5174b9279de1bad0ec05f7234ba587169` ([Runtime PR23](https://github.com/michaelrolphone-cmyk/RiscRTE/pull/23)).
`sdk/spectrum-temporal-sources.json` records the API-header hash and validated
Watch profile fingerprints. That profile has 12 requirements/grants, app-data
namespace 2, legacy KV@2 namespace 7 and preference KV@1 namespace 1. Its actual
target ELF passes loader/relocation checks, and the combined Runtime admits the
profile. Compatibility requires the explicit ABI2 app-data layout and pinned
backend; the Watch repository verifies the final source-bound assembly.
No install, release, flash, microphone/device test, neural training or automatic
format is performed by these builds or compatibility metadata.

## Capture and review

Monitor → Learn or Controls → Events opens the event library. Controls → Samples
and the Monitor room row open the existing room/snapshot library.

1. Choose New Event. The standard Points/Watch keyboard opens immediately.
   Enter a name and press Done.
2. Press Record Sound. It saves a changed name/shift setting and starts the mic
   when needed. Background preparation and quiet-wait progress are shown.
3. At Make the sound now, perform the action, then pause. Not This Sound records
   a confusing nonmatch for the current label. Four 64 ms columns of pre-onset
   context are retained.
4. Four quiet columns finish the example. Review duration, attack, decay,
   frequency evidence and impact count, then Save or Cancel. File operations
   safely pause the mic; previously requested live monitoring resumes afterward.
5. Repeat for gentle/hard, near/far or other variations. Examples stay separate;
   they are never collapsed into a mean. Each label has six shared sound or
   nonmatch slots. Capacity is checked before arming, with no silent eviction.

If name/shift saving fails, recording does not arm under an old setting. The
Retry Save / Discard screen remains reachable. Explicit Stop and Exit clear
live capture intent; temporary storage/alarm pauses preserve it. Retained
cleanup and fatal microphone errors always require recovery before any restart.

Eight event labels are supported. There is no automatic training, export,
background recording, speech understanding or semantic guarantee that a learned
sound corresponds to a particular physical action. A label needs a positive
example before it can match. A negative-only label is retained but cannot fire.

A window contains at most 64 columns (4.096 seconds including context/tail).
Reaching that limit, stopping early, or interruption produces a visibly marked
window requiring review. Nothing is silently called a completed event. The
review screen offers **Use as whole event** when the user knows that the captured
window contains the complete action. This confirmation is stored separately;
the original clipped/interrupted provenance remains set. It can be undone before
saving. Without confirmation, a window matches only compatible window examples.
Automatic termination-tail columns remain stored but are excluded from action
alignment, so a confirmed short stop can match a naturally completed event.

Stop before onset disarms without saving. Cancel explicitly discards the pending
window. Back cannot silently abandon an armed or reviewed example. Individual
examples and whole labels use two-tap deletion. A failed transaction stays
visible with Retry Save and Discard; pending deleted labels do not become an
unreachable recovery state. Storage errors never delete legacy room profiles.

## Monitor context

The third main tab is **MONITOR**, replacing the former LABELS presentation while
retaining frequency-label editing behind **EDIT**. Room identity is read from the
raw room tracker, not from the currently selected filter, so a saved room can be
identified while the plot remains RAW or while another room is manually selected.
The accepted room, a still-stabilizing candidate, ambiguity and no-match states
are distinguished explicitly.

The ranked detection list combines every credible temporal event candidate at or
above the 80% match threshold with every active frequency label and a MAYBE
SPEECH row when voice-like activity is detected. Confusing-negative
examples still veto their candidate. Close candidates remain visible when the
classifier result is ambiguous. Temporal confidence is the matcher's 0..100% heuristic
similarity. Frequency-label confidence maps local background SNR to a bounded
ranking score: 6 dB is 50% and 12 dB or more is 100%. Both are heuristic ranking
values rather than calibrated probabilities. Event amplitude is the matched
window's peak residual dBFS; frequency-label amplitude is its canonical excess
dBFS. These values are independent of display gain and selected plot FFT.

## Background-relative detection

The scene estimate and foreground features are separate. A raw power background
is learned from 64 canonical 512-point Hann frames at 16 kHz, with DC removed before
windowing. No display gain, filtered spectrum or inferred event class is fed back
as raw audio. The room matcher receives the unfiltered slow scene estimate.

A band enters foreground at four times its background power (about 6 dB), and
releases below twice background power (about 3 dB). A one-unit Q30 guard is only a
numerical floor. The user-selected FLOOR is an explicit excess-power RMS dBFS
floor, independent of display gain. It applies to frequency labels and event
onset. Event FLOOR and features use excess in bands that independently passed
SNR hysteresis. Tiny high-SNR bins cannot borrow large low-SNR fluctuations from
an otherwise steady ambient tone to form an event. Raw/slow room information is
not masked. Saved legacy spectral references retain their distinctive absent
frequencies when compared with salient live excess.

Frequency labels are checked on every 32 ms canonical frame, independently of the
selected display FFT. An 8192-point plot cannot cause a short frequency peak to
be skipped simply because the visual transform updates more slowly.

Newly active bands are protected from upward adaptation for at least 1024 frames
(32.768 seconds). Afterward, unknown sustained changes adapt with approximately
16.384-second upward time constant while not protected. Capture/review/matching
freeze active-band upward learning. A positively recognized ongoing event keeps
its active bands protected until it ends or a new completed comparison rejects
that identity. Nonactive bands continue slowly adapting. Downward movement uses
approximately 1.024 seconds and rounded steps so exact silence eventually reaches
zero. No saved room profile is modified by this learner. A new sustained motor
or alarm therefore retains its own evidence instead of being learned away during
its first short example. Physical operating conditions can still differ from the
software fixtures; explicit room selection and Raw view remain available.

## Features and constrained matching

Every 64 ms column combines two canonical frames. It stores:

- 64 log-frequency power bands packed as approximately 3 dB nibbles, eight bands per
 octave over 31.25–8000 Hz
- absolute residual level and dominant frequency, retained separately from the
 intensity-normalized shape
- positive spectral flux, foreground state and reliable tonal prominence

An example also retains attack, decay, action duration, impact positions/count,
pre-onset context and clipped/user-confirmed-end provenance. Tonal frequency
anchors use a robust median of marked tonal frames. A louder broadband attack
cannot replace the identical ringing tail's frequency evidence. Competing
similarly strong tonal clusters reduce that evidence's confidence. The raw
loudest-frame peak remains available in the saved example.

The coarse stage averages power over active columns and normalizes its peak;
fixed quiet tails cannot change intensity or tempo eligibility. Packed-shape
distance is power-weighted, so a dominant tone cannot disappear numerically
inside many broadband bins. Dynamic-time warping then compares the active
onset..end extent, including internal gaps but not recording padding. It permits
only (1,1), (1,2) and(2,1) steps inside a four-column corridor around proportional
time. Overall action speed is bounded to 0.5–2×; every consumed column is charged.
Peak phase, envelope, flux, attack/decay and impact spacing contribute evidence.
Completed impact sequences with nonzero impact counts must preserve their count:
a single impact cannot become a double merely by stretching time. Reviewed
partial repetitive windows retain their bounded tempo tolerance.

Per-label SHIFT is Off, ±1.5 semitones or ±3 semitones (zero, one or two log bins).
Alignment uses one bounded shift for the whole example, with a confidence penalty
per shift. Strong tonal comparisons also retain an absolute-frequency bound,
including an honest 63 Hz allowance for the canonical two-bin measurement spacing.
This is not an unbounded transpose-invariant classifier. The UI retains the
estimated signed shift and observed/reference tonal frequencies. Intensity
normalization is intentional for near/far and gentle/hard variants; absolute
levels are retained as evidence and the foreground FLOOR still applies.

The best positive needs at least80% heuristic similarity and an eight-percentage-
point lead over another label. A close confusing nonmatch vetoes its label.
Unknown, negative and ambiguous outcomes are displayed explicitly. Similarity is
not a calibrated probability. Recent temporal matches remain visible for four
seconds. Matching is cooperative and bounded; a queued event is retained while
another is compared, and additional missed onsets have an explicit counter.

## Persistence, compatibility and failure handling

The two collections are `spectrum-events-a.sqt` and `spectrum-events-b.sqt`, four
labels per file, under app-data namespace 2. Each complete file has an explicit
little-endian SQT2 v1 header, canonical-analysis identity, label/example IDs,
validated features, reserved bytes and CRC32. Empty collections take 1284 bytes;
a full four-label/six-example/64-column collection takes 59652 bytes. Two maximum
files total 119304 bytes, below the 128 KiB namespace quota. Deletion reduces the
variable frame payload. The quota and extra staging allowance are limits, not
physical capacity reservations on the shared 512 KiB volume.

One user edit is pending at a time. Immutable full old/new byte snapshots are
created before provider calls can yield. Every save obtains a fresh stat/read
revision because any app-data rename, including another app's write, can
invalidate all prior tokens. Exact old/new bytes—not a checksum alone—decide
whether the intended write is still safe or already persisted after an uncertain
commit. Retry does not add an example again. A different valid collection causes
a conflict and retains the draft; explicit Discard reloads the actual saved
collection. Unreadable, corrupt, unknown-version or unavailable files remain
reserved and are never replaced by defaults. Confirmed absence alone creates a
new empty collection. There are no cross-file transactions or chunked KV saves.

COMMIT_UNKNOWN, STALE, NO_SPACE and ordinary I/O faults preserve the pending edit.
RETAINED is different: after the microphone is closed, the app returns without
further UI, I/O, grant release or teardown. Runtime 0.1.30's pre-finalization barrier
must retain the invocation for explicit restart. Existing `storage.key-value@2`
namespace 7 and shared `storage.key-value@1` namespace 1 stay unchanged. No old
averaged profile can supply temporal information that was never recorded; no
fabricated sequence or destructive automatic migration is attempted.

Normal exit is blocked until a pending transaction/window is saved, explicitly
discarded or cancelled. A power loss or forced unload can still lose uncommitted
RAM. Full erase/reflash persistence is not promised. The new Runtime layout is
explicitly opt-in, requires layout ABI2 and has no autoformat/grow behavior.

## Software evidence and cost

The example library uses 118472 bytes; file safety keeps three bounded 59652-byte
snapshots, plus fixed detector/matcher state. Target ELF builds measure the whole
app rather than claiming only the DSP arrays are its RAM cost. The strict
pinned Xtensa development build measures 477724 bytes BSS, 9996 bytes data and
129187 bytes text, excluding runtime-owned display surfaces. Ordinary file
transactions run with the microphone closed, then resume previously requested
monitoring after safe completion. The current
build evidence records the complete 0.4.2 ELF.

A matcher tick offers 64 work units. Coarse preparation is an atomic eight-unit
operation; a DTW row costs one. A new template cannot begin with insufficient
remaining budget. The helper's minimum tick budget is eight. The legal maximum
8 × 6 × 64-active-column case takes 3456 charged units, 55 cooperative ticks including
completion, and at most 64 units per tick. A normal 64-column stored example with
four context and four tail columns has 56 active rows. Its full library takes 3072
units and 49 ticks. Host CPU timings are test evidence, not Watch real-time,
power-consumption or dropped-DMA qualification.

Normal and ASan/UBSan tests cover actual PCM pitch/intensity/tempo changes,
background FLOOR and steady ambient rejection, motor/alarm protection, exact
silence, octave mismatch hidden by broadband noise, single/double impacts,
ringing tails with differing broadband peaks, manual endpoint confirmation,
positive/negative/ambiguous matching, malformed and CRC-correct mutation cases,
full/stale/uncertain/conflicting saves, retained cleanup, and the actual app's
keyboard/capture/review/retry/discard flows. Native Nova raster tests inspect
ordinary, clipped, explicitly completed, full-storage and recovery screens.
`bash scripts/test_spectrum_app_data.sh RUNTIME_CHECKOUT` also links the production
Spectrum codec/client to Runtime's real `AppDataFiles.cpp`, with byte-identical
API headers and the exact clean Runtime commit required. Remote CI runs this
fixture against that published pin. Normal and ASan/UBSan runs exercise two maximum banks,
namespace isolation and cross-namespace CAS invalidation, committed-byte quota
and injected ENOSPC retry, actual rename-before/after uncertain outcomes,
same-bank conflicts and explicit discard, reopening the host-directory backend,
malformed-bank reservation, and a retained stage-close fault with no further
consumer I/O. This is host filesystem evidence; Runtime's own authority/ELF,
LittleFS recovery and layout suites provide the corresponding lower-layer checks.
No physical microphone, acoustic or device qualification is implied.

## Continuous input and speech qualification

Successful partial/empty RX reads remain live. The native provider treats an RX
deadline as a bounded wait; fatal SDK failures still close safely. Spectrum opts
out of idle sleep only while a requested stream is active. A 128 ms input gap
resets stale room/event/frequency/speech evidence and marks an in-progress event
as an interrupted window. After two seconds, WAITING FOR AUDIO is shown. Saved
room collection retains its explicitly resumable partial average.

Speech uses the vendored WebRTC fixed-point GMM from libfvad, 20 ms live PCM,
plus background-excess FLOOR, bandwidth, modulation and periodicity gates.
It has no neural training, transcription or speaker identity. MAYBE SPEECH is
intentionally uncertain; modulated voice-like machinery may still resemble
speech. Confidence is recent detector support. Raw, manual and automatic
filters, plot FFT and display gain produce identical speech state traces.
The detector uses a fixed 2320-byte state and a bounded 60-lag × 160-sample
periodicity search per 20 ms frame; no heap allocation or growing history.

Host tests include an 11-second public-domain JFK speech excerpt at three
intensities, synthetic voiced syllables, silence/DC, steady/alternating tones,
fan harmonics, impacts, and white/low-pass noise. Whole-clip detection counts are
coverage checks, not population accuracy measures. Fixture provenance and license
are in tests/fixtures/voice; the PCM is never included in device builds. The
library's LICENSE, AUTHORS, PATENTS and source/patch record accompany artifacts.

0.4.2 leaves all record formats and names unchanged: SPSGv1 room/snapshot records,
SQT2v1 temporal banks and 32-byte preferences/labels. It reads 0.4.0/0.4.1 records;
its writes remain readable by those versions on rollback. Room power averaging
and subtraction are unchanged; recognition now normalizes intensity independently
so adding a louder same-shape sample does not erase a quieter room match.
