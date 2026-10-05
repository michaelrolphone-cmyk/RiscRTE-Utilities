# DND for all notifications

The opt-in ALARM_DND_CONTROL profile reads namespace1 alert_dnd: one byte0/1.
Missing means off with no implicit write; malformed/unreadable state fails closed.
It suppresses sound and vibration for Alarms, Countdowns and Points in Time.
Visual alarm/countdown alerts, Points bookkeeping, schedules and configured
output modes remain intact.

A due occurrence receives a durable silenced marker. An active occurrence first
stops both outputs, then verifies that marker while preserving its token and
original visual timeout. Turning DND off cannot replay a marked occurrence,
including across a service restart. Failed cleanup retains resources.

Unmuted occurrences retain SAO1/PTO2 encoding. Muted occurrences use SAO2/PTO3
with the same32/64-byte sizes and checksum; new readers also accept old records.
Older services reject the new muted records rather than replaying them. Deploy
the complete paired cohort together; downgrading code alone does not translate
new persisted occurrence records.

The service binds only the additional alert_dnd read key. The shared controls
write it through their existing namespace1 grant. The enabled profile adds one
DND preflight read to an output phase, for at most three dependency calls.

Local time-critical build: production-source focused due/active/interrupted/
restart checks and target compilation performed; broad CI/hardware verification
is deferred at the owner's request. No hardware qualification is claimed.
