# Shared BLE Touchpad source convergence

This application is independent of the earlier native-resolution opt-in. Its source uses semantic scene and pointer intents only. It contains no display dimensions, raw contacts, framebuffer calls, product classes or backend selections.

The reusable HID session controller is extracted unchanged from shared Buttons at 40cbe0739ebc5cfddae238835a9e5fdbd6836e75. Buttons keeps its compatibility facade and original behavior, verified by its full existing focused tests. Touchpad reuses the same ownership, policy, scheduler and background components. Gesture recognition is moved into the generic System scene provider, preserving the original gesture algorithm while adding validated event replay and shared lifecycle arbitration.

Existing native sources and HBv1 records are preserved. Touchpad does not read or rewrite Buttons assignments. Bluetooth bond storage remains provider-owned, with explicit stopped-session Forget. Radio and background settings keep their original keys and authority instances. A connection change closes and reopens the scene before fresh pointer input; old deltas cannot be replayed.

Unavailable pointer/held input, HID, XY scroll, shared controls, alarms, manual power key, background learning and idle policy are disclosed. No UI means no Bluetooth or storage acquisition.

Qualification is pending. Hosted source-pinned tests and a repeated canonical ELF build are required. Product admission, public package bytes and physical hardware acceptance are separate evidence.
