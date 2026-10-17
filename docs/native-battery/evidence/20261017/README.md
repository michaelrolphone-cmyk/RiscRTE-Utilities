# Native Battery software qualification evidence

Implementation PR: https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/71

Integrated merge: `48b983154e295238f87ba5050ebdab9773c5eec7` (same implementation tree). All 14 candidate source CI workflows passed before integration.

Source commit: `b5791106a80dacb41607da3f762696d86ee04790`.
Implementation tree: `33757f8e9d6cb548e42e8bf2a975d50d6663903e`.
Equivalent local checkpoint: `5fcc973233919c4b9739c1cd61d29f6d48517aae`.

This evidence branch adds only documentation/artifacts to that exact implementation tree. It does not publish an application, assign an app version, activate a catalog entry, or change the retained Waveshare .59 package.

## Reviewed renders

`native-contact-sheet.png` shows overview, details, direct BLE view, read error, save error, failed Back, explanation, 0%, 100%, and unknown charge. `evidence.zip` contains all 40 individual PNG captures and their source-bound receipt. Sizes include 240x240, 410x502, 128x128 and 320x180. The contact sheet is generated from actual Battery/native-adapter framebuffers, with no bitmap editing.

BLE images are **direct-view-only**. The documented navigation entry remains paper-only; color access is deferred in https://tasker-workboard.hazy-auk-4334.chatgpt.site/?task=3776b062-bf77-4380-baa2-9bd6fe3dc2fa.

Final app-local view SHA256: `d9da25786ed766221250d1a8c89d95faea2780fe0b712633ca74bcea768e0bcb`.
The compact 100% ring, interior Estimate caption, USB POWER and Not reporting charge distinction were independently reviewed.

## Passing software component gates

- 196,632 geometry cases normal and ASan/UBSan.
- Four 297-case reduced controller/adapter matrices: synchronous and delayed async, normal and ASan/UBSan.
- Eleven explicit recipe contract tests; transaction and capture receipts included.
- Native push CI 39047695995 and PR CI 39047797508 pass and independently reproduce the local native ELF 290,776 bytes SHA256`682e6f590a99815c889cb2c42d76df7b0471123382ea9f7e15e6b0f36315b690`.
- The full-feature selector-off control exactly equals frozen baseline 285,096 bytes SHA256`7dc356b0db7adfb0289c5c42f668090ec7cb1f6aa0a8d13ebb40ac60f5123342`.
- The new native ELF also passes the exact .59 production structural/import admission fixture normal and sanitized, including 12 missing-import rejection controls, 4 malformed/truncated controls and 2 removable-image snapshots per run.

## Release-blocking inherited behavior

Full-profile diagnostics exercise all 15 modeled grants and the real Battery/adapter/Watch bridge code. They separately test an alarm-cue cancellation pass with contact queued at the idle deadline, and queued pre-visible contact versus genuinely fresh post-visible input. See the diagnostic source, commands, logs and receipt in the ZIP.

These diagnostics preserve the failure, not a correction. The shared correction remains stopped. Standalone availability is withheld until the actual shared idle and presentation-input defects are corrected and the final composed recipe is qualified. No reduced host pass waives these failures.

## Scope limits

All evidence is software qualification. No Xtensa target instructions or physical hardware were executed. Reduced UI matrices do not prove the full feature recipe. Production admission is structural/import validation, not fresh full Runtime grant-policy execution or radio/charging/wake behavior. The recipe is newly explicit; retained .59 compiler flags are not inferred from its ELF. Atomic helper compatibility still depends on deployment-owned API 1 namespace bindings.

Immutable source pins and the explicit feature recipe are in the implementation's `docs/native-battery/recipe.md` and `profiles/native-battery-qualification.json`. No Settings or shared lifecycle source was edited by this Battery checkpoint.
