# Native BLE Touchpad source qualification

The opt-in PORTABLE_BLE_TOUCHPAD_NATIVE_UI selector renders and hit-tests capability pixels directly. Legacy and paper deployments retain their existing behavior. Shared HID session, pairing, gestures and cleanup remain in the production controller. Touchpad owns an independent two-contact raw subscription fenced by the matching visible owner7 frame and provider sequence watermark.

The native test matrix covers six sizes (128x128, 160x128, 240x240, 410x502, 320x180, 502x410), 22 behavior scenes, fast/120 ms presentation, and normal/ASan/UBSan builds: 528 actual app/adapter cases. Cases include pairing movement/multiple-contact/gap rejection, drag, two-axis scroll, cleanup retries, Back/Home, repeated entry and pre-visible input. System tests additionally cover snapshot custody, stale completions, neutral gating, paper selector bypass and feature-off equivalence.

Run scripts/test_ble_touchpad_native.py --system-apps PATH with --sanitize and --slow as needed. Run scripts/test_hid_renderer.py --system-apps PATH --paper --native-touchpad for actual legacy paper bypass. Target builds use scripts/build_hid_apps.py --system-apps PATH --native-touchpad, optionally --paper-profile, and exact sdk/hid-touchpad-native-sources.json. The independent pin leaves legacy sdk/hid-sources.json unchanged.

These are source qualification artifacts, not a new firmware recipe, app release, catalog binding, or physical performance claim. Native and paper target ELFs retain ABI/export admission and provenance evidence. A deployment-specific version/release and hardware acceptance remain separate.
