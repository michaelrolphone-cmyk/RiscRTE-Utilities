# Contexts capability-sized qualification

Contexts already uses `ui.scene` components and the optional shared `ui.text-input`
host. This work qualifies those actual shipping paths; no portable raster adapter,
new application layout, controller change, product recipe, or release is introduced.

The native test disables the historical `SCENE_HOST_WATCH_240` clamp and supplies
real display capabilities for 160×160, 240×240, 320×180, 410×502, and 502×410 RGB565.
It runs the original app main, scene presenter, hit maps, and text host:

    python scripts/test_contexts_native.py --runtime RUNTIME --system-apps SYSTEM --output build/contexts-native
    python scripts/test_contexts_native.py --runtime RUNTIME --system-apps SYSTEM --output build/contexts-native-sanitized --sanitize

Each run covers 985 scenarios: legacy and shared paths, all five naming purposes,
keyboard input, cancel/Home, limits, delayed/frozen refresh, acquisition/poll/close
faults, retained ownership, checkpoint races, retry/reopen, and explicit saves.
Generated raster/storage SHA-256 receipts also assert legacy/shared parity outside
the intentionally different keyboard. ASan/UBSan are enabled by `--sanitize`;
LeakSanitizer is disabled for ptrace-hosted execution. Existing explicit ownership
and cleanup assertions remain active.

## Reproduced renderer correction

On the 160×160 Intro screen, keyboard focus previously placed Start at y=118–152,
under the fixed Back overlay at y=120–146. Outer pixels remained touchable, but the
center of Start selected Back. The shared presenter's focus scrolling now reserves
that fixed control's extent, as it already reserves a tab footer. Layout, touch
scrolling, and action semantics are unchanged. A separate System regression touches
the centered focused Start and Back at every tested size; it fails on the unchanged
predecessor and passes with the correction.

Existing 240×240 and rotated monochrome paper app/text regressions are also run.
Host evidence and target ELF validation do not qualify a device or change the
published Watch .64 firmware. Future firmware adoption must select the qualified
System source and receive its own integration and physical qualification.

The predecessor comparison retained 37 of 38 captured non-keyboard raster/storage
hashes unchanged on each existing display profile. Only the save-failed raster
changes with the intended focus-scroll offset; the captured save-status toast still
overlays its bottom. The dedicated centered-action test establishes Back clearance. Shared/legacy parity
remains exact for all 38 captured outputs per profile; controller source is unchanged.
