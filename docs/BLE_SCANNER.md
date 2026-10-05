# Watch BLE Scanner 0.1.0

A Nova7 scanner with an explicit Scan/Stop control, a scrolling results list,
service/manufacturer filter, and scrolling device details. Start a 15-second
passive scan after enabling Bluetooth in the existing quick controls. Airplane
mode and invalid radio settings fail closed. No scan starts on app launch.

Results contain address/type, advertised name, RSSI, last seen, report count,
16-bit service UUIDs, manufacturer identifier and raw advertising bytes.
Unencrypted BTHome v2 advertisements additionally decode battery, temperature
and humidity; encrypted or unsupported data is labelled without guessing.
The list holds 32 stable identities. Further devices are reported as a full list;
matching reports update existing entries. Rescan clears results only after
successfully claiming the controller. Discovered results are not persisted or exported.

## Host and ownership

The allocation-free host uses HCI Reset, Set Event Mask, LE Set Event Mask,
LE Set Scan Parameters and LE Set Scan Enable with passive scanning. It
respects command credits, matches command completions by opcode, times out
missing completions and bounds packet processing per foreground iteration.
Only legacy advertising reports are parsed. GATT connections, pairing and
manufacturer-specific encrypted decoders are not implemented by this increment.

Requires the Watch `twatch-ble` 0.3.0 append-only host lease. Ordinary packet and
power-control calls are refused while an app owns the lease. Closing resets the
controller and proves scanning has ended before restoring prior enabled state.
An unsuccessful close keeps its lease and capability grant; normal provider I/O
and unloading cannot continue with uncertain cleanup. An unsuccessful safe
power restoration is displayed distinctly from scan success.

The shared `PORTABLE_RADIO_SESSION` hook stops scanning before alerts, quick
controls, sleep, handoff and teardown. Dismissal/wake never restarts a scan.
The scanner does not change the saved Bluetooth/Airplane preference.

## Build and deployment inputs

- `sdk/ble-sources.json` pins the exact shared adapter.
- `python scripts/test_ble_scanner.py --system-apps PATH` runs parser/host and
  real app-controller fixtures in ordinary and ASan/UBSan modes, plus production
  Nova pixel frames. Includes 50,000 deterministic malformed packet inputs.
- `python scripts/build_ble_apps.py --system-apps PATH` emits the structurally
  validated Xtensa ELF, manifest, exact-source evidence and font licence notices.
- The Watch deployment must add `ble_scanner` to the current app inventory,
  launcher catalog and boot policy; grant display, touch, alarm, Bluetooth
  instance 16, preferences namespace 1, and the existing quick-control RTC/Wi-Fi
  capabilities. Use the standard Nova/alarm/navigation/sleep/quick-control flags
  plus `PORTABLE_RADIO_SESSION` and `lib/Bluetooth/include`.
- The new unique launcher glyph is `solid:f7c0` (satellite-dish).

This is source/target verification, not physical qualification. Before release,
verify discovery against known sensor advertisements on the actual Watch,
RSSI/name/details, full lists, Stop/Rescan, Bluetooth/Airplane transitions,
alert interruption, quick controls, sleep/wake, repeated launch/exit and measured
power return. No device access, flash, release or merge occurred in this work.

Protocol references: [Bluetooth HCI functional specification](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html),
[BTHome v2 format](https://bthome.io/format/).
