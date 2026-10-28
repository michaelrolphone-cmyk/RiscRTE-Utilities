# Explicit native Alarms / Countdown presentation

`build_badge_native_alarm_apps.py --native-alarm-presentation` selects the
additive `Apps/badge-native-presentation-alarms.json` profile. It does not change
service selection, clock-editor authority, foreground sleep selection or grants.
Those remain explicit independent flags. Omission retains the historical generic
profile, versions and compiler inputs.

The new flag adds `PORTABLE_ALARM_NATIVE_PRESENTATION`, `PORTABLE_NOVA_UI`,
`PORTABLE_APP_OWNS_TOUCH_CHROME`, `PORTABLE_RF_NATIVE_CANVAS`,
`PORTABLE_WATCH_SAFE_INSETS`, `PORTABLE_WATCH_NATIVE_HEIGHT`, and
`PORTABLE_WATCH_COLOR_ONLY`; the base recipe already selects
`PORTABLE_SAFE_VIEWPORT`. The application uses native primitives without selecting
`DAILY_NOVA_APP` or the daily-utilities application routing. The System adapter
owns the sole safe-origin/inverse-touch transform; application geometry is local
to the actual safe-content dimensions, not a fixed circular-panel resolution.

Build and owning-test receipts explicitly record `native_alarm_geometry`, selected
flags, profile and dependencies. Product graph CLI uses `--native-alarm-geometry`
and forwards this Utilities-specific flag. A selected profile is not evidence of
native-geometry qualification: target validation, normal/sanitized owning tests,
whole-product execution and modeled raster review remain separate gates.
Physical panel, wake timing and power behavior remain unverified.
