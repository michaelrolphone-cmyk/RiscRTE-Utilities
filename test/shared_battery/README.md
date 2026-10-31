# Battery actual Runtime qualification

`python scripts/test_battery_runtime.py --runtime RUNTIME --system-apps SYSTEM --output OUTPUT [--sanitize]`

This fixture compiles the production Battery controller and broadcast helper,
production `telemetry_broadcast` service, production scene components/profile,
and real Runtime loader/provider graph. Test-only app observers verify semantic
state without creating synthetic scene events. Native navigation goes through the
actual presenter. Hardware display/touch/gauge/Bluetooth and durable byte-store
endpoints are deterministic doubles.

The matrix covers 800x480 MONO1, 240x240 RGB565 and 410x502 RGB565; demand,
retained-provider and absent-UI admission; and preference-broker versus explicitly
registered atomic endpoint grants. Both storage brokers are compiled in for this
contract matrix; this is not a reproduction of each product's compile recipe.
The same application host ELF is reused byte-for-byte across all cases.

Each UI case observes an already-enabled stored broadcast before any action,
opens Details, changes page, refreshes, returns to the overview, opens Broadcast,
disables then enables via native input, leaves, reloads the application and checks
that the exact original four-byte preference remains. Accepted transactions are
collected by the real helper. Display ownership/canaries, repeated radio polling
within one active session, actual app fini/unload and balanced input/display
teardown are asserted. Absent UI causes zero hardware I/O and writes.

This does not prove real PMIC values, RF transmission, physical input, resident
Home control composition or Xtensa target execution. Controller tests separately
exercise context invalidation, retained custody, pending transactions and resident
handoffs. Output source/SDK receipts and failed attempts must be retained.
`BATTERY_REUSE_RUNTIME=1` permits small DSO rebuilds only with unchanged recorded
Runtime/hardware test inputs and SDK; final clean qualification is preferred.
