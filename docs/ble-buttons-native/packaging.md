# Native BLE Buttons standalone software qualification

The separate `Apps/native/ble_buttons.json` identity is `ble_buttons` version
`0.1.30-dev1`, tagged `native-ble-buttons-0.1.30-dev1`. This is above the delivered
BLE Buttons `0.1.29` predecessor. Legacy source manifest, all existing release and
catalog bindings, paper mode and frozen `.64` assets are unchanged.

## Inputs and evidence boundaries

The selected System pin is `f3731ab02127dea86a308472e3780068c5a0a1cf` (private BLE Buttons presentation owner); Runtime is
`0aab2fd3b03b4190d638e23c79a465afeecf5dfb`; generic Watch bridge is
`6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`. All target sources and fixtures are
staged from immutable Git objects. GCC is ESP32-S3 8.4.0+2021r2-patch5.

The exact feature recipe anchor is the selected System's
`docs/shared-app-crashes/replay/wave.json`, entry `ble_buttons`, with generated
fixture bytes validated against its split `inputs*.json` templates. This records
the 154,028-byte deployed predecessor ELF SHA-256
`b3f4633d8bc0d0612d6410eae3f41d8c66a0586025f076a26dd0f277cac7232d`.
The original replay used different source revisions and a normalization/compaction
step. This workflow preserves the full feature/compiler/linker switches, but
qualifies new ELF bytes with explicitly selected current sources. It does not
claim to reproduce the predecessor.

The earlier Wave `07cb64541edc54dc88de65e98a619b59b16a82bc`
`integration/ordered-input/ble_buttons-command.json` (SHA-256
`72aacc5319c3c87689dba8122145b59aa55b29d38ee5208951c8cd463e2ad711`) is historical
context only: its recorded 133,948-byte output is different from deployed bytes.
All sixteen deployed capability/API pairs remain mandatory, including atomic
key-value storage. Capabilities do not by themselves establish installation,
linked-loader, provider or device compatibility.

## Qualification and packaging

`build_native_ble_buttons_qualification.py` builds baseline, current selector-off,
and optional native targets. Native mode adds
`PORTABLE_BLE_BUTTONS_NATIVE_UI`, `PORTABLE_NATIVE_CANVAS`, `PORTABLE_ALARM_NATIVE`
and `PORTABLE_QUICK_NATIVE`, retaining the existing native overlay integration. Selector-off target
bytes must exactly equal the retained Utilities baseline built with original
System `947895afdb2ecef59654402cf08fe7ecdf92362f`. The recipe includes
atomic preferences, automatic home preferences, checked storage, native custody,
Watch color policy and result-only broadcast. It records compiler hashes,
commands, consumed source hashes, public symbols and structural admission.

`test_ble_buttons_native.py` exercises the production adapter with simulated
providers across seven physical profiles. Packaging requires complete normal and
ASan/UBSan runs in synchronous and asynchronous RGB565 modes, bound logs and
captures, unchanged consumed source bytes, and no unresolved temporal
counterexamples. Sixteen dedicated presentation-ownership temporal scenarios
must also pass across all seven profiles normally and sanitized, including
retained failures and terminal custody. Existing model, legacy renderer and paper renderer controls are
run separately against the pinned predecessor HID adapter.

`prepare_native_ble_buttons_release.py` requires a clean committed source,
checks source correspondence for every target and host input, includes licenses,
and runs the production loader admission/memory harness normally and sanitized.
It records ordinary package bounds, code, BSS, aligned data and resident section
budgets. This is static/host admission, not proof of device PSRAM capacity,
fragmentation, target instructions, Bluetooth transport or physical behavior.

`publish_native_ble_buttons.py` validates by default. `--stage-assets DIR` makes
and verifies the ten standalone assets without remote writes. `--publish` is an
explicit separate action: exact source and immutable tag are verified, draft
assets are downloaded and checked before publication, then public assets are
read back again. CI publication is manual-dispatch-only and additionally requires
`publish_development=true`, `main`, and successful qualification. No firmware or
catalog update is part of this tooling.

See `.github/workflows/native-ble-buttons.yml` for the reproducible command order.
Local qualification may inspect an uncommitted snapshot, but a release package
fails closed until every consumed input corresponds to clean committed source.
