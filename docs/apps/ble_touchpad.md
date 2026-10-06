# BLE Touchpad 0.1.0

A portable Nova7 touchscreen mouse for the Watch's 240×240 raw touch surface.
It is built from `Apps/ble_touchpad.c` and the shared HID application/model;
there is no firmware-owned user interface or direct application HCI host.

## Use

1. Enable Bluetooth and turn off Airplane mode in the existing quick controls.
2. Open Settings → Pair. On the host, select **Watch Touchpad**.
3. Compare the six-digit code, including leading zeroes. Tap Accept only if the
   host displays the same number, or Reject otherwise.
4. Move one finger within the pad to move the pointer. A stationary tap within
   300 ms clicks left; a stationary two-finger tap clicks right. Movement over
   8 pixels cancels a click. Two-contact movement does not become a pointer jump.
5. Reconnect uses the existing bond. Stop closes the session. Settings → Forget
   host has an explicit confirmation and runs only after the session is closed.
   Remove the old Watch entry on the host before pairing again.

The advertised name is copied by the driver; the generic driver's one saved
host bond is shared by the touchpad and buttons apps. Pair does not replace an
existing bond. Only one app owns Bluetooth at a time. A disconnected active
session waits for its saved host; after a pause, explicitly choose Reconnect.

## Boundaries and failure behavior

- The pad uses an independent `input.touch.raw@1` subscription, not the legacy
  single-pointer UI adapter. FT6336U's genuine two-contact reports are required.
- Host reports require authenticated encryption and the mouse subscription.
- Numeric confirmation requires a new raw contact after neutral on the pairing
  screen. Touches carried over from navigation/start cannot approve it.
- Quick controls, alarms, sleep, Settings, Back, errors and exit close the HID
  and raw-touch sessions. Dismissing an interruption never silently restarts RF.
- Queue gaps, unexpected sampling delays and connection changes release all
  inputs and require neutral touch before another action. Each click sends a
  press followed by a release, in order. Close failures retain the token and
  grant and retry cleanup before continuing.
- The app samples/polls on the 20 ms foreground loop. Driver watchdog cleanup
  is additional protection, not a substitute for normal release handling.
- Battery service uses the board's real valid percent when available, otherwise
  the API's 255/unknown state. No fixed battery percentage is advertised.
- Only the explicit unrotated 240×240 Watch touch profile is supported.

See [Watch integration requirements](../BLE_HID_INTEGRATION.md). Host fixtures,
real Nova rasterization and target ELF validation are development evidence;
physical input latency and host/device interoperability remain unqualified.
