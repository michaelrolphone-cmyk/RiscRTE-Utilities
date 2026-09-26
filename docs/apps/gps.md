# GPS

## Purpose

GPS is a read-only GNSS status/position utility. Its manifest identifies `gps.elf`, version **1.0.0**, minimum firmware **1.1.14**, categories `GNSS` and `Utilities`.

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
