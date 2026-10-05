# Audio Spectrum 0.3.0

The NOVA-7 microphone analyzer provides a spectrum, horizontal-history waterfall,
and saved frequency labels on the Watch's 240 × 240 display. The supplied design
was implemented in the native app and shared NOVA renderer, not a web view.

## Signal and measurement

- Explicit Start opens `audio.input@1` at 16,000 mono signed-16 samples/second.
  The upper range is 8 kHz, as requested. The app never displays frequencies beyond the captured Nyquist limit.
- FFT size: 256, 512, 1024, 2048, 4096 or 8192 samples. The corresponding bin
  spacing is 62.5 to 1.953125 Hz. Larger transforms take longer to gather a frame.
- Windows: Rectangular, Hann, Hamming, Blackman and Flat Top.
- Integer-only radix-2 FFT, coherent-gain correction, single-sided peak amplitude,
  with distinct DC/Nyquist scaling. Displayed level is dBFS plus selected display
  gain, not calibrated sound-pressure level. Gain does not alter microphone gain.
- Logarithmic or linear frequency and amplitude axes; low/high range controls;
  display gain from −24 to +60 dB; label detection threshold −90 to −20 dB.
- Peak trace and five palettes: NOVA, Inferno, Viridis, Gray and Jet.
- The only source is the live microphone. Synthetic PCM exists only in test
  fixtures; the app has no demo generator or demo source selection. Legacy saved
  demo preferences are read as microphone mode while preserving other settings.

## Interaction

SPEC, FALL and LABELS select the view. On SPEC, drag the vertical frequency
cursor; on FALL, drag its horizontal equivalent. The waterfall runs low to high
frequency from top to bottom, with newest time on the right. Tap away to dismiss
the cursor; tap its pill to name that frequency. The tag button toggles markers.

The label editor supports up to eight labels, sixteen printable ASCII characters
per name, and eight colors. Saved labels can be renamed, recolored or deleted;
a short Undo action restores the last deleted label. The active-label list shows
frequencies above the chosen threshold, sorted by level. Edit mode includes
inactive labels. Paging keeps controls visible on the small screen.

The gear opens one continuously scrolling controls view with fixed header and
Back footer. Range/scale, color/gain and window/FFT/detection settings use larger
native-raster lettering. The plots recover screen width previously lost to
unnecessary side gutters. The standard Points/Watch 32-key keyboard covers all
95 printable ASCII characters on three pages, with ABC/#, DELETE and DONE. Its
32 hit cells, navigation and action geometry are shared unchanged; the rejected
eight-key pager is not used. Back and Cancel retain nested
navigation ownership. Stop and Freeze close the microphone. Resuming requires
another explicit Start.

## Persistence and grants

Spectrum alone receives `storage.key-value@2` namespace 7. The existing 32-byte
preference/frequency-label formats remain unchanged. The adapter’s separate
namespace-1 preferences continue to use @1. Namespace 6 remains
Wi-Fi's private storage. Settings and individual label slots use independently
versioned, checksummed 32-byte records. Deletion writes an explicit tombstone;
there is no multi-key transaction or hidden reset. Missing records use defaults;
invalid/unreadable records and uncertain writes are surfaced, with explicit retry.
Retry rereads unresolved records without writing defaults over them. Unresolved
label slots stay reserved, and errors remain visible until their own records are
read successfully. Capture stops before restored source/settings are applied.
Saved data is not guaranteed to survive a full-device erase/reflash.

The microphone retains the existing safe acquisition/cleanup behavior. Alarm,
sleep, Back, read failure and interrupted ownership stop capture. Unconfirmed
cleanup keeps ownership retained rather than returning to another app. No audio
automatically resumes after an alarm or sleep.

## Verification and limits

The test suite runs production DSP and controller code with deterministic PCM,
partial reads, errors, persistence faults and touch events in normal and
ASan/UBSan builds. The capture fixture runs the production app through the real
shared adapter and RGB565 rasterizer, with stride guards and grant cleanup checks.
Target ELF validation checks architecture, imports, exports and bounded memory.

## Saved room and event samples

Controls → Samples opens the sample library. Start the microphone explicitly.
Choose ROOM + or EVENT + and enter a label with the same standard Points/Watch
keyboard. A room capture averages 64 new frames (2.048 seconds of live audio);
keep the room quiet. An event capture takes the current 32 ms canonical frame,
including a frame held by Freeze. Open an existing label and use ADD SAMPLE to
combine more observations. The saved frame count is visible. Eight labels are
shared between rooms and events, independently of the eight frequency labels.
There is no automatic recording, implicit training, eviction or relabeling.

Room collection can be stopped and explicitly resumed, or cancelled without
changing its saved profile. A pending room collection blocks normal app exit
until completed or cancelled. Event profiles are spectral templates: temporal
ordering (opening versus closing sequences), source identity and sound meaning
are not inferred. Collect representative frames under the correct label.

- RAW is the launch default. It shows the original spectrum.
- AUTO enables automatic background selection from explicitly saved rooms.
- USE selects a saved room manually and enables MANUAL. This is useful when
  two rooms have similar backgrounds. Manual selection is clearly distinguished
  from a measured match score.
- Event matches show their label and similarity percentage for two seconds.
  Unknown, quiet or ambiguous frames produce no new match. Similarity is not a
  probability or calibrated identification confidence.

### Stable signature identity and power subtraction

A separate fixed 512-point symmetric-Hann FFT analyzes only live signed-16 PCM
at 16 kHz, regardless of display FFT size, window, gain, frequency range or axes.
Its 128 non-DC bands combine pairs of Fourier bins (62.5 Hz band spacing).
The frame mean is removed before windowing so microphone DC offset cannot
leak into the learned first band. Bin zero is then excluded, and Nyquist uses the correct single-sided power weight. The record
stores the algorithm, sample rate, FFT size, window and band count. Unknown
identities are rejected rather than interpreted as compatible signatures.

Band values are mean-square power in Q30 relative to PCM full scale, normalized
by the window's mean-square energy. Each profile keeps exact 64-bit power sums
and a frame count, so repeated captures form one frame-weighted arithmetic mean.
It never averages decibels. The maximum count is 1,048,575; an addition that would
exceed it is rejected before changing saved data. Display settings do not alter
the samples. Matching and event templates use raw canonical power, even while
the display is filtered.

Both classifiers use normalized L1 power overlap. A match needs at least 80%
similarity, an eight-percentage-point lead over the next candidate and total
power within a factor of four (approximately ±6 dB). Silence is rejected. Room
selection additionally uses a 32-frame moving power average and 64 consecutive
matching frames before switching. Sixty-four misses remove an old filter. A
short burst more than four times the previous room power is excluded for up to
eight frames; a sustained level change is then allowed to adapt, so entering a
louder room cannot leave recognition permanently stuck. During uncertainty the
UI marks a held selection or ambiguity. No live frame modifies a saved profile
unless a capture was explicitly requested.

For display filtering, canonical frames since the previous plotted column form
an observed band-power mean. The app computes max(observed − saved background,0)
and its residual/input power ratio, takes the square root, and applies that
bounded gain to corresponding display FFT bins. Display gain is applied afterward.
This removes stable narrow-band backgrounds as well as broadband backgrounds
without treating coarse tonal energy as a uniform noise density. Raw amplitude
bins remain intact. The display history clears when the filter changes, so one
waterfall does not silently mix modes.
This is a 62.5 Hz background estimate; it can suppress parts of desired sounds
that overlap the background and cannot recover signals below it. It is not a
microphone-gain adjustment, waveform/audio output filter, calibrated SPL meter,
room impulse response, location guarantee or acoustic classifier qualification.

### Durable records and bounded capacity

Each `spectrum_s0` … `spectrum_s7` record is exactly 1,088 bytes: a 60-byte explicit
little-endian header and name, 128 64-bit sums, and CRC32. Tombstones use the same
validated format. Each write is one @2 commit with backend exact readback;
there are no cross-key commits. Existing `spectrum_cfg` and `spectrum_l0` … `l7`
remain independently checksummed 32-byte v1 records.

The maximum sample payload is 8,704 bytes plus 288 bytes of existing Spectrum
records and NVS overhead. The Watch's 24 KiB NVS is shared with other services;
these sizes are bounds, not reserved capacity. Full storage or another I/O fault
leaves an explicit UNSAVED state. Retry writes the same accumulated record,
without counting samples again. Normal exit is blocked until unconfirmed edits
are saved or explicitly discarded. Discard reloads whichever record actually
persisted after an uncertain write. Unreadable or corrupt slots stay reserved
and are never replaced by defaults. Delete and discard require a second explicit
tap. Retry never silently deletes another service's data. A power loss, forced
unload or full-device erase can still lose uncommitted RAM or stored records.

### Cost and additional verification

Canonical analysis uses exactly 2,304 butterflies per 512-sample frame, 256 power
bins and at most 1,024 profile-band comparisons across all eight slots. One frame
is 32 ms. No profile operation allocates memory or expands an unbounded history.
The dedicated analyzer uses 4,640 bytes, eight profiles plus mean caches 12,480
bytes, and the room tracker 544 bytes; a separate filtered-bin buffer preserves
raw FFT output. The pinned Xtensa target build measures 161,028 bytes of total
BSS, 9,956 bytes of data and 90,063 bytes of text for the shared Spectrum app
and adapter (excluding runtime-owned display surfaces).
Host timing is a regression signal, not a device real-time guarantee.

New normal and ASan/UBSan tests cover canonical identity across all display
FFT/window choices, bounded zero-floor subtraction and retained transient energy, exact averaging, hostile
schema/count/CRC inputs, identical-room ambiguity, quiet/unknown inputs, transient
rejection, sustained room changes, record capacity, uncertain writes and retry,
paused/cancelled sampling, frozen event capture, nested standard-keyboard
navigation, explicit discard and grant cleanup.

Host tests and captures are software evidence. They do not qualify physical
microphone frequency response, acoustic calibration, dropped DMA samples,
latency, battery current or real-device touch feel. The original HTML could be
read fully, but its browser rendering was unavailable in this execution session;
no pixel-identical browser-reference comparison is claimed.
