# Optional shared foreground alarm integration

This dependent source increment pins System Appsf204477b287263fa6c5a2d095829caf86b327506
(PR24) and advances rebuilt application versions: Battery1.0.7,
Calculator/Stopwatch0.1.2, Alarms/Countdown0.1.1. The ordinary alarm-service0.1.0
source and canonical contract remain unchanged from PR11.

When an explicit deployment compiles PORTABLE_ALARM_CLIENT, the shared adapter
owns the settled-frame service step, in-place modal, exact-token dismissal,
Back interception and failure-only output cleanup. Alarm/Countdown writers
therefore do not repeat step or stop_only after poll. An uncertain output stop
never returns from the shared poll, so the writer cannot release its grants or
return. A safely stopped failed foreground returns false and the writer releases
only its own normal grants. Existing non-opt-in staged builds keep their prior
local servicing/cleanup behavior. The new fixture profiles test both paths.

No namespace, app grant, schedule record, recurrence or native output behavior
is added here. Final Watch integration and physical qualification remain separate.
The paired0.1.7 provider-storage SDK build fixture remains valid for the unchanged
service; the final Watch deployment will select the separately reviewed0.1.8
Runtime output/capacity prerequisite.

Native retained sleep is propagated separately by the shared adapter. In that
case Alarm/Countdown and both Stopwatch polling paths immediately return without
releasing their private grants; Runtime retains before module fini. The ordinary
failed-presentation path remains adapter-owned bounded stop_only followed by
normal writer cleanup only when safe. Dedicated native-retention fixtures cover
full and partial Stopwatch dependency opens and both writer kinds.

## Points-dependent client increment

The recurring Points branch selects System Apps PR27,
911be9e8042f1bcc46038fb70189eebe4ca106c5, to render copied service labels in the
shared modal. Rebuilt identities are Battery1.0.8, Calculator/Stopwatch0.1.3 and
Alarms/Countdown0.1.2. The legacy unopted-in provider remains0.1.0; the explicit
recurrence provider is0.2.0 with points-manifest.json. See POINTS_IN_TIME.md.
