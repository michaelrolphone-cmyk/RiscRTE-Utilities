# Waterfall

Catalog application `waterfall.elf` for a 240x240 panel. It is not the watch
default app and it does not replace Clock. Add it to a launcher catalog later.

## What it does

Each pass takes one 256-pair burst, runs a 256-point FFT, and scrolls one row
onto the panel. The portable client writes that row into an RGB565 frame using
the four tones `0xFFFF`, `0xBDF7`, `0x630C`, and `0x0000`. The loop sleeps
through `poll`, which yields. This is a burst waterfall, not a continuous
80 MSa/s display. There is no transmitter and no FPGA stream.

## Capture

Sample words follow the published ESP32-S3 dump layout: signed 10-bit I in
bits 0-9 and signed 10-bit Q in bits 10-19. The dump-engine window
(circular 80 Msps control word, bank 0 at `0x3FCB0000`, run bit on then off)
is the sequence from h0m3us3r/eSpDR `board.h` and `capture_run`, commit
`f279bf823eee41796dfd1ac21f13e1ed9b418c82`.

That window runs only when `s3_iq_radio_ready` is set. This ELF does not set
it. Receiver bring-up is `radio_init` in eSpDR `esp32s3/src/radio.c`, which
calls `register_chipv7_phy`, the ROM regi2c helpers, and the Wi-Fi clock
gates. Those symbols are outside the watch ELF import allowlist
(`risc_runtime_get_api`, `memcpy`, `memset`, `memcmp`, `strcmp`, `strlen`,
`snprintf`, `malloc`, `free`, `strcpy`). No replacement register program is
included. Until that bring-up exists, the panel stays a quiet waterfall and
the status line says `NO RADIO`.

eSpDR's continuous 80 Msps path clocks the chip from an FPGA and bit-bangs
sixteen GPIO lanes. The watch has no FPGA, so that path is not built.
