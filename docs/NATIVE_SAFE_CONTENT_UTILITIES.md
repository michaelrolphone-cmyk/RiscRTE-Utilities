# Native safe-content Utilities (explicit development profile)

`--native-safe-content --safe-viewport` selects native layouts for the existing
Calculator, Battery and Stopwatch controllers. Default legacy/Nova/paper branches
and original golden assets remain unchanged. No new app or hardware capability
is added. This is not a published release or a hardware acceptance claim.

## Reused contract

The exact selected System adapter provides `portable_nova_native_fill`,
`portable_nova_native_round` and `portable_nova_native_text`. These paint the
submitted surface directly and use the original complete Orbitron/Rajdhani native
font faces (with their existing OFL licenses). No240px bitmap is enlarged and no
new font rasterizer is introduced. Text selects an original smaller face when
needed; long labels are visibly ellipsized without changing their stored value.

`screen_width/height` are capability-derived **content** dimensions. The adapter
adds the safe origin once to drawing and inversely translates touch once; apps
never width-scale either coordinate. The shared native layout uses the same
rectangles for draw and hit tests. Shapes/aspect ratios are not product whitelists.
Content must be240..1024 pixels on both axes; undersized content is rejected.

For circular/elliptical displays, the provider must supply a conservative safe
rectangle enclosing all visible text/control footprints. The T5 application API
has no aperture query; this profile does not infer a circle from square dimensions
or claim full-aperture layout.466x466 with69px insets gives328x328 native content.
Asymmetric insets, odd strides, portrait and landscape retain the same contract.

The existing System admission macros require this explicit combination:
`PORTABLE_UTILITIES_NATIVE_SAFE_CONTENT`, `PORTABLE_NOVA_UI`,
`PORTABLE_RF_NATIVE_CANVAS`, `PORTABLE_SAFE_VIEWPORT`,
`PORTABLE_WATCH_NATIVE_HEIGHT`, `PORTABLE_WATCH_COLOR_ONLY`,
`PORTABLE_WATCH_SAFE_INSETS`, `PORTABLE_APP_OWNS_TOUCH_CHROME`. RF_NATIVE_CANVAS is an existing generic native-pixel
renderer switch; it grants no radio capability. Apps call native primitives only,
not width-scaled Nova primitives. Paper and reduced three-face font profiles are
rejected rather than silently substituted.

## Controller boundaries

- Calculator retains its arithmetic, nineteen keys, keyboard navigation and
  contact state machine. Header/value area and five-row keypad reflow from native
  dimensions; drawing/hit testing share `calculator_key_rect`.
- Battery retains the existing telemetry interpretation/unknown-value semantics.
  Native list pages follow selection, with Up/Update/Down controls. It does not
  enable enhanced power/BLE writes or invent missing telemetry. Render retries
  use the same latest-state frame gate as other Utilities.
- Stopwatch changes presentation/hit regions only. Native UTC, namespace2,
  retained custody, save uncertainty, Reset confirmation and existing sleep
  boundaries remain in the original controller. This profile may also compile
  the legacy RTC controller; the Badge composition must continue explicitly
  selecting `--native-stopwatch-runtime`.

## Reproduction

Use exact clean System `6e4cda9072905c15b9a5ebc9b22913fec444a89d` (production bytes
same as09c56d8) and Runtime `273bdbccf65647a803ab0586a418f8283732baec`:

```
python scripts/test_native_safe_content.py --system SYSTEM \
  --system-revision 6e4cda9072905c15b9a5ebc9b22913fec444a89d --runtime RUNTIME
NATIVE_APP_CC=XTENSA_CC python scripts/build_portable_apps.py \
  --system-apps SYSTEM --system-apps-revision 6e4cda9072905c15b9a5ebc9b22913fec444a89d \
  --safe-viewport --native-safe-content --include-battery --navigation \
  --native-stopwatch-runtime RUNTIME --output OUTPUT
```

Existing foreground-idle builder arguments remain additive and unchanged. The
new profile records complete compiler inputs/defines and geometry header hashes,
including when foreground idle is not selected. Final product integration must
use that exact recipe rather than a separately hand-coded host renderer.

## Verified scope / remaining gates

New fixtures run actual three application sources and exact System Nova native
renderer against modeled surfaces and custody. Normal and ASan/UBSan cover six
physical geometries,257 additional generated layouts per app, native glyph-size
invariance, safe origin/inverse mapping, stride canaries, retained no-write,
Calculator touch arithmetic and error rendering, Battery scrolling/unknown
telemetry and Stopwatch state presentation. Rasters are inspectable PPM files;
colored outer area is a guard sentinel, not app artwork. They are software models.

Existing86 Python tests, legacy Calculator/Stopwatch/Battery normal and sanitized
fixtures, standalone native UTC failure tests and System safe viewport raster/
geometry tests were also run. LeakSanitizer is disabled because this container is
ptraced; address and undefined-behavior sanitizers remain enabled.

Isolated three-app Xtensa compilation and ELF structural/import checks are not a
complete firmware/runtime execution. Final integrated Home/Springboard launch,
actual button/touch cancellation, repeated foreground sleep/retained paths and
all selected build profiles must be requalified by the integrator. No full target,
sealed morning source, device, install catalog or publication was changed.

Native controllers own all touch chrome: adapter legacy width-scaled Back and swipe navigation are disabled only in this profile. Physical navigation is unchanged. Calculator preserves pending operator, six-decimal precision and error-reset cues; Stopwatch omits only an exact duplicate phase/status string, never approximate, error or unconfirmed-save status.

## Optional native alarm returning Light

Select all of `--foreground-alarm-sleep --alarm-client --tagged-alarm-utilities PATH`
with the existing foreground helper/runtime/Drivers SDK arguments. The exact
selected System recipe owns canonical tagged SDK staging, API2 requirements,
fresh ticket preparation and successful-Light resume; this builder does not
fork that protocol. Omission preserves previous profiles. Native geometry and
Stopwatch UTC remain independently explicit and unchanged. No Points, RTC
control, update or storage-volume privilege is added.
