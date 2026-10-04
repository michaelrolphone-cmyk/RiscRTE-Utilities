# Waterfall

Catalog application `waterfall.elf` version 0.1.1 for a 240x240 panel. It is
not the watch default app and it does not replace Clock. `default.elf` stays
Clock.

## What it does

The ELF acquires capability `radio.iq` API 1 (`instance_id` 0, the single
authorized provider) and calls `capture_burst` for 256 pair-words. It runs a
256-point FFT and scrolls one row. The portable client writes that row into
an RGB565 frame using the four tones `0xFFFF`, `0xBDF7`, `0x630C`, and
`0x0000`. The loop sleeps through `poll`, which yields. This is a burst
waterfall, not a continuous 80 MSa/s display.

The app does not touch the modem. There is no transmitter and no FPGA stream.
If acquire fails the status line is `NO GRANT`. A loaded driver reports
`BURST`, `PLL FAIL`, `PBUS FAIL`, `DUMP FAIL`, `NOT RUNNING`, or `BAD ARG`.
Back, or a tap in the top-left 80x30 corner, releases the grant and returns.

Sample words are the published dump layout: signed 10-bit I in bits 0-9 and
signed 10-bit Q in bits 10-19.

## Install and run (Watch PR #8)

Driver source is `michaelrolphone-cmyk/RiscRTE-Drivers` package
`s3-radio-iq-v1`. Build it with `python3 scripts/build_s3_radio_iq_v1.py`.
The artifact is `dist/s3-radio-iq-v1/driver.elf` and `manifest.json`. It
provides `radio.iq` API 1 and exports only `t5_driver_get`. It is not
`platform.radio@1` and it is not load-order instance 15 (`wifi`).

Build this app with
`python3 scripts/build_portable_apps.py --system-apps <System Apps checkout at b28428505c9e067bb6ea8a84d12f13ec0bcc4992>`.
Take `dist/portable-apps/waterfall.elf` and `waterfall.json`.

PR #8 `scripts/build_clock_deployment.py` writes a launcher SPIFFS store.
Keep `store/boot.json` `default_app` as `default.elf`. Add files the same
way calculator and stopwatch are added:

- `store/waterfall.elf`
- `store/waterfall.json` (the built sidecar: `type=application`,
  `entry=app_main`, requires `display.output` 1, `input.touch.raw` 1,
  `radio.iq` 1)
- `store/s3-radio-iq/driver.elf`
- `store/s3-radio-iq/manifest.json`

Append to `store/boot.json` `drivers`:

```json
{"manifest": "s3-radio-iq/manifest.json", "instance_id": 17}
```

Append to `app_capabilities` (do not add `radio.iq` to Clock or `default.json`):

```json
{
  "manifest": "waterfall.json",
  "grants": [
    {"capability": "display.output", "api": 1, "instance_id": 5},
    {"capability": "input.touch.raw", "api": 1, "instance_id": 6},
    {"capability": "radio.iq", "api": 1, "instance_id": 17}
  ]
}
```

Display instance 5 and touch instance 6 are the PR #8 launcher grants.

The launcher catalog is `scripts/build_launcher_apps.py` `daily_catalog.c`.
Today it has five entries and `portable_catalog_count` 5. Add Waterfall; do
not remove Clock:

```c
{.display_name="Waterfall",.file_name="waterfall.elf",.icon="solid:f012",.compatible=true}
```

Set the count to 6. Build waterfall with `-DPORTABLE_FORCE_FULL_FRAMES` and
`-DPORTABLE_RETURN_APP="springboard.elf"`, same return target as calculator
and stopwatch. Icon `solid:f012` is the glyph already used by this sidecar.

Boot still opens Clock. Swipe to the launcher and tap Waterfall. Exit returns
to `springboard.elf`, which returns to Clock. Do not point `default_app` at
`waterfall.elf`.
