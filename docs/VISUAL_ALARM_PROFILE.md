# Tagged raw-RTC visual alarm profile 0.4.4

This development profile combines PR45's visual behavior with the checked
paper sleep boundary. Build with `POINTS_IN_TIME_SERVICE`, `ALARM_DND_CONTROL`,
`ALARM_VISUAL_ONLY`, and `ALARM_SERVICE_TAGGED_V2`. It provides
`alarm.service@2`, with a validated descriptor output mask of zero.

The service resolves only storage, monotonic clock and raw RTC. It does not
resolve audio/haptic dependencies or invent physical output support. Persisted
preferences remain unchanged; copied effective mode is visual. Points are modal
visual alerts with the existing bounded invocation window, explicit ACK and
verified persistence, instead of the physical short non-modal cue behavior.

Use the eight-key visual storage policy and matching manifest. Raw RTC record
semantics remain distinct from the native UTC profile. Cleanup, DND, restart
replay, cancellation, foreground failure and checked sleep boundaries are
production-source tested. See [the ABI integrator contract](ALARM_ABI_RECONCILIATION.md)
and [native UTC profile](NATIVE_UTC_VISUAL_ALARMS.md).
