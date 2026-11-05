# Shared SDR full-controller canvas qualification

Run after the shipping build, from the same checkout:

`python3 scripts/test_shared_sdr_canvas.py --shared out/shared --canvas-system System --output out/shared-native-host`

The runner consumes the shipping build.json. It verifies staged dependency and ELF hashes and the canvas source pin, then preserves every shipping preprocessor definition, include path and forced include. It compiles the materialized adapter and portable Quick sources with the host compiler.

Host-only substitutions are explicit:
- The RF renderer fixture includes the unchanged Waterfall controller and supplies display, input, radio, storage, clock and battery peripherals.
- Physical display/input are accessed by the real canvas provider, not directly by the app. The app binds the canvas and its input companion.
- The native sleep implementation is replaced by the fixture's sleep hook. These scenarios do not qualify native sleep.
- The target-only SingleFloatDivisionCompat shim and Xtensa compiler/linker flags are omitted.
- One read-only observation function is appended to a copy of the shipping adapter to inspect input/brightness availability. No production adapter operation is replaced.

The matrix includes the nine existing native dimensions, seeded odd/asymmetric resolutions and insets across all four raster rotations and all four independent touch rotations in RGB565 and MONO1, a conservative rectangular safe area inside a circular panel, button-only and input-absent providers, and optional brightness absence/presence. Admission boundaries are logical safe-viewport 159/160 and 1024/1025 on both axes. Production admission is unchanged.

Every submitted physical image is compared pixel-for-pixel with the controller's logical image using an independent rotation/inset oracle and monochrome quantization calculation. Scripted logical targets are converted independently to raw touch coordinates. Scenarios check spectrum cursor endpoints, controls navigation, outside-content rejection, height-only resizing with a new geometry-change event, neutral input after resizing, capture/history behavior and cleanup. The resize scenario starts neutral; it does not establish cancellation of a contact held across resizing. Provider-owned padding may be initialized; outer guards and unused rows must remain unchanged.

Each process has fresh controller/provider state. Normal and address/undefined-sanitized builds run the same cases. Compiler, linker and scenario failures/timeouts preserve stage, command, logs and partial evidence.json before raising; only successful process exits constitute a passed case. Existing contract and native-geometry tests remain separate and unchanged.

This is host software integration evidence. It does not prove physical display visibility, e-ink waveforms/ghosting/refresh cadence, raw hardware input calibration, radio hardware or native sleep. The circular case validates a declared safe rectangle, not a shape-aware chord renderer.

## Review status

Initial implementation has been source-reviewed but not compiled or executed: the authorized Biome terminal was stalled and no staged shipping build was available there. Run the command in the owning CI after build_shared_sdr.py before treating any proposed coverage as verified.
