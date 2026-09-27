# RiscRTE Utilities

Independent source repository for optional RiscRTE hardware, location, storage-diagnostic, and connectivity utilities. `michaelrolphone-cmyk/T5S3-Reader` remains strictly read-only upstream.

## Scope

Utilities are useful optional end-user hardware/connectivity/location/diagnostic tools that are not required for foundational system operation and are not primarily MCU development/debug tools. Domain-specific apps such as the upstream 3D Model Viewer remain outside these four migration repositories.

## Application documentation

- [GPS](docs/apps/gps.md) — GNSS receiver status and coordinates.
- [LoRa](docs/apps/lora.md) — SX1262 packet monitoring and test transmit.
- [Battery Status](docs/apps/battery.md) — battery/gauge/charger telemetry.
- [SD Files](docs/apps/sd_list.md) — read-only paging diagnostic for `/sd`.
- [Web Server](docs/apps/web_server.md) — firmware-provider captive-portal/server status and lifecycle utility.

## Repository tree

```text
Apps/
  battery.c
  battery.json
  gps.c
  gps.json
  lora.c
  lora.json
  sd_list.c
  sd_list.json
  web_server.c
  web_server.json

docs/
  apps/
    battery.md
    gps.md
    lora.md
    sd_list.md
    web_server.md

utilities-manifest.json
```

## Documentation and parity policy

Every migrated utility has a dedicated Markdown page derived from actual source, manifest, interfaces, and behavior. A utility is not parity-complete until source, manifest/version, build/release behavior, and documentation match the approved upstream set. Source/document parity does not yet imply reproduced ELF parity because the independent compatibility-header/toolchain/release pipeline is still incomplete.

## Approved utility backlog

GNSS Stream Diagnostic 0.1.1 is approved for this repository but not yet integrated because the current write path rejected its source/helper Git objects. Required immutable upstream identities are tracked in the migration handoff.
