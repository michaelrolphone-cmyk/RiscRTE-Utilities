# GNSS Stream Diagnostic

## Purpose and classification

GNSS Stream Diagnostic is a location-stream, authorization, queue/backpressure, and revocation diagnostic around the firmware GNSS provider. It is classified as a **utility**, not an MCU development/debug tool: it consumes an authorized location capability and does not program or debug an external microcontroller.

## Manifest

- Display name: **GNSS Stream Diagnostic**
- Version: **0.1.1**
- Minimum firmware: **1.2.26**
- ELF: `gnss_stream_diagnostic.elf`
- Icon: `solid:f05b`
- Categories: `Diagnostics`, `GNSS`, `Developer`
- Upstream source blob: `376dede4cfee7e45e752f5fb9a32b03a6dfb45f7`
- Upstream manifest blob: `117b328a82b0d1850f03604cf4a40f6bba5d1f69`
- Required helper: `lib/NativeApps/include/RiscRteLocationRecords.h`, blob `f7dab9a4cd579550fa3dd391c022cd9df8a0e633`

The manifest does not declare a provider capability entry; authorization is requested at runtime through the device API.

## Host APIs

### T5AppApi v1

Requires `poll` and `millis`.

### T5UiApi v1

Requires `render_list` and `poll_event`.

### T5DeviceApi v3

The app requires a v3 structure large enough for `t5_device_api_v3` and uses v1 `inventory`, v3 `request`, and v2 `release`.

Inventory uses a fixed 12-entry array. A receiver must report provider `gps-nmea`, UART transport, AVAILABLE state, and capability `location.position`. The app requests `T5_DEVICE_RIGHT_READ` for that capability.

### T5LocationApi v1

Requires `subscribe`, `poll`, and `unsubscribe`. Subscription consumes the granted authorization lease and yields a location subscription plus stream handle.

### RiscRTE Stream API v2

Requires `record_read`, `record_info`, and a present v1 `close` callback. After subscription the app requires schema `location.fix.v1`, READ permission, and no WRITE permission.

## location.fix.v1 wire format

`RiscRteLocationRecords.h` defines a fixed **52-byte**, version-1, little-endian record. Fixed offsets cover version, sample/fix monotonic milliseconds, flags, latitude, longitude, altitude, HDOP, speed, heading, satellite count, and reserved bytes. It defines validity flags for altitude, HDOP, speed, and heading.

The helper comment establishes that sample/fix timestamps share the same wrapping 32-bit monotonic clock and that the provider publishes only a currently valid position no more than five seconds old.

This app consumes latitude, longitude, satellite count, sample time, and fix time. It validates exact size/version, bounds latitude to ±90 and longitude to ±180, formats coordinates to seven decimal places, and computes fix age as unsigned `sample_ms - fix_ms`.

The implementation copies IEEE-754 coordinate bytes directly and uses bit-pattern range checks to avoid unsupported software double helpers in the native ELF ABI.

## UI and workflow

Rows show Stream, Records, Queue / peak, Latitude, Longitude, Satellites, and Fix age (ms).

At startup the app shows **Discovering GNSS device**. If no matching receiver exists, Confirm retries. If permission is denied or errors, the diagnostic keeps the reason visible and Confirm retries instead of returning immediately.

After authorization, the app subscribes, validates the stream, and displays **Waiting for a GNSS fix** until records arrive.

## Pause and backpressure test

Confirm toggles paused reads. While paused, the app continues calling `location->poll()`, allowing producer activity to continue. `record_info()` supplies queued, capacity, and high-water counts. If paused and queue occupancy reaches capacity, status becomes **Queue full: producer backpressured**.

When reads are active, each 250 ms poll cycle drains at most eight records. `T5_STREAM_AGAIN` means no more record is currently available. The UI rerenders every 2000 ms while active and every 500 ms while paused.

## Consent-revocation test

The Next control is labeled **Revoke** once controls are ready. It deliberately releases the authorization while leaving the issued stream handle intact, then performs one raw `record_read()`.

The expected result is `T5_STREAM_DENIED` with returned size 0, displayed as **PASS: revoked stream denied raw read**. Any other result is a FAIL verdict. After the probe, the verdict remains visible until Back and normal reading does not resume.

## Failure and cleanup

Missing APIs show a visible runtime/permission error until Back. Failed subscription releases authorization. Unexpected schema/rights unsubscribes and releases authorization. Location-poll, record-read, and record-decode errors are shown before cleanup.

Normal cleanup disables controls, rerenders, unsubscribes, and releases authorization if it was not already revoked.

## Hardware ownership boundary

The source explicitly does not import the legacy GPS API, claim the UART, or manipulate a GNSS power rail. Device discovery is passive through inventory and the location provider owns GNSS hardware access/publication.

## Storage, network, persistence

No files are read or written, no network API is used, and no state is persisted.

## Published package

Upstream release `app-gnss_stream_diagnostic-v0.1.1` publishes `gnss_stream_diagnostic.elf` at **8372 bytes**, SHA-256 `7349b78db2486c79895b06730c1158bb000e914530abfc5b381b173c8a7d4cdb`.

This destination has not independently reproduced the ELF yet.

## Source/helper files

- `Apps/gnss_stream_diagnostic.c`
- `Apps/gnss_stream_diagnostic.json`
- `lib/NativeApps/include/RiscRteLocationRecords.h`
