# Battery Status

Battery Status is a read-only telemetry app.

It uses `T5AppApi` for input polling, `T5BatteryApi` for a firmware-owned telemetry snapshot, and `T5UiApi` for list rendering and selection.

Manifest: version 1.0.1, minimum firmware 1.1.18, ELF `battery.elf`.

The app displays mode, charge percentage, voltage, board state, and—when detailed telemetry is available—gauge/charger status, current, capacity, temperature, and related status fields. It does not initialize or directly access battery hardware.

Source: `Apps/battery.c` and `Apps/battery.json`.
