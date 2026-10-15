# Native RF canvas and status qualification

Add only `-DPORTABLE_RF_NATIVE_CANVAS` to each existing **Waterfall** recipe.
Retain every product-specific flag, grant and manifest version. Audio Spectrum
is the separate microphone app and is unchanged. Runtime and frozen Springboard
sources/artifacts are unchanged. This work does not include brightness changes.

## Rendering and input

All RF color pages paint rectangles and existing baked glyphs directly on the
native surface. Spectrum/frequency samples use native plot dimensions; other
pages lay out semantic control rectangles with matching inverse hit tests.
Glyph bitmaps are not enlarged. Captions center within physical button bounds.
The admitted color capability range is 160..1024 pixels per side; Controls retain
at least 240 logical vertical units and bounded scrolling on wide displays.

Spectrum and Waterfall reserve a 36px room/event status strip outside the complete
plot/axis rectangle, above the fixed bottom actions. Native plot width is
`capability_width - 12`; height is `capability_height - 110`. Unknown, shared-known
and legacy-known status all use that strip. History remains 228 bursts with at
most 438 frequency rows. No full-frame buffer was added. Paper retains its prior
geometry and separate 228/438 spectrum/waterfall capacities.

## Results

Machine-readable counts, source hashes, paired assertions and recipe outcomes
are in `rf-native-qualification.json`.

- Native rendering/input/status: 14 normal/ASan/UBSan runs × 41 checks = 574 checks,
  at 160x160,200x200,240x240,410x502,320x240,640x240 and240x320.
- All 56 plot/axis hash equalities pass while 56 status hashes change, covering
  unknown/shared-known/legacy-known status on both Spectrum and Waterfall.
- Physical plot edges and cursor pill remain usable with an asynchronous frame
  pending. Freeze/history, last control row, editor Back, receiver and footer
  reachability pass. The separate shared-input inventory covers all 19 RF views.
- Native primitive replay: 8 runs × 79 comparisons = 632 full-buffer/stride/custody
  comparisons. Text spans at most 15 replay rows and never visits more pixels
  than one immediate glyph pass. Host CPU measurements are not target timings.
- Pure DSP/room/event/neural models: 6 normal/sanitized runs pass.
- Legacy color and rotated paper: 8 runs ; 20 predecessor screenshot equalities
  across Spectrum, Waterfall, Controls, scrolled Controls and Monitor. Both
  source versions compile-time assert the original 228/438 capacities.
- Exact product-profile shared input: 240/242 runs pass for each product.
  `loss-open` fails normal/sanitized with the unchanged pre-existing assertion
  `!native_retained&&!runtime_lost`. Exact predecessor reproduces that assertion.
- Generic controller: 102/108 scene runs pass, plus 2/2 real Runtime storage runs.
  Neural completion, sleep-stop and legacy keyboard-cancel fail normal/sanitized. The exact predecessor reproduces the same assertions
  in the paired normal runs. Their causes remain unresolved; these are
  pre-existing scene/assertion failures, not a clean full-suite result.

Two original long-training failures were demonstrated to be ordered-edge pacing
assumptions: retaining the assertions and allowing two polls per capture passes
room/whole-event checks. The Wave discovery fixture also needed semantic native
header/footer coordinates and capability-derived Controls row positions. After
that fixture correction, all four affected scenarios pass normal/sanitized;
there was no candidate-only application failure in the final profile sweep.

## Repeatable commands

Run from the pinned Utilities checkout. Set SYSTEM and RUNTIME to the pinned
System and Runtime roots; BASELINE_UTILITIES is the exact predecessor. The target
replay inputs carry the pinned Watch/bridge source roots. These host peripherals
simulate raw input and do not qualify physical Watch touch/RF hardware.

```
python scripts/test_rf_native_canvas.py --system-apps "$SYSTEM" --output "$OUT/native"
python "$SYSTEM/scripts/test_rf_native_raster.py" --output "$OUT/raster"
python scripts/test_rf_models.py
python scripts/test_rf_legacy_canvas.py --system-apps "$SYSTEM" --baseline-utilities "$BASELINE_UTILITIES" --output "$OUT/legacy"
python scripts/test_rf_application.py --system-apps "$SYSTEM" --runtime "$RUNTIME" --native-canvas --target watch --keep-going
python scripts/test_rf_shared_input.py --system-apps "$SYSTEM" --runtime "$RUNTIME" --watch-recipe "$TWATCH_HOST_RECIPE" --target watch --extra-define PORTABLE_RF_NATIVE_CANVAS --skip-baseline --keep-going --output "$OUT/twatch"
python scripts/test_rf_shared_input.py --system-apps "$SYSTEM" --runtime "$RUNTIME" --watch-recipe "$WAVE_HOST_RECIPE" --target watch --extra-define PORTABLE_RF_NATIVE_CANVAS --skip-baseline --keep-going --native-height --output "$OUT/wave"
```

The two broad suites intentionally exit nonzero for their explicit remaining
pre-existing failures; `--keep-going` records them without relaxing assertions.
The focused native, raster, model and legacy gates exit successfully.
`--skip-baseline` skips only intentionally changed native predecessor images;
normal/sanitized pixel equality remains mandatory. SDK aliases are symlinks to
canonical headers, never duplicated copies.

HOST_RECIPE is a materialized exact Waterfall recipe record (a `waterfall` key is
accepted), with `command` and optional `defines` arrays. Keep all `-D` flags and
`-include` pairs. The replay producer remains authoritative for target builds.

## Target builds and checkpoints

- System native primitives: local `4d767977`, public equivalent `e49bded7`.
- Utilities native views: local `f7ca883f`, public equivalent `80865ac3`.
- Utilities separate status strip: local `ec855f5d`, public equivalent `d60badc0`
  (exact tree `fecd9a7ae7c8d7490a96670c249b9015b6f62a01`).
- Wave: 252240 bytes (+592), BSS 1202504, SHA256
  `a3f9e4f2ba63b5b035da6696a33c9b5bd3e3ecafd3dc86fc97ec096a0e21189a`.
- T-Watch: 243016 bytes (+4104), BSS 1201040, SHA256
  `62c25516e32c7c5e3ff878dd90b52df6a2987fd97cdbfe27ce55390f8c7df86e`.

These are host/target-build qualifications, not hardware, official-store
admission or Runtime ELF-loader execution claims. Product integration owns those
gates and the final replay/publication receipts.
