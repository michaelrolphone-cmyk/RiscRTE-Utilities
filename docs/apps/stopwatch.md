# Stopwatch 0.1.0

An optional, application-owned Stopwatch in the Utilities repository. The shared
portable implementation uses the existing T5 application drawing/input API and
the pinned SystemApps client adapter; it adds no board-specific driver or runtime
interface. The manifest is `Apps/stopwatch.json` and the entry point is
`Apps/stopwatch.c`.

## Controls and display

- **Start / Pause:** touch the left button, or use the existing Confirm action.
- **Reset:** touch Reset, then Confirm Reset. The first tap only arms confirmation;
  Start/Pause or Retry cancels it. Reset clears elapsed time only after an
  acknowledged storage commit.
- **Back:** the shared adapter's top-left Back touch target or existing Back/crown
  action exits. The app does not write storage when exiting.
- **Retry Storage:** retry an unavailable or invalid stored record, or a recoverable
  RTC/read failure. An already-running healthy clock keeps its monotonic precision. If Save Pause
  is pending, Retry retries that explicit pause save instead of reconstructing
  from the invalid RTC anchor.
- **Save Pause:** appears when an awake RTC failure or detectable clock discrepancy
  freezes the clock. Touch it to explicitly save that paused elapsed value.

The numeric display is `HH:MM:SS.cc`, with hundredths truncated rather than
rounded. The maximum is **99:59:59.99**. It stops at that limit and requires Reset
to start again. A cap transition does not itself write storage. The elapsed value
is checked on each app poll and rendered at most once per 50 milliseconds while
running; this is a responsive stopwatch display, not a guarantee of a hardware
10-millisecond refresh rate or laboratory accuracy.

The app supports 160–1024 pixel width and 240–1024 pixel height. The target
240 × 240 layout is checked for numeric, status, button, and footer bounds.
Touch coordinates and Back navigation come from the unchanged shared adapter.

## Time sources and restoration

During an uninterrupted app invocation, elapsed time advances using unsigned
monotonic millisecond deltas. A single 32-bit uptime wrap is handled correctly;
consecutive samples must be less than one full wrap apart. The stopwatch's
approximately 100-hour cap is much shorter than a full millisecond wrap.

For restoration, a running saved record holds its accumulated milliseconds and a
raw RTC whole-second anchor. Reopening the app, Light sleep, or a Deep-sleep reset
can reconstruct elapsed time if the scoped key/value namespace and battery-backed
RTC remain intact. The app uses the raw RTC calendar directly. It does not apply
the Settings app's display timezone or change RTC hardware state.

A restored running value is labeled **APPROXIMATE**. Each running restoration
introduces up to roughly one second of RTC quantization error relative to the
saved anchor, assuming an unchanged and correctly functioning RTC. Further
recovered pause/resume cycles can accumulate that uncertainty, so the app does
not promise a permanent ±1-second total bound. The approximate flag persists
through Pause and Start; Reset clears it. Merely reopening a saved paused value
does not add elapsed time or require a functioning RTC.

**An RTC forward adjustment while the app is closed cannot be distinguished from
elapsed time.** Such a change is counted and can reach the limit. Offline backward
adjustments are detected only when the current RTC predates the saved running
anchor; other edits may also be indistinguishable from elapsed time. RTC drift,
loss of backup power, clock reinitialization, and manual changes invalidate a
precision claim. Do not treat this as a clock-edit-resistant timer or rely on it
for safety-critical timing.

While running awake, the app compares RTC progression with the invocation's
monotonic anchor roughly once per second. An RTC read failure, a calendar value
outside 2000–2099, an invalid weekday, a value before the awake anchor, or an
RTC/monotonic discrepancy larger than the two-second tolerance freezes the
stopwatch. Small edits within that tolerance are not necessarily detected.
The frozen screen offers **Save Pause**, so no periodic clock check writes flash.
If the app is exited before that pause is confirmed, the old durable record
remains authoritative; reopening cannot promise to recover the unsaved frozen
value. A detected invalid restore blocks Start until a retry succeeds or the
user confirms Reset.

## Persistence and failure handling

The boot policy must provide the stopwatch its own `storage.key-value@1`
namespace, reserved as **instance 2**, with key **`stopwatch`**. The app acquires
instance 0, the runtime's unique-authorized-provider selection, so deployment must
not authorize multiple competing storage instances for this app. Settings retains
its separate namespace. Storage grant isolation is owned by the existing runtime,
not by a filesystem path or caller-supplied namespace.

Writes occur only for explicit Start, Pause / Save Pause, and confirmed Reset.
There are no heartbeat, drawing, exit, cap, or RTC-monitor writes. A missing key
means a new zero/paused stopwatch and does not create a stored default.

The 20-byte little-endian record contains:

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | Schema marker `SW01` |
| 4 | 1 | Running flag, 0 or 1 |
| 5 | 1 | Approximate flag, 0 or 1 |
| 6 | 2 | Reserved zero bytes |
| 8 | 4 | Accumulated milliseconds |
| 12 | 4 | Raw RTC seconds since 2000, zero for a paused record |
| 16 | 4 | FNV-1a checksum of bytes 0–15 |

The checksum is an accidental-corruption check, not cryptographic authentication.
Unknown versions, malformed records, invalid lengths, invalid flags, impossible
elapsed/anchor values, read errors, and oversized blobs fail closed. They are not
silently replaced or migrated. Retry reads again; two-tap Reset explicitly
replaces a bad record if storage becomes writable.

A successful `put` means the runtime completed a backend commit and exact
readback. A failed `put` is **uncertain**: the new value may or may not have
persisted. The app rereads rather than assuming the old value survived and shows
**SAVE UNCONFIRMED - RETRY**. A failed reread blocks Start. A failed Save Pause
after a known RTC discrepancy must never resume a running record using the
known-invalid anchor. Do not assume an action succeeded until the screen shows
its acknowledged state. Durable storage and RTC continuity are not guaranteed
across erase, reflash, factory reset, or storage/RTC hardware failure.

Capability acquisition checks runtime/table version, complete table size, and
required callbacks. All successfully acquired app storage/RTC grants are released
on exit or subsequent validation failure, including malformed null API tables.
Missing RTC or storage capability displays an unavailable message and allows Back.

## Verification

`tests/stopwatch_core_test.c` exercises the production core with record mutation,
100,000 deterministic encode/decode cases, 100,000 restoration cases, 100,000
saturating-tick cases, 100,000 formatting cases, all 36,525 supported calendar days, leap-year boundaries,
restoration failures, exact-limit transitions, and uptime wrap.

`test/native_apps/stopwatch_test.c` includes the production app and supplies bounded
T5/runtime/KV/RTC fakes. It covers start/pause/reopen and reset-like uptime restart,
RTC and storage errors, unknown/short/oversized records, ambiguous writes with
both persistence outcomes, failed rereads, two-tap reset and cancellation,
capability-table failure cleanup, monotonic wrap, commit latency, one hour of
running with no heartbeat writes, Back, layout bounds, and rendered states.
The fake raster uses the production numeric drawing code with a host label shim;
it is not evidence of physical panel or touch qualification.

Compile both with C11, `-Wall -Wextra -Werror`, AddressSanitizer and
UndefinedBehaviorSanitizer. The app fixture additionally needs
`lib/NativeApps/include` and the pinned SystemApps `lib/PortableApps/include`.
For example, from the Utilities root with SystemApps in `../system-apps`:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -no-pie tests/stopwatch_core_test.c -o /tmp/stopwatch-core
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -no-pie -Ilib/NativeApps/include \
  -I../system-apps/lib/PortableApps/include \
  test/native_apps/stopwatch_test.c -o /tmp/stopwatch-app
/tmp/stopwatch-core
/tmp/stopwatch-app
```

A ptraced runner may need `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot
inspect its process environment. Address and undefined-behavior instrumentation
remain enabled. Set `STOPWATCH_RENDER_DIR` to an existing directory to save eight
240 × 240 PGM fixture frames. Separate production-adapter/runtime integration and
on-watch checks are required to validate real grant isolation, RTC continuity,
physical crown/touch events, Light/Deep sleep, presentation, and lifecycle behavior.
