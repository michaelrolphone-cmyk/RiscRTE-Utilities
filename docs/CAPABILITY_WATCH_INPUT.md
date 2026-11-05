# Capability-sized watch input

Watch controllers use the display dimensions supplied by T5AppApi, backed by
`display.output.get_info`. No board name or per-device application binary is
needed. The native-height Nova renderer scales both axes by physical width/240;
input now uses its inverse. Without that profile the legacy centered viewport
uses matching offsets. Paper presentations retain their own mapping.

The correction covers Spectrum, RF, LoRa, Calculator, Battery, Stopwatch,
Frequency Generator, Alarms/Countdown and BLE Scanner/Touchpad/Buttons. HID raw
snapshots are validated against the physical display before mapping a copied
snapshot; sequence, neutral-contact, gap and encryption gates remain intact.

Audio Spectrum includes PR #68 / 5d8b116. Its operation-scoped storage proxy and
startup retention fence are preserved. Native storage exclusivity and the
custody adapter's terminal-retention behavior are not relaxed.

Run tests/watch_color_coordinates_test.c with PORTABLE_WATCH_NATIVE_HEIGHT and
the System SDK include directories. It checks every pixel of a rendered target
against inverse input coordinates at 240x240, 410x502 and 320x480. Existing
Spectrum, Calculator, Battery, Frequency Generator and HID behavior suites also
cover the unchanged 240-pixel UI. Local macOS runs used normal and UBSan builds;
ASan stalled before reporting a result. Target builds use ESP32-S3 GCC 8.4.0.

The RF trace test now counts the thirteen startup stage messages separately and
requires steady-state diagnostics to remain unchanged across 100 more captures.
Those messages predate this correction (0d80722); the previous total<=8 assertion
was stale. Spectrum's old opt-out preprocessing baseline predates the approved
storage lifecycle change and must be explicitly advanced with this correction.
Host tests and target ELF validation do not establish hardware qualification.
