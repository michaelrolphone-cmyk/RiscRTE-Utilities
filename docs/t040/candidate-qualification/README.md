# T040 neural shared-capture guard: qualification only

This additive lane preserves PR89 at `390b8ee8580db7d7b3e37b99c38cb10378d6d413`, its source-equivalent checkpoint `30867c87a7d3afb13252fd446784d484270b4ecf`, and original recovery. It changes no application, model, codec, manifest, frozen builder, release workflow, dependency pin, accepted binary or hash. No firmware release, merge, device or radio action is performed. Outputs are NOT FOR INSTALLATION OR DISTRIBUTION. No candidate package identity is assigned.

## Diagnosis

PR89's native run [39621278619](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39621278619/job/119295046468) passed actual adapter hosts and geometry but failed `Geometry, model purity and default source parity` with `AssertionError: ('selector-off token mismatch', [])` (exit 1). Its comparison is frozen at `12c7440c4c2fd9b6b78e10f11a983947c145b346`.

The [shared-input run 39621278384](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39621278384/job/119295043801) passed both lifecycle modes but failed `Shared-client lifecycle and opt-out isolation` with `AssertionError: audio_spectrum.c opt-out preprocessor token mismatch: []` (exit 1). Its comparison is frozen at `65bffc27f96088db3c2f76ee95ae812b223459f7`.

The guard intentionally changes default behavior. Those frozen controls remain intact and failed for this candidate. This lane does not rebase, suppress, convert or relabel them as passing.

## Evidence contract

- Rebuild unmodified parent `6b2047500905f8f45087cb49989103a926fc5c63` and the exact candidate through the unchanged target recipe. Parent must reproduce frozen selector-off ELF bytes. Candidate must fail exactly at the final selector-off byte equality gate (exit 1), after all three modules have passed imports, exports and structural loader admission. Only the two captured guard production inputs may differ.
- Omit the native release manifest argument. The ELF files are compiler/structure evidence, not installable packages or measured device fit.
- Reproduce the original shared-capture bug with the identical six-case regression fixture against pre-guard production, normal and ASan/UBSan. Both must exit SIGABRT (-6 in Python), with 1,839 training ticks, one checkpoint and one microphone close during the first quiet capture. Unrelated crashes do not count.
- Run that fixture against captured guarded production in normal and sanitizer modes. All room/event, pause, Cancel, failed Save/retry and confirmed Discard cases must pass. Run the unchanged full Spectrum and storage suites as controls.
- Run unchanged parent native and shared-input parity controls successfully, then capture the exact candidate token failures separately. Run candidate shared-input lifecycle independently without claiming frozen parity.
- Preserve T040's 61 production host scenarios across five geometries and two modes: 610 executions. Spectrum PR87's separate 65-case repair is not applicable.
- Bind source, dependency snapshots, target source closure, commands, logs, normal/sanitizer pixels, and every transported artifact file with SHA-256. Pin the overall receipt digest in the upload job output and reject wrong-head, incomplete, changed, added or missing evidence after artifact download. SHA-256 custody is not a cryptographic signature or third-party attestation.

## Remaining acceptance

Synthetic fixtures are not measured acoustic accuracy. The pinned full-feature Wave recipe is structural target qualification, not proof of the user's installed Watch composition. Real Watch capture, unknown/fallback trials, interruption/relaunch persistence, measured training time/heap/energy, product integration and device acceptance remain open. X4 RF-only profiles do not gain acoustic support.

The new draft is stacked on PR89 and does not change its branch. CI artifacts have 90-day retention; original recovery remains in PR89. A passing new lane means only that this documented non-distributable evidence contract passed, not that the original frozen release gates passed.
