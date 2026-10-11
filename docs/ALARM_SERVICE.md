# Shared Alarm and Countdown development slice

Alarms 0.1.0, Countdown 0.1.0 and alarm-service 0.1.0 are original Utilities
sources. They are staged in an explicit development inventory, separate from
Calculator/Stopwatch and the unchanged Reader migration cohort. Nothing here
repins a Watch image, changes Runtime, installs a package or qualifies hardware.

## Actual architecture and authority

`Services/alarm_service/service.c` is an ordinary singleton ABI-v2 provider ELF,
with entry `t5_driver_get`, manifest type `driver`, instance 0 and opaque
`alarm.service@1`. The existing loader has an ordinary singleton route rather
than a separate service type/catalog, so no new repository or firmware service
mechanism is invented. It depends on the selected `rtc.clock@2`,
`haptic.effect@1`, `audio.output@1`, trusted global `platform.clock@1` and the
reviewed `storage.key-value.bound@1` table. A deployment must select exactly one
provider per peripheral capability. The expected Watch providers are RTC device
8, haptic 9 and speaker 12; these are deployment facts, not constants in service
code. No fake hardware instance is assigned to the service.

The app writers have existing writable `storage.key-value@1` namespace 3.
They deliberately share schedule authority and can access each other's keys;
this is not per-writer security isolation. Only `alarm_cfg` and `timer_cfg` are
written by their implementations. The singleton's exact five-key map is:

- namespace 3 read: `alarm_cfg`, `timer_cfg`
- namespace 4 read-write: `alarm_occ`, `timer_occ`
- namespace 1 read: `alert_mode`

`storage-policy.example.json` describes the mapping, not an installable boot
file; the real boot policy encodes it as the existing key_value entry array.
Settings owns alert_mode: one byte, 1=vibrate, 2=sound, 3=both. Missing preference
means vibrate; malformed preference fails visibly. Settings integration is still
required. No app receives namespace 4, output authority or the provider KV table.

Every schedule and occurrence is one checksummed, versioned, little-endian
32-byte record. Schedule revisions and occurrence generations are independent
monotonic uint32 values; exhaustion fails closed. App edits cannot overwrite
acknowledgments. An equal-generation occurrence cannot change identity or state behind the service.
There is no cross-key transaction, erase or automatic repair.
The service rejects observed revision/generation rollback, revision reuse with
changed schedule bytes, corrupted records and contradictory occurrence data.

## Cooperative execution and copied status

`AlarmServiceV1.h` is owned by Utilities, not by the Runtime SDK. Its bounded
operations copy status, perform a phase, request reconciliation, acknowledge an
exact occurrence, prepare a sleep decision and stop outputs after foreground
failure. Static assertions
fix the value-only token/status/sleep layouts and 36-byte, 32-bit target function table.
A caller sets struct_size. No consumer pointer or callback is retained.

There is intentionally no provider poll callback. One explicit `step` normally performs one storage, RTC or output-table
operation. The final schedule reread and first mode-specific output operation
share one serialized phase with at most two dependency calls, so a cancellation
committed before that read cannot start stale output. Later cancellation uses
exact-current dismissal once output has begun. The caller must invoke it
at a settled-frame/input safe point and must continue while active. Steps never
invoke graph lifecycle operations. A reentrant callback is rejected as BUSY.
Normal idle reconciliation is requested each second; explicit writer refresh
restarts a partially read snapshot. Writes are always reread before an armed
revision or occurrence transition is relied on.

This is a bounded count of synchronous operations, not a hard wall-clock bound:
NVS has no hard latency guarantee. Existing RTC/output tables also contain
bounded peripheral transactions whose total driver call may exceed the graph's
8 ms poll allowance. Never place this step inside provider poll or call it from
an in-flight display callback. Awake monotonic consistency samples require less than
UINT32_MAX ms between reconciliation points; a longer awake gap fails visibly.
The explicit successful-Light boundary below starts a new awake comparison.

## Occurrence, recovery and cleanup

A due record is durably persisted and exactly reread before any output starts.
Its identity is kind + schedule revision + deadline + occurrence generation.
A fresh RTC check before output catches invalid/backward or inconsistent time.
A reset replay increments and verifies the stored generation before exposing the
new active token. Tokens from an earlier active generation cannot dismiss it.
Repeated acknowledgment of the still-current durably acknowledged occurrence is
idempotent, including after reloading its record. Superseded identities fail.

The fixed recovery window ends 60 raw-RTC seconds after the scheduled deadline.
A late startup beyond that window persists EXPIRED and never surprises the user
with an old alert. An invocation lasts at most 20 monotonic seconds and is
shortened to the remaining recovery window. RTC resolution is whole seconds;
this is not a subsecond accuracy promise. Sound is a genuine 8 kHz signed-PCM
square-wave pulse (256 mono frames every 500 ms), not a success-returning silence
stub. Both mode requests real audio and haptic output. Backend failure is exposed.

Dismissal queues only the exact current identity. Cleanup independently attempts
haptic stop, audio silence and audio close in separate phases. Close must prove
physical quiescence; queued zeros alone do not. Unsafe output state remains
visibly blocked. Only safe cleanup followed by confirmed persistent ACK produces
DISMISSED. IO from put is uncertain; exact readback can establish success, and a
failed read or mismatch blocks until explicit retry. Activation cannot sound
until its write is verified. Quiesce never accesses storage because Runtime
revokes provider KV before cleanup, and failed cleanup retains resources.

The existing backend may report some SDK-hidden lookup faults as missing; a
fresh service cannot prove that a missing record never existed. Observed
contradictions fail closed, but no stronger storage guarantee is invented.
Power loss can repeat or lose an alert around storage/output boundaries. There
is no exactly-once audible-output guarantee. Persisted recovery windows bound
replay. Offline forward RTC edits cannot be distinguished from elapsed time;
offline backward time earlier than the schedule creation is rejected. Detected
awake RTC jumps or invalid clocks require explicit retry. After an explicit
retry the new RTC basis is accepted, but an old creation bound still applies.

## Sleep and foreground integration contract

`prepare_sleep` requests a new full reconciliation and returns PENDING until a
fresh RTC-backed decision is ready. No output or uncertain cleanup may be live.
It returns the next raw-RTC deadline, or zero when none exists, and a copied
snapshot number. The caller must enter owned timed sleep immediately on the
same serialized task without another service or schedule-writer call. No
suppression Boolean survives refusal. A caller using the optional sleep suffix
reports successful Light return before resuming reconciliation; Deep must
instantiate the service and process due work before the normal intro.

This API does not itself enter sleep. The reviewed owned crown + timer backend
must be integrated by Clock. The common adapter must provide an in-place alert
before Back/request_launch handling; Calculator/Stopwatch RAM stays in the same
invocation. An old adapter built with PORTABLE_RETURN_APP queues a handoff before
the app sees Back and is therefore unsuitable for final alarm integration.
The staged writer builds omit that define and provide only their own local
alert/dismiss view while open. They do not solve delivery from other apps.

A false poll is not a settled presentation barrier. The staged writer never calls
normal step afterward. It calls stop_only, at most three calls with one output
operation each, to independently stop haptic, silence and close audio. This path
never touches storage, RTC, output start/write or graph lifecycle; it leaves the
same durable occurrence and token pending and the service blocked. A healthy
foreground must explicitly retry or dismiss. It preserves an already queued
dismissal. If physical stop is unconfirmed the failed app emits a diagnostic,
retains grants and yields without any further automatic I/O or handoff. It
cannot promise a visible error through a broken display. This deliberately
requires recovery/reset rather than guessing that resources are safe.

The final integration still needs a small explicit shared-adapter settled/error
barrier before normal step, Back interception before request_launch, and a
verified backend guarantee that output cleanup owns its buffers and can safely
stop while an unrelated display presentation is pending. Those are concrete
integration contracts, not assumptions justified by host mocks.

## Explicit Light-return clock boundary (0.1.1 / 0.4.2)

The ordinary service is now 0.1.1 and the Points/volume/DND variant is 0.4.2.
The existing `alarm.service@1` table remains exactly 36 bytes on the target.
`alarm_service_sleep_v1` adds one optional callback at offset 36 and has a
40-byte target size. The provider advertises that larger size in its unchanged
base prefix. Old consumers continue using the 36-byte prefix; a sleep owner
must have an exact matched paper-family provider in addition to checking API
version, size and non-null callback. A visual v1 lineage assigns a scalar to the
same offset: size alone cannot identify it. New integrations must use the
[tagged API-2 contract](ALARM_ABI_RECONCILIATION.md). No copied status, occurrence, sleep-decision,
durable record, namespace or dependency layout changes.

A valid external calendar and platform monotonic duration can diverge across
Light sleep because they may use independent oscillators while the CPU is
suspended. The awake two-second discrepancy check must not be applied across
that boundary. This does not establish that any particular device has drifted.

A successful `prepare_sleep` now retains its exact copied decision as a one-use
ticket. The serialized sleep owner calls `resume_sleep(context, &decision)`
only after native Light sleep returns OK, before another mutating service or
schedule-writer call. Native refusal never grants this reanchor. The callback
validates the ticket and makes exactly one fresh RTC read. Read failure, invalid
calendar/weekday, RTC earlier than the pre-sleep sample, or backward monotonic
time blocks with RTC error. On success it consumes the ticket, reanchors both
clocks, starts full reconciliation and returns OK. It does not start output,
write storage, acknowledge an occurrence, prove a deadline is due, or enter sleep.
Hybrid must call it before obtaining its next sleep decision; a normal Light
wake calls it before ordinary reconciliation. Deep reset retains the original
fresh-service recovery path.

Wrong/reused tickets return STALE before I/O. `step`, `refresh`, `acknowledge`,
`stop_only`, another `prepare_sleep`, provider start/stop and failure invalidate
an outstanding ticket. Memory-only `status` leaves it intact. Once reanchored,
the existing READ_RTC and ACTIVATE_RTC two-second awake checks remain unchanged.
Callers must preserve a failed resume/reconciliation rather than silently
invoking the explicit-retry `refresh` path. Recovery-window expiry, cancellation
checks and durable generation/acknowledgment logic are unchanged.

A ticket is a serialized lifecycle contract, not proof from hardware or a
security credential. A caller must not report an awake interval or refused
sleep as successful sleep. Forward edits during suspension are indistinguishable
from elapsed RTC time with the existing calendar-only interface; the pre-sleep
backward bound remains enforced. No new RTC precision or hardware qualification
is claimed.

`test_alarm_sleep_resume.py` runs the actual provider in legacy and exact
Points/Denver/volume/DND profiles, normal and ASan/UBSan. It covers divergent
independent clocks, Hybrid's immediate second decision, one-use/stale tickets,
reentry, every ticket invalidator, native-refusal behavior, invalid/backward RTC
and monotonic rejection, awake and activation-time jumps, late expiry and a
fresh Deep-style restart. Native Watch integration and physical timing checks
are separate requirements.

## Concrete remaining integration gates

- Complete and independently verify native I2S TX plus speaker close/partial-open
  retention and haptic GO clearing/partial-start cleanup. Existing table shape
  alone does not prove those semantics. Do not claim sound works from this PR.
- Add common in-place overlay safe points and Clock's owned Light/Deep timed wake
  flow, with RTC reevaluation on refusal/wake and due checks before intro.
- Add Settings' exact namespace-1 alert_mode control and explicit consumer grants.
- Resolve Runtime 0.1.7's eight app-policy limit: current Watch has seven
  manifests; two new apps need nine, and Timecard needs ten. This PR does not
  silently increase Runtime's bound or remove existing apps.
- Rebuild all affected distributables with fresh versions/exact pins, then test
  real physical-driver host flows, paired target ELFs/imports/ABI and final exact
  CI/artifact custody. Physical qualification remains separate.

Until these are connected, this is implementation progress rather than a
completed low-power Alarm/Countdown delivery. No awake-only substitute is shipped.

## Validation and provenance

Build uses exact System Apps b28428505c9e067bb6ea8a84d12f13ec0bcc4992 and Runtime
0.1.7 b2fc83280c54ca3ebd567184cc50e4785daa3951 (tree b3399091995ec67d84a151e8e18be59fe4475ee9).
Runtime headers are consumed directly, not copied/modified. The RTC consumer
header comes from the pinned shared client; haptic/audio declarations are
verbatim consumer-only copies from Watch 9cfa2aa4d572a290b41cf040a27cbec9d78bb35c.

- test_alarm_apps.py executes actual record, writer-app and service source with
  targeted fault injection, normal plus ASan/UBSan.
- test_alarm_runtime.sh builds the production service into a host-mapped ordinary
  ELF and runs actual Runtime, Graph, Module and dynamically loaded client ELFs.
  Only hardware and storage boundaries are modeled; namespace separation and
  app handoff use the real broker/loader. This is not a pure service mock.
- build_alarm_apps.py builds all three real Xtensa ELFs and checks imports,
  exports, structural validation and exact source provenance.
- test_alarm_target_loader.sh uses paired Runtime production loader/relocator,
  eight target address alignments, independent relocated section comparisons
  and allocation redzones. It models target layout and does not execute Xtensa
  instructions. Harness derives from the previously reviewed daily-tool audit.
- Existing source/parity tests and Calculator/Stopwatch tests remain required.

Pinned GCC 8.4 CI is the target build authority. Local GCC 14 checks are additional
controls only. Local sanitizer leak checking may be disabled in ptrace-based
execution; ordinary CI runs unmodified sanitizer defaults.

See [next scoped integration requests](ALARM_INTEGRATION_REQUEST.md) for the exact
Runtime capacity and smallest shared-adapter barrier proposals.
