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
No new firmware ABI, chip driver, sound, alarm wake, filesystem or timezone
policy is introduced by this subset. The Watch's existing caller return and
crown Back are deployment-owned. Standalone generic builds require an authorized
RGB565 display/touch implementation; Stopwatch also requires RTC and a scoped
key-value grant. Full merged reflashing may erase that saved state.

## Staged Alarm and Countdown work

[Alarms](docs/apps/alarms.md), [Countdown](docs/apps/countdown.md) and the
[ordinary alarm service](docs/ALARM_SERVICE.md) use a separate development
inventory and build. They preserve the delivered Calculator/Stopwatch profile.
The service and writers are implemented and tested, but genuine output backend,
common foreground preservation and owned low-power wake integration must be
finished before a working Watch image is delivered. This repository does not
claim awake-only alarms satisfy that requirement.

## Recurring Points service extension

[Points in Time](docs/POINTS_IN_TIME.md) adds an explicitly opted-in0.2.1 build of
the same ordinary Alarm singleton. Its fixed eight-point catalog and durable
start/end ledger each fit64 bytes, use two extra explicit key bindings, and keep
alarm.service@1 unchanged. The Points editor belongs to Productivity; the eight
schedule faces and deployment belong to Watch. Existing non-opted-in0.1.0 builds
retain their five-key authority and one-shot behavior. New source fixtures and
target ELF checks are part of CI; hardware qualification is separate.

## Shared Audio Tools

[Frequency Generator](docs/apps/frequency_generator.md) is a separate explicit
`audio_apps` development profile. It uses the existing speaker capability and
portable alarm-aware UI, with bounded synthesis, explicit Start, low initial
level and checked cleanup. Exact shared audio lifecycle and Runtime prerequisites
are required; this does not change the existing Reader migration cohort or
declare an untested Watch installation ready.

[Audio Spectrum](docs/apps/audio_spectrum.md) adds the second Audio Tools app: a
user-started microphone spectrum and scrolling spectrogram, sharing the existing
input provider and foreground lifecycle. It provides explicitly captured, labeled
room/event power signatures with bounded durable records, optional background
subtraction and conservative room matching. The MONITOR tab identifies the
current saved room and ranks detected events/frequency labels by detector
confidence with canonical amplitude and uncertain speech activity. Requested
live monitoring continues across idle deadlines and transient input waits, and
resumes after safe alarm/storage pauses. Explicit Stop, Freeze and Exit close
the microphone. Raw display remains the default.

## LoRa Messages

[LoRa Messages](docs/apps/lora_messages.md) adds an original portable Nova7 app
using the append-only selector in `radio.lora@2`. It offers an explicit
433/868/915 MHz or 2.4 GHz radio picker, compatible RF setup, the
standard Points keyboard, manual raw broadcast send/listen, bounded session
history and honest transmit/error status. No RF values, auto-transmission or
physical hardware qualification are inferred. The historical Reader LoRa
utility and its published-byte parity remain unchanged.

## Temporal audio development increment

[Audio Spectrum 0.4.3](docs/apps/spectrum_temporal.md) adds explicit positive and
negative short-event examples, background-relative labels, temporal matching and
bounded frequency-shift tolerance. Its app-data backend is pinned to published Runtime 0.1.32; the Watch repository
owns the final ABI2 image assembly and physical qualification. No new layout is
formatted or installed by this repo.
