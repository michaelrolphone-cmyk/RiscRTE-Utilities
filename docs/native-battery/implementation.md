# Battery native color UI

This is an explicit, Battery-local opt-in. `PORTABLE_BATTERY_NATIVE_UI` requires
`PORTABLE_NATIVE_CANVAS` and remains incompatible with the separate paper view.
The default and selector-off paths are unchanged. There is no shared Utilities
layout framework, SDK, provider, Settings or shared idle correction in this change.

## Implementation

- `Apps/battery_native_layout.h` computes header, content and footer rectangles
  directly from capability-reported width and height. Rendering and touch use
  the same half-open rectangles. Native input bypasses the old 240/width inverse.
- `Apps/battery_native_view.inc` uses the foundation's native text and shape
  primitives. No completed 240-pixel viewport is scaled and no app-owned bitmap
  is allocated. Native glyphs retain their selected face metrics.
- Wide/tall views use an integer progress ring. Compact views retain percent,
  status, caption and controls without the ring. Zero is valid;100 percent alone
  does not establish charge completion. Failed reads show unavailable values.
- Details keep the six telemetry fields. On compact panels the explanatory
  charging page is split into three pages, so autonomous charging, USB/protection
  conditions and missing cell-temperature/time-to-full measurements remain
  accessible. Navigation uses the same dynamic page count in both directions.
- Text is measured before rasterization. A bounded visible ellipsis is used only
  when a fixed-width field cannot fit. Compact refresh uses `Read`; failures
  retain `Retry`. Representative usable captures begin at128x128; the exhaustive
  tiny-dimension test establishes safe bounded geometry, not readable1px UI.

## Existing broadcast reachability

The historical source explicitly introduced **paper** broadcast controls while
keeping ordinary Watch profiles unchanged: [native BLE telemetry notes](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/blob/4fdcad2495c33fe63f5f6faca9b11ced3c739a5b/docs/NATIVE_BLE_TELEMETRY.md).
The entry is in [battery_paper.inc:26–28,64–70](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/blob/4fdcad2495c33fe63f5f6faca9b11ced3c739a5b/Apps/battery_paper.inc#L26-L70).
The color overview has no corresponding entry in the pinned baseline. This
increment preserves that documented reachability. Native BLE screenshots invoke
that existing view directly; they are not evidence of an end-to-end color entry.
Compact direct views prioritize save error, open/unpaired broadcast disclosure
and absence of cell temperature over decorative explanatory text.

A separate backlog item records the proposed color entry and its acceptance:
[Expose Battery color broadcast controls](https://tasker-workboard.hazy-auk-4334.chatgpt.site/?task=3776b062-bf77-4380-baa2-9bd6fe3dc2fa).
It has no active execution and is not implemented here.

## Qualification boundaries

The full target recipe is described in [recipe.md](recipe.md). It is a new,
explicit software-qualification recipe, not a reconstruction of the .59 compiler
command. Exactly15 retained capability/version pairs are checked. The old
`PORTABLE_UTILITIES_TARGET_X4` documentation marker is not selected: current
`utility_preference_atomic.h` admits the generic atomic capability using the
explicit preference selector. That is a runtime API dependency, not evidence
that every board has supplied a compatible grant.

Host UI tests compile the actual Battery controller and exact foundation adapter
with modeled hardware and a reduced feature recipe. They establish those modeled
paths only. Separate atomic controller cases establish transaction semantics but
linker-discard the renderer. Full-feature target ELF validation checks loader
structure and imports/exports without executing target instructions. These three
evidence types must not be substituted for each other.

Automatic idle remains selected in the full recipe. The shared lifecycle is not
suppressed or corrected here. An inherited lifecycle risk is not called a proven
Battery failure without an exact selected-recipe witness. Physical hardware,
actual charging accuracy/protection, RF transmission, product grant binding and
installation remain unqualified. No .59 asset is changed and no app package or
firmware is published by these scripts.
