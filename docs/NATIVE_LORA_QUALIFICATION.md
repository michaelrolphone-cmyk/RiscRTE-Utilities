# Native LoRa qualification boundary

This is a source-pinned, opt-in software qualification of LoRa Messages. It does
not assemble or install firmware, change a product recipe, emit an installable
manifest, or publish a release. Product `.64` pins and release inputs are not
changed. Native UI is disabled unless explicitly requested.

## Exact sources and fail-closed entry

`profiles/native-lora-qualification.json` contains complete immutable commits for
System and Runtime, the exact pre-change Utilities/System baselines, and all
selected defines. `scripts/build_native_lora_qualification.py` requires each
external checkout's HEAD to equal its pin and rejects tracked, staged, and
untracked changes. It then compiles from those Git objects, not live dependency
files. The same identity/cleanliness checks run again before a successful receipt
is written. There is no dirty-source, candidate, alternate-pin, or relaxed-check
switch.

Utilities must also be clean, with HEAD exactly equal to the explicit
`--utilities-commit` and descended from the pinned baseline. All current Utilities
target sources, the qualification script and profile are staged from that Git
commit. There is no working-tree snapshot acceptance. Every consumed dependency
is hashed and associated with its repository or compiler package. All three source
checkouts are rechecked before writing a successful receipt, which records their
exact commits and trees. The unchanged legacy manifest, source pin and inventory
are checked byte-for-byte.

The builder itself validates the complete canonical profile against its reviewed
contract hash before examining source checkouts or invoking tools. Modified pins,
selectors, translation units, capability or display contracts, scope or extra
profile fields fail closed. An intentional profile change requires changing the
recipe's contract identity as well as its reviewed profile.

## Supported target composition

The target uses the existing minimal `build_lora_apps.py` composition:

- LoRa Messages app and model
- Production System portable adapter
- The existing empty development catalog
- `PORTABLE_FORCE_FULL_FRAMES`, `PORTABLE_ALARM_CLIENT`,
  `PORTABLE_RADIO_SESSION`, `PORTABLE_NOVA_UI`,
  `PORTABLE_APP_OWNS_TOUCH_CHROME`

The shared-input composition adds all six existing safety selectors together:
`PORTABLE_LORA_SHARED_TEXT_INPUT`, `PORTABLE_TEXT_INPUT_CLIENT`,
`PORTABLE_APP_LAUNCH_GUARD`, `PORTABLE_CHECKED_STORAGE_RUNTIME`,
`PORTABLE_NATIVE_CUSTODY_FENCE`, `PORTABLE_ALARM_TERMINAL_RETENTION`.
It includes the real long-text client, presenter handoff and custody fence, with
missing canonical Runtime SDK headers resolved from the pinned Runtime source.

`--native-ui` adds exactly `PORTABLE_LORA_NATIVE_UI` and
`PORTABLE_NATIVE_CANVAS` to the shared-input target. It does not silently enable
any native selector in a default build or legacy recipe. Native admission is
RGB565-only and dimensions are capability-provided; the target has no fixed
resolution define. Paper and monochrome retain their legacy path. Their actual
behavior is checked by host tests separately from the target ELF checks.

The builder compiles both baseline and current source with the native selectors
absent, for both the original minimal and complete shared-input compositions. Both
pairs must be byte-identical, including the full target ELF. Baseline builds use
the exact old System adapter as well as old Utilities app, so parity does not hide
an off-path System change. The native target must consume its native layout/view,
private System API and private input implementation.

## Capability and host-test boundaries

The profile records the selected features' capability acquisition requirements:
display, raw touch, LoRa v2, key-value, RTC v2, alarm, and shared text input. The
adapter also attempts optional battery acquisition. Long shared text requires the
runtime-checked extension to text-input API v1; an API number alone does not prove
it is supported.

The existing legacy manifest lacks the shared text-input grant. It is not native
installation authority and is deliberately never copied beside the qualification
ELF. No deployment is supported by this output. A future deployment needs an
independently reviewed complete grant/manifest and product memory qualification.

The shared-editor host suite uses the target's five minimal selectors, six safety
selectors and two native selectors with actual production scene/text providers.
It also enables `PORTABLE_INPUT_NAVIGATION`, `PORTABLE_INPUT_NAVIGATION_LOCAL`
and `PORTABLE_ASYNC_RGB565` for modeled physical Home and delayed presentation.
Display/touch/RF/service backends are deterministic test fixtures. The
local-editor native-host suite models the same navigation/asynchronous interfaces,
adds the custody fence and return-to-Springboard define, and uses local keyboard
input instead of shared-editor selectors. Its fixture supplies those
peripheral interfaces; those extra host flags do not establish that the minimal
standalone ELF contains Watch navigation or sleep bridges. The separate optional Quick host pass additionally links the four production
Quick sources and selects Quick Actions, Quick Radios and Quick Native; that
supplemental composition is also explicitly outside the minimal target. Neither suite is
physical RF, physical sleep/navigation, or Runtime/ELF execution evidence.

## Full product composition is separate and unsupported here

System's retained `docs/shared-app-crashes/replay/twatch.json` LoRa entry is
reference evidence, not this target's recipe. It additionally selects Watch
navigation and sleep bridges, Quick controls/radios, motion/low-battery, broadcast,
Contexts, forced profile headers, catalog behavior, async display, product font/icon
subsets, native-height layout, retention, LTO and other product flags.

This qualifier does not remove inconvenient flags from that recipe and call the
result a product build. It qualifies the explicitly identified existing minimal
composition. `unsupported-full-product.json` retains the exact reference command and source
hash, explicitly lists its unselected product defines, and records `qualified:
false`. Full product closure, capability grants, memory admission, installation
and firmware integration remain unqualified. No held Files/Frequency/Lists change
is needed; Runtime use is exclusively its independent immutable Git commit.

## Running and interpreting evidence

Use official ESP32-S3 GCC `8.4.0+2021r2-patch5`, whose executable, subtools, libgcc
and consumed headers are recorded with hashes. Run with clean, exact checkouts:

```sh
python scripts/build_native_lora_qualification.py \
  --utilities-commit EXACT_40_CHARACTER_UTILITIES_COMMIT \
  --system /path/to/exact-system --runtime /path/to/exact-runtime \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc \
  --native-ui --output-dir /empty/path/to/lora-target
python -m unittest discover -s tests -p test_native_lora_qualification.py -v
```

A successful target run validates ELF32 little-endian Xtensa `ET_DYN`, the three
exact public function exports, bounded imports supported by pinned Runtime public
libc, section layout, and pinned Runtime loader structural admission. The loader
harness also requires malformed and truncated variants to be rejected. Each mode
retains build/dependency commands, source closure hashes, ELF/symbol listings and
logs. `qualification.json` is written only after every check and both parity pairs
pass; `sha256.json` covers the resulting files. A failed attempt retains logs, but
has no successful qualification receipt. Existing nonempty output is rejected.

Static ELF totals exclude stack, runtime heap, framebuffers and provider
allocations. Target instructions are not executed. Passing means the stated
standalone software boundary passed, not that a physical device/product is ready.

The dedicated `native-lora-qualification.yml` workflow has read-only repository
permissions, no publish input/job, no firmware assembly, and no install step. It
runs target contracts/parity, System ownership and 40 feature-off compositions,
normal/sanitized native host tests including actual paper controls, the bounded
optional-Quick host controls, and the real shared-editor suite. Evidence is
retained for 14 days. Remote CI execution requires the exact pinned System commit
to be available in its repository; merely adding the workflow is not a CI pass.
