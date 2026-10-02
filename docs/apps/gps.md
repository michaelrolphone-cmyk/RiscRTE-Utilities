# GPS

## Purpose

GPS is a read-only GNSS status/position utility. Its manifest identifies `gps.elf`, version **1.0.1**, minimum firmware **1.1.14**, categories `GNSS` and `Utilities`.

## RiscRTE interfaces

The app uses:

- `T5AppApi`
- `T5GpsApi`
- `T5UiApi`

It requires app polling and timing, GPS `supported`, `start`, `stop`, and `read`, plus shared list rendering.

The application does not parse NMEA or directly access a UART. Receiver detection, baud handling, GNSS parsing, and hardware ownership are provided by the GPS service/driver exposed through `T5GpsApi`.

## User-visible data

The screen shows:

- GPS status: Fix, Searching, Off, or Unsupported
- Latitude to seven decimal places when a fix is valid
- Longitude to seven decimal places when a fix is valid
- Satellite count

The footer additionally reports one of:

- GPS driver unavailable
- GPS driver could not start
- Receiver probing with current baud
- Receiver detected / waiting for satellite fix
- Fix age in milliseconds and active baud rate

## Lifecycle

On start, the app checks whether GPS is supported. If supported it starts the GPS service and reads initial state. It refreshes GPS state continuously but redraws the screen no more often than every three seconds.

Back, application exit, or poll termination leaves the loop. The app then calls `gps->stop()`.

## Failure handling

If the GPS interface reports unsupported, the UI tells the user to install the GPS driver package. A failed start is shown as Off with a driver/package diagnostic. Until a valid fix is available, latitude and longitude render as `--`.

## Source

- `Apps/gps.c`
- `Apps/gps.json`

## Current release identity

Reader release `app-gps-v1.0.1` publishes `application-gps-1.0.1-xtensa-esp32s3.rte.zip` (SHA-256 `32f3e8d34fe3d40576686992a58d2538278f8ffb40ec5de9c0561a93b4158cb4`, 4,776 bytes). Its nested `gps.elf` is 3,804 bytes with SHA-256 `a85520bd944c72420fedd898700980c0602ce2af4fccb8700d6f3c511cfc86be`. The ZIP digest and inner package metadata were checked against Reader workflow artifact 11209466823.

## Independent-build baseline

Source remains unchanged; the manifest now matches Reader `3722a3f44a3294ba5e8adab830807a2523df3b03`. See [build evidence](../BUILD.md), [readiness and removal criteria](../MIGRATION_READINESS.md), [source audit](../source-drift.json) and [published-byte comparison](../release-parity.json). Host fixtures exercise actual app C with simulated APIs; they do not establish hardware/runtime qualification.

## Limits and failed operations

Missing required APIs/function pointers cause a silent return. `read()` results are ignored: read failure has no explicit diagnostic or guaranteed clearing of stale data. Input polling waits 50 ms; redraw is at least three seconds apart. Unsupported and start-failure guidance does not prove installed driver permission, leases, receiver connectivity or provider startup. No map, navigation, recording, export, files, persistence or altitude/speed/HDOP display is implemented.
