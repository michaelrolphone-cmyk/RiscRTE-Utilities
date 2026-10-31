# Countdown control provider

`countdown.control` API 1 is an elapsed-duration semantic interface. The shared application receives no RTC representation, time zone, display geometry, or device identity. `prepare(ARM, seconds, observed_revision)` accepts 1..359999 seconds; absent/disabled config presents default 300 seconds. `prepare(CANCEL, 0, observed_revision)` increments only the timer record revision. The app must persist the exact 96-byte opaque command before apply, and retry those bytes rather than recompute a deadline.

The provider reads/writes only `timer_cfg` (raw calendar) or `timer_utc_cfg` (UTC), bound read-write to existing configuration namespace 3. It neither accesses nor rewrites scheduler-owned namespace 4 occurrences, existing alarm records, Points records, volume, or time-zone preferences. It reports schedule slot 1 while preserving the original global-alert UI: any current scheduler occurrence is displayed and may be dismissed. Dismissal validates the entire copied token against the actual current scheduler identity before acknowledgement; schedule writes remain Countdown-only.

Raw and UTC adaptation is confined to provider builds. Raw elapsed time uses the deployed raw RTC calendar seconds directly; UTC uses epoch seconds with the existing record epoch. Neither performs local-calendar/DST arithmetic. Existing scheduler recovery, alert delivery, and output ownership remain unchanged. API2 scheduler output modes are disclosed through HAS_SOUND/HAS_HAPTIC and VISUAL ONLY status. The raw API1 compatibility composition requires the existing sound/haptic scheduler.

The UTC recipe enables the published clock-editor descriptor guard. Prepare and apply refuse mutation during a live clock-edit intent. Exact after-image replay reconciles without another record write; uncertain write/readback keeps the command retryable, while mismatched before-images are stale. RETAINED fences prohibit further provider calls or teardown.

Run `scripts/test_countdown_control.py` with pinned Runtime/System source roots, optionally `--sanitize`. `scripts/build_countdown_control.py` builds reproducible raw/UTC provider packages using the selected System target builder. Those provider variants do not create app variants. Namespace binding, final product admission, target execution, and physical behavior require separate consuming-product qualification.

Source foundation: published Utilities 39953161f51c291e7853dd42bdccfce959ce7fe3 alarm_control/AlarmRecords contracts. No unpublished Alarms preferences extension is included.
