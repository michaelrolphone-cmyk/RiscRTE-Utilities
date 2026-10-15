# Selected first-attempt repairs

These are opt-in product profiles. They do not change the default alarm API or
remove RTC-jump detection. No physical-device verification is claimed.

## Watch: verified editor changes

The delivered Watch Settings writes and reads back the RTC, while the alarm
service still holds the earlier RTC/monotonic anchor. Its next step correctly
classifies that unannounced jump as `ALARM_RTC`; Retry clears the anchor. The
regression test reproduces this sequence before exercising the fix.

Select `ALARM_CLOCK_CHANGE_CONTROL` with the existing Points, volume, DND and
UTC8/Denver defines. The tagged optional API1 suffix permits the Settings
profile `PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL` to announce an already
verified RTC write. The service independently checks the current RTC (at most
one elapsed second), invalidates old Light-sleep tickets, and reconciles without
editing or acknowledging stored occurrences. Ordinary refresh still rejects
unexpected jumps. Existing invalid/backward schedule policy remains intact.
The Settings control checks suffix compatibility before writing. Pair service
0.4.9 (`watch-clock-change-manifest.json`) with selected Settings 1.3.24.

## X4: typed no-I/O BUSY

The storage SDK defines BUSY as a refusal before I/O begins. Previously a BUSY
at an initial key read, catalog stat or ledger creation permanently entered
`ALARM_STORAGE`; a restart succeeded. These cases are reproduced. A healthy
empty first creation also passes, including an independent local harness using
Runtime .28 AppDataFiles and the pinned ESP-LittleFS implementation. Thus BUSY
is a demonstrated first-boot failure path, not proof of the device's trigger.

Select `ALARM_STORAGE_BUSY_RETRY` with the existing tagged native-UTC catalog
profile and service 0.4.10 (`catalog-native-utc-startup-manifest.json`). BUSY
returns `ALARM_PENDING` at the same cooperative phase, retaining active tokens
and verified generations. The ledger helper releases pending bytes on BUSY,
which cannot represent an uncertain commit. IO and COMMIT_UNKNOWN still require
readback, and custody retention is never converted to a retry.

Selected System clients must also use `PORTABLE_ALARM_BUSY_DEFER` with tagged
API2. This consumes foreground input while LOADING is deferred, yielding between
attempts rather than exhausting the adapter's 64-step loading guard. It does not
fabricate READY, dismiss errors or bypass alert/custody handling. No finite retry
limit is imposed on a no-I/O refusal.

## Reproduce

Run `scripts/test_alarm_first_attempt.py --runtime RUNTIME --system-apps SYSTEM`
against Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70` and System
`57e549ef5a1c727e8a24658e572f402e006fe82d`. It includes normal and ASan/UBSan
runs; leak detection is disabled for compatibility with ptrace execution.
The Watch System branch adds `scripts/test_settings_clock_change.py`; the X4
System branch adds `scripts/test_alarm_busy_adapter.py` for both client variants.

Set `NATIVE_APP_CC` to Xtensa ESP32-S3 GCC 8.4 and run
`scripts/build_alarm_first_attempt.py` with the same dependency arguments.
It checks both target profiles with the actual Runtime ELF loader at eight
alignments, comparing all relocated sections and allocator redzones. This is
host-side target relocation qualification, not Xtensa instruction execution.

## Delivered X4 source preservation

The selected X4.101 service 0.4.8 came from recovered Utilities source
`e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d`, rather than the general Utilities
head. This branch also preserves its catalog owner-transfer, allocation-free
wire-value equality and post-scan sleep-time resampling. Those existing repairs
are necessary to retain the delivered catalog memory and wake behavior. Their
original Git blobs are `966ba991df34b37cba334ea72439ef22037987e5` (service),
`ccd63f0e94463ec23ff9e32b6c9fef129bb7bd71` (catalog) and
`c5eeb471fcab95172c3ef876abae3bedb9ff8644` (PointsCatalog header).
