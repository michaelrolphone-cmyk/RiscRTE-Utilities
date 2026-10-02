# Independent utility builds

No Reader checkout is required. Python 3.11+, a host C11 compiler, and PlatformIO 6.1.19 with `espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5` are sufficient.

```sh
python -m unittest discover -s tests -v
python scripts/test_apps.py
python scripts/build_all_apps.py
python scripts/check_release_parity.py
```

Set PLATFORMIO_CORE_DIR or NATIVE_APP_CC for an existing pinned compiler. Outputs are development-only ELF/JSON integrity sidecars and evidence under dist/apps; this pipeline never publishes a release/catalog or installs packages.

## Immutable SDK inputs and LoRa release profiles

`sdk/baseline.json` locks the independent SDK snapshot, bounded compiler helpers, ELF validator and host fixtures. The shared pinned snapshot remains Reader `a5e2db59077cc889079668dc9cd7428b08bc32a1`; this refresh does not sweep later unrelated SDK/tooling changes into Utilities. LoRa 1.0.0 retains its historical three-header profile. A separate LoRa 1.0.1 profile locks the exact public headers from Reader release source `f7f006f78bf1f83c28f3ce05728b8973e895956b`; these package source and header blobs are unchanged at current Reader master `3d9bc4f373679f5ae8dd184db6a8d0afa5a40231`. Each profile is content-locked and version-selected by the app manifest.

## Published artifact identity and build policy

Current Reader versions are GPS 1.0.1, LoRa 1.0.1 and Battery Status 1.0.2. Each is published as an RTE ZIP; the release baseline records the ZIP digest/size and the nested `.package.json`-verified ELF digest/size. The utilities pipeline compares the actual development ELF with that published ELF payload. It does not create or migrate an install package.

These current versions use Reader's `--strip-unneeded` release policy. The build applies that policy only to the audited version tuples (GPS 1.0.1, LoRa 1.0.1 and Battery Status 1.0.2); historical versions keep their previous full-symbol build behavior. The target CI must reproduce all three published payload hashes before the parity PR can merge. Future versions require source/profile review and an explicit build-policy update. Never rename symbols or change package bytes under an existing version to conceal drift.

## Checks and limits

Python tests cover the immutable SDK lock, exact inventory, manifests, path/collision rejection, strong/weak import rejection, integrity stamps, source-aware conflict classification and current release provenance. Three app host fixtures exercise the real GPS, LoRa and Battery Status source against simulated APIs; the LoRa fixture runs both normal display and denied display-arbitration scenarios. Additional failure fixtures cover missing interfaces, unsupported/start failures, telemetry and empty/oversize packet behavior.

Each cross-built ELF passes entrypoint/import checks and the source-owned structural validator. CI uploads exact-head development artifacts and build-evidence.json (compiler, SDK, repository SHA, payload hashes). Evidence produced from a dirty local tree is explicitly marked; CI is the authoritative clean-head record. Fixtures and structural validation do not prove real GPS reception, RF transmission, battery validity, concurrency, hotplug or hardware lifetimes.

## Source-aware sync

```sh
python scripts/check_baseline.py --reader /read-only/Reader --ref COMMIT --output audit.json
```

This reads Git objects only and never modifies Reader or app sources. Review unchanged, upstream-only, external-only, converged and conflict classifications; preserve external fixes and reconcile conflicts rather than copying blindly. Rebaseline SDK/export inputs only after review. A source audit is not artifact or runtime parity.
