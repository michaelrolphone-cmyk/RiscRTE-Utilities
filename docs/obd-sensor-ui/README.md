# OBD 0.2.3 sensor selection responsiveness

Based on the reviewed 0.2.2 app at a9c6dfea6800676bc272c118cab80c6c41e45c9a.
The original was reproduced byte-for-byte before patching. This is an SD app
replacement for X4 0.1.105; native firmware, BLE provider, ELM parser, request
sequence and timeouts, dashboard data model, scene provider and fonts are unchanged.

## Changes

- User edits acknowledge the logical scene without waiting for an old frame.
  Periodic data repaint still waits, and live contacts retain the input fence.
- A bounded scene pump advances up to eight slices, checking a four-millisecond
  budget between calls and returning the first semantic event immediately.
  No polling delay is added. Display frames remain owned by the shared presenter.
- Save checkbox edits once after leaving the picker, or at existing app-close /
  shared-control checkpoints, rather than synchronously saving between taps.
- Checkbox values are idempotent requests, not blind inversions. A previously
  selected unsupported sensor may be removed but cannot be newly enabled.
  Existing selection limits and the last-graph-series rule remain.

## Evidence

Production app and production shared scene, with explicit modeled BLE and display:
- Before: the second single tap changes the app model but leaves the checkbox
  document stale while a frame is pending. After: document changes within the
  next loop (10 modeled ms), and both selections persist when leaving.
- Same 480x800 / 100-ms display fixture: swipe to completed frame falls from
  1100 ms to 230 ms. These are fixture timings, NOT measured physical performance.
- All 25 existing scenarios plus both asynchronous interaction regressions pass
  normally and under ASan/UBSan. Value, unsupported-removal, work-budget, event
  ordering and activity-forwarding controls pass too. ELM code is unchanged.
- ESP32-S3 GCC 8.4.0 build repeats byte-identically: 77,720 bytes,
  SHA256 afd304767b74a36edb1c3605762cd1ed6eeb6f5882ac6fbaed697ff79380f7ea.

Replace only Apps/obd-dashboard/obd_dashboard.elf and its manifest.json on SD
while OBD is closed. Preserve settings.v1 and all other SD files. Restart before
loading the replacement. No full flash, reset, or firmware update is required.
Edits left in the open picker are session-local until leaving or clean app exit;
an abrupt power loss before that checkpoint can discard those pending edits.
Physical touchscreen/radio/storage performance still requires testing.
