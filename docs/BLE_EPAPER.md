# BLE Scanner 0.2.1: Nova7 e-paper and sensor provider

Reconciles PR40 head `4c4171734226b5832cc5a8a618f018d61ca31cb9` with
main `11cf206949cc8f9a70e9cff6b7fe2c925e03724f`. The capability-selected retaining
MONO1 presentation uses the same `bluetooth.sensors@1` model, saved names and
detail formatter as Watch. No app-private scan commands or parser are restored.

For X4's native 800×480 MONO1 buffer and GT911 480×800 touch:

    python scripts/build_ble_apps.py --system-apps <System-Apps> \
      --paper-profile --display-rotation 90 --navigation --partial-damage \
      --return-app springboard.elf --output-dir dist/ble-paper

System Apps is pinned to `969d2210e1bf517c1299801c8ad205c956e06810`, including
bounded synchronous paper progress. The default build keeps Watch full frames,
quick controls and its keyboard. The paper profile omits compact quick controls
and Wi-Fi. Layout selection follows display capabilities rather than board names.

## Behavior

Six 88-pixel rows, inverse selection, literal device names, paged details and
Font Awesome icons preserve the paper presentation. Sensors selects recognized
BTHome devices, including encrypted devices, rather than arbitrary BLE services.
Both layouts show provider readings, repeated metrics and unavailable-data labels.
Names sort first while preserving discovery order within each group; details keep
their device identity when aliases change the list order. Only detected devices
appear. Paper Name stops scanning and opens the shared character/action keyboard
with larger paper cells. Save verifies persistence; an empty name clears it;
Back cancels or returns from details. More and swipes page through details.

Snapshot-only touch works; swipes commit on release and footer gaps do nothing.
Idle does not refresh periodically. Scan results refresh at most every three
seconds, excluding explicit actions; startup avoids slow display submission.
The shared provider owns scan timing and exclusive controller custody.

When Bluetooth policy is disabled, Scan offers explicit Enable BT. It preserves
Airplane and other policy bits, checks controller state and storage readback,
and rolls back on failure. Unconfirmed cleanup retains the invocation. Enabling
does not scan; another explicit Scan is required. There is no pairing or ATT write.

## Required composition for the integration owner

Use RiscRTE-Drivers `aa5ce0140102bf4d1039e5f208588a6793d06b28`:
`Drivers/ble_sensors/manifest.json` packages the logical `ble-sensors` 0.1.0
provider (driver ABI 2), providing `bluetooth.sensors@1`. Install its package
manifest as `ble-sensors/manifest.json`, **without a hardware instance_id**.
Resolve its dependencies `bluetooth.hci@1` and `platform.clock@1`.

Grant Scanner `bluetooth.sensors@1` at logical instance 0 and retain
`bluetooth.hci@1` instance 16 for explicit enable/Watch quick controls;
`storage.key-value@1` namespace 1 for `quick_radio` and CRC-checked `bs...` aliases;
`display.output@1`, `input.touch.raw@1`, `alarm.service@1`, and, with
`--navigation`, `input.navigation@1`. `rtc.clock@2` remains the optional clock
contract. The paper manifest excludes `net.wifi`; Watch retains it.
Deploy the matching app manifest with the ELF. This integration unit does not
activate grants, install packages or change firmware composition.

## Regressions and evidence

- `test_ble_scanner.py`: production app and alias persistence; both layouts'
  named-first order, BTHome filtering, stable detail identity, correct viewport,
  repeated readings, paper keyboard/save, and alias reload; enable/rollback and
  retained-cleanup failures.
- `test_ble_renderer.py --drivers PATH`: eight Watch scenarios using the actual
  app, adapter, rasterizer and sensor provider over a synthetic controller.
- `test_ble_paper.py --drivers PATH`: thirteen scenarios with native guarded
  MONO1 stride, rotated snapshot touch and 1.6-second presentation latency,
  including save/cancel and persisted-name assertions. Idle submits once.
- Both target profiles pass Xtensa ELF structural and import/export validation.

Both host scripts also take `--system-apps PATH`. The default sanitizer set is
ASan+UBSan; `BLE_SANITIZERS=undefined` selects UBSan alone. This reconciliation
passed 69 Python tests, normal and UBSan host suites, and both target builds on
macOS. Local ASan stalls during runtime initialization before test code; ASan
remains enabled by default in Linux CI and is not claimed as a local pass.

Screenshots under `docs/nova/screens/ble-*.png` are production MONO1 captures.
No device access, RF test, flashing, release or hardware qualification occurred.

## Controls integration

The integrated Scanner0.2.3 pins System Apps2aa0cf63 and retains explicit
`--home-app default.elf --quick-actions` for paper builds. Paper grants no
Wi-Fi capability. Shared sensor decoding, names, order and details are preserved.
