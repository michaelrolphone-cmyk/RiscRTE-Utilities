# BLE Buttons 0.1.0

Four configurable Nova7 remote controls sharing the same secure BLE HID session
and driver bond as [BLE Touchpad](ble_touchpad.md). The host sees **Watch Buttons**.
Pair/reconnect, six-digit comparison, rejection, Forget and cleanup work the
same way as Touchpad.

## Assignments

Default buttons are Page Up, Page Down, Left Ctrl+C and Left Ctrl+V. Keyboard
keys and mouse buttons remain held while one contact stays within its original
button. Moving out, adding another contact, a queue gap or interruption releases
it. Held keys/buttons refresh every 400 ms; one-shot wheel and pointer movements
do not repeat while held. A contact that starts outside a button cannot slide
into it to trigger an action. Controls remain unavailable until their own
keyboard/mouse subscription is ready on an authenticated encrypted connection.

Settings → one of buttons 1–4 opens a scoped editor. Tap the action type to cycle:

- Keyboard: A–Z, digits, punctuation, navigation/editing keys, F1–F12 and common
  lock/system keys. `<` and `>` select the HID key.
- Modifiers: independently toggle Left/Right Ctrl, Shift, Alt and GUI. Header
  Back returns to the unsaved draft. Keyboard layout and shortcut meaning are
  host-owned; these are HID usages, not Unicode text macros.
- Mouse button: left, right or middle.
- Mouse wheel: up/down by one or three units per press.
- Mouse motion: left/right/up/down by one, five or fifteen pixels per press.

Save commits all four validated assignments to `hid_buttons` in private scoped
namespace 11. The versioned 40-byte record has a checksum; reads reject malformed
or corrupt records. The app reports success only after a successful write and
exact readback. Failed writes remain visibly **SAVE FAILED** with Retry save and
Cancel; they do not replace the in-memory active assignments. Cancel and Back
leave the previous assignments intact. Missing storage uses defaults without
silently saving them. Unreadable saved data is reported.

See [integration requirements](../BLE_HID_INTEGRATION.md) for exact grants,
standalone builds, test coverage and remaining physical qualification.
