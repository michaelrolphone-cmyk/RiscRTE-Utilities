# BLE Scanner native source qualification

Native opt-in source implementation across Utilities and System. Existing .64 source/recipe/artifacts stay frozen.

- Production geometry derives from display capabilities and powers rendering and hit tests directly.
- App + System adapter + actual BLE sensor driver exercise 128×128, 160×128, 240×240, 410×502, 320×180 and 502×410; normal and ASan/UBSan; 3 ms and 120 ms asynchronous presentation.
- Verify scan/stop, details/Back, sensor filter, footer gap, explicit policy/Airplane refusal, initial previsibility and stale rows; immutable pending pixels, stride guards and complete provider cleanup.
- Exhaust 7,102 geometry pairs and boundary hits, plus maximum/invalid dimensions.
- Actual shared scene/text/profile services verify naming save/cancel/Back/repeated invocation, unavailable and retained close, and unchanged paper/enable behavior with native selector compiled in.
- Adapter ownership reducer covers frame visibility watermark, down/move/release, same-layout held release, changed layout cancellation, reset with pending frame, stale completion, snapshot faults and generation overflow.
- Feature-off Scanner model/renderer/paper and existing Buttons adapter regression suites remain required.
- Native target builder is a separate opt-in source-pin path. Existing legacy source pins remain unchanged.
- Source PRs and both PR/main CI must pass. Physical hardware performance remains unverified.

## Verified source results

- 240 actual application/adapter/sensor-driver cases passed across six display sizes, fast and 120 ms asynchronous presentation, normal and ASan/UBSan.
- 7,102 geometry pairs passed per mode, including rectangle bounds and edge hit tests.
- 42 actual shared scene/text/profile-provider naming and paper cases passed across both modes.
- Existing feature-off Scanner model, production renderer and paper suites passed.
- Independent full-suite rerun passed.
- Exact clean System source: `d79406e71ef7f59e54e1079259bca3aa040b5e2f` (System PR 135).
- Native color and paper-selector-bypass ESP32-S3 ELFs passed structural loader validation, import allowlist and exact three-entry export checks. Local pre-publication receipts explicitly record a dirty Utilities working tree. CI repeats from the published source commit.

Run host qualification:

```
python scripts/test_ble_scanner_native.py --system-apps ../RiscRTE-System-Apps --drivers ../RiscRTE-Drivers
```

Run target qualification with the clean pinned System checkout:

```
python scripts/build_ble_apps.py --system-apps ../RiscRTE-System-Apps --native-ui --navigation --output-dir dist/ble-scanner-native
python scripts/build_ble_apps.py --system-apps ../RiscRTE-System-Apps --native-ui --paper-profile --display-rotation 90 --navigation --partial-damage --output-dir dist/ble-scanner-native-paper
```

`--native-ui` uses `sdk/ble-native-sources.json`; the legacy source pin file remains unchanged. This source qualification does not publish or install an application, assign a new release version, or establish physical-device qualification.

## Input findings fixed before publication

First/new layouts wait for the matching completed presentation plus neutral-input fences. A previously visible identical hit map remains actionable while its matching scanner-owned repaint is pending. This avoids starvation when display latency exceeds the active scan repaint cadence. Same-map completion preserves queued valid starts; changing row identity, filter, scan epoch, detail mapping or naming transition invalidates prior contacts. Unrelated pending owners and old-generation completions cannot arm scanner actions.
