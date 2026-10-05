# LoRa Messages 0.1.0

An original portable Nova7 application, independent of the historical Reader
`lora.elf` packet-monitor/Ping utility. It uses the existing Watch
`radio.lora@2` provider; no Runtime radio implementation, pins or region defaults
are introduced in the application.

## What works

- Explicit **Listen**, **Stop**, **Compose**, **Send** and RF setup.
- The shared 32-key Points/Watch keyboard, all 95 printable ASCII characters,
  240-byte drafts, Delete, page switching, Done and cancelable nested Back.
- Draft review pages show every character before the explicit Send action.
- Six exact RF settings: frequency in Hz, bandwidth in Hz, spreading factor,
  coding-rate denominator, output power in dBm, and preamble symbols.
- A checksummed/versioned 32-byte `lora_profile` record in dedicated namespace9.
  Missing or corrupt settings have no radio defaults and trigger no write.
  Applying settings saves them without starting the radio. Failure to save is
  visible; the new profile remains explicitly labeled active for the session.
- Newest-first bounded history of16 packets, with full-text pages, local RTC
  timestamps following shared namespace1 `time_format` 12/24 preference, RSSI and
  quarter-dB SNR for received packets. Bad RTC becomes `Time unset`.
- The history and unfinished draft live in this invocation's RAM and clear on
  exit; the screen explicitly says **Session History**. Opening editors/history
  does not discard the draft. The profile alone persists across invocations.

The transport is raw broadcast text. It is not Meshtastic, LoRaWAN, addressed
chat, encryption or a delivery-acknowledgement protocol. Peers must use matching
RF parameters and the installed provider's fixed packet mode/sync word.
`TRANSMITTED / NO RECEIPT` means the chip reported TX completion, not that a peer
received the text. A refused send or uncertain status is **Send unconfirmed**.
Only a new explicit Send retries; it may duplicate a packet. There are no
background/automatic transmissions, acknowledgements or retries.

## Hardware contract and explicit RF policy

`Apps/PortableLoRaV2.h` copies the radio declarations from Watch commit
`2a4fbae8fb2425bf830c302863a5e195106e77c9`, including target size assertions.
It preserves the board-local ABI without presenting it as a canonical Runtime
SDK interface. The provider accepts `hardware.device@1` and rejects configuration
outside its actual selected band. Hardware identity is never inferred from an
API response, installed package or this source tree.

The Watch source has eight explicit non-Plus T-Watch-S3 hardware profiles for
SX1262 433/868/915 MHz or SX1280 2.4 GHz, each with BMA423/BMA456H choices. Its
SX1262-915 manifest declares instance11, SPI logical controller1, SCLK3/MOSI1/
MISO4, CS5, reset8 (active-low), busy7 and IRQ9 (active-high), a 902–928 MHz range,
TCXO voltage selector0 and board-battery instance4 dependency. These are the
checked source declarations, not a physical identification of the owner's unit.
The complete profile and wiring stay in Watch; this app reads no pins and
hardcodes no selected frequency or power.

Before use, the owner must verify their actual board/radio and antenna and enter
an appropriate permitted channel, bandwidth and transmit power for their
location and peer. Driver range checks alone do not establish legal RF use.
The application initially leaves all six fields unset. Airplane mode blocks new
radio actions; unknown radio policy fails closed. The app permits the
provider-supported numerical combinations only; the selected driver performs
the final physical-band check. It never silently fills parameters.

## Ownership and error behavior

The generic `PORTABLE_RADIO_SESSION` adapter hooks cancel this app's operation
before alarms/cues, sleep, quick-controls and unload. Wake or modal dismissal
never resumes listening or transmission. A confirmed cancellation releases chip
operation ownership. Failure to cancel retains the invocation and all grants;
there is no further normal provider I/O, handoff or unload. Even a failed
configure/send is treated as possibly owning hardware until cancel succeeds.

Receive uses5-second windows, rearming while the user-requested Listen remains
active. CRC errors are discarded; binary packets are bounded/sanitized for
viewing with an explicit binary marker. Malformed lengths, driver failures,
unexpected states and an additional monotonic watchdog stop the operation.
TX allows the provider's maximum60-second bounded timeout, and never resends on
an error. Stop during TX records an unconfirmed delivery outcome.

## Integration and software evidence

`lora_apps` is an explicit build inventory. `sdk/lora-sources.json` pins the shared
radio-session adapter. `scripts/build_lora_apps.py` checks clean dependency
identity, strict target compilation, native ELF structure, import/export
allowlists and generated manifest/evidence. It does not publish or install.

The Watch builder must include `lora_messages` in launcher/catalog/current-app
inventories, the existing `twatch-lora` provider/device dependency graph, explicit
`radio.lora@2` instance11 and storage namespace9 grants, namespace1 shared
preferences, RTC8, and the standard display/touch/navigation/battery/alarm
lifecycle grants. Namespace1 is read-only to this app. Use Nova and app-owned
chrome, `PORTABLE_RADIO_SESSION`, the standard Points keyboard, the selected
Watch time conversion, navigation/sleep and the normal caller return policy.
Use `LORA_RETURN_APP="springboard.elf"` for deliberate root Back. It cancels the
radio before queuing the return and leaves a retryable UI if launch is refused.
Do not wrap nested Back in a generic pre-poll return macro.

Tests:

```sh
python scripts/test_lora_messages.py --system-apps /exact/system-apps
python scripts/test_lora_renderer.py --system-apps /exact/system-apps
python scripts/build_lora_apps.py --system-apps /exact/system-apps
```

Model/controller tests cover normal/error/retry/retained cleanup, timer wrap,
invalid/overflow RF values, record corruption, bounded history and payloads,
no implicit RF activity, keyboard and nested cancellation, all draft review
pages,12/24h timestamps, missing providers and unconfirmed storage writes.
The real app/adapter/rasterizer is exercised in ten240×240 scenes, with normal
and ASan/UBSan builds and framebuffer-stride/lease cleanup assertions. Software
mocks/compilation do not qualify radio compatibility, RF legality, reception,
transmission, timing or power on physical hardware. No device action is taken.
