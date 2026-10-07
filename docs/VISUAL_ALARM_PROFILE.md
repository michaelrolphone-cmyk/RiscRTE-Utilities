# Visual-only Points and alarm service

The optional `--visual-only` build of `scripts/build_points_service.py` produces
`alarm-service` 0.4.2 in `dist/visual-points-service`. It builds the existing
service with `POINTS_IN_TIME_SERVICE`, `ALARM_DND_CONTROL`, `ALARM_VISUAL_ONLY`,
and unconverted RTC wall time. The Watch profile retains its UTC+08/Denver policy. It requires only
`storage.key-value.bound@1`, `platform.clock@1`, and `rtc.clock@2`. A board
without speakers or a haptic actuator needs no fake or unused output provider.
The Watch audio/haptic profile, manifest, and behavior remain unchanged.

The visual profile uses the eight explicit bound keys in
`visual-points-storage-policy.example.json`. It has no alarm-volume authority.
Portable preferences and durable occurrence records retain their existing
mode values 1–3. It never rewrites a saved Watch mode into a device-specific
value. Its copied `alarm_status_v1.mode` is `ALARM_MODE_VISUAL` (zero).

`alarm_service_outputs_v1` is an optional append-only descriptor containing the
unchanged `alarm_service_v1` prefix and `uint32_t output_modes`. The old target
function table is still 36 bytes; the extended visual table is 40 bytes. Copied
status remains 104 bytes. `alarm_service_output_modes(service)` reads the
extension only when `struct_size` permits it. It returns zero for visual-only
and the supported `ALARM_MODE_VIBRATE | ALARM_MODE_SOUND` bits otherwise. Legacy
providers required both physical backends and therefore return both bits.
An absent or invalid service is not proof of availability; callers still need
to validate/acquire the service normally. Consumers should label a successfully
acquired zero-output service "VISUAL ONLY" and disable physical-output choices
without changing saved preferences. Volume cannot create sound on this profile.

Points are visible foreground alerts on this profile, with the existing bounded
label/token/status and 20-second invocation limit, rather than Watch's 350 ms
non-modal physical cue. The same durable preactivation write/readback, cancellation
reread, exact-token dismissal, bounded replay window, DND mute marker, ACK, RTC
consistency checks, foreground-failure block and sleep deadline reconciliation
are retained. Normal cleanup and `stop_only` keep their bounded phase contract;
there is no physical output to stop. Quiesce never accesses revoked storage.

An integrating deployment must select the real alarm-aware retained foreground
adapter in every app and perform service reconciliation before sleep/after wake.
The service does not wake or draw independently. Physical e-paper visibility,
wake delivery, and full-board qualification require testing on the device.
Host fixtures and target ELF validation do not establish those hardware facts.

The real-source visual fixture covers copied descriptor compatibility, visible
Points and ordinary alarm delivery for every stored mode, durable ACK, replay and
stale-token rejection, storage failures, cancellation, storage-free stop-only,
DND, timeout, and the next timed sleep deadline. Existing Watch fixtures remain
part of the same CI suite.
