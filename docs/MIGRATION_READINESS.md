# Utilities migration readiness

## Current Reader master checkpoint

Reader master: `3d9bc4f373679f5ae8dd184db6a8d0afa5a40231`. Utilities target base: `4343b808f762b67bb14c39fb50f28d32819af07e`. This sync copies versions already published by Reader: GPS 1.0.1, LoRa 1.0.1 and Battery Status 1.0.2. No new version is invented. Source, manifest, release ZIP digest and embedded ELF identity are recorded per package in [source drift](source-drift.json), [release provenance](release-parity.json), and [the release baseline](../sdk/release-baseline.json).

| App | Current release / minimum firmware | Current-master change | Runtime evidence still needed |
| --- | --- | --- | --- |
| [GPS](apps/gps.md) | 1.0.1 / 1.1.14 | Manifest-only release update; source unchanged. Reader RTE asset and nested ELF identity verified. Utilities exact-head build and payload-byte comparison are a pre-merge CI gate. | Real receiver/driver permission, lease, fix, loss and exit |
| [LoRa](apps/lora.md) | 1.0.1 / 1.1.15 | Copies Reader's current-source arbitration-failure display fix and matching API headers; uses the API headers from the published build; the exact ELF byte comparison remains a pre-merge CI gate. Exact-head build and payload-byte comparison are a pre-merge CI gate. | Actual RX/TX, unavailable radio, arbitration gaps and recovery |
| [Battery Status](apps/battery.md) | 1.0.2 / 1.1.18 | Manifest-only release update; source unchanged. Reader RTE asset and nested ELF identity verified. Utilities exact-head build and payload-byte comparison are a pre-merge CI gate. | Board/gauge/charger availability and truthful telemetry |

The published RTE ZIPs were verified against Reader release metadata and actual nested `.package.json` and ELF bytes from workflow artifact run [36965130240](https://github.com/michaelrolphone-cmyk/T5S3-Reader/actions/runs/36965130240), artifact 11209466823. The target's independent build/release-parity workflow must pass on the exact PR head before merge. This evidence does not qualify hardware behavior.

## Prospective U1 ZIP and cutover readiness

Current-master source/release parity is separate from U1 package installation or provider cutover. No U1 ZIP acceptance, install/version enforcement, installed-package continuity, live catalog change, Reader source removal, deployment or device flashing is authorized or implied here. Remaining hardware ownership, driver permissions and runtime lifetimes remain unproven.

Evidence: [build and test method](BUILD.md), [source drift](source-drift.json), [release bytes](release-parity.json), [immutable SDK and versioned LoRa profile](../sdk/baseline.json), and [maintenance claim](WORK_CLAIM.md).

## Safe Reader removal checklist (not authorization)

- Keep replacement build/toolchain/SDK inputs and source history independently available; verify all IDs, supported targets and source/manifest/docs/artifact parity
- Establish compatible external release/catalog/install/upgrade paths explicitly; Reader release scripts still hard-code its repository
- Preserve historical URLs/releases, stable IDs, numeric version lineage, rollback and unknown user data; reject same-version changed content or distribution
- Validate bounded dependency/permission/lifetime handling and real target runtime flows; host mocks and structural ELF validation are not that proof
- Remove only migrated utility app/build references once replacement coverage is established; shared host APIs, SDKs, hardware drivers and battery settings are not obsolete merely because apps moved
- Publishing, live catalog/source cutover, Reader deletion and deployment need their own authorization. This checkpoint grants none
