# Contexts provider-pair qualification

## Result and scope

The cumulative full-profile System scene and repaired text provider are compatible
with the final standalone Contexts application at 240×240 and 410×502. The mixed
assembly pair is not suitable for editing: its older scene rejects policy-input
continuity, so the repaired text provider returns UNAVAILABLE before the first
text poll. Contexts safely restores the prior modal state; normal editing cannot
start. This is an actual-provider behavior test, not a header-only comparison.

This checkpoint changes tests and documentation only. Apps and lib are unchanged.
It does not select a product pin, build firmware, publish, or qualify hardware.

## Exact inputs

- Application: Utilities public `186ba80899fc9746609860821df61c7bbe0e2dc8`,
  tree `736ca442855992ef2e595e244d191329455809a5` (local equivalent
  `edf2cb2cfed9da0aa99a0d6bf7771cf49df6f6d3`). Sealed ELF: 88,020 bytes,
  SHA-256 `8db9d4be5801635bdb58e3f5ddaa41bcbdb744243a0e875a4a8252496cd24402`.
- Repaired scene/text: System `f4b30f8ebcc6d8cb905c8e90c91b1691acbe8165`.
- Incompatible mixed scene: System `1aaf53e23ca9651a5cf9d1debe44941c9d7d0e58`.
  This commit is an ancestor of f4b30f8, not a newer source revision.
- Native Runtime: public `a6c7f273025a59b1383e9c2795fa0ff0a8e9efad`, local
  `71aa21674afd48510623de34822827e0f45bbb33`, tree
  `2e4cc470d8af03c9cbef2540adb85164d795f9b8`.
- Catalog headers: System `72f435fcee376ec62d17ece6180753385ea547d9`.
- Original 34-face header SHA-256:
  `e75de6b17b85e1d7fa2ebc4b89514ac7345f671027c3b15dfdbd98f9b06b5ea2`.
- Full telemetry font include SHA-256:
  `2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1`.

Per-run JSON records source hashes and separate scene-provider and app/text-provider
SDK hashes. The legacy scene is compiled against its original four changed
headers; the text provider and application retain their repaired headers. Full
profile compilation applies neither SCENE_HOST_WATCH_240 nor a telemetry-off flag.

## Compatibility change

No shared document/API struct layout is changed by the relevant lifecycle delta.
The old scene's configure function accepts only SHARED_CONTROLS (bit 1).
The repaired text checkpoint requires POLICY_INPUT_CONTINUITY (bit 2), which the
old scene rejects. The cumulative scene adds this behavior and FOCUS_CHANGED
resume handling, contact-ID reuse repair, bounded queued input continuity, and
finer keyboard raster/input sampling, plus longer/password keyboards and generic
telemetry components. The current Contexts source/features and prior checked
storage and unrelated application pins remain untouched.

The cumulative source full manifest is version 0.4.6. For this composer, retain
its existing omission of optional input.navigation. The resulting provider
provides ui.scene@1 and requires exactly display.output@1, input.touch.raw@1,
platform.clock@1 and ui.presentation-profile@1. Scene touch Back, touch Home,
and native app-exit are distinct test paths. Native app-exit is the relevant
runtime contract for crown exit; physical crown electrical behavior is untested.

## Reproduce host qualification

Set RUNTIME, SYSTEM, CATALOG, UTILITIES and FACES to the inputs above. Set FONT to
that exact generated telemetry include. LEGACY is a materialized old scene tree
containing Services/scene_host and the original RiscSceneLifecycleV1.h,
RiscSceneComponentsV1.h, RiscSceneCheckpointV1.h and SceneKeyboardV1.h at their
original sdk/app or Services/text_input paths. Use verified local bytes; the
runner does not fetch Git objects or access the network.

```sh
run_pair() {
 python scripts/test_contexts_provider_pair.py \
  --runtime "$RUNTIME" --system-apps "$SYSTEM" --catalog-system "$CATALOG" \
  --utilities-source "$UTILITIES" --face-header "$FACES" \
  --telemetry-font "$FONT" --legacy-system "$LEGACY" --output "$OUTPUT" "$@"
}
for geometry in 240x240 410x502; do
 for flavor in normal sanitized; do
  sanitize=""; test "$flavor" = normal || sanitize="--sanitize"
  run_pair --pair mixed --geometry "$geometry" $sanitize
  run_pair --pair mixed --geometry "$geometry" --no-navigation $sanitize
  run_pair --pair cumulative --geometry "$geometry" --all $sanitize
  run_pair --pair cumulative --geometry "$geometry" --no-navigation $sanitize
  run_pair --pair cumulative --geometry "$geometry" --no-navigation --catalog $sanitize
 done
done
```

The broad 185-case suite uses the navigation-capable test transport and covers
editing reasons, keyboard/modal ownership, storage and retention faults,
checkpoint races, async/frozen display completion, Home/Back and cleanup.
It is not a claim of 185 no-navigation cases. The no-navigation suite separately
starts the real host with only its four declared dependencies and uses raw touch
Back/Home and native app-exit. The three catalog cases exercise boundary 96,
32/33-byte exact workflow keys, real touch-swiped 95-row selection, refresh,
modal Back during an unfinished frame, edit retention, logical geometry and
in-bounds bottom controls using the full display height. Synthetic owner
transports replace hardware; scene and text implementations are real.

Mixed-pair default checks remove the fixture's UNAVAILABLE injection and assert
that the actual text provider returns UNAVAILABLE. Running mixed `--cases
text-2-done text-2-native-home` intentionally fails the normal-edit expectations,
which is useful negative evidence, not a supported combination.

## Reproduce bounded target/loader qualification

Use GCC 8.4.0 (esp-2021r2-patch5), and a Python with pyelftools if compaction is
selected. COMPACTOR is the approved loader-metadata-v3 helper.

```sh
NATIVE_APP_CC="$XTENSA_GCC" python scripts/test_contexts_provider_target.py \
 --runtime "$RUNTIME" --system-apps "$SYSTEM" --legacy-system "$LEGACY" \
 --telemetry-font "$FONT" --compactor "$COMPACTOR" --output "$FRESH_TARGET_OUTPUT"
```

The target helper preserves complete raw originals and checks both raw and
compacted modules with the native ELF structural validator. Compaction proves
retained loader segments/sections, code/fonts, global/weak and relocation-needed
symbols, and retained relocations unchanged. It removes only non-allocated
metadata; it does not remove app features or fonts.

- Cumulative raw: 467,088 bytes,
  `a264c572f180d53e6f8e4632bd2834e5dc0b3a3581c95e5fe9995197deea4359`.
- Cumulative loader-metadata-v3: 298,204 bytes,
  `b110f57972fadf67f36ddc08f28bc3dfa03e030d46d5fd72d4b64890ff201cfb`.
- Controlled local old raw: 302,332 bytes,
  `593f0bde1a573f96b48394e910350600c455930fcc30a885381108e6f143d243`.
- Controlled local old compact: 197,584 bytes,
  `c383a5487619fcf469d22d3254ac03e3267e14677e4f69860fa2a807260e9718`.

The controlled same-stage compact difference is +100,620 bytes. It is not an
assembled-store delta. The reported Mac composer old artifact is 208,776 bytes,
SHA-256 `2fd96f8f965d10215e126263cf256651949e75c61a6cae201db2d9f4ed52f956`;
its reported raw predecessor is 315,000 bytes, SHA-256
`6676ce70db2043c6c600a54ffec6e6201f9dd37d253dcd829fa7b399ee0951e9`.
Its exact build/normalization recipe has not been reproduced here. Replacing
that supplied artifact with this compact candidate is +89,428 payload bytes
arithmetically, before store metadata, alignment, admission and full-image gates.

## Recorded matrix

808 cases passed: 740 cumulative navigation-capable lifecycle cases, 48 targeted
no-navigation lifecycle cases, 12 no-navigation real-touch/catalog/geometry
cases, and 8 actual mixed-provider safe-unavailable cases, all across both
geometries and ordinary plus ASan/UBSan builds. The target helper independently
reproduced raw/compact hashes and validated all four provider ELFs.

## Composer handoff and remaining limits

Select the cumulative scene component and preserve the existing repaired text,
final Contexts ELF, catalog grant (instance 0), full profile, four-dependency
manifest overlay and unrelated Files/other app pins. Re-run the actual assembled
cohort admission, dependency/grant validation, retained compaction proof and
whole-image capacity checks after substitution. This checkpoint does not itself
repin any product.

The app chooser supports 95 native rows plus Refresh. 96–128 rows visibly refuse
selection without truncation or manual paging; a generic virtual-list extension
remains out of scope. Saved workflow keys remain bounded to 32 exact characters.
No physical device, Xtensa instruction execution, X4 run, or complete firmware/
store admission is claimed. The old composer's unmatched recipe is a separate
provenance comparison and does not invalidate the corrected component's own
reproducible qualification.
