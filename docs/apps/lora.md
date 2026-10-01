# LoRa

## Purpose

LoRa is a raw SX1262 packet monitor/transmit utility. Its manifest identifies `lora.elf`, version **1.0.0**, minimum firmware **1.1.15**, categories `Connectivity` and `Hardware`.

The app intentionally treats received payloads as opaque bytes. It is a radio/packet utility, not a decoder for a higher-level correction or messaging protocol.

## RiscRTE interfaces

The app uses:

- `T5LoRaApi`
- `T5UiApi`

Required LoRa operations are `supported`, `default_config`, `start`, `stop`, `read_state`, `poll_packet`, and `transmit`. It also uses optional `prepare_display` / `finish_display` hooks when available to let firmware arbitrate shared display/radio pins around UI refreshes.

## Radio configuration

The app starts from the service default configuration and then explicitly sets:

- Frequency: **915.000 MHz**
- Bandwidth: **125 kHz**
- Spreading factor: **7**
- Coding rate: **4/5**
- Sync word: **0x12**
- Preamble: **8 symbols**
- CRC: enabled

The rendered status also states explicit header mode, normal IQ, preamble 8, and CRC on.

## Packet display

The UI shows:

- Radio status
- Active radio configuration
- Last received packet length
- Raw packet bytes as hexadecimal
- Printable ASCII view, with non-printable bytes replaced by `.`
- RSSI in dBm
- SNR in dB
- RX and TX packet counters

The public API caps valid packets at 255 bytes (`T5_LORA_MAX_PACKET`), although the display buffers have 256-byte capacity. Only the latest packet is retained. RSSI/SNR display truncates tenths to integer units.

## Transmit workflow

Confirm sends the literal nine-byte payload:

`T5S3 ping`

Transmission is only attempted while the LoRa state reports Ready.

## Hardware and failure states

If the service reports unsupported, the footer states that SX1262 hardware is unavailable on the board. Only ERROR status displays numeric `last_error`; not every failed operation becomes visible.

Hardware ownership and SPI/radio implementation live below the app in the LoRa service/driver.

## Source

- `Apps/lora.c`
- `Apps/lora.json`

## Independent-build baseline

Source and manifest exactly match Reader `a5e2db59077cc889079668dc9cd7428b08bc32a1`. See [build evidence](../BUILD.md), [readiness and removal criteria](../MIGRATION_READINESS.md), [source audit](../source-drift.json) and [published-byte comparison](../release-parity.json). Host fixtures exercise actual app C with simulated APIs; they do not establish hardware/runtime qualification.

## Limits and failed operations

Missing required APIs/function pointers cause a silent return. Start, state-read, transmit and display-hook results are ignored. State/render refresh happens on startup, a packet or Ping, not on a timer. Ready can coexist with nonzero last_error, so failed transmit/restoration need not appear as an error. No configuration UI, arbitrary transmit payload, history, recording or protocol decoding is implemented.

At the pinned Reader baseline the host implementation supports BOARD_T5S3_PRO, and inherited default TX power is 22 dBm. Shared-pin display arbitration stops/restores the radio; startup includes a 1500 ms delay. Continuous/lossless reception is not established. These are current firmware implementation facts, not permanent ELF ownership architecture. Do not transmit without appropriate hardware and local authorization.
