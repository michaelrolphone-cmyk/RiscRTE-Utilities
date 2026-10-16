# Delivered alarm first-boot qualification

This test-only checkpoint closes a profile-specific cold-start coverage gap.
No production source, configuration, version or firmware bytes change.
Physical first boot remains **not tested**.

## Shared Watch 0.4.12

T-Watch 1.1.43 and the Waveshare 0.1.52 candidate preserve the same alarm
provider from their 1.1.41/0.1.50 alarm-repair packages:

- `alarm-service/driver.elf`: 20,684 bytes,
  SHA256 `189ce61b0e4a2b08cbd83ff90564a4c79a6c5e9a3d74b4ad4dcc2a61b4b51604`
- `alarm-service/manifest.json`:
  SHA256 `d8de271face1fa0464bc23e0f8b8d5f75e82505d9479d6504132368f5e1740a2`
- Utilities source `fc9f93402050f020a3422ee889620b2414c7ad5d`
- Runtime headers/loader `659d859acb122640ef4003bb8d7099501d4381be`
- System headers `755cc5174a6deaee120d108cbffe67ae809e1e6f`
- Watch compactor `2f77e01c19f20418e2f91e7181422bc4aabd3c85`
- Xtensa GCC 15.2.0 `esp-15.2.0_20251204`

The exact recipe is `scripts/build_alarm_sleep_retry.py`, with Points,
UTC+08/Denver, volume, DND, clock-change control and
`ALARM_RECONCILE_BUSY_RETRY`. The old dedicated cold-start fixture selected
`ALARM_STARTUP_BUSY_RETRY`; the newer exact-profile fault matrix began only
after the RTC anchor was established. Neither established the missing
exact-profile cold-start BUSY contract.

`alarm_color_cold_start_test.c` now starts the production provider once with
the shipped defines. In each normal and ASan/UBSan execution it checks:

- Two healthy first boots, with empty and saved storage
- 36 fault cases: nine initial read phases, empty/saved storage, and one/129
  consecutive typed no-I/O BUSY refusals
- Recovery through `step()` only, without `refresh()`, Retry or restart
- LOADING visibility and no output, premature sleep ticket, durable mutation,
  fabricated READY state or lost user preferences/schedules while deferred
- Nine genuine IO-error fences and a malformed-record rejection

A negative control removing `ALARM_RECONCILE_BUSY_RETRY` from the fixture is
rejected by its first cold BUSY assertion. Both real-profile runs passed.
The existing startup runner invokes this case, preserving its legacy baseline
reproduction tests. CI also runs the original anchored sleep, Points, catalog
and first-attempt suites, builds the target twice, compares exact output and
checks the delivered ELF/manifest hashes above. Runtime loader checks exercise
relocation semantics, not Xtensa instructions.

```sh
python scripts/test_alarm_color_startup.py --runtime ../runtime \
  --system-apps ../system --output ../evidence/startup
python scripts/test_alarm_color_sleep.py --runtime ../runtime \
  --system-apps ../system --watch ../watch --output ../evidence/sleep
python scripts/build_alarm_sleep_retry.py --runtime ../runtime \
  --system-apps ../system --watch ../watch --cc "$XTENSA_GCC15" \
  --output ../evidence/target
```

Use the exact pins above for byte reproduction. The shared-color CI workflow
uses the tested head SHA explicitly on PRs, then rechecks the merged main SHA.

## Delivered X4 0.1.122 counterpart

X4 uses a different alarm-service profile, 0.4.10. Its unchanged delivered
`alarm/driver.elf` was rebuilt from:

- Utilities `80efb298c9ca8d0f62f9a31840c9e51d380b40f6`
- System `0cae9c90b74982e784f5e59c655f246dfbc69178`
- Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70`
- Xtensa GCC 8.4.0 `esp-2021r2-patch5`

`build_alarm_first_attempt.py` produced a 78,024-byte raw ELF. Watch's
`loader-metadata-v3` compactor, script SHA256
`07936b7ccbc53809ef8d907b19d434b047e4b7e4a2a8501bd32bd9465f691d55`,
removed only permitted loader metadata while verifying retained sections,
symbols and relocations. The 56,516-byte result exactly matched delivered
SHA256 `f4b68c8aaa61d7518593fb70e5bac99f5c5185689440440b7ea103cd9d015ac8`.
This byte identity is recorded in the compact receipt beside this document.

The exact historical first-attempt suite passed normal and ASan/UBSan checks
for empty first boot, startup BUSY, catalog faults, clock change, Points and
sleep. Selected foreground adapter tests passed 1,000 deferred startup and
mid-reconciliation ticks, real-error display and terminal custody handling.
This retains `ALARM_STORAGE_BUSY_RETRY` and `PORTABLE_ALARM_BUSY_DEFER`; color
Watch results must not be attributed to X4 or vice versa.

The delivered X4 identity is independently recorded in its
[released baseline](https://github.com/michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO/blob/608f43ec30fd97644b4cb63706251c8e8a0b11f9/docs/native-idf61/released-baseline.json)
and [published source-input receipt](https://github.com/michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO/blob/608f43ec30fd97644b4cb63706251c8e8a0b11f9/minimal/evening-102-inputs.json).

These host and target-build results establish software behavior on the tested
profiles. They do not identify the actual physical first-boot fault trigger,
exercise real flash/RTC peripherals, or imply hardware acceptance.
