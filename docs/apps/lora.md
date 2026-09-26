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

The local display buffers can represent up to 256 payload bytes in the ASCII view and the corresponding spaced hexadecimal representation. The documentation does not imply a transport limit beyond what the app's current buffers display.

## Transmit workflow

Confirm sends the literal nine-byte payload:

`T5S3 ping`

Transmission is only attempted while the LoRa state reports Ready.

## Hardware and failure states

If the service reports unsupported, the footer states that SX1262 hardware is unavailable on the board. Radio errors display the service's numeric `last_error`.

Hardware ownership and SPI/radio implementation live below the app in the LoRa service/driver.

## Source

- `Apps/lora.c`
- `Apps/lora.json`
