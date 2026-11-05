# Native Frequency Generator source qualification

PORTABLE_FREQUENCY_NATIVE_UI explicitly selects the native RGB controller. It
also requires PORTABLE_NOVA_UI, PORTABLE_NATIVE_CANVAS and
PORTABLE_APP_OWNS_TOUCH_CHROME. No product recipe, release manifest, shared
utility_frame helper, tone core or Runtime/T5 ABI changes are made.

The layout uses capability-reported dimensions directly. Frequency, Hz, level,
specification, status, Back and all eight ordinary controls have independently
computed rectangles. The source accepts160..4096 pixels per axis; qualification
covers160x160,240x240,320x180,410x502 and502x410. Compact screens use smaller text
and targets (minimum18px accepted layout height); this does not claim the old
240px view's44px target promise on small displays. The old view is unchanged
unless the explicit native selector is enabled. Paper/default composition parity
is verified with the selector off. The adapter's availability query refuses a
non-RGB format; native product/paper composition remains separate adoption work.

The System owner10 hooks fence pre-visible and inherited held contacts, require
matching completed layout generations, and retain same-map repaint input. The
app requires DOWN and UP to match one painted target; crossing or queue gaps
cancel. Top-edge taps rejected by Quick are replayed as ordered DOWN/UP only for
Frequency, so Back retains the same control matching as the other buttons.
Back queues the existing adapter return path, which closes audio before launch.
Quick/foreign frames invalidate native hit ownership. A read-only repaint query
allows a fresh app image after modal dismissal, before accepting fresh input.

Tone generation, phase continuity, digital volume and ramping,20..7000Hz limits,
16kHz/256-frame output, explicit Start,60-second cutoff, alarm suspension, native
sleep retention and failed open/write/close/release behavior remain shared.
Nothing automatically restarts the speaker after an alarm or Quick dismissal.

## Reproduction

From Utilities, using the accompanying System source:

    python3 scripts/test_frequency_native.py --system-apps ../system
    python3 scripts/test_frequency_native.py --system-apps ../system --snapshot
    python3 scripts/test_frequency_native_standalone.py --system-apps ../system
    python3 scripts/test_frequency_native_standalone.py --system-apps ../system --snapshot
    python3 scripts/test_frequency_native_optout.py --system-apps ../system
    python3 scripts/test_frequency_generator.py --system-apps ../system
    python3 scripts/test_frequency_renderer.py --system-apps ../system --compact
    NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc python3 scripts/build_frequency_native.py --system-apps ../system
    NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc python3 scripts/build_frequency_native.py --system-apps ../system --snapshot

The standalone target selects capability-backed PORTABLE_INPUT_NAVIGATION and
an explicit springboard.elf return destination. The separate standalone host
matrix uses exactly those feature flags and29 scenarios (580 runs per path).
It has no deployment-local navigation/sleep bridge. The richer local-interface
matrix covers the app/adapter contract with modeled local bridges, including
retained sleep; it is not a linked Watch firmware or hardware qualification.
Both paths preserve the existing audio builder defaults. The dedicated
profiles/frequency-native-qualification.json declares required input.navigation@1
alongside display, touch, audio and alarm grants, and separately records optional
Quick/telemetry capabilities. The builder validates grant/flag correspondence
and copies this qualification-only candidate beside the ELF. It is not a catalog
entry and must not use the incomplete legacy app manifest for installation.

Each local-interface native matrix runs30 scenarios x5 sizes x2 latencies x2 sanitizer modes
(600 runs per presentation path). The fixture compiles the actual app and
adapter and models only providers. It checks painted captions and bounds,
stride guards, immutable pending RGB565 bytes, full resource cleanup, audio
failure retention, alarm output custody, retained native sleep, Quick dismissal
and subsequent input, repeated module entry, held/pre-visible input, crossing,
queue gaps, navigation, clamps, simultaneous touch/Confirm and60-second cutoff.
Receipts record source hashes and individual cases. The dedicated
sdk/frequency-native-sources.json pins the exact clean System commit required
by the native target builder. Qualification-only CI reads that pin and retains
development artifacts; it has no release-publishing permission or step. System separately tests
owner generations, foreign frames, stale snapshot completion and feature-off
preprocessed token equivalence. Target builds verify ELF structure and exact
exports/import allowlist; they do not install or publish anything.

The existing legacy renderer fixture was corrected to use its shared elapsed
script clock for crown Back and combined Stop rather than raw provider poll
counts. It preserves the meaningful assertions and tests no writes after the
paired Stop. The cutoff scenario now stops the fixture using that same clock.
The original scene6 failure was reproduced on the pinned baseline app/adapter.

Physical fit, speaker behavior, touch accuracy, display performance and product
firmware adoption remain unqualified.
