# Optional shared foreground alarm integration

This dependent source increment pins System Apps751daeed69ecd0c8886e593f519e66a9408304d7
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
