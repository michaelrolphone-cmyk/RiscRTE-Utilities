# Watch Scanner modal lifecycle

The Watch Scanner sensor-name editor opts into the shared checkpoint helper.
Naming still stops the foreground BLE scan before opening text input. Alias
validation and checked persistence happen only after the text session is fully
closed. No BLE scanning, model, radio ownership, alias format or draw code changed.

A settled editor services eligible Contexts audio capture and background steps.
The Scanner's existing foreground-radio restrictions still prohibit Contexts RF
and background BLE advertising. Routine battery admission preserves the shared
input subscriptions; a possible idle sleep fully detaches the editor first. The
pause mode is latched across AGAIN. A failed resume retains the invocation rather
than repeating policy dispatch. Missing, malformed or unsupported checkpoint
suffixes close cleanly without writing an alias.

The selected Watch scene has exactly display, touch, clock and presentation-profile
dependencies. Local PMU Back uses System's local-only attention bridge; a pending
Runtime Home exit closes the editor without redraw or a springboard relaunch.
The System pin must include the local attention restoration in 9957e7c.

## Reproduction

Run `scripts/test_scanner_policy.py` with the exact System, Runtime, Watch and
Drivers commits declared by `.github/workflows/scanner-input-policy.yml`. The
runner compiles the actual Scanner, PortableApps adapter, compact scene host,
shared text host, scene profile and BLE sensors driver. Synthetic hardware
boundaries expose policy calls and assert resource custody and alias writes.
The fixture retains the actual four scene dependencies. The BLE sensor API header
is copied into an isolated fixture include directory, as in the predecessor host
qualification, to avoid conflicting sibling telemetry header copies.

The 23 predecessor workflows cover scan/stop, details, nested Back, accepted and
cancelled aliases, repeat editing, delayed presents, rapid queued taps, Home and
Runtime loss at text/storage boundaries. Fifteen policy workflows cover 65-second
idle editing, low battery, pause AGAIN/BUSY, retained pause and resume, paused Home
and PMU Back, missing/malformed/unsupported suffixes, sleep refusal and two sleep
failure returns. Normal and ASan/UBSan workflows must pass with byte-identical
rendered frames. `--no-leak-checks` is only for environments where LeakSanitizer
cannot operate; ASan and UBSan remain enabled.

The legacy BLE model and production renderer checks remain available through
`scripts/test_ble_scanner.py`. Target flag-off equivalence, stack/storage bounds,
capacity, lossless ELF packing and complete Runtime cohort admission are separate
Watch recipe gates. This source checkpoint does not claim a fitted target or
hardware qualification until those gates and independent review are complete.

X4 Scanner 0.2.24 has unresolved selected-binary provenance and is not rebuilt by
this Watch-only change. No delivered or sealed device image is modified here.
