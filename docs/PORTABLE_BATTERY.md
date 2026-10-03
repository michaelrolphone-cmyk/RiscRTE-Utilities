# Portable battery telemetry

Battery 1.0.2 -> 1.0.3 keeps the existing shared app and row/navigation logic.
The basic footer now says "basic telemetry": the generic battery provider may
be a PMU rather than an ADC, and the app should not imply a particular device.
The System Apps portable client adapter can supply its existing drawing/input/
battery interfaces from generic runtime grants. No Watch-specific battery app is
created. Detailed charger/gauge views retain their existing behavior.

Historical migration/release snapshots remain unchanged. The current app is an
intentional development delta. Same-version release bytes must still match;
strictly newer versions are reported separately as development builds. Host
fixtures and target ELF/integrity validation remain required.
