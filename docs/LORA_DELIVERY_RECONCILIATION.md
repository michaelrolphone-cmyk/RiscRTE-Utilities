# LoRa delivery reconciliation — private software qualification

Original task: cf71e334-ff2c-4272-b1f5-c83d9e651960, eight points. No duplicate task or completion claim. This document supersedes the qualification-status paragraph of LORA_SHARED_RECONSTRUCTION.md; that earlier reconstruction checkpoint is retained unchanged.

## Original acceptance clauses

| Original clause | Completed software evidence | Remaining delivery boundary |
|---|---|---|
| LoRa Messages uses reusable shared scene/intent components with complete app behavior preserved. | Public session/presenter suites, strict controller compilation, 29 root cases per mode, 40 actual Runtime/Graph cases per mode; full selected Home lifecycle and explicit compose/send/history/profile tests. | Native owner reports final-05 Watch/Wave assembly and metadata admission passed. Cloud lifecycle tests remain separate; tests cover recorded cases, not exhaustive physical behavior. |
| Source ownership and component reuse are verified independently of the native-resolution task; reuse any already-completed migration work. | Utilities base586593; original message model/profile reused; Scanner f057 helper blobs unchanged; Runtime0052, Systemeb4 consumer contracts, bb0 provider/Home dependencies, Watchba88 production host. | Public publication remains refused/stopped. No dependency task is marked complete by this lane. |
| Deliver this application as one byte-identical ELF across X4, T-Watch and Waveshare. | Standalone and generic inventory builders produced canonical/repeat/three selections identical to b55ba8b0fa7036c889c81cd32c88e76d17b2d54b790bd10688934f966c3dc46b (60364 bytes). X4 additive projection/admission passed. Native owner reports final-05 Watch and Wave assembly passed with canonical b55ba8b0 unchanged. | Native owner independently read all eight Watch/Wave before/after normal/sanitized metadata preparation rows as passed. Public publication remains stopped; no installed-binary execution or physical delivery claim. |
| Optional capabilities abstract UI/device/storage; providers own hardware, presentation and physical lifecycle. | Actual Runtime/Graph optional UI/text/radio/storage absence; production scene/text/Home owner transitions; alarm API1, idle, controls, delayed presentation/cancellation, authority loss and retention. Production civil DSO through Runtime tested with RTC hardware fixture. Separately, native owner reports final-05 metadata preparations with hardware_calls=0 and storage_calls=0; Watch i2s_tables=2, Wave=1. | Physical drivers are explicit fixtures. No installed target ELF execution claim. Ancillary optional BLE/network/contexts/install/wake modules are absent in this harness. |
| No device-class/backend or fixed-resolution app branches; disclose unavailable hardware features in the UI. | Unchanged one controller uses semantic document/intents and optional grants; providers select Watch compact and Wave dynamic presentation. Wave410x502 full lifecycle46+46 passed. Radio-absent projection preserves original absence; no provider/grant is invented. | Wave product has no original radio authority. Host send tests use an explicit radio fixture and do not assert Wave hardware availability. |
| Preserve existing features and saved data. | Explicit-only RF transmission,240-byte draft,255-byte received payload/history, exact32-byte profile roundtrip; original namespace9 and read-only namespace1. X4 unrelated payload/authority preserved; authority helper preserves reviewed legacy grants and rejects unknown/malformed grants. Original civil provider bytes remain selected. | Native final-05 assembly and metadata admission passed per owner report. Complete pinned handoff confirms Watch118/125 and Wave105/111 internal files unchanged, Wave44/45 SD files unchanged, and all unrelated authority unchanged. No saved-data migration or physical RF/haptic/audio/deep-reset claim. |

## Current qualification

Cloud evidence is under /workspace/lora-qualification. Native final-05 evidence is separately owner-reported below; those receipt files were not read in this cloud lane. Historical failures remain retained.

- evidence/session and evidence/presenter: normal/sanitized public source qualification.
- private-evidence-text-bounds: strict compile and29 root cases per mode.
- private-target-evidence and inventory-v1-target: canonical/repeat/three identical selections and import/export validation.
- graph-v14-normal / graph-v14-sanitized:40 actual Runtime/Graph cases each.
- resident-v15-normal / resident-v15-sanitized:46 cases each; narrower WATCH_MOTION_WAKE/tagged visual-alarm profile.
- full-home-v3-{watch,wave}-{normal,sanitized}:46 cases each, original selected full Home build flags and legacy alarmAPI1.
- selected-touch-v4-{graph,watch,wave}-{normal,sanitized}:40/46/46 cases each, bb0 production providers and compact touch-only input semantics.
- panel-civil-v3-wave-{normal,sanitized}:46 cases each at pinned Wave410x502; full automatic+serial Home profile. Final fixtures use geometry-correct raw touches and monotonically advancing RTC with30/90-second alarm deadlines.
- panel-civil-v3-civil-{normal,sanitized}:20 cases each; production civil provider with simulated RTC endpoint, summer/winter/year-rollover stamps in model/history/detail, invalid time, and actual Runtime authority loss with terminal no-I/O retention.
- product-x4-v1 and product-x4-v1-admission-{normal,sanitized}-absolute: additive LoRa row; original X4 Home and unrelated bytes/authority preserved; metadata admission only.
- radio-schema-v1-tests.json:32 schema/projection regressions against native-verbatim WatchAPI2/instance11 and Wave absence. radio-binding-v2-{normal,sanitized}:6 cases each through actual Runtime/Graph with authorized radio instance11 and unchanged unique-match app request0. memory-delta-v1-tests.json:8 archive regressions. Delta materialization verified; native final-02 SIGKILL cause remains unproven, final-03 radio-presence guard failure preserved.

## Inherited provider provenance

Pinned Watchab9ff339 and Wave07cb6454 docs/t046/evidence/assembly-report.json identify exact installed scene/text hashes, Systemc88523a0bf72ff6b3f159a66bb7e504c86e1b41a, RuntimeSDKa91edf3fada6a559ec107186dd163d0c8da10d29, GCC8.4.0 and objcopy compaction. Recorded16scene+2text source hashes and21consumed provider/canvas/time sources match testedbb0. Our GCC15 rebuilds differ; no fresh binary equality is claimed or required here.

Civil trace is bounded and incomplete: Wave's pinned appdata-candidate/software-checkpoint.json records System1aaf53e2, Runtime815013e4 and exact source/command, but outputce015945 (4632bytes) differs from installed20fcda30 (4396bytes); no connecting transformation receipt was found. Watch's pinned1.1.6 recovery retains e9158f05 (31656bytes) from1.1.5; its c0d821 modernSystem recipe does not establish that inherited ELF's original build. The civil source implementations at those named pins match testedbb0. Preserve both original civil binaries. Missing documentary linkage is disclosed; no silent provider replacement and no installed-binary execution claim.

## Reproduction

Use the final overlay packet on a separate, verified public Candidate586593 base, with exact public-pins from archive-adapter-v1. Existing clean_candidate_base.py supports verified archive bytes without fabricated Git metadata. The complete source inventory in the final packet binds added/changed files and every Candidate source byte to its public base or private overlay hash. No artifacts or credentials are included.

From Candidate, choose a fresh output path for every command:

```sh
python3 scripts/test_lora_scene_session.py --output ../fresh/session
python3 scripts/test_lora_scene_presenter.py --system-apps ../System --output ../fresh/presenter
python3 scripts/check_lora_scene_source.py --runtime ../Runtime --system ../System --output ../fresh/compile
python3 scripts/test_lora_scene.py --runtime ../Runtime --system ../System --output ../fresh/root
python3 scripts/test_lora_scene.py --runtime ../Runtime --system ../System --sanitize --output ../fresh/root-san
NATIVE_APP_CC=../toolchain/xtensa-esp-elf/bin/xtensa-esp32s3-elf-gcc python3 scripts/build_shared_scene_apps.py --runtime ../Runtime --system-apps ../System --app lora_messages --output ../fresh/inventory
python3 scripts/test_lora_panel_civil.py --runtime ../Runtime --system ../System --providers ../HomeSystem --watch ../Watch --home-system ../HomeSystem --home-utilities ../HomeUtilities --profile wave --scene-profile color-dynamic --width 410 --height 502 --output ../fresh/panel
python3 scripts/test_lora_panel_civil.py --runtime ../Runtime --system ../System --providers ../HomeSystem --watch ../Watch --home-system ../HomeSystem --home-utilities ../HomeUtilities --profile wave --scene-profile color-dynamic --width 410 --height 502 --civil-case all --case send --case receive-history --output ../fresh/civil
```

Repeat panel/civil commands with --sanitize and separate outputs. Exact earlier Graph/Home commands and dependency hashes are retained in their evidence directories. The included lora-shared-reconstruction.yml is the historical prepared public CI recipe, not an executed CI pass or full-matrix runner.

Native execution remains native_lora_entry.py with verified CPU-specific compiler, exact four input ZIPs, verified public source aliases and all four owned Home first/repeat paths. It rebuilds/gates canonical LoRa before product projection and native metadata admission. Use a new output directory and retain final-01/02/03/04 failures. Native source gates remain the reviewed557 files plus18 adapters; new qualification-only files are separately bound by the final source packet.

Public Git publication is refused and stopped pending the separate Android resolution. Physical testing, flashing and deployment have not occurred. Original task remains incomplete pending owner reconciliation.

## Final-04 correction

The prior radio-delta-v1 guard incorrectly imposed provider instance0 and was rejected by native final-04 before assembly. That avoidable regression is recorded against existing waste evidence (radio-instance-regression-waste revision694ceee05e22730d645665e1cd61fcab8a6d9bcec3cf5a61160a54c698ecfb1e); no new task or points. The corrected helper validates one exact radio grant schema (API2, integer instance0..INT32_MAX), preserves the original instance, rejects duplicate/conflicting/malformed rows, and relies on the assembler's pinned whole-artifact SHA before decoding for input identity. No universal instance constant or new grant is introduced.

Actual Runtime0052 acquire semantics treat request0 as a unique authorized match. The unchanged controller successfully sends and receives via a hardware.device-backed external radio fixture at11, and handles radio absence, in6 normal+6 sanitized cases. Initial binding-v1 metadata rejection was a fixture missing hardware.device, before app execution; retained. No physical radio driver or installed target ELF was executed.

Apply reviewed radio-schema-v2 delta to existing native root; it preserves the canonical gate and source hashes. final-source-overlay-v2 supersedes final-source-overlay-v1 for source reconstruction. Native final-05 replay passed per the owner-reported evidence below; no new local replay was run for this documentation update.

## Native final-05 — owner-reported evidence

The coordinating parent reports that the native owner independently read exact final-05 receipts. Both Watch and Wave assembly lanes passed, canonical LoRa SHA256 b55ba8b0fa7036c889c81cd32c88e76d17b2d54b790bd10688934f966c3dc46b unchanged. All eight Watch/Wave before/after metadata preparation rows passed normal and ASan/UBSan. Every row reported hardware_calls=0 and storage_calls=0; Watch i2s_tables=2, Wave i2s_tables=1.

Native evidence root: /workspace/shared/lora-native-20261101/evidence/native-products-final-05. Receipt labels below are those supplied by the parent; no unreported filenames are inferred.

| Receipt label | Bytes | SHA256 |
|---|---:|---|
| entry-result | 240 | c28bf64df362ea542c327c82c9b711b5e66e0e10247e20cdf792d211cdd8246a |
| assembly | 20767 | 109086cfd72528d78f8c0e778ac92e726dfa6f3157d470b5d3ee2ca2544c1347 |
| normal | 174614 | 0869aae12017eef568a58a475ac489404890b0d9cdbe95f338dfdf8750f9e929 |
| sanitized | 174881 | bc971b7a720aa47bcf0ddba536637436224ad15d011c0289329badd7719c2862 |

Attribution: native-owner report relayed by parent thread01a18d20-43e1-769e-90d2-b80352f69018, not locally read native receipts. The complete preservation handoff has now been read from its exact Humus revision below; original native receipt files remain on the native desktop. These product assembly/metadata results do not replace the separately qualified cloud Runtime/Graph/Home lifecycle evidence or imply execution of installed target ELFs.

This is a separately checkpointed documentation-only delta atop final-source-overlay-v2. The immutable qualified source packet, production code, native source manifest, controller and ELF are not replaced. Public Git publication remains refused/stopped. Physical testing, flashing and deployment were not performed. Original task completion remains with its owner; no duplicate task or point credit.

## Complete native preservation reconciliation

Directly read Humus personal/main lora-reconstruction-20261101/native-products-final-1158, revision 995509329edb4778c90b62b2721005b06faa9f4c16b8633e563bbc9aac027b73, snapshot 88feb625f18818d1ef246560c9f169668834e449ec59e9037765f9280142ef3d. This is the pinned native-owner record, not local access to native artifact files or a new execution.

- watch: 125 store files, 118 unchanged; allowed changes: boot.json, default.elf, default.json, lora_messages.elf, lora_messages.json, rtc/driver.elf, rtc/manifest.json. Store SHA256 3eb4d4f1a6efad26d6f3f8c93d1e264a2962018ad570c65df6a3272088effa2b; independent roundtrip verified. SD 0/0 unchanged.
- wave: 111 store files, 105 unchanged; allowed changes: boot.json, default.elf, default.json, lora_messages.json, rtc/driver.elf, rtc/manifest.json. Store SHA256 da244051048477aca4868372298e6a1350bfd54d521aeaa4016b05b825366e7d; independent roundtrip verified. SD 44/45 unchanged.

Both stored LoRa ELFs equal b55ba8b0fa7036c889c81cd32c88e76d17b2d54b790bd10688934f966c3dc46b. Watch radioAPI2/instance11 and Wave absence preserved; all unrelated authority unchanged. Wave SD only replaces Apps/lora_messages/lora_messages.elf. Original installed scene/text/civil identities remain selected. Home Watch0.25.1 automatic and Wave0.25.2 automatic+serial are validated prior first/repeat package reuse, not fresh Home builds. All eight metadata admission rows passed normal/sanitized, hardware_calls0/storage_calls0; Watch i2s_tables2, Wave1.

| Native receipt path | Bytes | SHA256 |
|---|---:|---|
| admission-normal/receipt.json | 174614 | 0869aae12017eef568a58a475ac489404890b0d9cdbe95f338dfdf8750f9e929 |
| admission-sanitized/receipt.json | 174881 | bc971b7a720aa47bcf0ddba536637436224ad15d011c0289329badd7719c2862 |
| canonical/packages.json | 46934 | b25dfe467a07a63deff8b57e98522620728d5d26d64d66cd3f249c103006e332 |
| commands.json | 2949 | bd0f055a7b1bf038f62357a751e436697bd5736fa6eb4e777128992d87310d3f |
| entry-result.json | 240 | c28bf64df362ea542c327c82c9b711b5e66e0e10247e20cdf792d211cdd8246a |
| home-reuse.json | 2268 | 00869b2e8b68670f3f9160eebaba69d0b688485ed11230f264ffcaa64b7e75e4 |
| products/assembly.json | 20767 | 109086cfd72528d78f8c0e778ac92e726dfa6f3157d470b5d3ee2ca2544c1347 |

The native entry `pending separate LoRa-focused extension` and assembly `pending separate execution` markers are retained unchanged and scoped to that runner. Separate completed cloud production Runtime/Graph/Home lifecycle receipts reconcile the software qualification boundary:

| Cloud receipt | Cases | SHA256 |
|---|---:|---|
| selected-touch-v4-watch-normal/receipt.json | 46 | bc7c587b70bc62898e6ffc7c1a01189afc465f2d7da5a800646c7d1c52764e82 |
| selected-touch-v4-watch-sanitized/receipt.json | 46 | 200d0ea651ad102dddc1f4c8156bad4a3e84bfa19be37bafe367b33d1d131416 |
| selected-touch-v4-wave-normal/receipt.json | 46 | bc7c587b70bc62898e6ffc7c1a01189afc465f2d7da5a800646c7d1c52764e82 |
| selected-touch-v4-wave-sanitized/receipt.json | 46 | f8d337b1a541f730de96d376a8b08f387fbd8979296a23bd194b2c6a802dc107 |
| panel-civil-v3-wave-normal/receipt.json | 46 | 54b1f225b5790221ff986cef3bfbca04098d20330bcfb03ffd24fbd6c702d53a |
| panel-civil-v3-wave-sanitized/receipt.json | 46 | a0ad579e223728ed14ca1ce09778a17eb1ba8151aa60ac04dd1fea80ae21e46a |

These six receipts each contain46 passing cases, selected full Home flags and bb0 production providers; panel suites use Wave410x502. External hardware/radio/storage endpoints remain fixtures. This is source-backed lifecycle proof, not installed target/provider ELF execution or device testing. Native preservation handoff is complete. Publication remains refused/stopped; physical testing, flashing and deployment were not performed. Task disposition remains with the owner. Qualified source packet and original evidence remain unchanged.

## Publication status — 2026-11-01

The prior publication hold has been resolved, and owning publication is now in progress. Earlier refusal statements above are historical checkpoints; their evidence remains retained. This execution lane prepares the delivery payload only and has performed no Git mutation. Publication success is not yet claimed. Physical testing, flashing and deployment remain unperformed.
