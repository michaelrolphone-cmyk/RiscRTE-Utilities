# Audio Spectrum 0.1.1

One shared Audio Tools application toggles between a live microphone spectrum and
scrolling spectrogram. This is original Utilities app source, reused by Watch
without an app fork. It does not implement a driver or a new Runtime capability.

## Interaction and privacy

- Capture is off at entry. Only **Start** (or Confirm) opens the microphone.
- **Stop** closes capture and preserves the last plot. **Freeze** (or Down) also
  closes capture, retaining the plot with an explicit `FROZEN / MIC OFF` label.
  Start begins a fresh plot/history; Freeze does not keep recording in the back.
- Tap the Spectrum or Spectrogram tab to change the view. Left/Right toggle it.
  Changing views while stopped does not activate the microphone.
- The on-screen Back region and the deployment's crown Back close capture before
  returning. The current display/touch rotation remains the shared adapter's job.
- Alarm preemption and idle sleep close capture. Wake and alarm dismissal never
  restart it; another explicit Start is required.
- There is no recording file, storage grant, network access, permission prompt,
  saved history, microphone auto-start, or automatic recovery after read failure.
  PCM and plots exist only in bounded application RAM.

## Provider and DSP contract

The app acquires existing `audio.input@1` only on Start. `Apps/AudioInputV1.h` is a
consumer-only declaration of the Watch `twatch_audio_in_api_v1` layout. Its `level`
member is retained for ABI compatibility and is unused. The app opens mono 16 kHz
PCM and requests at most 256 signed 16-bit frames per read. The existing Watch
`twatch-mic` provider supplies decimated PDM PCM with a 40 ms native read bound.
The runtime prerequisite is the separately reviewed bounded PDM RX work based on
Runtime 0.1.16; this app alone does not make older hardware backends functional.

Successful short reads are assembled until 256 samples exist. An empty successful
read contributes no samples; eight consecutive empty reads stop capture. A false
read or `got > 256` stops immediately and discards the partial window. Failed
opens are closed because a native failure can still leave an owned cleanup token.
The app never closes a stream unless it attempted its own open. Cleanup is
idempotent. A failed close or grant release retains the invocation in a yield-only
loop rather than returning, retrying provider I/O, or releasing potentially live
resources. Native-retained sleep returns immediately without app cleanup or grant
release, honoring the shared adapter's separate native ownership barrier.

The fixed radix-2 FFT uses 256 samples, per-block DC removal and a symmetric Hann
window, Q15 integer twiddles, and scaling by two at each of eight stages. Magnitude
uses an integer square root. No ESP-DSP, floating point, libm, heap allocation or
new firmware import is required by the DSP. The underlying frequency spacing is
62.5 Hz. Bins 1 through 128 cover 62.5 Hz through 8 kHz; DC is excluded. Maximum
magnitude reduction maps these 128 bins into 112 display columns. The 0 label marks
the left frequency-axis endpoint, not a plotted DC measurement.

Bars are logarithmically quantized relative PCM magnitude. The spectrogram stores
112 intensity bytes per row in a 64-row ring, with the newest row at the top and
older rows moving down. Plot/history updates are limited to at most 10 per second
and require a new complete transform. The history therefore spans at least about
6.4 seconds when full, longer if capture or rendering is slower. The renderer uses
three gray levels plus white, with a monochrome dither fallback. These are relative
visual levels, not calibrated sound-pressure levels, calibrated dB, precise peak
frequency measurements, or hardware-qualified audio analysis.

The complete fixed DSP/history state occupies 9,964 bytes. A read buffer uses 512
additional stack bytes. Shared adapter framebuffers and provider DMA are separate.
The app's coordinate bounds are checked for displays from 240×240 to 1024×1024;
the actual Watch layout uses a centered 224×128 plot, separate tabs, axis, status
and Start/Freeze controls without overlap.

## Build and verification

The shared audio build/inventory is coordinated with Frequency Generator. This
change supplies the app source, consumer header, DSP, manifest, docs and focused
fixtures; it does not alter the legacy portable-app inventory or release parity.
Its common-adapter build must opt into `PORTABLE_AUDIO_SESSION` together with
`PORTABLE_ALARM_CLIENT`. Watch adds its existing navigation and local sleep glue.
Do not omit the lifecycle hook when integrating the app into an alarm deployment.

Run from the repository root:

```sh
python scripts/test_audio_spectrum.py --system-apps /path/to/system-apps
```

The script runs the real source in normal and ASan/UBSan builds. On ptrace-based
executors where LeakSanitizer cannot run, use `ASAN_OPTIONS=detect_leaks=0`; the DSP
and app do not allocate heap memory, and address/undefined-behavior checks remain
active. Fixtures emit 240×240 PGM frames into `build/audio-spectrum-tests` for
visual inspection.

Tests cover all 127 interior bin-centered synthetic tones, direct floating-point
DFT comparisons in the test only, zero and DC at both signed extremes, alternating
full scale, 2,000 deterministic random blocks, segmentation-equivalent partial
reads, ring wrap/order, and malformed inputs. Fake-provider app tests cover user
Start, both views, Stop/Freeze, repeated use, partial/empty/oversize/failed reads,
API denial/malformed tables, failed open/close/release, UI failure, Back, alarm or
sleep stop without restart, native-retained return, and geometry bounds in several
sizes. The shared adapter's independent tests cover actual modal/sleep ordering.

These tests and target ELF/import validation are software-only evidence. Physical
microphone capture, PDM clock behavior, displayed noise floor, execution cost on
hardware, rotation/crown/touch interaction on the real Watch, and alarm/sleep
interaction on a powered device remain unverified. No physical microphone,
speaker, flashing, RF code, persistent-storage or network/update code is touched.
