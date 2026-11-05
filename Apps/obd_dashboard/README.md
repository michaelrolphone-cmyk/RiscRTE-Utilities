# OBD Dashboard 0.1.1 — X4

A native foreground ELF with an intent-driven NOVA scene interface. The app uses
only `ui.scene@1` and `bluetooth.serial@1`; panel/touch/HCI details belong to
providers. `elm.c` is an original bounded presenter-independent ELM client.
`app.c` declares shared components and consumes revision-bound intents.

## Adapter compatibility

Requested adapter: ViaBecs Amazon ASIN **B0CCYKZ8YF**, checked 2026-10-11 at
https://www.amazon.com/dp/B0CCYKZ8YF. The current listing describes Bluetooth LE
and iPhone in-app connection. It does not publish the GATT UUIDs, and this is not
a hardware validation of the owner's particular adapter revision.

Requires an **ELM-compatible, unpaired/open BLE serial** adapter. Scan results
are real advertisements, not a hardcoded OBDII name. Unique recognized FFE0,
FFF0 or Nordic UART candidates are selected after service discovery; otherwise
the user chooses a discovered vendor channel. CCC acknowledgement, ELM banner,
AT setup, positive ECU Mode01 support reply and protocol agreement must all
succeed before a LIVE state. A successful BLE connection alone is not vehicle
data. Classic-SPP-only and pairing/bonding-required adapters are not supported
by this initial provider. The X4's ESP32-S3 is BLE-only, as documented at
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/ble/overview.html.

## Screens and values

Drive: RPM, vehicle speed, coolant temperature, ECU voltage, throttle and engine
load. Engine: manifold pressure, mass airflow, short/long bank-1 fuel trim, oil
and intake temperature. More: fuel level/rate, engine run time, check-engine lamp
and reported code count (not code descriptions). Link: selected adapter/ELM ID,
ECU/protocol, last round trip, reply/error counts and transport counters.

Supported PID bitmaps select only available readings. Unsupported, waiting,
stale and paused readings show `--` with their status, never a made-up zero.
Units toggle between mph/F and km/h/C; pressure remains kPa, airflow g/s and fuel
rate L/h with explicit units. Adapter selection and unit choice last for the
current app invocation; no persistent pairing/profile is written.

Polls are serialized with at least 150ms between requests. Due supported PIDs
are serviced fairly; actual cadence depends on the adapter/vehicle. UI updates
are coalesced to at most one per second for live values and published only if
the document changes, after touch/display custody settles. This is not a claim
of measured hardware frame rate. Pausing drains any outstanding reply and stops
new queries. Resume requests fresh readings; Disconnect/Home release the radio.
Shared Quick Controls close the BLE session and return to disconnected setup.
Live radio sessions inhibit idle-policy sleep; normal policy resumes on exit.

## Protocol and failure handling

Only ATZ, ATE0, ATL0, ATS1, ATH1, ATSP0, ATDPN and explicit Mode01 PID reads are
sent. No raw terminal, clearing codes, programming, actuator tests or manufacturer
ABS/SRS decoding. Standard PIDs: 01,04,05,06,07,0B,0C,0D,0F,10,11,1F,2F,42,5C,5E;
00/20/40 are capability queries. Implementation/formula references are the
original python-OBD project's command/decoder and ELM protocol documentation:
https://github.com/brendan-w/python-OBD/blob/master/obd/commands.py,
https://github.com/brendan-w/python-OBD/blob/master/obd/decoders.py and
https://github.com/brendan-w/python-OBD/tree/master/obd/protocols.
No source from that project is bundled into this original client.

Handles byte-fragmented echoes/status/hex/prompt replies, CAN 11/29-bit
single-frame headers and ELM legacy protocol headers. Selects the lowest
responding ECU from 0100 and keeps all later PID data bound to that identity;
different ECUs are not combined. Protocol/header mismatches, invalid PID/mode,
short/oversized/non-ASCII frames, negative replies, NO DATA and timeouts cannot
be shown as valid samples. Timeout disconnects rather than allowing a late reply
to satisfy the next request. This small Mode01 client does not implement
multi-frame VIN/DTC retrieval or custom vehicle PIDs.

## Deployment and validation

The X4 firmware needs the new BLE provider, installed app/grant registration and
Home/Springboard catalog entry. An ELF-only copy to the preceding firmware is
not sufficient. SD path: `Apps/obd-dashboard/obd_dashboard.elf`, accompanied by
`manifest.json`. Keep the existing SD apps, books, fonts and state.

```
python scripts/test_obd_dashboard.py --runtime ../Runtime --system ../System --drivers ../Drivers --output build/obd-tests
python scripts/test_obd_dashboard.py --runtime ../Runtime --system ../System --drivers ../Drivers --output build/obd-tests-san --sanitize
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc python scripts/build_obd_dashboard.py --runtime ../Runtime --system ../System --drivers ../Drivers --output build/obd-target
```

Tests execute the real app and real scene with independent board/serial fixtures:
scanning, selection, live screens, pause/resume with no static redraw or new
queries, disconnect, ECU timeout, missing provider, lost native authority,
uncertain close retention and shared-controls radio handoff. Separate engine
tests cover all 16 decoders, units, masks, multi-ECU isolation, header families,
malformed streams, clock wrap and a 200-request read-only session. Test values
and native framebuffer captures are simulated inputs, not vehicle measurements.
Both normal and ASan/UBSan runs and Xtensa structural/import/stack checks are
performed; hardware and owned-adapter compatibility remain pending.

Perform initial connection and setup while parked. Use proper engine ventilation;
this display is informational, not a replacement for vehicle instruments/alerts.

## 0.1.1 native integration correction

The first component-only JSON used catalog fields that the native loader rejects.
The delivered manifest now contains the required architecture/entry, native
supported_file_types, and explicit foreground lifecycle. Vehicle protocol and
GUI behavior are unchanged. Build metadata reads the declared manifest version;
full-image qualification independently runs the real Runtime manifest parser.
