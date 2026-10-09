# Explicit native paper Quick Controls

The native utility and alarm builders accept paired `--paper-transitions` and
`--motion-system` options. They retain their original System time-helper, Runtime
SDK, tagged alarm SDK, Watch, and raw-paper pins. Only the selected native
adapter and shared presentation sources use the separate clean exact motion
checkout recorded in `Apps/native-paper-motion.json`.

Selected versions are Calculator/Stopwatch 0.1.13, Battery 1.1.6, BLE Scanner
0.2.8, BLE Touchpad/Buttons 0.1.8, Waterfall 0.2.4, Alarms 0.2.8 and Countdown
0.1.12. Normal native and Watch/raw manifests keep their existing versions.
No app-to-app crossfade, runtime grant, time policy or radio feature is added.
RF launch, draft, event-review and storage-discard guards are preserved.

## Build

Use the existing GCC 8.4.0 2021r2-patch5 compiler; no install is needed:

```sh
export NATIVE_APP_CC=/workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc
python3 scripts/build_native_utc_utilities.py \
  --system-apps ../x4-plain-app-logs-63 --runtime ../x4-alarm-app-runtime-public \
  --paper-transitions --motion-system ../x4-complete-ui-012
python3 scripts/build_native_utc_alarm_apps.py \
  --system-apps ../x4-plain-app-logs-63 --adapter ../x4-plain-app-logs-63 \
  --runtime ../x4-alarm-app-runtime-public --raw-system ../x4-points-system-raw \
  --watch-system ../x4-alarm-app-system-watch \
  --paper-transitions --motion-system ../x4-complete-ui-012
```

The default selected destinations are `dist/native-paper-motion-utilities` and
`dist/native-paper-motion-alarm-apps`. Both builders support `--output`.
Every native ELF has a distinct manifest, boot policy and `x4-native-app.json`
receipt; the cohort receipt includes effective source hashes, motion selection,
SDKs, flags, imports/exports and license copies including FontAwesome notices.
Each build validates all native, raw-paper and Watch targets, with exact bytes
against the existing immutable raw/Watch app baselines.

## Verify

The three composed host runners accept the same paired motion options:

```sh
python3 scripts/test_native_utc_utility_controllers.py \
  --system-apps ../x4-plain-app-logs-63 --runtime ../x4-alarm-app-runtime-public \
  --paper-transitions --motion-system ../x4-complete-ui-012
python3 scripts/test_native_utc_alarm_adapter.py \
  --system-apps ../x4-plain-app-logs-63 --adapter ../x4-plain-app-logs-63 \
  --runtime ../x4-alarm-app-runtime-public \
  --paper-transitions --motion-system ../x4-complete-ui-012
python3 scripts/test_native_utc_utilities.py \
  --system-apps ../x4-plain-app-logs-63 --runtime ../x4-alarm-app-runtime-public \
  --drivers ../watch-power-drivers \
  --paper-transitions --motion-system ../x4-complete-ui-012
python3 -m unittest discover -s tests
```

Normal and ASan/UBSan runs cover 188 Calculator/Stopwatch/Battery cases,
152 actual Alarms/Countdown adapter cases, and 146 BLE/HID/RF cases. Added
interactions exercise repeated open/close with exact background restoration,
physical Home during opening, crown dismissal followed by a new Home contact,
replaced/multiple-contact cancellation, and successful reopening. RF repeated
controls never restart capture automatically; keyboard drafts, event review,
failed saves, discard and launch guards keep their previous behavior. HID
input is neutralized and its session closes before the sheet takes ownership.

The 74 Python checks pass. Nine normal-native ELFs match the previous staged
cohort byte-for-byte; all 18 raw-paper/Watch outputs also match their immutable
baselines. Evidence is emitted under `build/native-paper-motion-*` and the
target output directories. This checkpoint uses the shared adapter pin named
in the profile; a later shared-source pin requires rerunning these composed
tests and targets.

Provider timing and resources are deterministic doubles. Real panel cadence,
backlight hardware, power loss and installation are not qualified. Source and
target artifacts remain local; no frozen BIN or release branch is changed.
