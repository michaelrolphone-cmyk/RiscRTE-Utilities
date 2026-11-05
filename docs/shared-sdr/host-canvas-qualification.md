# Shared SDR full-controller canvas qualification

Run after the shipping build, from the same checkout:

`python3 scripts/test_shared_sdr_canvas.py --shared out/shared --canvas-system System --output out/shared-native-host`

The runner consumes the shipping build.json. It verifies staged dependency and ELF hashes and the canvas source pin, then preserves every shipping preprocessor definition, include path and forced include. It compiles the materialized adapter and portable Quick sources with the host compiler.

Host-only substitutions are explicit:
- The RF renderer fixture includes the unchanged Waterfall controller and supplies display, input, radio, storage, clock and battery peripherals.
- The required telemetry broadcast service and Quick Wi-Fi/Bluetooth controls use host copied-state models with checked pause, disconnect and enabled/status transitions. No network or radio hardware is driven; telemetry has no invented sensor fields. Optional canvas input, brightness and alarms remain absent in their intended lanes.
- The Runtime retention callback records unexpected terminal retention; every clean scenario asserts zero retention before and after finalization. This is not a terminal-retention behavior test.
- Physical display/input are accessed by the real canvas provider, not directly by the app. The app binds the canvas and its input companion.
- The native sleep implementation is replaced by the fixture's sleep hook. These scenarios do not qualify native sleep.
- The target-only SingleFloatDivisionCompat shim and Xtensa compiler/linker flags are omitted.
- A host-only copy of the shipping adapter adds read-only availability/pending-state observations. Its geometry refresh function is wrapped only to count calls, delegating every call unchanged to the shipping implementation. Failure diagnostics report dimensions and refresh opportunities without forcing refresh or changing its result.

The matrix includes the nine existing native dimensions, seeded odd/asymmetric resolutions and insets across all four raster rotations and all four independent touch rotations in RGB565 and MONO1, a conservative rectangular safe area inside a circular panel, button-only and input-absent providers, and optional brightness absence/presence. Admission boundaries are logical safe-viewport 159/160 and 1024/1025 on both axes. Production admission is unchanged.

Every submitted physical image is compared pixel-for-pixel with the controller's logical image using an independent rotation/inset oracle and monochrome quantization calculation. Scripted logical targets are converted independently to raw touch coordinates. Scenarios check spectrum cursor endpoints, controls navigation, outside-content rejection, height-only resizing with a new geometry-change event, neutral input after resizing, capture/history behavior and cleanup. The resize scenario starts neutral; it does not establish cancellation of a contact held across resizing. Provider-owned padding may be initialized; outer guards and unused rows must remain unchanged.

Each process has fresh controller/provider state. Normal and address/undefined-sanitized builds run the same cases. LeakSanitizer is disabled; cleanup evidence comes from the explicit grant, frame, session, writable allocation, receipt and input-ownership assertions, not a general leak scan. Compiler, linker and scenario failures/timeouts preserve stage, command, logs and partial evidence.json before raising; only successful process exits constitute a passed case. Existing contract and native-geometry tests remain separate and unchanged.

This is host software integration evidence. It does not prove physical display visibility, e-ink waveforms/ghosting/refresh cadence, raw hardware input calibration, radio hardware or native sleep. The circular case validates a declared safe rectangle, not a shape-aware chord renderer.

## Review status

At Utilities head bb60dc6fba5b8510c7203236dafb1a66eca66d9d, [Shared SDR convergence run 41994346318](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/41994346318) passed all 55 controller scenarios normally and all 55 with AddressSanitizer/UndefinedBehaviorSanitizer, plus both generated-custody negative-control suites. [Preserved evidence artifact](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/41994346318/artifacts/13399766734) contains commands, per-case results and logs. The same run passed the three product roundtrips and normal/sanitized selected-Runtime admission. Canonical ELF SHA-256 is 67d251b91ba58b123005e1d6e97c2006eaf1b96dd866a2c6eafdc6b277473655, 296528 bytes. These are exact-head host results, subject to the limitations above and below; they are not hardware qualification.

## Optional acquisition integration

The Utilities lifecycle materializer stages a hash-checked custody wrapper with one central contract for display.output v1/0, ui.canvas-input v1/0 and alarm.service.facade v2/0. Only an initially empty grant that remains empty, with the same live Runtime before and after health, can report clean unavailability. Dirty refusals, changed identity, failed health and retained custody remain terminal. The original release path is unchanged.

The focused host test includes that actual generated wrapper and tests exact request identities, dirty/pre-dirty grants, health/identity changes and release failure/uncleared grants. The full controller separately compiles the same wrapper with shipping definitions.

Remaining qualification gap: the manifest labels additional services optional even though current startup paths may require them. This matrix supplies telemetry broadcast, Wi-Fi and Bluetooth controls. It does not establish that every manifest-optional capability can be absent.

## Replay and resize regression

A real top-controls tap must reach the RF controls page and leave no consumed replay pending. Normal controller iterations must then adopt the new geometry with unchanged pixel/input assertions. The first resize is natural and unprobed. At a separate subsequent resize boundary, an explicit guard probe temporarily sets only the adapter replay-pending flag, calls the real refresh implementation, verifies old logical dimensions remain, and restores the original flag. It requires every other adapter deferral flag to be clear. Its preliminary real display get_info call refreshes provider mapping/revision and geometry/gap state while leaving the adapter's cached dimensions unchanged; the probe is not purely observational. This injected guard test is distinct from physical-input replay coverage; it does not claim a simultaneous touch/navigation race has been exercised.
The replay regression does not newly qualify every existing cancellation, idle-loop discard, modal or handoff path, and does not establish global replay persistence across those paths.

Resize commands wait for actual frame/lease/receipt settlement before changing physical dimensions, rather than assuming a fixed number of provider polls implies settlement. The command remains queued without advancing the scenario; failure to reach a settled boundary within 256 provider cuts is an explicit test failure. No production operation is invoked to force settlement, and this does not qualify resizing an outstanding writable frame.
The barrier includes the adapter's frame/token/facade-lease/receipt deferral bits, matching production refresh. It does not claim every local software raster snapshot is empty.
