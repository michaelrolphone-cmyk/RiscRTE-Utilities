# Hollow Trail 1.2.2 launch memory repair

## Observed failure

The X4 0.1.90 device trace admits `Apps/hollow_trail/hollow_trail.elf`,
loads it successfully, and enters the app. Startup then reports
`Hollow Trail: native B PSRAM allocation failed (518415 bytes)` and returns.
The Runtime unloads the game and reloads Home normally. This is not a missing
SD file, an ELF admission failure, or a game file-selection issue.

The migrated 1.2.1 app is in Utilities, originally imported from the read-only
Reader history. Its startup requests 3,065,119 bytes for the scenery arena,
518,415 bytes for each of two full-resolution planes, and 64,815 bytes each for
journal and optional staging. Canvas additionally owns two 64,800-byte buffers.
The resulting peak is 4,361,179 app-owned bytes. The old host test had no heap
budget and could not detect this launch failure.

A 4 MiB app-owned test budget reproduces the precise native-B failure. That
budget is a deterministic regression fixture, not a measurement of the device's
free PSRAM. The device trace does not establish whether its immediate allocator
failure was caused by fragmentation or insufficient total available memory.

## Repair and lifetime contract

The original tile scheduler skips depth 2 in visible, prefetch, and composition
work because dynamic terrain owns that layer. In low-resolution modes, only the
first near/wide pair of this 552,960-byte bank is used for the composed backdrop.
Native modes bypass that low-resolution backdrop and can use the entire bank
as their 518,400-byte scratch raster B. These uses are mutually exclusive.

Native scratch access now invalidates the composed backdrop cache. Returning to
a low-resolution mode rebuilds it, including after native cutscene dissolves.
Compile-time bounds prevent the aliased native raster from exceeding its bank.
Native raster A, all cache geometry, the journal, optional staging, original
960x540 raster, all chapters, ending, camera modes, cutscenes, and controls are
preserved. Rendering still performs no per-frame allocation.

One 518,415-byte allocation is eliminated. Total app-owned peak is 3,842,764
bytes (518,415 fewer), including optional staging and Canvas buffers. Native
scratch remains within the already-owned scenery allocation. It is never freed
separately. Startup rollback and retained-invocation ownership remain intact.

## Regression checks

Run `scripts/test_hollow_memory.py --runtime R --system-apps S --output O`,
optionally with `--baseline OLD_UTILITIES` for the original 1.2.1 failure.
The suite exercises the actual exported entrypoint under ASan/UBSan with:

- A 4 MiB app-owned budget: intro, movement, journal, and clean exit
- Uncertain display completion and retained invocation
- Optional staging allocation failure followed by direct-packing gameplay
- Required journal allocation failure and complete cleanup
- Every startup allocation failure, alignment, repeated allocation/cleanup
- Pixel-for-pixel independent versus aliased native scratch across all ten
  chapters and three camera positions, with low/native/low round trips
- All nine cutscene tracks plus intro and western-vista dissolves

The ordinary `test_canvas_apps.py` suite still covers the shared adapter and
Model Viewer. Exact target ELF admission and product-selected SD path/hash
validation belong to integration. Hardware heap margin and device gameplay
remain unverified until the resulting package is tested on an X4.
