# BLE Buttons queued taps on Watch and X4

The original X4 report (secure connection, Ready tiles, no commands) predates the queued-tap repair shipped in Buttons 0.1.19. Delivered X4 0.1.57 includes that repair in Buttons 0.1.21; its actual GT911 0.1.9 and shared raster path freshly pass the 26-case regression. No newly observed X4 0.1.57 hardware failure is claimed.

A distinct shared-app omission is confirmed on Watch: the same valid short DOWN/UP pair is consumed while the final snapshot is neutral, but recovery was compiled only for paper resident clients. A 20 ms programmed Ctrl+Alt+Delete contact through the production FT6336U 0.2.1 provider emits no report before the change and one exact press/neutral pair afterwards. The old FT6336U 0.2.0 source has an already-known sequence defect and is not used as the corrected-provider control.

The production change makes the existing queued-tap recovery available to every Buttons profile. Its single-contact, same-tile, ordered-sequence, 100 ms, neutral, authenticated/encrypted, subscription and connection-generation gates remain intact. Touchpad and pairing logic are unchanged.

Live reservation: 72 remote Utilities branch heads and 92 heads/tags were inspected on 2026-10-10. The highest discovered resident Buttons version is 0.1.21; generic historical Buttons is 0.1.14. Reserve Buttons 0.1.22 for both builders. No provider, Touchpad or product image version is changed.

Working checkpoint: the focused real-FT6336U safety matrix passes 25 cases normally and under ASan/UBSan. Full real-NimBLE report qualification, existing suites, target builds and final source sealing are in progress. No RF or physical device was used.
