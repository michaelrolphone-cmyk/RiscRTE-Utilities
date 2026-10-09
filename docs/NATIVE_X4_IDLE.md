# Explicit native X4 utility idle profile

The combined native cohort selects the shared System `scripts/portable_idle_build.py` and the product `minimal/apps/portable_idle_sleep.c` directly. Utilities supplies no competing idle, battery, brightness or radio policy. The existing native, paper and Watch profiles retain their source pins and versions when the three idle arguments are absent.

The selected profile requires clean System `7b175418e3063d9f4c571acf06c366144831b2af` and Runtime `c77e717571a80bc18009b9c3a98532fee32e6fd2`. Pass all three arguments:

- `--x4-idle-source`: product `minimal/apps/portable_idle_sleep.c`
- `--x4-idle-sdk`: final typed product driver SDK directory
- `--x4-idle-runtime-sdk`: selected Runtime `sdk/driver`

`build_native_broadcast.py` builds Battery 1.1.9, Calculator/Stopwatch 0.1.16, Scanner 0.2.11, HID Mouse/Buttons 0.1.11 and Waterfall 0.2.7. `build_battery_power.py` forwards an explicit idle selection to that same Battery build. `build_native_utc_alarm_apps.py` builds Alarms 0.2.11 and Countdown 0.1.15; select the same combined checkout for `--system-apps` and `--adapter`, with `--paper-transitions --ble-broadcast` and without `--motion-system`. Its raw and Watch checkouts are still required for the four exact flag-off comparisons.

All native selections retain tagged alarm API 2, native read-only IANA time, paper orientation, Quick Controls and default-off telemetry. The shared profile adds reversible automatic Light, low-battery preference handling and Quick Radios. Clean wake restores confirmed saved background radio intent. No target adds RTC or realtime-control authority. Waterfall's actual capture predicate inhibits automatic idle while capture is active; suspension clears native foreground capture intent, so restoring background radios cannot restart capture.

`native_idle_build.py` records the final compiled include directory, every final SDK header digest, System/helper/build source hashes, exact physical grant bindings and compiler-discovered source dependencies. Scanner's foreground `bluetooth.hci@1` instance 0 remains separate from physical background/sleep instance 16. The largest declared grant set is Waterfall's 15. App-owned alarm dependencies retain immediately after any false acquisition, including an empty handle; no later acquisition, cleanup or sleep is attempted.

## Local verification

Use the existing pinned GCC 8.4.0 / 2021r2-patch5 via `NATIVE_APP_CC`. Set `PLATFORMIO_SETTING_ENABLE_TELEMETRY=no`; no toolchain installation or publication is needed.

- Build the broadcast and alarm cohorts into separate output directories. Target import/export and loader validation runs within each builder.
- Run `scripts/test_x4_idle_utilities.py --system-apps SYSTEM --broadcast-build BROADCAST_OUTPUT --alarm-build ALARM_OUTPUT`. It compiles the actual Battery, Calculator, Stopwatch, Alarms and Countdown controllers, selected shared adapter and actual product Light helper with the exact target defines/SDK. Hardware providers alone are doubles. Normal and ASan/UBSan cases cover clean typed wake, saved Bluetooth intent, low-battery timing, retained acquisition/panel failures, false app dependency acquisition and brightness OFF preservation. Waterfall's production capture state and suspension are exercised independently.
- Run `scripts/test_native_utc_alarm_apps.py` with the unchanged historical native System/Runtime sources for the expanded controller uncertainty tests.
- Run `scripts/build_native_utc_utilities.py` against its historical native pins for all 14 exact paper/Watch ELF comparisons. The alarm builder adds four more.
- Run `python -m unittest discover -s tests -p 'test_native_idle_build.py'` for version, complete-option and exact grant inventory checks.

These receipts qualify host execution and target structure, not physical hardware behavior. Frozen firmware 0.1.16 is not changed or republished.
