# Native Model Viewer qualification

This development-only change qualifies the existing capability canvas Viewer.
It does not change a product recipe, release catalog, firmware ABI, or the
separate T056 scene/intent architecture work.

## Bounded repairs

- The initial/reset projection now uses the smaller of the historical
  width-derived scale and a height-derived viewport limit. The normalized mesh,
  camera rotations, pan and zoom remain the same; both exact mesh and cached
  preview paths use the same scale. Outside the preview sampler’s supported
  scale range (64–4096), interactive pan/zoom uses the exact mesh. At 160×160 the original default cube projected
  vertically from 5.391 to 152.609 into a viewport from 52 to 106, with 24 vertex
  occurrences outside. The repaired projection spans 57.507 to 100.493.
- The narrow header uses “3D VIEW” where “3D VIEWER” would overlap the shading
  control. Status and message text use bounded complete bitmap cells and an
  ellipsis when necessary. Existing strings that fit retain their original
  raster and scale.
- Toolbar taps require an in-bounds target, the same contact identity, at most
  14 pixels of movement and release within 260 milliseconds. A drag, replacement
  contact or second finger cancels the action until release. The shading
  control retains its existing capture policy.
- CanvasApp guarded statements are split across lines to compile with strict
  warnings. These edits have identical preprocessed tokens for Viewer and Game.
  The unused Viewer scalar helper was removed and the reference preview sampler
  is explicitly inline.

The Viewer still creates its logical image at the capability-reported size,
after its established portrait rotation. Its output mapping remains 1:1; a
320×180 surface therefore carries a native 180×320 portrait logical image.
Hollow Trail retains its independent fixed scene and letterbox behavior.
The historical 480-wide paper camera scale is preserved, including its slight
horizontal clipping of the default cube; the five required native sizes are
checked for complete default/reset viewport fit.
The viewport fit is a camera projection, not scaling a completed UI image.

## Preview fallback cost

Scales in 64–4096 retain the existing cached mip-sampler path and its bounded
per-pixel work. Outside that range, a pan/zoom frame rerasterizes the mesh,
which can cost more CPU than reusing the preview. No latency or hardware
performance improvement is claimed. Existing limits still apply: 80,000 input
triangles; at most 4,500 sampled triangles for interactive wireframes; filled
geometry retains all triangles to avoid holes and uses its existing two-pixel
interactive raster step. Work is clipped to the viewport. The renderer checks
its input/service hook between batches of at most eight triangles and within
filled raster work, with the existing 16 ms service cadence. Normal-range
preview construction and reuse are unchanged.

## Qualification boundaries

The native suite exercises the actual Viewer, CanvasApp, bitmap rendering and
raw-touch mapping at 240×240, 410×502, 160×160, 320×180 and 502×410, with larger
paper controls. It includes padded output stride, exact logical-to-physical
raster correspondence, model/file handoff, camera controls and asynchronous
presentation custody. Run both ordinary and address/undefined-sanitized builds.
The local environment cannot run LeakSanitizer under its tracing setup; local
results use exact fixture allocation accounting, requiring zero live blocks
on clean exit and retained ownership on uncertain cleanup. The dedicated CI
workflow also enables LeakSanitizer.

The existing Canvas suite additionally exercises real application entrypoints
for Watch, portrait paper, rotated paper, modern/legacy SD seek and Hollow
Trail, including retained cleanup. The eight paper/Game output images from the
original and repaired source were byte-identical; the two Watch images change
intentionally with the viewport-fit repair.

The separate strict target qualification pins Utilities, System, Runtime and
compiler identities, resolves the complete source/header closure, validates
ELF/import/export/loader structure and checks repeat-build determinism. It is
not a replacement for the existing Canvas/product builder or a published
artifact. The final qualification receipt must name the frozen Utilities
commit; exploratory builds are not final evidence.

Software-rendered evidence and host/target checks do not qualify physical
panels, touch hardware, display latency, installation or the .64 product.
Independent review and authorized publication remain separate.

## Reproduce

From Utilities, with paths to the exact dependencies:

```sh
python3 scripts/test_native_viewer_qualification.py
python3 scripts/test_viewer_native.py --runtime R --system-apps S --output O/native
python3 scripts/test_canvas_apps.py --runtime R --system-apps S --output O/canvas
python3 scripts/build_native_viewer_qualification.py \
  --utilities-commit FULL_FROZEN_COMMIT --system S --runtime R \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc --output-dir O/target
```

Keep the strict target output outside all three source repositories and use an
empty output directory. Dependencies are fixed to System
`d66687f7e008ec324e0b5f8f020572c6f3f70968` and Runtime
`13eca114cb58469cf6004fafc3da84fbd9d29beb`. The target profile pins the actual
GCC 8.4.0 tool executables and compiler-header closure. The native runner’s
source hashes and stable-source check bind the observed host results to their
input files.
