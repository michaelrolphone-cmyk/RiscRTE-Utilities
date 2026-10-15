# Native RF canvas checkpoint

Select `-DPORTABLE_RF_NATIVE_CANVAS` in **Waterfall only**, retaining every existing
product recipe flag. Requires the matching System adapter primitive API and
`PORTABLE_NOVA_UI`. Audio Spectrum is a separate microphone app and is unchanged.
The color capability contract is 160..1024 pixels per side. Paper retains its
existing admission, coordinates and 228/438-element spectrum/fall capacities.

All RF color pages draw rectangles and baked font glyphs directly on the native
surface. Spectrum/frequency samples use native plot dimensions. Non-plot pages
map existing semantic control rectangles and their inverse hit tests; glyphs are
not scaled. Controls retain at least 240 logical vertical units with a bounded
scroll range, including landscape. Pill captions center in the physical button.
History remains 228 bursts with 438 frequency rows maximum; there is no new
full-frame allocation. Asynchronous frame ownership remains in System.

Repeatable gates (set SYSTEM, RUNTIME and WATCH to pinned checkouts):

```
python scripts/test_rf_native_canvas.py --system-apps "$SYSTEM" --output build/native
python "$SYSTEM/scripts/test_rf_native_raster.py" --output build/native-raster
python scripts/test_rf_application.py --system-apps "$SYSTEM" --runtime "$RUNTIME" --watch "$WATCH" --native-canvas --target watch
python scripts/test_rf_models.py
```

The native application matrix covers 160x160, 200x200, 240x240, 410x502, 320x240,
640x240 and 240x320, normal and ASan/UBSan (14 runs). It exercises real native
plot-edge/cursor-pill touches with an asynchronous frame pending, freeze/history,
Controls scrolling through the last row, shared event editor Back, receiver and
footer reachability. The native primitive gate is 8 runs, 79 comparisons each;
full-buffer and stride comparisons prove immediate/replayed equality. Text replay
visits at most 15 glyph rows, and never exceeds one immediate pass's pixel work.

The existing `test_rf_shared_input.py` 19-view inventory must also run using the
product's exact replay-derived `--watch-recipe`, pinned `--system-apps` and `--runtime`
roots, `--target watch --extra-define PORTABLE_RF_NATIVE_CANVAS
--skip-baseline` (add `--native-height` for 410x502). `--skip-baseline` disables only
the intentionally changed predecessor screenshot comparison; normal/sanitized
output equality remains required. SDK aliases are canonical symlinks.

Qualification status at the first native checkpoint:
- 12 native runs at 200/240/410/320x240/640x240/240x320 passed before the additional
  minimum-size, last-row/editor and pending cursor checks were added.
- Both products' 19-view, delayed-frame and retained-release focused shared input
  scenarios passed normal/sanitized (6 runs each); final caption centering needs
  renewed screenshots.
- Native primitive 8-run gate passed, 632 comparisons.
- Model normal/sanitized tests passed (6 runs).
- Full legacy controller timing checks need ordered-edge pacing. The unchanged
  room check fails identically against predecessor and candidate: line70,
  signature_goal64, polls196, captures108. Twofold long training waits pass all
  23 room assertions; no assertions are relaxed. Remaining full-feature scenes
  are still being qualified, and this checkpoint is not hardware qualification.

Room/event labels still overlap plot data in this checkpoint. Their relocation
is intentionally a distinct second source commit and visual regression gate.

## Separate status-layout checkpoint

Native Spectrum and Waterfall now reserve a36px status strip between the complete
plot/axis rectangle and the fixed bottom actions. Unknown, shared-known and
legacy-known room/event readouts use this strip. Paper output keeps its previous
positions. Native plot height is `capability_height - 110`; width is
`capability_width - 12`.

Final native gate:14 normal/sanitized runs,41 checks each (574 checks). Both views
compare every plot/axis byte while switching unknown/shared-known/legacy-known
status; all56 plot-hash equality checks pass while all56 status hashes change.
This also covers physical plot-edge/cursor-pill input while a submitted frame is
pending, all control rows, shared editor Back and receiver/footer reachability.
The rendered240x240 and410x502 contact sheet was inspected separately.

Final target recipe rebuilds (exact prior flags plus native selector):
- Wave:252240 bytes (+592), BSS1202504, SHA256
  `a3f9e4f2ba63b5b035da6696a33c9b5bd3e3ecafd3dc86fc97ec096a0e21189a`
- T-Watch:243016 bytes (+4104), BSS1201040, SHA256
  `62c25516e32c7c5e3ff878dd90b52df6a2987fd97cdbfe27ce55390f8c7df86e`

The full generic controller sweep passes102/108 scene runs plus2 real Runtime
storage runs. Neural completion, sleep-stop and legacy keyboard-cancel each fail
identically on candidate and exact predecessor, normal/sanitized. Do not describe
that legacy sweep as clean. Final exact product-profile shared-input qualification
and its paired inherited failures are recorded in the subsequent report.
