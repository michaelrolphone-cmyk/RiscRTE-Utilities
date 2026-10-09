# BLE Touchpad 0.1.14

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
   8 Watch pixels (16 on paper) cancels a click. Tap, then touch nearby again
   within 300 ms and drag to hold the left button. Lift or leave the pad to
   release it. A second stationary tap remains a second click.
5. Move two fingers to scroll. Near-horizontal and near-vertical starts lock
   that axis; a diagonal start remains free X/Y. A deliberate turn can leave
   an axis lock. The screen identifies a provider that needs a scrolling update.
6. Reconnect uses the existing bond. Stop closes the session. Settings → Forget
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
- The raw coordinate profiles are 240×240 Watch and 480×800 paper. Watch FT6336U supports two real contacts; X4 GT911 needs the multi-contact provider 0.1.9 or later.

See [Watch integration requirements](../BLE_HID_INTEGRATION.md). Host fixtures,
real Nova rasterization and target ELF validation are development evidence;
physical input latency and host/device interoperability remain unqualified.

See [Nova7 paper HID](../BLE_HID_PAPER.md) for the capability-selected 480×800
presentation, paper grants, Home/Back targets and verification evidence.

## Gesture and report contract

The independent recognizer consumes one complete snapshot per controller report.
It matches contact IDs, ignores ordering, and cancels on duplicates, replacement,
more than two contacts, leaving the pad, a queue gap or lifecycle interruption.
After two-finger scrolling, neither a remaining finger nor a reintroduced finger
can unexpectedly move the pointer before all contacts lift. Stationary drag holds
refresh every 400 ms; failed reports enter the existing checked session cleanup.

Scroll direction starts after an 8-pixel centroid slop (16 on the paper display).
A dominant-to-transverse ratio of at least 2:1 selects a rail. Diagonal starts
remain free for their entire gesture. Rail exit uses a decayed motion average
(three quarters previous, one quarter current), a wider transverse displacement
threshold of two slops, and three consecutive qualifying reports. This is the
common direction-lock/Schmitt-trigger approach with explicit hysteresis, informed
by Chromium's [scroll-rail input discussion](https://groups.google.com/a/chromium.org/g/input-dev/c/f-etbJtfEfc/m/cOjGbfsoHF0J).
It follows the familiar [tap-and-drag gesture](https://wayland.freedesktop.org/libinput/doc/latest/tapping.html),
with release on lift and no drag lock after lift. Thresholds are product defaults,
not a claim that these small touchscreens have been physically tuned.

Four Watch pixels (eight on paper) produce one wheel unit with fractional motion
retained between reports. Positive X sends Consumer AC Pan to the right; downward
finger motion sends negative vertical Wheel. No pointer movement is substituted
for a missing horizontal wheel. The optional size-gated
`risc_bluetooth_hid_scroll_api_v1` tail requires BLE HID 0.1.3; older providers keep
pointer, click and drag and display “Scroll needs driver update”. Legacy mouse
and diagnostic API prefixes remain compatible. Hosts that cache the old HID
report map may require removing and pairing the device again after this upgrade.

Focused normal and ASan/UBSan tests cover both surface densities, signs, axes,
initial diagonal motion, deliberate unlock, fractional deltas, ID reorder,
pinch rejection, timer wrap, all-up gating, drag refresh, report failure,
Settings/Back/disconnect/gap and cleanup retry. Physical gesture feel and host
report-map refresh remain hardware tests.
