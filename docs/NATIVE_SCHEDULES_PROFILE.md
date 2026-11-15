# Explicit native visual Alarms / Countdown service

This optional development profile selects only the two existing native-UTC
schedule kinds. It does not install apps, select a product provider, arm a
hardware timer or complete sleeping alarms. Physical behavior is unverified.

## Service selection and authority

`native-utc-schedules-manifest.json` selects alarm-service0.5.0, tagged API2.
Build flags: `ALARM_NATIVE_UTC`, `ALARM_NATIVE_SCHEDULES_ONLY`,
`ALARM_SERVICE_TAGGED_V2`, `ALARM_VISUAL_ONLY`, `ALARM_DND_CONTROL`.
The selection rejects simultaneous Points/Points Catalog flags. Historical
native Points and legacy Watch profiles retain their exact default behavior.
No disabled Points config is written or synthesized: unselected Points keys
are neither read nor changed, whether missing or already populated.

The provider needs platform.realtime@1, platform.clock@1, and exactly six
storage.key-value.bound@1 keys, from the example policy:

- namespace3, readonly: `alarm_utc_cfg`, `timer_utc_cfg`;
- namespace4, read/write: `alarm_utc_occ`, `timer_utc_occ`;
- namespace1, readonly: `alert_mode`, `alert_dnd`.

There is no namespace5, raw RTC, time-control, audio/haptic, catalog or AppData
provider grant. Absolute schedules already contain UTC deadlines; timezone
conversion belongs to the writer/display apps, not this service. Its record and
sleep-ticket domain remains seconds since2000, bounded by native Unix2038;
never mix those fields directly with Unix epoch values.

The ordinary verified-write, durable occurrence replay/ACK, DND, bounded
reconciliation, freshness and terminal-custody algorithms are reused. API2
consumers must validate the tagged descriptor, handle every operation result,
and stop all ordinary callbacks after ALARM_RETAINED. This explicit profile
also fences unknown results from native realtime reads and bound KV get/put;
get-only codes returned by put are invalid for that operation and fence custody.
No verify, clock, storage, cleanup or restart callback follows such uncertainty.
Typed KV INVALID/BUSY are clean refusals (block without verification); typed IO
retains its documented possibly-committed/readback reconciliation semantics.
Historical profiles keep their prior behavior. A valid prepare_sleep
result is only a service decision/ticket, not authority to sleep hardware.

## Current pinned build and evidence

`build_native_schedules_alarm.py --runtime R --system-apps S --cc GCC --output O`
requires exact clean Runtime273bdbc and System6e4cda9 and compiler15.2
esp-15.2.0_20251204. It records complete compiler command/dependency hashes,
exports/imports, manifest and six-key policy, then runs the actual Runtime
Xtensa loader across eight alignments. It also rebuilds legacy Watch and native
Points selections with pre-change Utilities1eb8948 source and proves identical
ELFs using the same compiler/SDK. Historical release compiler expectations and
profile files are not rewritten.

`test_native_schedules_alarm.py --runtime R --system-apps S --output O` runs
normal and ASan/UBSan owning fixtures plus actual Runtime/ProviderGraph/Module
execution with explicit file sources, exact six-key policy, tagged client,
UTC due/ACK, UNSET refusal and retained context. Hardware/time/storage and host
instruction mapping are modeled. Standalone tests cover absent/populated Points
isolation, due/replay/ACK/cancel, stale/reused tickets, malformed time, DND,
IO-before/after-commit, typed clean refusals, unknown positive/negative and
CONTEXT results for every error-returning provider operation (including resume),
and terminal no-callback boundaries. Monotonic time has no error return; backward
and range-invalid values are separately rejected. The historical
native Points behavioral fixture runs unchanged alongside the new selection.

## Integration still required

Select the service and exact bound-key policy in a future product graph. Grant
API2 only to checked consumers. Home Deep and foreground Light must be adapted
to fresh deadline/ticket custody, timer+key wake, terminal errors and restoration;
then qualify real product paths and device acceptance. This profile alone does
not establish sleeping alarms, durable Deep application recovery or sound.

The service builder and owning test script accept `--system-revision` with an
exact lowercase 40-character SHA; the default remains historical System
`6e4cda9072905c15b9a5ebc9b22913fec444a89d`. The selected checkout must be clean
(including untracked files) and its HEAD must equal that pin. Receipts report the
selected SHA; Runtime, service ABI, six-key policy and legacy byte-parity checks
are unchanged. This permits explicit successor qualification, not an implicit
profile upgrade or sleeping-alarm integration claim.
