# Utilities CI dependency roles

The Native UTC and Scanner workflows select distinct source roles explicitly. This repair changes CI/test tooling only. It does not edit application C, manifests, versions, grants, frozen source-pin files, or an existing product artifact.

## Native UTC service

- `native-system`: exact clean System `1d589d90bf27c7ffb76420de46564088ddb3714f`, required by the unchanged `sdk/native-utc-alarm-sources.json` verifier. Used for UTC behavior, both actual-Runtime runs and the four-profile target build.
- `regression-system`: exact System `d52a74bfb4acc01cc3f0a9dda2c95ba2ba679ee9`, retained for Points, Alarm/Countdown, volume and DND app regressions.
- Both roles use Runtime `30dcec5ce6ce33223f2b203a2399283e1f758567`.

The roles are separate checkouts. The exact UTC verifier, frozen per-file hashes, actual loader checks and flag-off Watch byte comparison remain unchanged.

## Scanner controls and current application

`legacy-scanner-control` runs all former model, lifecycle, Watch pixel, paper pixel and target-build checks against the frozen compatible legacy source cohort:

- Utilities `ce1c949d797a406d8e16646f5abe1aac0691ef52`
- System `a7f08a9db7342a69ef5b9bc03e3b1ea60dafbb7c`
- Drivers `aa5ce0140102bf4d1039e5f208588a6793d06b28`

These are explicitly historical controls. Their local-keyboard fixtures do not qualify the current shared-text Scanner.

`current-shared-host-scanner` compiles the current Utilities application and fixture, with no archived target artifact required. `test_x4_ble_shared_text.py --source-only` takes full explicit clean revisions for all five dependency roles:

- System `8a75862929e4e83c8f66b3d58cc1c36d44830c84`
- Runtime `0f17a435f99d02d60ca50df1d1a51fcef123db89`
- Drivers `4088b6892c2e2654b0342f04a7d191068e4a8e2e`
- Reader `45cf61ac013fb618e8d7fed63217a35350484d52`, display base/power headers only
- X4 `cbf4bfd34372bf87ccf60429e062a6e96d043aca`, display metrics/snapshot headers only

The four display headers are additionally checked against their exact delivered SHA-256 values. Dependency revisions and trees, compiled input hashes, the profile hash, commands, scenarios and logs are recorded. Dependencies are checked again after qualification. Header staging uses copies and never edits source dependencies.

The source recipe uses the existing delivered Scanner profile unchanged, production app/adapter/time helpers and production shared scene/text/profile services. It runs all 24 ordered-input/modal/lifecycle/retention cases normally and with ASan/UBSan (48 total). It also runs the existing production Scanner controller fixture normally and with ASan/UBSan, comparing LCD and paper actions with frames ready versus withheld. This preserves row actions, shrink/empty/repopulate behavior, prompt retry and scan cadence coverage.

The current target uses the existing `build_x4_resident_clients.py` with explicit compatible System/Runtime revisions and the staged display SDK. `check_scanner_ci_profile.py` verifies that target and host have identical delivered defines, requirements and grants, all 78 compiled target dependency hashes agree with the host witness, selected version and source identities agree, all 50 host checks passed, the foreground has no shared renderer, and the target ELF still matches its receipt hash. This is a separate non-installable development target; it does not replace a frozen product output.

## Local verification, 2026-10-10

- Six dependency-role/custody unit tests passed, including rejection of abbreviated/wrong/dirty source pins and mixed receipt/source-only modes.
- Current source-only: 48/48 shared-host scenarios plus two LCD/paper ready/withheld controller runs passed with exact public System `8a758629`.
- Both existing receipt modes remain supported: 48/48 matching-target scenarios and 48/48 archived-receipt/current-source scenarios passed.
- Current Scanner target compiled with pinned Xtensa GCC 8.4.0+2021r2-patch5; structural validation, import/export/foreground checks and target/host profile/input agreement passed.
- All frozen legacy host/model/pixel suites and both Watch/paper target builds passed.
- Native UTC behavior, Points/Alarm/volume/DND regressions and actual-Runtime runs passed normally and with ASan/UBSan. All four native UTC target profiles passed the actual loader at eight alignments; the flag-off Watch target is byte-identical to its frozen baseline.

Local sanitizer runs set `ASAN_OPTIONS=detect_leaks=0` because the executor traces child processes; ASan and UBSan remained enabled. No hardware, radio, endpoint, install, release or product qualification was performed.
