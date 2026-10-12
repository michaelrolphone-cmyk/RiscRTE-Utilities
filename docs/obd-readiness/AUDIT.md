# OBD 0.2.2 test readiness audit

Status: working checkpoint, 2026-10-12. No hardware, adapter, vehicle or target
instruction execution is claimed. This audit preserves the selected OBD 0.2.1
feature set, including dashboard, plots, diagnostics, alerts and trip recording.

Selected source: Utilities `52422dfb3a1dac21e0ca82e7a1300098c2a0d16f`.
Current target SDK inputs: Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70`,
System `f4b30f8ebcc6d8cb905c8e90c91b1691acbe8165`, Drivers
`8ec572af94ee6e844b1685d8d050471cd58cad87`.

## Verified correction

`service_link` drains received bytes before calling the periodic timeout tick.
The 0.2.1 parser could therefore accept a complete reply at or beyond its request
deadline. `obd_receive` now checks that same monotonic deadline before decoding
bytes. Regression covers reset banner, a live PID and 32-bit clock wrap. The
unpatched source fails the new test. Normal and ASan/UBSan runs pass the protocol,
diagnostics, model, arithmetic and all 16 existing actual-app/scene simulations.

The current Xtensa target builds with the same import allowlist, exported entry
points and 1,536-byte per-frame stack ceiling. Current maximum compiler-reported
frame is 752 bytes; this does not establish a dynamic call-chain peak.

## Reproduction

Run `scripts/test_obd_dashboard.py` in normal and `--sanitize` modes with the
Runtime, System and Drivers roots above. Run `scripts/build_obd_dashboard.py`
with the same roots and the ESP32-S3 Xtensa 8.4.0 compiler. `NATIVE_APP_CC` can
select that compiler. The GitHub workflow pins these dependencies.

The final product overlay and complete X4 cohort qualification are still in
progress. This checkpoint is source and tests only. No binary is delivered here.
Physical BLE compatibility, connection latency and vehicle readings require
the owner's parked hardware test. Pairing/bonding-required and Classic-SPP-only
adapters remain unsupported by the existing BLE provider.
