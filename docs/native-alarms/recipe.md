# Native Alarms software qualification

This separate recipe preserves the full fifteen-capability Alarms/Countdown
contract and builds both applications against immutable shared dependencies.
The native view is opt-in. It does not replace the .59 firmware, its app bytes,
the legacy manifests, or an existing catalog entry.

Packaging requires the actual full-feature temporal coverage contract and
hash-bound passing receipts. Passing the recipe's synthetic contract tests is
not a target, controller, temporal, memory, or hardware pass.

## Exact inputs

- Utilities selector-off baseline: `4fdcad2495c33fe63f5f6faca9b11ced3c739a5b`
- Initial Utilities candidate base: `68fa96840136817ec22f465245123a9581ff891c`
- System checkpoint: `02f65827d524ec7b1bd6a5d0576a9d731cc589d5`
- Generic Watch bridges: `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`
- Runtime: `48229bd1593a2e24e95bf4537b495ee615990fd3`
- Compatibility profile: `b9862e1379813a0914d4926ef59a3feb6fcf656a`

The System checkpoint must be updated together in the profile, builder and
workflow if the final reviewed foundation changes. External sources are staged
from these Git objects, never external working trees. The compiler dependency
output records the actual transitive production source closure.

The original .59 Alarms manifest is version 0.2.6 with SHA256
`0977a98cbf3c799ca13a89a9c6adce473b8f7538738c9148b019bcda03c02011`.
Its 153,428-byte ELF has SHA256
`7f5cb21cff86d3f993a42dc1e254aa568590aa6488c5217acd6bd899b67b3862`.
The original Countdown manifest is version 0.1.9 with SHA256
`7f3571ae3429bae2baf5eaf43c79e75a64048d872a1f340b8fa121ed9f63d148`.
Both manifest byte streams are retained unchanged. They are capability evidence,
not release manifests for newly compiled ELF files. The full target recipe does
not claim to reproduce either historical .59 ELF.

## Reproduce

Use Python 3.11+, a C/C++ host compiler, Git, and the existing official
ESP32-S3 GCC 8.4.0 2021r2-patch5 toolchain. The builder does not fetch inputs or
install a compiler. Pass repositories containing the exact required Git objects.

```sh
python -B -m unittest discover -s tests -p 'test_native_alarms*.py' -v
python -B scripts/build_native_alarms_qualification.py \
  --system /path/to/System-Apps --bridges /path/to/Watch \
  --runtime /path/to/Runtime --profile-source /path/to/Waveshare \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc \
  --output-dir /tmp/native-alarms-qualification --native-ui
```

Omit `--native-ui` for the two exact baseline and two current selector-off
controls. Native selection adds these five flags:

- `PORTABLE_ALARMS_NATIVE_UI`
- `PORTABLE_NATIVE_CANVAS`
- `PORTABLE_APP_OWNS_TOUCH_CHROME`
- `PORTABLE_QUICK_NATIVE`
- `PORTABLE_ALARM_NATIVE`

Every build retains the complete COMMON recipe: automatic and manual sleep,
local navigation, alarm client, Quick/radios, motion wake, BLE broadcast,
contexts, async RGB565, native-height display, atomic Quick/preferences ABI,
checked storage, custody fencing and alarm terminal retention. App return is
`ALARM_RETURN_APP="springboard.elf"`; neither `PORTABLE_RETURN_APP` nor
`ALARM_NATIVE_UTC` is selected. There is no manual-only sleep suppression.

The output contains `baseline`, `legacy`, and optionally `native` directories,
each with separate Alarms and Countdown ELF files, commands and symbol reports.
Both selector-off ELF files must byte-match their exact-baseline build. Every
ELF must be little-endian ELF32 Xtensa ET_DYN, export exactly the three application
entry points, contain no undefined weak imports, satisfy the explicit import
allowlist, and pass the pinned structural loader validator.

## Host and temporal evidence contract

Four reduced controller receipts are required: `host-normal`, `host-sanitized`,
`async-normal`, and `async-sanitized`. Each has 18 rows: both applications at
120x128, 128x128, 160x128, 160x240, 240x240, 410x502, 502x410, 320x180 and 800x480.
The named host case is `geometry-editor-persistence`.
The normal host run retains 28 exact provider raster captures, whose hashes are
verified before publication. Geometry tests also check every integer width
120–1024 and height128–1024 with both field counts, plus rejection and overflow
boundaries. Header/footer controls keep native pixel sizes rather than scaling
a completed viewport.

All receipts bind the exact target receipt hash, capability list, external pins,
compiled production dependency hashes, unchanged-source flag, sanitizer and
async mode, and individual application/case/geometry/exit-code/log hashes.
Missing cases, duplicate rows, failed cases, changed logs, source mismatch or
unbound receipts prevent packaging. Evidence paths cannot escape their root.

`profiles/native-alarms-controls.json` owns the measured temporal contract and
case descriptions implemented by `scripts/test_alarms_native_temporal.py`.
Native and raster-snapshot modes exercise 24 cases at240x240,410x502,128x128 and
320x180 for both actual controllers. These cover raw queue watermarks, redraw
continuity, stale page/invocation tokens, snapshot/presentation failure custody,
Quick modal/atomic volume, persistence uncertainty and alarm interruption.
Automatic idle uses the real local Watch bridge; explicit sleep tests call the
production sleep boundary directly and do not claim a physical button gesture.
Each entry needs `mode`,
`applications: ["alarms", "countdown"]`, `cases`, and `sizes`. Every listed mode
runs both normally and under ASan/UBSan. Supported mode identifiers are
`temporal`, `snapshot`, `selector-off`, and `original`; listing a mode is not proof
that it has been implemented. An original failure control additionally requires
explicit source hashes and any exact external pin overrides. Native/snapshot
full-feature controls must compile the complete target dependency closure;
selector-off controls must match the legacy closure. All temporal receipts use
async RGB565 and include their `mode`.
Qualification runners and fixture dependencies must also match the exact source
commit or its enclosed source bundle. Original controls use the unchanged
Utilities editor with the current pinned dependencies and measure legacy
early-touch/cancellation behavior; they are not historical .59 execution.

## Immutable package and release gate

`Apps/native/alarms.json` independently versions native Alarms as 0.3.0. The
reserved standalone development tag is `native-alarms-0.3.0-dev1`. Countdown is
qualified as a sibling control; it has no native release manifest or catalog
publication in this change.

After all target and host gates pass, `prepare_native_alarms_release.py` requires
a clean exact Utilities source. It invokes the pinned shared memory inspector and
production Runtime admission code for both native ELF files, normally and under
ASan/UBSan. It checks actual section placement requirements, ordinary application
admission, missing-import rejection, unsafe-state rejection and driver-role
rejection. It preserves every compiled native production dependency, exact
recipe/test sources, available license notices, commands and evidence hashes.

`publish_native_alarms.py` validates only by default, including when
`--validate-only` is explicit. Publication requires a separate authorized
invocation with `--publish`. Qualification CI has read-only permissions. The
main-only publisher depends on completed qualification, consumes the exact
commit's retained artifact, and explicitly passes `--publish`. Pull requests
and work branches cannot publish a release.
The publisher refuses changed source, changed bytes, incomplete/detached evidence,
wrong or partial existing releases, or an existing tag belonging to another
source. It never overwrites assets. It verifies downloaded checksums and the
complete enclosed evidence bundle before accepting an existing release or making
a newly uploaded draft public. A failed partial release requires review.

Software qualification does not execute Xtensa instructions or qualify physical
memory fit, device installation, drivers, touch hardware, wake behavior or radio
behavior. All fifteen providers and deployment-owned bindings remain required.
The optional `app.catalog` request is not promoted to a sixteenth capability;
unsupported catalog launches remain unavailable without a separately reviewed
grant. Firmware assembly, installation and catalog integration remain separate.
