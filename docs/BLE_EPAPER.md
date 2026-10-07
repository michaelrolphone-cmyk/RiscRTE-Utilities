# BLE Scanner 0.2.0: Nova7 e-paper

The shared scanner selects this static high-contrast layout only when the shared
adapter reports a retaining MONO1 portrait surface. Watch's 240×240 UI, quick
controls, packet parser, passive scan and exclusive HCI host lifecycle remain.

For X4's native 800×480 MONO1 buffer and GT911 480×800 touch, build:

    python scripts/build_ble_apps.py --system-apps <System-Apps> \
      --paper-profile --display-rotation 90 --navigation --partial-damage \
      --return-app default.elf --output-dir dist/ble-paper

System Apps is pinned by sdk/ble-sources.json. The paper profile omits compact
Watch quick controls and their Wi-Fi dependency. Runtime layout selection still
uses display format, retaining flag and logical dimensions, not a board name.
The unchanged default build retains Watch full frames and quick controls.

## UI and radio safety

Six 88-pixel result rows, inverse selection, literal device names, paged details,
large Scan/Stop/Services/Back controls, and Font Awesome satellite-dish/chevron
icons use the existing licensed monochrome font subset. Snapshot-only touch
works; swipes commit on release. Footer gaps do nothing. No animation, periodic
idle refresh, automatic launch scan, connection, pairing or ATT writes.

During passive scanning, new data is presented at most once per three seconds;
command startup is not blocked by a slow paper refresh. Explicit user actions
can present immediately. Detail ages are static scan snapshots. Scan duration
and command deadlines remain those in the shared controller.

If Bluetooth policy is disabled, Scan offers an explicit Enable Bluetooth
button. Only its activation changes the HCI controller and namespace1 quick_radio.
It refuses Airplane, preserves Wi-Fi/previous-radio bits, verifies hardware and
persistent readback, and rolls back on failure. Unconfirmed rollback or release
retains the invocation. Enabling never starts a scan: a separate Scan is required.
No background radio-settings page is implied. Airplane must be disabled using
an admitted radio-settings controller; this scanner cannot clear it.

## Composition and evidence

Grant bluetooth.hci@1 instance16 with the existing PortableBluetoothHost control
and lease suffix; storage.key-value@1 namespace1 read/write quick_radio;
display.output@1, input.touch.raw@1, alarm.service@1 and (with --navigation)
input.navigation@1. rtc.clock@2 remains the adapter's optional clock dependency.
No Wi-Fi dependency is emitted by the paper profile.

`test_ble_scanner.py` covers the parser and host cleanup, including failed claim,
restore, release and explicit-enable rollback. `test_ble_renderer.py` exercises
Watch unchanged. `test_ble_paper.py` compiles the real app and adapter against a
native landscape MONO1 surface (104-byte guarded stride), portrait snapshot touch
and 1.6-second present latency. Ten scenarios run ordinary and ASan/UBSan: idle,
footer gap, complete scan, stop, details, bidirectional swipe, Back, enable-only,
enable-then-scan and Airplane. Idle submits exactly once; enable-only sends no
scan command; active scan completes and releases its lease before app exit.

Both exact-pinned GCC8.4 target profiles pass the structural ELF and import/export
validators. These are development artifacts, not hardware qualification.
Screenshots in docs/nova/screens/ble-*.png come from the actual MONO1 adapter.
