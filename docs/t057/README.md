# T057 OBD Test 0.2.4

The original OBD 0.2.2 executable and registration must remain installed.
Only the separately registered OBDTest executable is updated. Its settings and
CSV files remain isolated under /Apps/obd-test and /OBD-Test.

Existing work retained: 0.2.2 parser/deadline/backpressure and BLE serial fixes;
0.2.3 idempotent checkbox values, deferred settings writes and bounded raster
pumping. The actual-app tests still exercise taps while a frame is pending and
scrolling during live BLE. Their 10 ms and 230 ms timings are host fixtures,
not physical measurements.

New changes:
- An ELM handshake failure uses the existing bounded reconnect budget. Each
  retry closes/reopens transport, resets ELM state and never replays a code-clear
  command. Three unsuccessful retries stop; only ten seconds of fresh ECU RPM
  resets the budget. Cancel and retained/reload-required custody still stop work.
- New settings default to mph; existing CRC-valid saved unit choices remain.
- GRAPH -> MOTION HISTORY plots velocity, acceleration and jerk over 30 seconds,
  two minutes or ten minutes. Velocity follows the unit selector. Derivatives
  use SI units and actual fresh ECU sample intervals; jerk uses the distance
  between adjacent acceleration interval midpoints. Duplicate reads do not
  create samples. Stale data and reconnects break derivative continuity.
  These finite differences inherit OBD speed quantization and noise; no
  calibrated accelerometer or measured vehicle dynamics are claimed.
- Recording polls all 23 implemented sensor PIDs that the ECU reports as
  supported, irrespective of dashboard selection. Exclusive-create CSV files
  contain session elapsed_ms, ECU, PID, canonical value_milli and unit. No
  wall-clock timestamp is invented: this app has no RTC grant. Automatic
  reconnect retains one recording file; explicit stop/disconnect/exit or exhausted
  retries closes it. Pre-recording samples are omitted. SD failures remain visible.
- Settings offers session landscape on scene telemetry API version 2. The shared
  presenter owns raster and input transforms. Rotation closes/reopens the scene
  only after input and presentation settle, without closing BLE or recording.
  Older providers show an update requirement for landscape while other features
  remain usable. Update the paired scene provider to enable landscape.

Run scripts/test_obd_dashboard.py with --runtime, --system, --drivers and --output
paths, then repeat with --sanitize --test for the isolated test identity.
Use the System T057 committed telemetry fonts to preserve the delivered X4 raster. Build with scripts/build_obd_dashboard.py
using the same paths and --test. Dependency pins and target hashes are recorded
with the published artifacts. No RF, vehicle or physical display qualification
has been performed.
