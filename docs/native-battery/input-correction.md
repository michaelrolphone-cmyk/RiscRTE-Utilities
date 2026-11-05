# Native Battery 1.2.0 development delivery

The first native component (Utilities48b983) preserved capability-sized geometry
but failed two final composed controls: pre-visibility Details input and Light
sleep during alarm cancellation with a newly captured held contact still queued.
The Settings correction did not select Battery, so it did not repair either case.
Those failed observations remain historical counterexamples, not waived checks.

Systemfca52ae3a3e452d309408268e6e485cb43ac2c04 adds Battery-private invocation
and layout generations. The app declares its layout before attempting to acquire
a frame. The adapter captures the original generation on submitted async tokens
and raster snapshots. Only completion of that owner's current generation sets
visibility; a raw snapshot watermark rejects queued pre-visibility contact starts.
An already owned contact survives same-layout telemetry repaint. Page changes,
new invocations and unrelated overlays cannot borrow an old token's eligibility.
Physical contact activity still holds idle independently of action eligibility.

## Reproducible qualification

The workflow uses exactly the full eight-translation-unit, fifteen-capability
recipe. No manual-only sleep workaround or removed feature makes it pass.
`test_battery_native_temporal.py` verifies compiled dependency hashes against the
selected Git objects and target receipt, and runs:

- 13 original full-feature scenarios with corrected expectations
- 11 temporal/custody scenarios at240x240,410x502,128x128 and320x180
- normal and ASan/UBSan, with and without raster snapshot staging (57 each)
- original public System9d2c55b3 + Utilities48b983 negative controls (13 each)
- exact native-selector-off negative controls (13 each)

The original/selector-off controls deliberately assert the old failure. Positive
controls require fresh post-visible actions. Snapshot failure is terminal and
preserves retained grants, subscriptions and display custody with no subsequent
provider call; regressed sequence or malformed dimensions fail closed. A display
status failure cannot release a provider-owned frame. The same-layout telemetry
case preserves an owned release while discarding later queued pre-boundary taps.

Four reduced controller matrices retain all297 cases each: synchronous and
delayed-async, normal and ASan/UBSan. Geometry checks remain separate. Target
baseline and selector-off ELFs must be byte-identical. The native ELF must pass
structural and exact import/export checks, followed by the pinned production
Runtime's ordinary application admission and allocation-placement helpers in
normal and sanitized host tests. No Xtensa instruction is executed by these tests.

## Publication boundary

`Apps/native/battery.json` is the standalone1.2.0 manifest. The legacy
`Apps/battery.json` and its catalog row are unchanged. Version identity belongs to
the separately named native manifest and exact ELF digest; no old firmware app
version is attached to changed bytes. Native selection remains explicit.

Only a successful exact-main qualification may publish immutable prerelease
`native-battery-1.2.0-dev1`. Publication validates source identity, complete
capabilities, all required controls, unchanged source closure, exact ELF/manifest,
production admission and license notices before draft-first asset upload. The
release includes direct ELF/manifest, source-bound target/admission records,
SHA256SUMS and a source/evidence archive. Existing tags are verified, never moved.

All15 providers, deployment-owned bindings and preferences namespace1 remain
required. Optional `app.catalog` is not granted by this manifest. Color BLE entry
remains deferred. Physical-device fit, installation path, power, charging, RF and
hardware behavior require separate qualification. Neither the .59 firmware nor
its payload is replaced by this standalone development release.
