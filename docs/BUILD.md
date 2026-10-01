# Independent utility builds

No Reader checkout is required. Python 3.11+, a host C11 compiler, and PlatformIO 6.1.19 with `espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5` are sufficient.

```sh
python -m unittest discover -s tests -v
python scripts/test_apps.py
python scripts/build_all_apps.py
python scripts/check_release_parity.py
```

Set PLATFORMIO_CORE_DIR or NATIVE_APP_CC for an existing pinned compiler. Outputs are development-only ELF/JSON integrity sidecars and evidence under dist/apps; this pipeline never publishes a release/catalog or installs packages.

## Immutable inputs and compatibility profile

`sdk/baseline.json` hashes source-owned headers, bounded compiler helpers, ELF validator, host fixtures and public firmware exports. The main snapshot is Reader a5e2db59077cc889079668dc9cd7428b08bc32a1. Build scripts are audited standalone adaptations, not claimed byte-identical upstream copies.

Published versions retain full symbol tables. GPS 1.0.0 and Battery 1.0.1 reproduce against the current pinned SDK. LoRa 1.0.0 additionally uses its release-era three headers from 3af3c24f3c02e33af12019852393936c82367226. New declarations in T5AppApi.h change GCC's local symbol numbering: current headers differ only at six bytes in `.strtab` (`digits$2452`/`ping$2478` versus `$2506`/`$2532`). Using the historical profile restores the immutable release digest without altering code or ABI. Never rename symbols or overwrite historical assets to mask drift.

All three actual built ELF byte hashes and sizes equal GitHub's published asset digests in `sdk/release-baseline.json`; `docs/release-parity.json` records results. Therefore app versions do not change. Any changed distributable or distribution format requires a numeric version increment beyond the accepted published lineage before release, even if only symbol/packaging bytes changed.

## Checks and limits

10 Python tests cover immutable inputs, exact inventory, manifests, path/collision rejection, strong/weak import rejection, integrity stamps, source-aware conflict classification and actual-byte parity. Three upstream C fixtures cover GPS searching/fix/redraw/stop, LoRa configuration/RX/Ping/arbitration callbacks, and Battery detailed telemetry/update/navigation.

Three additional C fixtures cover missing APIs/functions; GPS unsupported/start failure/stop; LoRa unsupported Confirm cannot transmit, 255-byte formatting and empty-packet preservation; Battery failed startup and failed refresh preserving the prior screen. Fixtures compile the real app sources against simulated APIs. They do not prove real GPS reception, RF transmission, battery validity, concurrency, hotplug or hardware lifetimes.

Each cross-built ELF passes entrypoint/import checks and the source-owned structural validator. CI uploads exact-head development artifacts and build-evidence.json (compiler, SDK, repository SHA, payload hashes). Evidence produced from a dirty local tree is explicitly marked; CI is the authoritative clean-head record.

## Source-aware sync

```sh
python scripts/check_baseline.py --reader /read-only/Reader --ref COMMIT --output audit.json
```

This reads Git objects only and never modifies Reader or app sources. Review unchanged, upstream-only, external-only, converged and conflict classifications; preserve external fixes and reconcile conflicts rather than copying blindly. Rebaseline SDK/export inputs only after review. A source audit is not artifact or runtime parity.
