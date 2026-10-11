# Native Alarms and Countdown broadcast profile

The existing native builder accepts `--ble-broadcast` only alongside explicit
paper transitions and its pinned System adapter. Selected versions are Alarms
0.2.9 and Countdown 0.1.13. Both declare telemetry.broadcast API1 instance0,
using existing namespace1 preferences. Missing broadcast settings default OFF;
Bluetooth-off and airplane remain authoritative. The provider and native alarm
API2 versions are unchanged.

The app-owned KV and alarm-service facades pause advertising before calling an
external operation that can retain the invocation. Pause/release uncertainty
prevents that operation and latches the existing custody fence. Existing UTC
schedules, occurrence ownership, save/retry behavior and Watch/raw paper paths
remain unchanged. Quick Controls, Home, draft, pending-save and error regressions
run through the actual controllers and shared adapter.

Normal and ASan/UBSan composition tests include both display geometries, existing
motion/retention scenarios, active advertisement before private storage, failed
pause and failed service-grant release. Xtensa imports/exports and real loader
validation are required; the builder also compares four flag-off Watch/raw-paper
ELFs to the existing baseline. Physical advertising and hardware timing are not
qualified by these fixtures.
