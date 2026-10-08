# Watch BLE broadcast service 0.1.0 reservation

Reserved 2026-10-07 after checking remote branch/tag and open pull-request state.
This is an ordinary, deployment-selected telemetry.broadcast@1 provider. It owns
its session across app invocations; every selected foreground client polls it.
The Watch profile selects boot-session lifetime only after the default app starts.
System adapter and Utility source changes are isolated from delivered artifacts.
Battery source 1.1.5 is reserved for control/status. The Watch integration owns
fresh deployment identities for every relinked app.

Dependencies: bluetooth.telemetry@1 and platform.clock@1. No provider storage
binding is introduced. Existing foreground apps load/save ble_broadcast and read
quick_radio through their already-authorized shared namespace1, then pass a
copied policy structure. This preserves installed-provider namespace admission.
Control writes are verified; failed writes latch publication off until explicit
retry. Missing preferences mean enabled, but Bluetooth Off/Airplane remains final.

All seven standard scalar telemetry metrics are supported, including actual
source-provided temperature, humidity, pressure and light. The current Watch
source provides only battery percent, voltage and charging. No board lookup,
fabricated reading or raw hardware access is involved. Bounded selection gives
one field per supported metric priority, then fits additional fields within the
31-byte BTHome payload; omitted current fields are reported in status and UI.
The Battery page explains that nearby receivers can read these open values.

The service polls at foreground settled-frame boundaries. It pauses and proves
close before sleep, radio controls, failure exit, HID or passive BLE scanning.
Ordinary app switches keep ownership. Faults release immediately and retry after
five seconds; failed cleanup retains the graph and allows only cleanup retries.
No task or ISR is created and no code or callback is placed in Runtime.

Hardware RF, phone/Home Assistant interoperability, current and actual timing
remain unqualified. No flash, merge, catalog or release publication is included.

## Reproduction

- `python3 scripts/test_telemetry_broadcast.py --system-apps PATH --drivers PATH`
- `python3 scripts/test_battery_broadcast.py --system-apps PATH --drivers PATH`
- `NATIVE_APP_CC=.../xtensa-esp32s3-elf-gcc python3 scripts/build_telemetry_broadcast.py --system-apps PATH --drivers PATH`

The core and UI/service integration pass normal and ASan/UBSan execution;
GCC8.4 target output passes the production ELF validator and bounded symbol
checks. The Battery screen's production 240x240 raster was visually inspected.
Selected readings are re-evaluated every five seconds so newly available sensor
fields are included, without silently advertising an unavailable value.
A persisted Disabled preference survives provider restart. Invalid broadcast or
radio records fail closed and are displayed as unavailable; reads never replace
an invalid stored value. Corrupt broadcast control can be repaired only by an
explicit Enable/Disable command and verified readback.

App-data coexistence: the app-local PortableBroadcastAppData facade stops RF
before each native stat/read/replace. Once native storage returns RETAINED, the
facade and app perform no further radio or storage I/O. It preserves original
outcomes/revisions/bytes. Pause refusal prevents the storage call entirely and
keeps the invocation inside a cleanup-only retry loop; it does not synthesize a
storage-retained result without the corresponding native fence. Spectrum/RF use this fence; Timecard integration must
bind its actual app-data capability through the same facade before its bridge.
The facade's normal/sanitized tests cover ordering, uncertainty and no-I/O-after-
retention; the actual Spectrum temporal app also passes with the BLE flag.
