# Audio Spectrum 0.2.0

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
- Demo is an explicit deterministic signal source processed by the same FFT. It
  never plays sound and is clearly identified. A failed microphone does not
  silently become a demo measurement.

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

The gear opens three pages of controls. Source/range/scale, color/gain and
window/FFT/detection controls are separate from the main plot. A bounded native
keyboard covers all 95 printable ASCII characters. Back and Cancel retain nested
navigation ownership. Stop and Freeze close the microphone. Resuming requires
another explicit Start.

## Persistence and grants

Spectrum alone receives `storage.key-value@1` namespace 7. Namespace 6 remains
Wi-Fi's private storage. Settings and individual label slots use independently
versioned, checksummed 32-byte records. Deletion writes an explicit tombstone;
there is no multi-key transaction or hidden reset. Missing records use defaults;
invalid/unreadable records and uncertain writes are surfaced, with explicit retry.
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

Host tests and captures are software evidence. They do not qualify physical
microphone frequency response, acoustic calibration, dropped DMA samples,
latency, battery current or real-device touch feel. The original HTML could be
read fully, but its browser rendering was unavailable in this execution session;
no pixel-identical browser-reference comparison is claimed.
