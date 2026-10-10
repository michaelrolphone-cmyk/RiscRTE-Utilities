# X4 paper battery broadcast selection

`Apps/native-ble-broadcast.json` selects the common native custody adapter,
checked paper motion and the unchanged Watch telemetry-broadcast 0.1.0 service.
Battery 1.1.7 adds capability-selected paper broadcast controls. Basic gauge
screens show percent, millivolts and charging; they never imply a temperature
sensor or PMU controller. The existing Watch and raw-paper flag-off Battery ELF
bytes are unchanged against the original pinned profiles.

Broadcast is off when `ble_broadcast` is absent. Enable/Disable saves the checked
four-byte record in existing shared namespace 1 and reports failed persistence
for retry. A saved Enable is only intent: Bluetooth-off/airplane policy prevents
advertising, and the screen directs the user to Quick Controls. Nearby receivers
can read the open BTHome payload without pairing. Failed/invalid settings remain
off, and valid zero percent remains distinct from unavailable gauge data.

`build_native_broadcast.py --system-apps SYSTEM --runtime RUNTIME` produces the
seven selected utility versions, exact manifests/boot grants, compiler/source
receipts and license custody. Set `NATIVE_APP_CC` to pinned GCC 8.4. Scanner,
Touchpad, Buttons and Waterfall select `PORTABLE_BLE_FOREGROUND`, keeping
telemetry paused for their complete foreground invocation. RF temporal AppData
uses the accepted pause/retention wrapper. Native UTC/alarm API2, paper motion
and plain stage logs remain selected. No ordinary Watch profile is changed.

`test_native_broadcast.py` compiles the actual Battery controller, native adapter
and production service with strict simulated peripherals. Eleven cases run both
normally and with ASan/UBSan: default-off toggle, missing Bluetooth policy,
airplane, persisted enabled, close failure, acquisition/release retention,
malformed/off records and save retry. Healthy cases release every grant; the
observed maximum is seven. Production-rendered paper frames verify both the
controls and Bluetooth-off guidance. The selected Battery and all seven utility
ELFs pass target import/export and loader validation. Service fixtures and target
checks cover copied policy, bounded retry, metric selection and cleanup.

The product-owned real Runtime test separately loads the production three-
provider chain and checks exact BTHome bytes, navigation persistence, real slot
capacity, timer silence and terminal cleanup. These tests simulate peripheral
hardware. No device was flashed, no RF was transmitted, and source publication
is still blocked on inherited System ancestry.
