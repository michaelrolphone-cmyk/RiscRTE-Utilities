# Native BLE Buttons

The opt-in standalone successor keeps the existing HID controller and assignment
model while replacing only the color Buttons view. It uses capability dimensions,
native rectangles, the existing native font/icon rasterizers and the same
rectangles for physical contact mapping. It does not scale a completed 240-pixel
viewport. Touchpad and 480x800 paper remain on their existing paths.

## Geometry and behavior

`Apps/ble_buttons_native_layout.h` partitions the physical canvas into a header,
content and footer, then app-local grids. The main view, Settings, mapping editor,
eight modifier controls, forget confirmation and pairing confirmation all share
these definitions with hit testing. Text uses native font measurement and bounded
fallback/ellipsis; the Back icon is the native `solid:f053` asset. Representative
profiles are 128x128, 160x128, 240x240, 410x502, 320x180, 502x410 and 480x800.
Exhaustive sampled bounds tests include 13,671 dimension pairs. Bounds acceptance
alone does not imply useful readability at every nonzero dimension.

The existing pairing comparison and authenticated-report gates, mapping kinds,
modifier bits, atomic save/retry behavior, ordered fast contacts, multi-contact
cancellation, mouse/wheel actions and retryable cleanup remain in the shared
controller. Legacy Touchpad gesture/scroll tests remain required.

## Presentation and independent input subscriptions

The initial native geometry prototype exposed a real pre-visible input failure:
a tap opened Settings before first completion; a contact that began before
visibility also activated Settings on its later release. The original failing
receipt, logs and captures are retained in `evidence/previsibility-baseline/`.
It is historical negative evidence, not a qualification of the successor.

The selected System dependency adds app-private owner5 under
`PORTABLE_BLE_BUTTONS_NATIVE_UI`. Its generation/layout/token mechanism follows
the existing native owners. The UI subscription filters unseen/stale contacts;
the independent raw HID subscription separately queries the same provider's
visible sequence watermark without consuming events. A fresh DOWN must start
after visibility. Same-layout redraws preserve an owned hold and release; page
changes cancel it. Stale completion tokens cannot reset a newer owner. Visibility
does not disable healthy HID polling, release, close or cleanup retries.

## Full-feature recipe and evidence

See [packaging.md](packaging.md) for exact predecessor recipe provenance, selected
source pins and the standalone publication pipeline. Native mode also selects the
already-delivered `PORTABLE_ALARM_NATIVE` and `PORTABLE_QUICK_NATIVE` overlays.
The retained recipe without these selectors exposed off-screen Alarm dismissal
and rejected Quick rendering at 320x180; the selected native recipe qualifies
both touch dismissal and Quick open/close across all seven profiles. The shared
overlay implementations are not rewritten here.

Production-adapter tests exercise native primitives and actual RGB565 pixels,
physical input, same-layout release, page changes, stale completion, repeated
entry, raw-HID ownership, snapshot faults and retained lifecycle failures. Separate
controller tests cover all mappings, modifiers and save/forget/pairing actions.
Normal and ASan/UBSan receipts include commands, consumed dependencies, stable
source checks, logs and captures. Existing selector-off source-token and full
target-byte comparisons remain exact.

These are software/host and structural ELF admission results. Physical panel,
touch controller, Bluetooth transport, target instruction execution, device
memory fit and installation are not qualified. No delivered firmware or historical
release asset is replaced by this standalone development successor.
