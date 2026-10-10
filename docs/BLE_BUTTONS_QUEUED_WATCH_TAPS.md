# BLE Buttons queued taps on Watch and X4

The original X4 report (secure connection, Ready tiles, no commands) predates the queued-tap repair shipped in Buttons 0.1.19. Delivered X4 0.1.57 includes that repair in Buttons 0.1.21; its actual GT911 0.1.9 and shared raster path freshly pass the 26-case regression. No newly observed X4 0.1.57 hardware failure is claimed.

A distinct shared-app omission is confirmed on Watch: the same valid short DOWN/UP pair is consumed while the final snapshot is neutral, but recovery was compiled only for paper resident clients. A 20 ms programmed Ctrl+Alt+Delete contact through the production FT6336U 0.2.1 provider emits no report before the change and one exact press/neutral pair afterwards. The old FT6336U 0.2.0 source has an already-known sequence defect and is not used as the corrected-provider control.

The production change makes the existing queued-tap recovery available to every Buttons profile. Its single-contact, same-tile, ordered-sequence, 100 ms, neutral, authenticated/encrypted, subscription and connection-generation gates remain intact. Touchpad and pairing logic are unchanged.

Live reservation: 72 remote Utilities branch heads and 92 heads/tags were inspected on 2026-10-10. The highest discovered resident Buttons version is 0.1.21; generic historical Buttons is 0.1.14. Reserve Buttons 0.1.22 for both builders. No provider, Touchpad or product image version is changed.

## Qualification

- 25 real-FT6336U queue/safety cases pass normally and under ASan/UBSan, including sampled/queued/held taps, timing limits, stale status, contact/sequence changes, report failures, mouse/wheel/motion and cleanup.
- Actual production app + FT6336U + real NimBLE secure pairing/CCCs produce the exact programmed keyboard press and neutral ATT reports in FIFO order. Report and boot protocols pass normally and under ASan/UBSan; two repeated contacts per run have no delayed watchdog duplicate. The unchanged baseline produces no ATT report for the identical contact.
- Current X4 raster/actual-GT911 tests pass all 26 cases against both installed 0.1.9 and separately qualified 0.1.10, normally and under ASan/UBSan.
- The full existing generic HID model/application/Nova suite passes its declared System a7f08a9 pin, including normal and ASan/UBSan. The app/model portions also pass against current System; its old direct-Nova fixture is incompatible with new raster helper internals, so that attempted aggregate is not claimed as passing.
- The real NimBLE provider protocol suite and port tests pass both modes; production provider and pairing code are unchanged.
- Both declared target profiles compile with their existing structural/import gates. X4 uses current qualified System bd98b2e (public 8a758629), native 0f17a435 (public b25b1d4), and explicit raster selection. Generic Watch uses its unchanged declared System a7f08a9 pin. Buttons is 0.1.22; both Touchpad targets retain their versions and are byte-identical to their corresponding unchanged-source control.

## Reproduce

Run `scripts/test_hid_buttons_watch_queue.py --system-apps SYSTEM --watch WATCH --output ABSENT_DIR`, optionally with `--sanitize`. WATCH supplies corrected `drivers/current/twatch_touch/driver.c` (0.2.1), `sdk/driver` and `include`. The backend simulates only I2C/GPIO/time; production touch and app code are compiled independently.

For the wire seam, add `--seam` to build `app-seam.so`. Build the unchanged Drivers 58ad371 real provider with `scripts/build_ble_hid.py --host`, then run `scripts/test_hid_buttons_wire.py --drivers DRIVERS --provider DRIVER_SO --app-seam APP_SO --output RESULT_JSON`, optionally `--boot-protocol`. Sanitized host libraries use ASan/UBSan with the ASan library preloaded into Python and leak detection disabled. `--expect-baseline-loss` is only for the unchanged app control.

No RF, physical host/device or Windows/macOS key acceptance was tested. This source increment does not change the delivered X4 0.1.57 image. Secure connection and Ready indicate eligibility, not proof that a host acted on a report. The original X4 hardware report remains attributable only to its unconfirmed older firmware until retested.
