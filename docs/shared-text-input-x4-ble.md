# BLE shared text-input: X4 paper and Watch source checkpoint

The original source-and-host-test checkpoint is preserved below. The locally reserved X4 0.2.19 successor profile and target qualification are documented in [x4-ble-shared-text-target.md](x4-ble-shared-text-target.md). Neither is a published or hardware-qualified release.

## Source custody

- Shared BLE client base: `e962715ac0f900d1d380dc29333ecf2e259465f3`.
- Installed X4 paper frontend: Utilities `bda1c2ec01d2c18e39f822fbcf6cc8ac3b12aa9e`, `Apps/ble_scanner.c` and `Apps/ble_scanner_paper.inc`.
- Final shared host/profile: System `52cac98887d5823302abb997c0fcb2e70f8c25f0`, following `6bc3692f5569e81ef9f8b519f7254f4f77056097`.
- Native-custody host fixture uses Runtime SDK from `5a5c14de2dbcf189d1ca4912dfde1773a4d9813d` or an identical later SDK snapshot. The generated test receipt records the actual revision.
- Scanner provider fixture: Drivers `aa5ce0140102bf4d1039e5f208588a6793d06b28`.

At the original checkpoint, the source manifest said BLE `0.2.16`. Installed X4 used a separate `0.2.18` build override. That checkpoint assigned no successor. The current isolated integration reserves `0.2.19` after checking live refs and all-state pull requests; it updates source, native profile, catalog identity and receipts coherently. Do not deploy under either historical version.

## Preserved behavior

The entrypoint selects paper presentation when the adapter offers it, including the X4 logical 480×800 viewport over its physical 800×480 MONO1 panel. It retains the Watch 240×240 frontend. Paper list selection, six visible rows, detail pagination, native-toolbar release replay, Bluetooth enable, Back/navigation, and three-second scan repaint cadence come from the installed frontend. The scanner provider and display driver are unchanged.

Both frontends now use only `PortableTextInputClient` and the one `ui.text-input` provider. The local Watch/paper keyboard key tables, hit-testing, and keyboard rendering are absent from BLE. Naming still closes scanner custody first, suspends the app presenter, exchanges copied text, closes the shared session and grant, resumes the presenter, then verifies alias persistence. Failed saves preserve the draft for retry. Uncertain modal cleanup fences launch and retains the invocation.

The generic BLE build script also preserves the installed paper/rotation/navigation/partial-damage/Home/Quick Actions options while requiring exactly one `ui.text-input@1` native manifest entry. It does not replace the full X4 resident product recipe or reproduce its complete installed flags/catalog.

## Physical orientation correction

The installed app's `PORTABLE_DISPLAY_ROTATION=90` maps `(x,y)` to `(y,479−x)` on the native panel. The shared scene profile previously selected scene rotation 90, mapping `(x,y)` to `(799−y,x)`: the editor was upside-down and visible keys disagreed with touch coordinates.

System `52cac988` selects scene rotation 270 and touch rotation 0 in the centralized production `PROFILE_FLAGS`. This matches the installed app's physical mapping and the GT911's already-portrait coordinates. The portrait profile version is `0.1.1`; its old `0.1.0` policy is superseded for X4. The BLE fixture compiles the actual profile source using those production flags and checks physical keyboard pixels independently of the shared transform. It also asserts low-latency intent for every paper app/editor presentation.

## Host qualification

Run from this checkout, passing the exact dependency checkouts:

```sh
python -m unittest discover -s tests -p test_ble_inventory.py
python scripts/test_ble_scanner.py --system-apps "$SYSTEM"
python scripts/test_ble_renderer.py --system-apps "$SYSTEM" --drivers "$DRIVERS"
python scripts/test_ble_cleanup_retry.py --system-apps "$SYSTEM" --drivers "$DRIVERS" --output build/ble-cleanup
python scripts/test_ble_paper_renderer.py --system-apps "$SYSTEM" --drivers "$DRIVERS" --runtime "$RUNTIME"
```

Qualification completed:

- Six inventory/build-contract tests.
- Existing alias/model/app lifecycle and nine Runtime-loss-boundary tests, ordinary and ASan/UBSan, plus Nova pixel snapshots.
- Existing actual Watch app/adapter/shared-host rendering flows, ordinary and ASan/UBSan.
- 112 scanner cleanup/retry/fault regressions.
- 64 actual `app_main` replays: Watch, X4 paper, and X4 with the production native custody fence, each ordinary and ASan/UBSan. Coverage includes idle, scan/stop, nested Back, accepted naming, touch cancellation, navigation cancellation, missing provider, repeated editing, cancellation followed by two Back steps, and X4 explicit Bluetooth enable/scan.
- Real shared-host unsubscribe failure: native retention is invoked; app finalization preserves live grants/subscriptions, no alias write or launch occurs, and re-entry is rejected.
- Physical 800×480 output with independent upright 480×800 pixel checks and stride guards; real logical touch drives the shared editor.
- Negative control: the unmodified shared BLE `e962715` entrypoint fails the X4 `app_frames > 0` assertion.
- Negative control: the prior portrait scene rotation 90 fails the physical Q-key border assertion.

The dual-profile fixture links the production BLE app, adapter, scanner provider, scene provider, text provider and compiled profile. Display, touch, radio transport, key-value storage and the Runtime function table are host fakes. Its native test exercises the real adapter invocation fence through a fake retention callback; it is not a full product Runtime boot or a radio/panel hardware test. Separate capacity qualification replays the actual BLE naming controller through production Runtime and resident Home, with 26 pinned providers and peak BLE grant use 36/42.

`build/ble-paper-renderer/summary.json` records exact revisions, source hashes and all completed cases. Captured PPM files remain in profile/scenario subdirectories.

No target compiler, PlatformIO build, publication, installation, hardware operation, or X4 `.50` product modification was performed for this checkpoint.
