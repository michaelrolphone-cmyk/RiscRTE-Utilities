# X4 BLE Scanner 0.2.19 shared text-input target

This is the locally reserved successor client for X4 0.1.51 composition. It is
not published, flashed, hardware-qualified, or independently installable.
Home 0.3.22 and X4 0.1.50 are unchanged.

## Exact profile and source custody

- BLE client/paper implementation is unchanged from source checkpoint
  `042772fd820a5071eeb2bbe84c4d4fc9dd7339ec`.
- System: `c336b776c869029ecd9f096aa5e6497759581440`.
- Runtime SDK/import policy: `615fb236b591bc6974a35ae23c7b2b785c0a5016`.
- Scanner fixture/provider: `aa5ce0140102bf4d1039e5f208588a6793d06b28`.
- Installed BLE 0.2.18 resident build receipt SHA-256:
  `7ca8df7c18f776aebb47e721aea1c9c25f6c4523d436b5522156359133f0aa6e`.

`Apps/x4-ble-resident-profile.json` preserves all 20 installed defines and all
10 grants. It adds `PORTABLE_TEXT_INPUT_CLIENT`, `PORTABLE_APP_LAUNCH_GUARD`,
and exactly one `ui.text-input@1` declaration and instance-0 grant. It retains
tagged alarm v2, native-time support, telemetry default off, resident policy,
terminal custody retention, partial-damage/low-latency presentation, physical
Home and launcher destinations. There is no local keyboard fallback.

`build_x4_ble_shared_text.py` uses the installed resident recipe, adapted with
exact dependency pins. It stages the byte-identical Bluetooth sensor/telemetry
contracts together so combined host fixtures include one physical SDK. The
generic BLE developer builder is not this product build.

## Reproduction

Use the already-installed pinned Xtensa GCC 8.4.0 esp-2021r2-patch5 compiler in
`NATIVE_APP_CC`. No PlatformIO invocation is required. If one is introduced,
set `PLATFORMIO_SETTING_ENABLE_TELEMETRY=No` explicitly.

```
python scripts/build_x4_ble_shared_text.py --resident-shell-client \
  --system-apps "$SYSTEM" --runtime "$RUNTIME" \
  --display-sdk "$INSTALLED_DISPLAY_SDK" --output "$BUILD"
python scripts/verify_x4_ble_target.py --build "$BUILD" \
  --baseline-receipt "$INSTALLED_BLE_RECEIPT" --custody "$INSTALLED_CUSTODY" \
  --runtime "$RUNTIME"
python scripts/test_x4_ble_shared_text.py --build "$BUILD" \
  --drivers "$DRIVERS" --output "$EVIDENCE"
```

## Qualification

The target checks validate ELF32 little-endian Xtensa shared-object structure,
imports against the selected Runtime native allowlist, the literal foreground
resident descriptor `(1,16,2,0)`, exactly four exports, no foreground copy of
Quick Actions, version identity, unchanged paper app sources, and the complete
installed flag/grant delta.

The exact target receipt supplies all production defines, includes and sources
for the X4 host replay. Twenty-four scenarios run normally and under ASan/UBSan. The fourteen UI/lifecycle scenarios cover:
idle, scan/stop, nested Back, Bluetooth enable, naming save, touch cancel,
navigation Back, missing provider, terminal cleanup failure, cancel then Back,
repeated editing, Home, naming cancellation then Home, and resident controls.
Ten additional scenarios remove Runtime liveness at text acquire/open/poll/close/
release and alias-storage acquire/get/put/release, plus failed text acquire with
an empty grant. Every peripheral and ordinary Runtime callback asserts that no
I/O occurs after loss. The existing retention callback remains available to
seal the invocation. Source-only System predecessor `52cac988` fails the
post-loss health-call assertion; successor `c336b776` passes.

The real app entrypoint, adapter, scanner, scene/text/profile providers run.
The native-time callback is separately exercised after clean app return;
the paper app owns its header and does not itself render a time toolbar.

Physical 800×480 MONO1 output is sampled independently into upright logical
480×800 captures. Keyboard border pixels, touch-driven text, low-latency intent,
stride guards, alias persistence after close, zero leaked grants on clean exit,
and frozen resources after terminal retention are asserted. Re-entry after
retention must fail. Native realtime, tagged alarms and resident/telemetry
callbacks use bounded fakes; hardware transport, panel/touch and Runtime dispatch
are not a full product boot. Full composition/admission, exact native firmware
binding and physical X4 qualification belong to the product integration.

Existing BLE model/lifecycle/Runtime-loss tests, Watch raster tests, 112 cleanup
fault/retry replays and 64 Watch/X4/X4-custody app_main replays remain part of the
regression run. All run in the isolated successor worktree.
