# Battery Status

## Purpose and classification

Battery Status is a read-only RiscRTE hardware telemetry utility. It presents the firmware-owned battery snapshot and does not initialize, configure, or directly access the battery gauge or charger hardware.

This repository classifies it as a **utility** because it is optional hardware telemetry rather than a foundational first-use system workflow.

## Manifest

- Display name: **Battery Status**
- Version: **1.0.2**
- Minimum firmware: **1.1.18**
- ELF: `battery.elf`
- Icon: `solid:f240`
- Categories: `System`, `Hardware`

The upstream manifest declares no optional provider capability.

## Host interfaces

### T5AppApi

Used for input polling. The app exits when polling fails, an exit is requested, or Back is pressed.

### T5BatteryApi

Used through `read()` to obtain a `t5_battery_state_t` snapshot. Battery Status does not communicate with BQ27220 or BQ25896 directly; firmware supplies normalized telemetry for those devices.

### T5UiApi

Used for list rendering, touch hit testing, and previous/next selection helpers.

## User interface and navigation

The app renders a list titled **Battery Status**. Confirm requests a fresh battery snapshot. Up/Left move selection backward, Down/Right move forward, and a touch hit selects the tapped row. Back exits.

The implementation supports up to **28 rows** and stores each formatted row value in a **72-byte** buffer.

## Summary fields

The list always begins with operating mode, state of charge, and voltage. When detailed telemetry is unavailable it also shows board name and gauge state.

Voltage uses gauge voltage when a gauge read is valid; otherwise it uses the battery-voltage field.

## Detailed telemetry

When `detailed_telemetry` is true, the source renders:

- average and instantaneous current,
- state of health,
- remaining and full capacity,
- configured/profile battery capacity,
- temperature converted from deci-kelvin to Celsius,
- VBUS input state,
- BQ27220 online/read state,
- gauge state,
- gauge charge voltage and taper current,
- battery-full, gauging-full, taper, and charge-inhibit flags,
- BQ25896 online/read state,
- VBUS, system, and battery voltages,
- charge regulation voltage,
- configured charge current,
- precharge and termination current,
- charger ADC current,
- charger charge-status text,
- and whether charging is enabled.

The application formats these values for display only and does not modify charging policy or battery configuration.

## Footer/status behavior

If battery management is unavailable, the footer reports that condition.

With basic telemetry, the footer contains board name, charge percentage, and an indication that only basic ADC telemetry is available.

With detailed telemetry, it contains board name, charge percentage, USB input state, and average current.

## Failure behavior

If a required API is unavailable, required function pointers are missing, or the first battery read fails, `app_main` returns without entering the UI loop.

If a refresh requested with Confirm fails, the application keeps the previously rendered state.

## Persistence and storage

The source does not persist application state and does not read or write files.

## Hardware ownership boundary

Gauge/charger discovery, initialization, register access, charging configuration, and telemetry acquisition are firmware responsibilities. Battery Status only reads the normalized `T5BatteryApi` state.

## Source

- `Apps/battery.c`
- `Apps/battery.json`

Authoritative current-master blobs for version 1.0.2:

- source: `10c8def52738cae0059eda4aeb68699a3165328b`
- manifest: `c8f3483b28e8d423322ac5e5d56f6c45063b8508`
- Reader source commit: `3d9bc4f373679f5ae8dd184db6a8d0afa5a40231`

## Current release identity

Reader release `app-battery-v1.0.2` publishes `application-battery-1.0.2-xtensa-esp32s3.rte.zip` (SHA-256 `5d83032114fb0ab5335b249b890ce5ad36b4ba38b838054f5b68f3bc72f70d41`, 7,304 bytes). Its nested `battery.elf` is 6,284 bytes with SHA-256 `039d0e6a351955b48f53bf74a4b0d14855adab9479a07be0f07a993b7ffd05cf`. The ZIP digest and inner package metadata were checked against Reader workflow artifact 11209466823.

## Independent-build baseline

Source remains unchanged; the manifest now matches Reader `3d9bc4f373679f5ae8dd184db6a8d0afa5a40231`. See [build evidence](../BUILD.md), [readiness and removal criteria](../MIGRATION_READINESS.md), [source audit](../source-drift.json) and [published-byte comparison](../release-parity.json). Host fixtures exercise actual app C with simulated APIs; they do not establish hardware/runtime qualification.

## Snapshot and hardware boundaries

Snapshots do not auto-refresh. Confirm requests a new snapshot; navigation redraws the in-memory state. Failed initial read exits; failed Confirm read leaves the old screen immediately without an error notice, but the provider may have mutated the in-memory state, which a later navigation redraw can show. Detailed telemetry availability does not guarantee every numeric field is valid. Gauge/charger diagnostic rows distinguish missing/read-error devices. The app only reads telemetry; the current host bridge calls battery-management initialization, so the complete hardware path is not promised side-effect-free.
