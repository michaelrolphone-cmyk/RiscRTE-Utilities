# RiscRTE Utilities

Independent source repository for optional RiscRTE hardware, location, and connectivity utilities. During migration, `michaelrolphone-cmyk/T5S3-Reader` is treated as a strictly read-only upstream source.

## Scope

Utilities are useful end-user hardware/connectivity tools that are not required for foundational system operation and are not primarily MCU development/debug tools.

## Application documentation

- [GPS](docs/apps/gps.md) — GNSS receiver status, coordinates, satellite count, and driver state.
- [LoRa](docs/apps/lora.md) — SX1262 raw packet monitoring, signal information, counters, and test transmit.

## Repository tree

```text
Apps/
  gps.c
  gps.json
  lora.c
  lora.json

docs/
  apps/
    gps.md
    lora.md

utilities-manifest.json
```

## Documentation and parity policy

Every migrated utility has a dedicated Markdown page derived from its actual source, manifest, interfaces, and current behavior. Interfaces, capabilities, configuration, limits, dependencies, failure states, and workflows are documented where the implementation establishes them. Future or hypothetical behavior is not added as specification.

A utility is not parity-complete until source, manifest/version, build/release behavior, and documentation match the approved upstream utility set.
