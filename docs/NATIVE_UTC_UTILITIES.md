# Native UTC utility cohort

`Apps/native-utc-utilities.json` selects seven independent development profiles:
Calculator 0.1.10, Stopwatch 0.1.10, Battery 1.1.3, BLE Scanner 0.2.5,
BLE Touchpad 0.1.5, BLE Buttons 0.1.5 and Waterfall 0.2.1. Existing default
manifests and builders retain their versions and behavior. This is a local
source/build qualification, not a hardware or release qualification.

## Exact composition

The builder requires clean System `63beac6870d68fa2c0d44bb3dc33a7c1c0976ad6`
and Runtime `30dcec5ce6ce33223f2b203a2399283e1f758567`. The System helper stages
the tagged SDK directly from Utilities `637e13b0bce62ad49b756bec2468a6271d163fc7`,
including its MIT license and source hashes. The linked alarm client explicitly
selects `alarm.service@2`; there is no descriptor suffix inference or API1
fallback. No alarm/provider implementation is changed here.

Each app explicitly links `PortableNativeTimeSource.c` once. Its shared paper
and QuickActions clock takes one closed read-only `runtime.realtime@1` sample
and reads the exact namespace 1 IANA preference. A missing preference is the
Reader's virtual UTC default. Invalid preferences, I/O failures, UNSET native
time and malformed snapshots display unavailable. There is no RTC fallback,
recovery or native clock control. The canonical Runtime headers replace the
legacy bundled headers only in the temporary native include stage.

All native profiles retain the shared Home and QuickActions presentation. BLE
Scanner's native paper handler now accepts the shared rejected-gesture replay
(`began` plus `released`) once, keeping its upper-left Back target usable.
Other controllers and RF's app-owned launch guard are unchanged. RF still
vetoes Home/Quick app handoff during unsaved edits, event review/capture or
pending persistence, drains IQ ownership, and keeps the full spectrum,
waterfall, receiver, labeling, room/signature and temporal event functionality.
RF's private KV2 namespace is 8 and its app-data namespace is 3. Shared
preferences use KV1 namespace 1. No duplicate capability/API/instance rows are
emitted in boot policies.

## Stopwatch state isolation

`PORTABLE_STOPWATCH_NATIVE_UTC` selects a separate controller. It reads and
writes only namespace 2 `stopwatch_utc`, a 20-byte checksummed `SWU1` record.
The anchor is UTC seconds since 2000, bounded by the native signed-Unix 2038
limit. Legacy `stopwatch`/`SW01` is neither read nor migrated, and an SW01 record
placed at the new key is rejected. An absent new key begins with zero elapsed
and paused state.

Awake elapsed updates use the same monotonic millisecond logic and 99-hour
limit as the legacy core. Restart restores whole-second elapsed time from the
UTC anchor and marks it approximate. Backward UTC movement and awake drift of
more than two seconds freeze the stopwatch and require explicit save/reset or
retry. Native clock loss does the same. A forward clock edit while the app is
not running cannot be distinguished from elapsed time; restart recovery stays
explicitly approximate. Timezone edits affect clock presentation, never elapsed
state or its anchors.

Every write is read back before it is accepted. A failed or unverifiable save
keeps the exact pending record and blocks Home, Back and Quick app handoff.
Retry verifies that record; a write returning I/O after committing is accepted
only if the readback matches. Namespace and native-reader releases are checked.
CONTEXT, unknown statuses and uncertain release fence the whole invocation;
no further provider calls, rendering, release attempts or frees follow.

## Reproduce

Use the already available pinned Xtensa GCC 8.4.0 2021r2-patch5 compiler. No
software installation or hardware connection is needed.

```sh
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
  python scripts/build_native_utc_utilities.py \
  --system-apps /path/to/clean-System-63beac6 \
  --runtime /path/to/clean-Runtime-30dcec5
python scripts/test_native_utc_utility_controllers.py \
  --system-apps /path/to/clean-System-63beac6 \
  --runtime /path/to/clean-Runtime-30dcec5
python scripts/test_native_utc_utilities.py \
  --system-apps /path/to/clean-System-63beac6 \
  --runtime /path/to/clean-Runtime-30dcec5 --drivers /path/to/Drivers
```

The target builder validates all seven ELFs, import/export sets and manifests,
and compares fourteen flag-off Watch/paper ELFs against Utilities `dd366baa`.
It emits `build-evidence.json`, licenses, boot policies and one
`x4-native-app.json` per native app with ELF, source and compiled-SDK hashes.
The old profile comparisons use each existing builder's pinned adapter source;
they do not claim a newly selected native ELF is byte-identical to Watch.

The first host suite runs the real Calculator, Battery and isolated Stopwatch
controllers through the production adapter in RGB565 and retaining MONO1,
normally and under AddressSanitizer/UndefinedBehaviorSanitizer. It covers UTC
restoration, multiple pause/resume cycles, raw-record isolation, tag rejection,
clock jumps/loss/range/UNSET, pending save verification and navigation, timezone
changes, repeated gestures and fenced native/KV/release failures.

The second suite reuses the established host BLE/HID/RF peripherals and scripted
scenes with a native Runtime/API2 wrapper. Actual controllers and adapter source
are unmodified by the harness. It covers scanning and aliases, pairing,
assignments and retry, shared QuickActions, IQ views and controls, labels,
temporal events, and RF's Home/Quick draft and pending-store guards. Missing,
invalid and unavailable time/zone cases preserve app functionality; native
CONTEXT and release failures inside the actual Quick modal verify that pixels,
provider counters, grants and frees remain frozen after retention.

The legacy host fixtures use a false health callback as their bounded end-of-
script signal. This suite translates that fixture-only signal into a synthetic
Home input with a successful handoff so it does not misrepresent a Runtime
health failure as normal shutdown. Hardware transport remains simulated; the
native custody failures tested here are explicit typed fault injections.

Source commits and generated artifacts remain local because publication of the
separately owned System ancestry has not been cleared.
