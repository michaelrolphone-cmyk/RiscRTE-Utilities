# BLE HID 0.1.3: Nova7 paper presentation

Utilities base: `9889ee7c510fa8efeaba6cf8de8d25e42c88ae43`.
System Apps: `2aa0cf63346e507525af884bbfbf6b69313442c4`.
Production Runtime admission fixture: `2317b4fca374bb2b9cfe8dd204aa22244d1db609`.

Both ordinary HID apps select paper through `paper_presentation_get()` from
retaining MONO1 display capabilities. Watch retains the 240×240 Nova UI, action
model, accepted pairing/transport diagnostics, reconnect gating and cleanup.
Paper requires logical 480×800 touch; the deployment rotates its native 800×480
MONO1 surface by 90 degrees. No board-name selection or new HID provider exists.

Paper presents a 416×400 touchpad, four 200×192 button surfaces, 88-pixel action
rows, all eight modifiers and every existing keyboard/mouse assignment. Raw
mouse movement preserves pixel deltas, not scaled Watch coordinates. Existing
one/two-contact click limits, authenticated subscription requirements, gap gates,
held reports and ordered release reports remain shared. Paper avoids redraws
while an assigned key/button is held. Each control reports readiness; unavailable
mouse/keyboard subscriptions never send reports.

The paper names are **Paper Touchpad** and **Paper Buttons**. Enable Bluetooth
policy in the deployment's existing radio settings before starting. Paper
QuickActions deliberately has no app HCI/Wi-Fi authority: it labels radio
controls unavailable. It truthfully gates sound/frontlight/Torch on the existing
provider capabilities. Opening QuickActions drains HID; dismissal preserves
button drafts and requires explicit Reconnect. Save failures retain the draft
and expose Retry save; Cancel, Back and Home never save it implicitly.

Local Back returns from modifiers to the editor, from other nested views to the
main screen, and from the root to `springboard.elf`. Global Home directly queues
`default.elf` after cleanup. Rejected top-edge taps use the shared adapter's
began+released replay without being lost or repeated. Pairing consumes a fresh
raw contact; a paper-specific neutral gate prevents its release from becoming a
Reconnect/Settings tap after a slow refresh. Failed close, release and
unsubscribe retain the invocation until cleanup succeeds.

## Build and authority

```sh
python scripts/build_hid_apps.py --system-apps ../RiscRTE-System-Apps
python scripts/build_hid_apps.py --system-apps ../RiscRTE-System-Apps --paper-profile
```

The paper output is `dist/hid-paper/{ble_touchpad,ble_buttons}.{elf,json}`;
Watch stays in `dist/hid-apps`. Each directory contains a build receipt, content
hashes, exact flags/source hashes, and font licenses. Only canonical Runtime
fields appear in deployed manifests; deployment notes live in receipts.

Both profiles select `PORTABLE_PAPER_HID`, `PORTABLE_ALARM_CLIENT`,
`PORTABLE_RADIO_SESSION`, `PORTABLE_NOVA_UI`, `PORTABLE_APP_OWNS_TOUCH_CHROME`,
`PORTABLE_QUICK_ACTIONS` and `PORTABLE_RETURN_APP="springboard.elf"`.
Paper adds `PORTABLE_DISPLAY_ROTATION=90`, `PORTABLE_INPUT_NAVIGATION` and
`PORTABLE_HOME_APP="default.elf"`. Watch retains `PORTABLE_FORCE_FULL_FRAMES`
and `PORTABLE_QUICK_RADIOS`. Keep `PORTABLE_TOUCH_ROTATION=0` (the default).

The machine-readable deployment receipt is [Apps/paper-hid.json](../Apps/paper-hid.json).

| Capability | API | Paper binding |
|---|---:|---|
| display.output | 1 | 3 |
| input.touch.raw | 1 | 4 |
| input.navigation | 1 | 6 |
| board.battery | 1 | 7 |
| rtc.clock | 2 | 8 |
| bluetooth.hid | 1 | Global0, existing generic provider |
| storage.key-value | 1 | namespace 1, shared policy/preferences |
| alarm.service | 1 | deployment's visual service |
| storage.key-value (Buttons only) | 1 | private namespace 11, `hid_buttons` |

Paper raw touch acquires the default granted binding (instance 0); it does not
mistake paper navigation instance 6 for Watch raw-touch instance 6. Watch keeps
its existing explicit raw-touch request. Namespace 10 remains driver-owned bond
storage. No new driver dependencies or product grants are activated by this repo.

## Verification and inspected pixels

```sh
python -m unittest discover -s tests -v
HID_SANITIZERS=undefined python scripts/test_hid_apps.py --system-apps ../RiscRTE-System-Apps
HID_SANITIZERS=undefined python scripts/test_hid_renderer.py --system-apps ../RiscRTE-System-Apps
HID_SANITIZERS=undefined python scripts/test_hid_renderer.py --system-apps ../RiscRTE-System-Apps --paper
python scripts/test_hid_runtime_prepare.py --runtime ../RiscRTE --packages dist/hid-apps
python scripts/test_hid_runtime_prepare.py --runtime ../RiscRTE --packages dist/hid-paper
```

Actual app/adapter fixtures cover both displays, pairing accept/reject/movement/
multitouch/gaps, mouse subscriptions and reconnect, transport failure and explicit
retry, held keys, alarm/QuickActions interruption, ordinary and raw Home, nested
Back, save/cancel/retry, draft preservation, and close/release/unsubscribe retry.
MONO1 has guarded native strides and a simulated 1.6-second presentation delay.
Every scenario requires all grants/subscriptions and held HID reports to drain.
Peripheral HID APIs are fake; the actual generic radio backend is unchanged and
no physical pairing or radio connection was attempted.

Production Runtime `prepare()` reads the built manifests, resolves synthetic
provider declarations at the deployment's physical binding IDs, accepts both
packages and rejects an unsupported manifest field or a missing required grant.
It does not execute an ELF or activate any driver. Both target profiles also
pass the real ELF structural validator, target ABI and import/export checks.

Normal and UBSan runs pass locally. An ASan+UBSan pairing probe timed out on this
macOS host before emitting fixture diagnostics; ASan is **not** claimed as passed.
Linux CI retains the default ASan+UBSan suites and both target/admission profiles.

The actual [scanner reference](nova/screens/ble-enable.png) was opened and
inspected at 480×800 before authoring. The checked-in paper captures below are
from the production adapter/rasterizer, use its Rajdhani/Orbitron typography and
available Font Awesome masks, and were visually inspected for clipping and layout:

- [Touchpad](nova/screens/hid-paper-touchpad.png)
- [Buttons](nova/screens/hid-paper-buttons.png)
- [Six-digit pairing](nova/screens/hid-paper-pairing.png)
- [Unsaved editor](nova/screens/hid-paper-editor.png)
- [Capability-gated QuickActions](nova/screens/hid-paper-quick-actions.png)

No Runtime/product edits, hardware/radio actions, merges, releases or BIN
qualification are part of this checkpoint. Physical panel latency, actual host
interoperability and device power behavior remain integration qualification.
