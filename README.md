# RiscRTE Utilities

Independent source repository for optional RiscRTE hardware, location, and connectivity utilities. During migration, `michaelrolphone-cmyk/T5S3-Reader` is treated as a strictly read-only upstream source.

## Scope

Utilities are useful end-user hardware/connectivity tools that are not required for foundational system operation and are not primarily MCU development/debug tools.

## Application documentation

- [GPS](docs/apps/gps.md) — GNSS receiver status, coordinates, satellite count, and driver state.
- [LoRa](docs/apps/lora.md) — SX1262 raw packet monitoring, signal information, counters, and test transmit.
- [Battery Status](docs/apps/battery.md) — firmware-owned battery, gauge, charger, capacity, current, voltage, and temperature telemetry.

## Repository tree

```text
Apps/
  battery.c
  battery.json
  gps.c
  gps.json
  lora.c
  lora.json

docs/
  apps/
    battery.md
    gps.md
    lora.md

utilities-manifest.json
```

## Documentation and parity policy

Every migrated utility has a dedicated Markdown page derived from its actual source, manifest, interfaces, and current behavior. Interfaces, capabilities, configuration, limits, dependencies, failure states, and workflows are documented where the implementation establishes them. Future or hypothetical behavior is not added as specification.

A utility is not parity-complete until source, manifest/version, build/release behavior, and documentation match the approved upstream utility set.

## Independent builds and readiness

- [Pinned standalone build and focused test coverage](docs/BUILD.md)
- [Per-app readiness and safe Reader removal conditions](docs/MIGRATION_READINESS.md)
- [Source drift audit](docs/source-drift.json) and [published ELF byte evidence](docs/release-parity.json)

CI produces development artifacts only. Current-master parity is distinct from prospective U1 package changes and hardware qualification.

## Licenses

See [project MIT license](LICENSE) and the preserved [Apache-2.0 license for vendored ELF loader files](lib/elf_loader/license.txt). Original notices remain intact.

## Shared daily tools for the minimal runtime

Calculator 0.1.0 and Stopwatch 0.1.0 are original shared Utilities apps. Their
explicit `portable_apps` inventory uses the separately pinned portable client
profile, leaving the three Reader migration/release-parity apps unchanged.
The same source is built into the Watch deployment; there is no Watch app fork.

- [Calculator](docs/apps/calculator.md): four operations, decimal entry, sign,
  delete, repeated equals and bounded deterministic six-place arithmetic.
- [Stopwatch](docs/apps/stopwatch.md): monotonic fractional timing while open,
  persisted state and RTC-based approximate recovery after app changes or reset.

`python scripts/test_daily_apps.py --system-apps /clean/pinned/system-apps`
exercises real app source plus failure/property tests under normal and sanitizer
builds. `python scripts/build_portable_apps.py --system-apps /clean/pinned/system-apps`
builds target ELFs against exact shared source
`fd6bed09fe6716c8d3a75c0538c4f232f3cc388e`. Results are development evidence in
`dist/portable-apps`, not release/install catalogs or hardware qualification.
Waterfall 0.1.1 is another shared catalog app on the same portable profile.
It is not the watch default and it does not replace Clock. It acquires
`radio.iq` API 1 and draws one scrolled row per 256-pair burst. Receiver
bring-up and the SRAM dump stay in the `s3-radio-iq-v1` driver. There is no
transmitter and no FPGA stream. See [Waterfall](docs/apps/waterfall.md).

No new firmware ABI, chip driver, sound, alarm wake, filesystem or timezone
policy is introduced by this subset. The Watch's existing caller return and
crown Back are deployment-owned. Standalone generic builds require an authorized
RGB565 display/touch implementation; Stopwatch also requires RTC and a scoped
key-value grant. Full merged reflashing may erase that saved state.
