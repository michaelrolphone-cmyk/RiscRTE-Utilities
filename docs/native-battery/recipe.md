# Full-feature Battery qualification recipe

This is a local software-qualification recipe for the app-local native Battery
view. It does not assemble firmware, install an app, publish anything, or claim
to reconstruct the historical Waveshare .59 Battery ELF. Existing builders and
release manifests are unchanged. The new builder defaults to the legacy UI.

## Reproduce

Python 3.11+, a host C11 compiler, Git, and an existing ESP32-S3 GCC toolchain are
required. No network fetch, tool installation, or full image download occurs.
Pass repositories containing the exact objects listed below; their HEAD and
working-tree contents are never used as external source inputs.

```sh
python -m unittest discover -s tests -p test_native_battery_recipe.py -v
python scripts/build_native_battery_qualification.py \
  --system /path/to/System-Apps \
  --bridges /path/to/Watch \
  --runtime /path/to/Runtime \
  --profile-source /path/to/Waveshare \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc \
  --output-dir /tmp/battery-native-qualification \
  --native-ui
```

Omit `--native-ui` to build only the exact-baseline and current legacy controls.
The selected native build adds precisely `PORTABLE_BATTERY_NATIVE_UI` and
`PORTABLE_NATIVE_CANVAS`. Every invocation requires an empty output directory.
The source-owned pinned toolchain helper also supports `NATIVE_APP_CC` and the
ESP-IDF ESP32-S3 compiler wrapper; the bare unified Xtensa compiler is not used
with its incorrect default CPU configuration.

The three outputs are `baseline/battery.elf`, `legacy/battery.elf`, and, only
when selected, `native/battery.elf`. All use the same full-feature configuration.
The legacy current-source ELF must equal the frozen Utilities baseline ELF
byte-for-byte. This comparison protects native-selector-off behavior; it is not
a comparison with a historical shipped binary. The target receipt is
`qualification.json`. The retained .59 manifest is evidence only: no changed
ELF is assigned its historical release version or emitted as a release package.

## Immutable source inputs

| Owner | Commit | Inputs |
| --- | --- | --- |
| Utilities baseline | `4fdcad2495c33fe63f5f6faca9b11ced3c739a5b` | Battery control, frame/atomic helpers, alarm/Contexts headers, source-owned ELF validator |
| System foundation | `fd9a028c5d38eb517c1328b869684c4330a5d4d9` | Portable adapter, native canvas, Quick/radios, broadcast/Contexts, custody, SDK helpers and fonts |
| Watch bridges | `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4` | Generic local navigation/sleep, alarm sleep resume, motion, Contexts face names |
| Runtime SDK | `48229bd1593a2e24e95bf4537b495ee615990fd3` | App SDK; explicit Runtime/atomic-header override in the staged System include directory |
| Product alarm compatibility | `b9862e1379813a0914d4926ef59a3feb6fcf656a` | `profiles/shared-editor-inputs/watch-alarm-profile.h` and `AlarmServicePinnedV1.h` |

The current Utilities app source is recorded by commit, working-tree dirty
state, and actual compiler dependency hashes. It must descend from the stated
baseline. External sources are copied from exact Git blobs to small local
staging directories, not compiled from another task's working tree. Every actual
compiled external dependency is checked against its immutable object. Source
hashes are checked again after all profile builds; changing inputs fails the
qualification rather than creating a mixed receipt.

## Complete capability and source contract

`profiles/native-battery-baseline-manifest.json` is the exact manifest from
Waveshare .59 `sd-apps.zip!/Apps/battery/manifest.json`, SHA-256
`d02c27407a5ab4461812e080b98262f49953d72524e8a7878ba8b05568c81934`.
Both its bytes and these fifteen capability/version pairs are checked:

- `display.output@1`, `input.touch.raw@1`, `rtc.clock@2`
- `alarm.service@1`, `telemetry.broadcast@1`, `contexts.service@1`
- `input.power-key@1`, `power.sleep@1`, `display.power@1`
- `board.battery@1`, `storage.key-value@1`, `storage.key-value.atomic@1`
- `net.wifi@1`, `bluetooth.hci@1`, `motion.accel@1`

The JSON profile lists every define, forced header and translation unit.
The exact shared `PortableNativeCustody.h` is force-included in both controls
and the native profile: the existing atomic Battery controller otherwise only
sees its retain declarations under a native-time-toolbar selector, which would
conflict with the retained RTC policy. No compiler warning is suppressed to
conceal that dependency.
`battery.c` includes the real Battery controller with charging, unavailable/read
error, refresh and nested Back behavior. The full System adapter, local Watch
navigation/sleep bridges, and all four Quick translation units compile in both
controls and native mode. Alarms retain API 1 and the compatibility sleep suffix;
RTC retains API 2 and the fixed UTC+08-to-Denver policy. Native geometry does not
silently select a new time or alarm ABI.

Meaningful dependency-closure gates require the actual Battery atomic helper,
broadcast client, Contexts client, atomic Quick implementation, native custody,
Runtime atomic header, and every selected translation unit. The native profile
also requires an actual app-local `battery_native*` input. A manifest-only claim
or accidentally reduced host fixture cannot satisfy these gates.

The explicit Battery broadcast writer uses
`PORTABLE_UTILITIES_ATOMIC_PREFERENCES` and `PORTABLE_BROADCAST_RESULT_ONLY`;
Quick and automatic foreground preferences retain the explicit atomic selectors.
The selected current Utilities header does not require the obsolete experimental
board gate. Regular and atomic preferences use namespace 1; physical capability
instance bindings otherwise remain deployment-owned. Async RGB565, navigation
retention, local alarm-aware sleep and motion remain selected.

The historical feature-evidence command also selected
`PORTABLE_RUNTIME_APP_CATALOG`, although the exact .59 manifest does not grant
`app.catalog@1`. The recipe preserves the selector and records that optional
request explicitly. Contexts catalog launches remain unavailable with the exact
fifteen-capability contract unless a separately reviewed deployment provides the
additional grant. This recipe neither adds a sixteenth requirement nor asserts
that unavailable catalog actions work.

## Admission, inherited dependencies and limits

The builder admits only little-endian ELF32 Xtensa `ET_DYN`. It requires exactly
`app_main`, `app_module_init`, and `app_module_fini` dynamic exports, rejects
undefined weak imports, checks strong imports against the explicit standard
runtime/libc allowlist, and runs the pinned source-owned ELF structural loader
validator. Commands, defines, actual source hashes, compiler identity/hash, ELF
hashes/sizes and symbol lists are retained. These checks do not execute Xtensa
instructions or establish physical memory, driver, radio, charging, wake, or
display qualification.

Automatic idle and explicit sleep continue to use the pinned shared adapter
and generic Watch bridge. `PORTABLE_APP_SLEEP_LOCAL` stays enabled;
`PORTABLE_SLEEP_MANUAL_ONLY` is never introduced. Existing shared idle/lifecycle
behavior, including any retained-surface or custody dependency, is inherited.
This app-local UI recipe does not correct, waive, or hide that dependency and
does not resume any separately stopped shared-idle work. Hardware lifecycle
qualification remains a separate gate even when target admission and legacy
byte identity pass.

Focused Python tests exercise exact source/manifest pins, all fifteen capability
versions, every feature-selector and translation-unit omission, native opt-in,
weak/unknown imports, ELF architecture admission, dirty external working-tree
isolation, and rejection of idle suppression or unsupported publication claims.
The separate app-host layout/controller fixtures complement these checks; they
do not replace this full-feature target recipe.
