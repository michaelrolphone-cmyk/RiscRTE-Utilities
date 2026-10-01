# Utilities migration readiness

## Indexed current-master checkpoint

Reader source: `a5e2db59077cc889079668dc9cd7428b08bc32a1`. External base: `c9e992a0be59d658d4cb044308c17d2d0bfd7ec2`. All six app source/manifest inputs match (no external fixes discarded). All three independently built artifacts reproduce published bytes; no version changes, releases or live catalog writes.

| App | Version / minimum firmware | Source and independent build | Remaining runtime evidence |
| --- | --- | --- | --- |
| [GPS](apps/gps.md) | 1.0.0 / 1.1.14 | Current-master parity; published-byte identity | Real receiver/driver permission, lease, fix, loss and exit |
| [LoRa](apps/lora.md) | 1.0.0 / 1.1.15 | Current-master parity; historical-header profile restores byte identity | Actual RX/TX, unavailable radio, arbitration gaps and recovery |
| [Battery Status](apps/battery.md) | 1.0.1 / 1.1.18 | Current-master parity; published-byte identity | Board/gauge/charger availability and truthful telemetry |

Evidence: [build/test method](BUILD.md), [source drift](source-drift.json), [release bytes](release-parity.json), [immutable SDK](../sdk/baseline.json), [claim](WORK_CLAIM.md). These are implemented applications with explicitly documented limitations, not placeholders promoted to hardware-complete.

## Prospective U1, not current-master acceptance

Read-only U1 inspection at `48d8445094c478f11064f7700117b10205a2a990` found identical app C but prospective ZIP versions GPS/LoRa 1.0.1 and Battery 1.0.2. Do not copy those manifest versions into this current-master checkpoint. U1 bundle layout, install/version enforcement and BQ ownership work are distinct from this independent repository build. Configurable independent sources are subsequent ecosystem work; remaining GNSS/LoRa hardware ownership is prospective. Old firmware ZIP consumption, live index transition and installed-package continuity remain unproven here.

## Safe Reader removal checklist (not authorization)

- Keep replacement build/toolchain/SDK inputs and source history independently available; verify all IDs, supported targets and source/manifest/docs/artifact parity
- Establish compatible external release/catalog/install/upgrade paths explicitly; Reader release scripts still hard-code its repository
- Preserve historical URLs/releases, stable IDs, numeric version lineage, rollback and unknown user data; reject same-version changed content or distribution
- Validate bounded dependency/permission/lifetime handling and real target runtime flows; host mocks and structural ELF validation are not that proof
- Remove only migrated utility app/build references once replacement coverage is established; shared host APIs, SDKs, hardware drivers and battery settings are not obsolete merely because apps moved
- Publishing, live catalog/source cutover, Reader deletion and deployment need their own authorization. This checkpoint grants none
