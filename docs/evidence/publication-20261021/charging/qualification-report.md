# Watch charging-screen qualification

Task: Complete Watch charging screen (`d9fcd068-5f2a-4f6e-87ee-7f99b6daf1e2`).
Observed: 2026-10-21 UTC.

## Result

The charging screen is already implemented in the published Watch 1.1.46 Battery 1.1.3 payload. No missing production implementation or reproduced in-scope defect was established. This deliverable reconciles the requested screen with its original reference and adds only focused host tests. It does not change production source or any accepted artifact byte.

Fourteen charging-specific scenarios passed with the real frozen app and shared adapter in both ordinary and AddressSanitizer/UndefinedBehaviorSanitizer builds: 28 runs. Independent review and a separate rerun also passed all 28. Twelve production-source framebuffer captures were visually inspected, including charging, completion, unknown estimates, error and every Details page; no clipping or overlap was observed.

Keep the task awaiting qualification, not completed: physical display/touch and correspondence with actual charging/input state have not been tested.

## Original request and supported screen scope

T036 retains the original request to select the best applicable designs from Power Screens — NOVA-7.html, rather than implement every mockup. The exact Library reference read for this review is `libfile_3578ce7a6d508191b4bdfe7fe2aee5b6`, backing `file_000000009a8481fd9f9ae6a25fcdc425`, 13,290 bytes. Its mockups include charging, full, fast charge, 80% hold, thermal and other power states.

Frozen Utilities `docs/nova/BATTERY_POWER.md` documents the implemented selection, and `Apps/battery_power.inc` implements it:

- A black NOVA overview with a cyan progress ring, large known percentage or unavailable estimate, and a separate charging-state label/caption.
- CHARGING, CHARGE COMPLETE, USB POWER, CHARGE DISABLED, THERMAL LIMIT, NO BATTERY and READ ERROR states are driven by the available normalized telemetry. Gauge 100% is not proof of charger completion. A known charger-complete flag remains distinct even at a 97% or unavailable estimate.
- Overview controls are Back, Details and Refresh/Retry. Four Details pages expose voltage/input, charger enabled/completion, presence/PMIC thermal regulation and the autonomous-charging explanation. Details offers Prev, Next, Refresh/Retry and nested Back. The touch rectangles are at least 44 pixels high and wide.
- The UI only reads charging telemetry. It does not initialize the charger, change charge settings, clear interrupts, or implement an 80%-hold/Charge Now control. Charger initialization remains separate from this task.
- Wireless/fast-charge detection, wattage, time-to-full, exact cell temperature/cutoff, unsupported-charger diagnostics, plug-in animations and charger-control buttons are not established by the available contracts. They were not invented to imitate the reference. PMIC thermal regulation is not a cell-temperature measurement; usable USB input is not proof of a particular cable fault.

This is a scope-to-existing-source reconciliation, not a new product requirement or an assertion that every reference mockup shipped.

## Exact published candidate and source identity

The earlier read-only power-status reconciliation decoded the current published Watch store and tied its inherited Battery payload to an exact producer recipe. That evidence is reused, not counted as a new build or rerun:

- Watch 1.1.46 product commit: `ab9ff33975680c5bb8d9a2e7ed346d0300732a4e`.
- Store SHA-256: `e50d26000dc0366be61abbf480cd39ee98323ea5ef489536baf0cf7c4097b126`; 125 files / 61 ELFs.
- Battery 1.1.3: 155,448 bytes; SHA-256 `fdcb36ebee771f968c6febe53e1817a712276cd5c54752ad8250b8ecf32c1c6c`, independently rehashed in this qualification.
- Inherited producer: `24270c36cd4bc1edb7bb2e8d860501eaa5105d43`.
- Frozen Utilities source: `ca7fe82f43adc4ad8a8fa477a5336c8f5c693770`.
- Frozen System adapter: `96742a20af660808cae6cd3a36eadd12a83a460f`.
- App source SHA-256: `d955bea31acedaeeb646c0db6c26abf58fb1d0d6fe53963bb6784f0718ae8563`.
- Adapter source SHA-256: `98f705ebcbe3ea7fdcb270354d93108555b0d96b95eb86c54281d5f35318b249`.

The Battery ELF contains the charging and Details labels, and its immutable producer recipe compiles the NOVA power profile. It is inherited from the older payload, not rebuilt from the later default/Home input lock. Current-main app and adapter source differ materially from these frozen versions. Nothing here qualifies current main, requests rollback, or changes product pins.

## Evidence mapping

Existing power-status evidence already covers 8,192 PMU status combinations, 256 adapter snapshots, 20 app/UI scenarios in two modes, failed-read recovery, bounded polling, nested navigation and sleep/custody handling. That evidence is recorded as reused overlap. It is not a claim that PMU initialization, physical charging or the separate low-battery task was redone here.

New focused fixture coverage:

1. Charging at 42%.
2. Charger-complete at 97%, with a green complete state.
3. Gauge 100% with usable USB but no complete flag remains USB POWER.
4. Charging with unknown SOC retains CHARGING and an unavailable estimate.
5. Charger-complete with unknown SOC retains completion without fabricating a percentage.
6. Input present with charger disabled.
7. PMIC thermal limit without fabricated cell temperature.
8. No battery with unknown estimate.
9. Basic-provider charging without invented extended input/completion information.
10. Read failure clears stale telemetry and exposes Retry.
11. Charging → read failure → complete at 97% → USB-only recovery.
12. All four Details pages, wraparound, refresh, nested Back, reopen and final return while charging.
13. Stray extended bits without the validity bit do not establish USB, thermal or completion state.
14. Failed app return remains retryable while charging.

Every run checks bounded polling, no stored-setting writes, intact framebuffer stride guards and complete ordinary resource cleanup. Static scenarios repaint once rather than animate continuously.

## Local deliverable and reproduction

Local-only branch: `tasker/watch-charging-qualification-20261021`.
New commit: `1ce0c6518b56aa29f7cc11dd9b5ead99f5f0a0ed`.
Parent: `1f80de804698972780b9f6be5cef1665dda03680`, the prior test-only fixture correction on frozen Utilities. This deliverable adds two test files and does not modify that prior fixture or production code.

Run from the qualification directory:

```sh
ASAN_OPTIONS=detect_leaks=0 python Utilities/scripts/test_battery_charging.py --system-apps System
```

The archive includes the new test source/runner, the two-file patch, test logs, inspected contact sheet and provenance receipts. The source repositories themselves are not bundled. `local-deliverable.json` records exact identity and limits; `SHA256SUMS` covers the evidence package.

## Remaining qualification and restrictions

- These are frozen-source host tests with a focused power profile. They do not requalify the full native-toolbar, BLE, Contexts or low-battery feature composition.
- No target compiler is available (`xtensa-esp32s3-elf-gcc` and `xtensa-esp-elf-gcc` are absent). No fresh target ELF build or byte-for-byte reproduction was attempted.
- LeakSanitizer was disabled because the environment's ptrace setup does not support it; AddressSanitizer and UndefinedBehaviorSanitizer passed. This is not a LeakSanitizer pass.
- Screenshots use modeled telemetry, not a physical watch. Charge current, SOC accuracy, actual charger behavior/protection, device display/touch and sleep power remain unqualified.
- Remaining task acceptance is on-device screen/state correspondence and controls on the identified published candidate under the separately authorized device-testing flow. No immediate hardware request is made here.
- No device access, flash, RF, push, PR, merge, tag, release, catalog change, product-pin change, unknown delivery replay or outside-owner contact occurred.
