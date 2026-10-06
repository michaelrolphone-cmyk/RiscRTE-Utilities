# LoRa Messages 0.1.1

An original portable Nova7 application, independent of the historical Reader
`lora.elf` packet-monitor/Ping utility. It uses the existing Watch
`radio.lora@2` provider; no Runtime radio implementation, pins or region defaults
are introduced in the application.

## What works

- An explicit radio picker: **SX1262 / 433 MHz**, **SX1262 / 868 MHz**,
  **SX1262 / 915 MHz**, or **SX1280 / 2.4 GHz**. The selectable Watch provider
  exposes all four in one firmware. A fixed provider exposes only its exact
  supported choice; other rows are unavailable. Choosing a row does not detect
  or convert the physical radio or antenna.
- Explicit **Listen**, **Stop**, **Compose**, **Send** and RF setup. The first
  radio action asks for a choice; existing RF settings never imply a choice.
- The shared 32-key Points/Watch keyboard, all 95 printable ASCII characters,
  240-byte drafts, Delete, page switching, Done and cancelable nested Back.
- Draft review pages show every character before the explicit Send action.
- Six exact RF settings: frequency in Hz, bandwidth in Hz, spreading factor,
  coding-rate denominator, output power in dBm, and preamble symbols.
- A checksummed/versioned 32-byte `lora_profile` record in dedicated namespace9.
  Record v2 stores the explicit choice and whether RF values have been set.
  Missing or corrupt settings have no radio defaults and trigger no write.
  Legacy v1 records preserve the RF fields but show **Choose radio**; no choice
  is inferred from frequency, installed packages or driver identity. Changing
  radio preserves prior RF values for editing; a mismatch blocks Listen/Send.
  Picker selection and applying settings save without starting the radio. Failure to save is
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

`Apps/PortableLoRaV2.h` copies the radio declarations from the Watch source pinned
in `sdk/lora-sources.json`, including target size assertions. The original
36-byte Xtensa `radio.lora@2` table prefix is unchanged. Its checked optional
44-byte suffix adds `profile_info` and `select_profile`; this app reports an
unavailable picker for older tables rather than guessing hardware.
It preserves the board-local ABI without presenting it as a canonical Runtime
SDK interface. The provider accepts `hardware.device@1` and rejects configuration
outside its actual selected band. Hardware identity is never inferred from an
API response, installed package or this source tree.

The new selector uses IDs0 unset,1 SX1262/433,2 SX1262/868,3 SX1262/915 and4
SX1280/2.4GHz; supported bits are `1 << (id - 1)`. The provider owns compatibility,
wiring and allowed choices. Its selectable typed configuration v2 has a bounded
allowed-profile mask; fixed typed configuration v1 remains unchanged. In the selectable Watch deployment, admission, profile queries and selection
perform no radio initialization or transmission. Initialization occurs only when
the user's subsequent Listen/Send configures RF. Fixed v1 profiles retain their
existing eager hardware admission behavior; picker code itself never calls
configure, receive or send.
Changing a selection first cancels/releases the old initialized radio, retaining
the previous choice and resources if cleanup fails. The app checks every result
and preserves its previous saved choice on a failed selection.

The Watch source also has eight explicit non-Plus T-Watch-S3 hardware profiles for
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
The application initially leaves the radio choice and all six fields unset. Airplane mode blocks new
radio actions; unknown radio policy fails closed. The app permits the
provider-supported numerical combinations only; the selected driver performs
the final selected-profile range check. It never silently fills parameters.

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
allowlists and generated manifest/evidence. `scripts/check_lora_abi.py` compares
the complete consumer declaration with the clean, immutable Watch source pin.
Neither script publishes or installs.

The Watch builder must include `lora_messages` in launcher/catalog/current-app
inventories, the selector-capable `twatch-lora` provider/device dependency graph, explicit
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
python scripts/check_lora_abi.py --watch /exact/watch
python scripts/test_lora_messages.py --system-apps /exact/system-apps
python scripts/test_lora_renderer.py --system-apps /exact/system-apps
python scripts/build_lora_apps.py --system-apps /exact/system-apps
```

Model/controller tests cover normal/error/retry/retained cleanup, timer wrap,
invalid/overflow RF values, record corruption, bounded history and payloads,
no initialization or TX/RX on picker actions, all four compatible radio profiles,
fixed-provider masks, legacy-record migration without inference, retained choice
on failed cleanup, selector suffix-size checks, keyboard and nested cancellation, all draft review
pages,12/24h timestamps, missing providers and unconfirmed storage writes.
The real app/adapter/rasterizer is exercised in twenty240×240 scenes, with normal
and ASan/UBSan builds and framebuffer-stride/lease cleanup assertions. Software
mocks/compilation do not qualify radio compatibility, RF legality, reception,
transmission, timing or power on physical hardware. No device action is taken.
