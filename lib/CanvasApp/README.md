# Capability canvas apps

Model Viewer 1.3.0 and Hollow Trail 1.2.1 migrate the actual T5S3-Reader
1.2.8/1.1.49 sources. `migration-source.json` records original hashes and the
Reader commit. All ten chapters, the journal and ending remain in Hollow Trail.
The small `CanvasApp` client provides display, input, storage and lifetime
adaptation to the current runtime; no Reader firmware or hardware takeover is
linked. Its internal T5-shaped tables only reuse the existing renderer/engine.
Target modules import the Runtime API and checked libc allocation functions.

Model Viewer supports OBJ and ASCII/binary STL, shaded/wire rendering, rotate,
pan, zoom, two-finger gestures and double-tap reset. It sizes itself to the
Watch's 240-square RGB565 display or X4's portrait viewing area. A built-in cube
works on first launch. Tap the filename to choose Cube, Saved OBJ or Saved STL;
tap Rotate/Pan/Zoom and drag the model. Back/Home returns to the launcher.
X4 File Browser launches `.obj` and `.stl` from SD through `file.open`, with
bounded streaming reads and optional seek. Maximum input size is 32 MiB and the
original 80,000-triangle bound remains. SD access never bypasses its provider.

On Watch there is no SD provider. Saved files are `model.obj` and `model.stl`
in the viewer's private AppData namespace (product namespace 64), each at most
64 KiB, with the existing four-file/128-KiB namespace quota. A provisioning or
explicit transfer tool must place custom models there; this app does not add a
wireless file uploader. Missing/bad files keep the current model and show an
error. File associations are used only in products that grant a volume.

Hollow Trail is selected only for X4. Its original 960x540 scene is fitted to
landscape output with black letterbox bands, and touch Left/Right/Up/Down,
Jump, Use, Pause and Notes.
Use also confirms menus; Notes opens the original journal. The X4 Home button
exits. Original story pacing and physics are preserved, including the opening
sequence. Optional Reader typography/SIMD/USB-controller providers are not
claimed in these products; the original scalar and compact journal paths run.

Rendering writes only app-owned working/snapshot buffers. Input and simulation
continue while display tokens are queued; only the latest logical frame is
copied into an acquired provider surface. Queued pixels remain immutable.
Token completion, touch sequence boundaries, provider cleanup and retained
failures are checked before releasing memory or grants. E-paper refresh speed
is determined by the selected panel provider and is not hardware-qualified here.

Build using `scripts/build_canvas_apps.py --runtime R --system-apps S --target
x4|watch --output O`. `test_canvas_apps.py` invokes the actual exported entry
and imported engine with adversarial slow display, short taps, rotation,
model loading, game movement/journal and uncertain cleanup under ASan/UBSan.
Build receipts include the compiler's complete header/fragment closure. Product
catalogues, exact grants and whole-native-cohort admission are separate required
integration gates; a standalone app build is not an installable firmware image.
