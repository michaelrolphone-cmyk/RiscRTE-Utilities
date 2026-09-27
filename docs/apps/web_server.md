# Web Server

## Purpose and classification

Web Server is an optional connectivity/server utility. It is classified as a **utility**, not an MCU development/debug tool: it starts and observes a firmware web-server provider and does not inspect or program an external microcontroller.

## Manifest

- Display name: **Web Server**
- Version: **1.0.0**
- Minimum firmware: **1.1.16**
- ELF: `web_server.elf`
- Icon: `solid:f0ac`
- Categories: `Connectivity`, `Developer`
- Upstream source blob: `f4087ce1b2b4dc5a703f5d0217ca26560b3d8b47`
- Upstream manifest blob: `08e1d13ba34f482d8781f2e2f15ae66667f92b11`

The manifest declares no provider capability entry.

## Host interfaces

### T5AppApi

The app requires `millis` for redraw throttling.

### T5UiApi v1

The app requires `render_list` and `poll_event`. Back or Exit leaves the loop.

### T5WebServerApi v1

The app requires `default_config`, `start`, `stop`, `service`, and `read_state`. The interface exposes Unsupported, Stopped, Running, and Error states and provider error values for invalid configuration, storage, Wi-Fi, DNS, and HTTP failures.

## Startup and provider ownership

The app calls `default_config()` and passes that configuration unchanged to `start()`. It does not override SSID, password, hostname, or document root itself. It immediately reads provider state and renders it; the return from `start()` is not used as the UI verdict.

The app does not implement AP setup, captive-portal DNS, mDNS, HTTP routing, or SD serving. Those behaviors are provider-owned and are not specified here beyond state exposed by the provider.

## User-visible state

The list title is **Web Server** and subtitle is **Manifold captive portal**. Rows show Status, Wi-Fi SSID, Portal, IP, Clients, Requests, and SD root.

If provider strings are empty, the UI uses display fallbacks `Manifold`, `manifold.local`, `--`, and `/html`. These are presentation fallbacks; the source does not establish that they are actual provider configuration.

Error status shows `last_error`. Running status instructs the user to connect to the AP and open `manifold.local`. Otherwise the footer says Back exits and stops the access point.

## Runtime behavior

Every loop calls `service()`, reads state, and polls the UI with a 20 ms wait. A redraw occurs when client count or request count changes and at least 2000 ms have elapsed since the previous redraw. Other state fields do not independently trigger that throttled redraw condition in this source.

On Back or Exit, the app calls `web->stop()`.

## Storage, network, and persistence

The app displays the provider document-root path but does not directly open or mutate SD files. It defines no HTTP endpoint paths itself. It stores no persistent state.

## Failure handling

If any required API or callback is missing, `app_main` returns. Provider errors are surfaced through `status` and `last_error`; the app otherwise continues servicing/reading state until exit.

## Published package

Upstream release `app-web_server-v1.0.0` publishes `web_server.elf` at **5076 bytes**, SHA-256 `063047a55f2f6bd292724a9ccb0e4a974cedb1be5eacb265730ca2f28fd310b5`.

This destination has not independently reproduced the ELF yet.

## Source files

- `Apps/web_server.c`
- `Apps/web_server.json`
