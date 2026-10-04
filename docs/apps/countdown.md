# Countdown 0.1.0 (staged)

A single persisted countdown from 00:00:01 through 99:59:59, with hour/minute/
second +/- controls, Start, Cancel and Retry. Its absolute raw-RTC deadline lets
it survive app handoffs and reset. While armed the unedited view shows remaining
whole seconds. Editing changes the proposed duration without modifying the
saved running countdown; Start commits a new revision. There is no Pause or
recurrence in this minimal slice.

The app writes only timer_cfg in namespace 3. The singleton owns occurrence
persistence, output and dismissal. A changed or invalid RTC fails visibly rather
than silently becoming a completed timer. Offline RTC adjustment limitations,
reset recovery and the uncompleted shared-overlay/low-power integration are
specified in the [service contract](../ALARM_SERVICE.md).

While a save is uncertain, the right control reads WAIT and cannot submit a
cancellation that accidentally retries an enabled record. Resolve the explicit
pending save with Retry Save first, then Cancel creates its own disabled revision.
