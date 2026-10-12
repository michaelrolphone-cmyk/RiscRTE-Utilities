# OBD 0.2.2 test readiness audit

Software qualification, 2026-10-12. No physical adapter, vehicle, RF connection,
or target-instruction execution is claimed. The selected 0.2.1 dashboard, plots,
diagnostics, alerts, logging, controls and clear-confirmation features remain.

The app starts from Utilities `52422dfb3a1dac21e0ca82e7a1300098c2a0d16f`.
Dependency pins:
- Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70`
- System `f4b30f8ebcc6d8cb905c8e90c91b1691acbe8165`
- Drivers `3a1b31c6d2cc90d7f082dd13d20ff7df77bd127c` (BLE serial 0.1.1)

## Verified corrections

1. Receive-before-tick deadline race. The app drains RX before its periodic tick;
   previously, an already late reply could be accepted. Receive now checks the
   same deadline before decoding. Tests include setup, live PID and clock wrap.
2. Broadcast capability discovery. An unrelated ECU's negative response could
   reject another ECU's valid support bitmap. Only positive replies select the
   initial ECU; subsequent PID replies and rejections remain ECU-bound.
3. CAN payload bounds. Nine or more CAN bytes could be accepted as a single-frame
   reply. The parser now enforces the eight-byte frame ceiling while retaining
   valid padding, short frames, 11/29-bit headers and legacy protocol handling.
4. Persistent write backpressure. A provider returning zero bytes indefinitely
   could leave setup stuck. Five seconds of uninterrupted backpressure now closes
   the link and gives a retry message. Transient backpressure still recovers;
   partial writes fail closed, without appending or repeating a command suffix.
5. Ambiguous manual channel rows. Rows sharing service/TX UUIDs were identical
   even when RX characteristics differed. Existing rows now show bounded TX/RX
   handles and UUID text; screen structure and controls are unchanged.
6. Required provider correction. BLE serial 0.1.1 separates RX subscription mode
   from TX properties. A transmitter's Extended Properties bit no longer causes
   an indication-only receiver to be subscribed as notifications.

Every defect has a failing-before/passing-after regression. No vehicle traffic
was sent during these checks. Mode 04 remains an explicit, expiring, stationary-
engine-off confirmation flow, exercised only with fixtures.

## Coverage

The app suite runs protocol/diagnostics/model/arithmetic tests, five focused
lifecycle/channel check groups, and 25 actual-app/native-scene scenarios normally
and with ASan/UBSan. The added app scenarios cover persistent/transient blocked
writes, partial writes, Home during scan/connect/blocked TX, unknown vendor
channel selection, unavailable optional diagnostics, and the absent-peer
three-attempt reconnect limit. Existing cleanup/custody, pause/resume, persistence,
alerts, graphs, diagnostic reads and confirmed-clear regressions are retained.

All 87 existing checkpoint UI frame bytes are unchanged by the five app fixes.
The manual-channel text correction is separately checked for distinct RX rows.
The BLE provider passes 11 actual NimBLE HCI/ATT fixture cases normally and under
ASan/UBSan, including notifications, indications, authentication refusal,
write acknowledgement timeout, overflow and ten close/reopen cycles.

Local sanitizer processes disable LeakSanitizer for the ctypes/runner environment;
address and undefined-behavior checks are active. These tests do not establish RF
performance, owned-adapter compatibility, vehicle values or dynamic peak stack.

## Reproduction

Run `scripts/test_obd_dashboard.py` with `--runtime`, `--system`, `--drivers`, and
`--output` roots. Repeat with `--sanitize`. Run `scripts/build_obd_dashboard.py`
with the same roots and ESP32-S3 Xtensa GCC 8.4.0 (`NATIVE_APP_CC` selects it).
Generate System telemetry fonts with its pinned generator before host scene
compilation. The GitHub workflow pins all dependencies and performs these steps.

Run Drivers `scripts/test_ble_serial.py`, repeat with `--sanitize`, then run
`scripts/build_ble_serial.py`. The final X4 recipe validates exact public source
pins, target hashes, manifests, authority, all selected images and sparse packing.
Complete normal/sanitized product cohort qualification is recorded separately.

## Parked hardware test

- Use the paired firmware and cumulative SD overlay after their release time;
  preserve existing SD content and settings. Full initial flashing erases internal
  saved data, so do not assume a full image is a non-destructive update.
- Park before setup. Plug in the adapter, switch ignition on as appropriate for
  the vehicle, and close other OBD apps that may own the adapter connection.
- Search, select the actual advertised adapter, connect, and verify ECU discovery
  reaches LIVE. If manual channels appear, use their distinct TX/RX identities.
- Check supported readings, pause/resume, Disconnect, Home and a reconnect.
  Unavailable readings should show as unavailable, without manufactured zeroes.
- Read-only testing does not require clearing codes. Keep any engine operation
  safely ventilated. This app is informational, not a replacement for instruments.

Classic-SPP-only and pairing/bonding-required adapters remain unsupported. BLE
connection alone is not ECU validation. Confirm actual compatibility on hardware.

Protocol reference: [ELM Electronics ELM327 data sheet](https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DSH.pdf).
No external emulator implementation is included in this client.
