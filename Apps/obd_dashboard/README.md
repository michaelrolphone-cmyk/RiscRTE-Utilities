# OBD Dashboard 0.2.0 — NOVA-7 X4

Implements the owner's `obd-dashboard.html` design as a native intent-driven app.
Reference SHA-256: `9ca9bf25f1b4bd6bc4c3ccf7eebf5a463747e94314d06b5cb4dea3f314a26770`. The HTML's simulated-car controls, example VIN,
placeholder faults and assumed Classic/PIN/baud text are not production data.

## Screens and actions

LIVE has the large speed readout, RPM bar with hatched upper range, eight
selectable sensor tiles, mini trends, actual BLE/protocol badges, settings and
customization buttons, and fixed LIVE / GRAPH / FAULTS / INFO navigation.
Tapping a tile opens its graph, 30 s / 2 min / 10 min span, min/average/max,
upper/lower limit controls and dashboard selection. Speed and RPM stay visible.

GRAPH overlays up to three sensors using solid, dashed and dotted strokes.
The full-range overlay and auto-scaled detail plot use elapsed monotonic time,
not a fabricated sample frequency. Missing/stale samples make gaps. Graph pause
freezes its series, range and legend values; it does not stop vehicle polling.
Settings has a separate Pause polling command. At most eight dashboard tiles and
three graph series are accepted. At least one graph series is retained.

FAULTS reads stored (03), pending (07) and permanent (0A) codes from the selected
ECU. A small local dictionary reproduces the seven examples, labels and possible
causes in the supplied design; those priorities are reference labels, not an
ECU-provided severity or a diagnosis. All other codes are shown numerically with
an explicit missing-description notice. Lists are bounded to 32 codes each.
Mode 02 frame 0 is read only after verifying its associated DTC. It is not
presented as a distinct freeze frame for every code. Readiness uses actual PID
01 support/completion bits, including spark/compression-ignition distinctions.
It does not certify inspection eligibility.

INFO reads VIN (0902), OBD standard, fuel type, clear/MIL distances, runtime,
selected ECU/protocol, actual ELM identity, input voltage (ATRV), and counters.
Adapter log shows the latest 30 real TX/RX lines, bounded and clipped to the UI.
Neither a second ECU nor a VIN is invented. Available PIDs depend on the vehicle.

Threshold limits have per-sensor enable, a global switch, sustained fresh-sample
debounce, an inverted warning tile, alert modal, acknowledgement, five-minute
mute, See graph and a 32-entry history for this drive. History times are elapsed
seconds because no wall-clock capability is required. These configurable limits
are user preferences, not vehicle-specific protection thresholds.

## Real transport versus design assumptions

The existing reusable BLE serial 0.1.0 provider is unchanged. Scan and explicitly
select the adapter; discover real GATT characteristics (FFE0/FFF0/NUS or a selected
vendor channel). Connection steps reflect adapter/ECU responses, not timed fake
progress. A dropped connection retries the same selected address/type up to three
times; only a sustained fresh ECU session resets that budget. Forget clears this
invocation's selection. Pairing/bonds are not claimed. Classic SPP and
bonding-required BLE services remain unsupported. The physical ViaBecs adapter
has not been tested.

The setup retains headers (ATH1), unlike the mockup's example, to validate which
ECU sent a response. The original single-command-at-a-time parser is retained
and extended with bounded ISO-TP/legacy VIN assembly, diagnostics, explicit
negative/no-data states and seven additional generic PIDs (23 decoded readings
in total). Protocol AUTO/CAN/KWP/ISO selections apply on the next connection.

GEAR (EST) shows `--`: neither calibrated vehicle ratios nor a generic gear PID
is available. Economy uses a fresh ECU fuel-rate PID when reported; otherwise
it is explicitly estimated from MAF only when the ECU reports gasoline. The
MAF estimate assumes stoichiometric gasoline (14.7 air/fuel, 745 g/L); it is not
applied to unknown, diesel, electric or hybrid fuel-type codes. Stale/zero-speed
inputs show `--`. No simulated gear or fuel consumption is shipped.

## Deliberate code clearing

Normal polling is read-only. The design's CLEAR action is the sole exception:
it opens the warning/KEEP dialog, then one explicitly confirmed Mode 04 request
can be sent only while RPM and speed both report zero, are fresh within 1.5 s,
and belong to the same selected ECU. The confirmation expires after 1.5 s and
is checked again after serial backpressure. It cannot replay across reconnect.
The UI reports success only on a valid positive 44 response, then rescans. It
never claims to erase permanent codes. Clearing may erase freeze-frame data and
reset readiness. Configure and use interactive controls only while parked.

## SD state and trip recording

The app acquires `ui.scene@1`, `bluetooth.serial@1` and `storage.volume@1` through
the capabilities API. The X4 assembly binds only the already-existing SD volume
(instance 9); no Arduino, panel, GPIO or NimBLE internals enter the app.

Preferences are CRC-validated and transactionally replaced at
`/Apps/obd-dashboard/settings.v1`. Layout, units, graph choices, refresh interval,
protocol and alert limits persist. Missing/corrupt/failed SD state is reported
without inventing a successful save. The adapter selection is session-only.

Log trip creates `/OBD/trip-00000001.csv` and increments the name, using exclusive
creation and skipping existing paths; it never truncates an earlier trip. Rows
contain elapsed_ms, ECU, PID, canonical value_milli and unit. Only fresh new
responses are appended. Sync occurs every two seconds and on stop; failures stop
recording and report an error. Exit/shared controls close the file and Bluetooth
session before handing off. Abrupt power loss can lose unsynced rows. Old books,
fonts and Reader state are not modified.

## Shared UI and deployment

Generic instruments, two-column tiles, code rows, normalized line plots, status
tiles and fixed tabs are implemented in scene-host 0.4.0. The app submits copied
semantic documents/curves; geometry, raster glyphs, clipping, hit regions and
lifecycle remain provider-owned. Existing scene/page/Reader prefixes are intact.
Refresh waits for completed presentation before its next 1/2/5-second interval,
so continuous updates do not starve taps. Unchanged documents are skipped.

Both the updated provider/firmware registration (including the SD grant) and the
0.2.0 SD app/manifest are needed. Old providers show UPDATE FIRMWARE and close
cleanly instead of reading beyond an old API table. Full 0x0 images replace
internal settings/NVS/bonds/AppData; they are not preserving OTA updates.

## Tests and build

Run `scripts/test_obd_dashboard.py --runtime R --system S --drivers D --output O`
and repeat with `--sanitize`. The protocol suite covers all old decoders, new
PIDs, CAN11/CAN29/legacy VIN, codes, frame association, malformed fragments,
negative replies, clearing constraints and serial backpressure. Model tests cover
bounded history, stale gaps, frozen values, statistics, limits/debounce/mute and
CRC. Private PIC division helpers are compared to 100,000 native divisions.

Twelve actual app+scene scenarios inject DOWN/UP and swipe events: navigation,
persistence/reopen, recording, timeout, ambiguous cleanup, lost native custody,
provider absence, shared controls, alerts/mute/history, old-provider fallback,
confirmed clear/ack, short SD writes and trip failures, SD retained ownership,
and reconnect to the selected peer. Captures use clearly identified fixtures,
not vehicle measurements. Host tests do not execute Xtensa instructions or RF.

`scripts/build_obd_dashboard.py` produces the Xtensa ELF, native manifest and
source/import/static-frame receipt. Hardware, vehicle-specific compatibility,
actual sample cadence and physical display timing remain untested.

## Protocol references

ELM Electronics, ELM327DSJ (2016): Mode 04 reset/positive response (p.35), VIN
(p.42), CAN headers/ISO-TP (p.44). The adapter handles bus flow control.
https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf
Generic PID definitions cross-checked with the published CSS Electronics table:
https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979

## 0.2.2 readiness audit

See [audit and reproduction](../../docs/obd-readiness/AUDIT.md). Late RX is checked
against the request deadline before decoding; the existing UI and features are
preserved. Hardware compatibility remains unverified.
