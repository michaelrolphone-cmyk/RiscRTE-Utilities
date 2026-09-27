# RiscRTE Utilities

Independent source repository for optional RiscRTE hardware, location, storage-diagnostic, and connectivity utilities. `michaelrolphone-cmyk/T5S3-Reader` remains strictly read-only upstream.

## Scope

Utilities are optional hardware/connectivity/location/diagnostic tools that are not required for foundational system operation and are not primarily MCU development/debug tools. Domain-specific apps such as the upstream 3D Model Viewer remain outside these four migration repositories.

## Application documentation

- [GPS](docs/apps/gps.md) — GNSS receiver status and coordinates.
- [LoRa](docs/apps/lora.md) — SX1262 packet monitoring and test transmit.
- [Battery Status](docs/apps/battery.md) — battery/gauge/charger telemetry.
- [SD Files](docs/apps/sd_list.md) — read-only paging diagnostic for `/sd`.
- [Web Server](docs/apps/web_server.md) — firmware-provider captive-portal/server status and lifecycle utility.
- [GNSS Stream Diagnostic](docs/apps/gnss_stream_diagnostic.md) — authorized `location.fix.v1` stream/backpressure/revocation diagnostic; source file still pending.

## Repository tree

```text
Apps/
  battery.c
  battery.json
  gnss_stream_diagnostic.json
  gps.c
  gps.json
  lora.c
  lora.json
  sd_list.c
  sd_list.json
  web_server.c
  web_server.json

lib/
  NativeApps/
    include/
      RiscRteLocationRecords.h

docs/
  apps/
    battery.md
    gnss_stream_diagnostic.md
    gps.md
    lora.md
    sd_list.md
    web_server.md

tools/
  validate_repository.py

utilities-manifest.json
```

## Documentation and parity policy

Every migrated utility has a dedicated Markdown page derived from actual source, manifest, interfaces, helpers, and behavior. A utility is not parity-complete until source, manifest/version, required helper/build inputs, build/release behavior, and documentation match the approved upstream set. Source/document parity does not yet imply reproduced ELF parity because the independent compatibility-header/toolchain/release pipeline is still incomplete.

GNSS Stream Diagnostic is intentionally marked partial until exact `Apps/gnss_stream_diagnostic.c` can be attached; its manifest, helper header, and source-derived documentation are present, but source parity is not complete.

## Validation tooling

`tools/validate_repository.py` checks the checked-in utilities registry against app manifests, source/document presence, README documentation links, versions, minimum firmware versions, file names, duplicate IDs, and utility classification. Entries explicitly marked with a `source-pending` migration status may omit their source file while staged migration work remains incomplete; all other registered apps must have source, manifest, and dedicated documentation present. The validator is intended to be run with `python3 tools/validate_repository.py` until repository CI is wired to execute it automatically.
