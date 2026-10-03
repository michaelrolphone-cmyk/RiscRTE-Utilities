# Portable battery telemetry

Battery 1.0.2 -> 1.0.4 keeps the existing shared app and row/navigation logic.
The basic footer now says "basic telemetry": the generic battery provider may
be a PMU rather than an ADC, and the app should not imply a particular device.
The System Apps portable client adapter can supply its existing drawing/input/
battery interfaces from generic runtime grants. No Watch-specific battery app is
created. Detailed charger/gauge views retain their existing behavior.

Historical migration/release snapshots remain unchanged. The current app is an
intentional development delta. Same-version release bytes must still match;
strictly newer versions are reported separately as development builds. Host
fixtures and target ELF/integrity validation remain required.

A snapshot with SOC outside 0..100 displays `Unknown` in the charge row and footer, while valid voltage and charging data remain visible. The portable adapter uses UINT16_MAX for unknown SOC within the existing copied snapshot layout; no canonical header or ABI layout changes. Zero percent remains a valid known value.

The pipeline keeps current inventory and source-manifest versions in agreement,
while historical release provenance stays pinned to Battery 1.0.2. It accepts a
strictly newer Battery development version without hard-coding an intermediate
version in the provenance test. Same-version changed bytes and version downgrades
remain failures; newer versions are explicitly not reported as release parity.
