# Shared color alarm sleep reconciliation

Alarm service 0.4.12 is one ordinary capability-provider ELF for both color
watches. It changes no public ABI, platform ownership, namespace, record layout,
clock policy, output policy, board selection or application source.

## Reproduced failure

Waveshare's deployed 0.4.11 source at
03adae46fa9448720321ad27efd344fadd3e8507 deferred a typed storage BUSY only before
its first RTC anchor. Any later transient BUSY in a schedule/preferences read
latched ALARM_STORAGE (-4). The real shared Watch sleep helper reports that as
stage 4, detail -4. All nine read phases reproduce the exact error after normal
initialization. TWatch's previous profile lacks even the startup-only deferral;
the same shared read-reconciliation fix applies.

The Runtime's bound-storage ABI explicitly says BUSY means no I/O began.
The new ALARM_RECONCILE_BUSY_RETRY profile retries only those typed read failures
at the exact read-only reconciliation phase. It preserves the committed schedule.
No sleep ticket is issued while reconciliation is incomplete. The existing
64-step Watch budget still refuses sleep safely if BUSY persists, and a later
attempt recovers without a permanent service error. A deadline that becomes due
while waiting defeats sleep and is delivered normally.

Writes, uncertain output, corrupted records, unavailable/denied storage and real
I/O errors keep their failure rules. Historical opt-in profiles are unchanged.
This is a deterministic software reproduction, not a measured physical cause or
a claim that every on-device storage fault is transient.

## Qualification

Run scripts/test_alarm_color_sleep.py with --runtime, --system-apps, --watch and
--output. It executes the real provider and independently compiled shared Watch
policy under normal execution and ASan/UBSan, reproduces all nine old failures,
checks transient/persistent waits, ordinary polling, deadline priority, corrupt
records and hard errors, and reruns existing RTC-edit, sleep and Points fixtures.
The startup and legacy first-attempt suites remain separate regression gates.

scripts/build_alarm_sleep_retry.py requires --runtime, --system-apps, --watch,
--cc and a fresh --output. It selects GCC15.2 esp-15.2.0_20251204, builds only the
public service ABI, compacts with the pinned Watch tooling, rejects unexpected
imports/exports and runs Runtime's target loader at eight alignments with exact
relocation checks and allocation redzones. CI builds twice and compares bytes.
There are no SDK-private or layout-sensitive libc imports.

Product releases must compose from each official native-IDF6.1 baseline and
verify that only alarm-service/driver.elf, its manifest and cohort metadata
change. Bootloader, native firmware, partition table, all other store files and
settings identities are preserved. Factory images erase user data; hardware
sleep, scheduled wake and alarm outputs remain to be confirmed on each watch.
