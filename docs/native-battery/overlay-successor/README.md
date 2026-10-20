# Native Battery 1.2.1 overlay successor

This explicitly selected development recipe composes the existing native Battery UI with native Quick Controls and native alarm rendering/input. It changes no System, Watch or Runtime source, no regular Battery manifest and no released identity. The production app source and its temporal/custody/input corrections are retained unchanged.

## Exact selection

- Utilities parent: `68fa96840136817ec22f465245123a9581ff891c`, tree `f53a7a5ac26a201cf266ba5f61b90c9e34e1bad9`.
- System: `b8e34df8b33096663d92b469dff225d130217354`, tree `d0525431eec60eef2c8e25028e3cc60ccab0f498`. This includes the qualified paper coordinate correction and Battery input union.
- Watch bridges: `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`; Runtime: `48229bd1593a2e24e95bf4537b495ee615990fd3`; alarm profile: `b9862e1379813a0914d4926ef59a3feb6fcf656a`.
- Native-only additions: `PORTABLE_QUICK_NATIVE` and `PORTABLE_ALARM_NATIVE`. All previous common selectors, eight translation units, three forced headers, 15 capability/API pairs, preferences namespace 1, radio namespaces 15/16 and optional catalog limitation remain intact. Legacy remains the default.
- New immutable identity: `native-battery-1.2.1-dev1`, manifest version `1.2.1`. The frozen 1.2.0 release hashes are retained in `profiles/native-battery-previous-release.json`.

## Actual application controls

The unchanged Battery controller and shared adapter run with the complete production feature selection, generic Watch navigation/sleep bridges and modeled capability providers. No application, input reducer, renderer, Quick session, alarm client or persistence implementation is replaced by a stub. Test wrapper additions expose read-only state. Raw reports open the actual modal and activate controls.

Eight scenarios run at 240×240, 410×502, 128×128 and 320×180:

1. Brightness, volume, DND, Wi-Fi, Bluetooth and airplane persistence, followed by reopening from committed provider state without repair writes.
2. Alarm interruption of Quick with exact-current dismissal.
3. Queued action edges during asynchronous presentation.
4. Atomic save refusal without a commit or retained invocation.
5. Rejected stale ACK followed by a new exact-token dismissal.
6. Native alarm outside-edge rejection followed by an inside-edge dismissal.
7. Torch interruption and brightness restoration.
8. Held brightness gesture cancellation by an alarm, without a late commit.

Each control requires exact completed Battery underlay restoration, immutable submitted pixels and zero leaked grants, subscriptions, frames or pending presentation tokens. The original temporal/custody tests, reduced UI matrices and selector-off controls remain required. Shared native Quick/alarm geometry, complete labels, paper composition, sparse-start rejection and opt-out qualification remain pinned to the System source and its independently qualified matrices.

Local results: 34 contract tests and all 96 direct-normal, direct-ASan/UBSan and snapshot-normal overlay cases passed. Final snapshot-sanitizer compilation exhausted temporary storage; a private-TMPDIR retry produced no executable. That local leg is not a pass. Exact-head CI must pass all 128 overlay cases and every retained gate before publication. The compact local evidence includes command, source closure, executable and per-case log hashes.

## Target and release gates

The existing target builder checks actual Xtensa ELF type, exact exports, approved imports, unchanged structural admission and selector-off equality against the historical Utilities baseline. Native Quick and alarm layout/render source must appear in the actual compiled closure.

Release preparation downloads the immutable 1.2.0 ELF, manifest and compiler command, verifies their retained hashes, requires identical actual imports/exports and capability pairs, and reports ELF, executable, data, resident and input-plus-sections allocation deltas. The same pinned production Runtime admission checker executes normal and sanitized placement/negative-admission controls. No allowlist, provider contract, loader or memory limit is relaxed.

Only a main-branch exact-source qualification may publish the new development release. Existing tags/assets are never replaced. Source closure, recipe, controls, original baseline assets, comparison and admission receipts are included in the release archive.

This is software qualification. Target instructions, physical-device memory fit, installation paths and hardware behavior remain untested. No held fixture or Runtime diagnostic upload is involved.
