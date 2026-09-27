# SD Files

## Purpose and classification

SD Files is a narrow, read-only SD-card enumeration diagnostic. It is classified as a **utility**, not a foundational file manager: it only pages through the root directory `/sd` and exposes no open, copy, move, rename, delete, package, or file-association workflow. File Browser remains the foundational file-handling app.

## Manifest

- Display name: **SD Files**
- Version: **1.0.0**
- Minimum firmware: **1.1.5**
- ELF: `sd_list.elf`
- Icon: `solid:f07b`
- Categories: `Files`, `Diagnostics`
- Upstream source blob: `664b5eabf176a7c8ebe26d1448cb293751a1fbfc`
- Upstream manifest blob: `86cbc4ed7096eb45a64905daa47acea3014762b3`

The manifest declares no required or optional provider capabilities.

## Host API usage

The app uses only `T5AppApi` ABI v1. `has_directory_api()` checks `struct_size` through the `dir_close` member and requires `dir_open`, `dir_next`, and `dir_close`. The app also uses `screen_height`, `clear`, `draw_text`, `present`, and `poll`.

## User workflow and input

At launch the app opens exactly `/sd` and renders a page headed **SD card: /sd**. Entries are shown in provider enumeration order. Directories are prefixed with `[DIR] `; files receive spacing before the name. The source does not sort entries or descend into directories.

Rows per page are derived from screen height and clamped to 1–30. Each formatted line uses a 64-byte local buffer.

When more entries remain, Confirm, Down, Right, or a tap advances to the next page. At end-of-directory, the same action closes and reopens `/sd`, restarting enumeration. Back or an exit request leaves the app. An `armed` gate prevents a continuously held action from advancing repeatedly without an intervening idle poll.

## Rendering

The app draws directly through `T5AppApi`; it does not use `T5UiApi`. Each page clears the display, draws header/rows/footer, then calls `present(true)`. If enumeration is empty or already at end, the page displays **(empty or end of directory)**.

## Storage and persistence

The only opened path is `/sd`. The application performs directory enumeration only and does not create, alter, remove, or read file contents. It persists no application state.

## Failure handling

If the required directory API is unavailable, `app_main` returns. If `dir_open("/sd")` fails, the app displays **Unable to open /sd** and remains visible until exit. If reopening after end-of-directory fails, the main loop terminates and closes the directory handle.

## Network and hardware boundary

No network API or direct hardware/provider API is used. SD mounting and directory semantics are host responsibilities.

## Published package

Upstream release `app-sd_list-v1.0.0` publishes `sd_list.elf` at **3516 bytes**, SHA-256 `2626f53540422f8594a70d2ae73bf2bf53e312d6c3b3fa5d18b9a548f668ba78`.

This destination has not independently reproduced the ELF yet.

## Source files

- `Apps/sd_list.c`
- `Apps/sd_list.json`
