# Alarms 0.1.0 (staged)

One one-shot alarm, edited with hour/minute +/- controls. Arm Next selects the
next occurrence of that civil time today or tomorrow, using the existing shared
RTC display conversion. The time basis is shown. A DST gap or ambiguous fold is
explicitly rejected; there is no recurrence, snooze or timezone framework.
Cancel creates a new disabled schedule revision. Corrupted saved state is never
silently overwritten. Failed commits are reread and shown as unconfirmed until
verified or retried. Saving a record is distinct from the service confirming it.

The shared app writes only alarm_cfg in namespace 3. An ordinary singleton
service owns occurrence state and all output. Its exact-current alert can be
dismissed while this app is open. The common foreground overlay and low-power
wake integration remain explicit gates; this app is not a completed Watch
alarm delivery. See [service contract](../ALARM_SERVICE.md).

While a save is uncertain, the right control reads WAIT and cannot submit a
cancellation that accidentally retries an enabled record. Resolve the explicit
pending save with Retry Save first, then Cancel creates its own disabled revision.
