# Recurring Points: ordinary Alarm service 0.2.1

Points in Time is a Productivity-owned optional end-user app. Utilities owns the
reusable recurrence/record helpers and this ordinary singleton provider ELF;
Watch owns the eight face renderers, app selection and existing owned sleep
integration. System Apps owns the shared in-place alert shell and time/display
policy. No date scheduler, RTC policy, callback, thread or timer is added to
Runtime or firmware. Reader remains read-only.

## Opt-in identity and unchanged ABI

Build service.c with POINTS_IN_TIME_SERVICE and select points-manifest.json:
alarm-service 0.2.1, ordinary driver ABI2, capability alarm.service@1. The separate
legacy build without that define still selects manifest.json (0.1.0), its two
one-shot records and exact five-key fixtures. Never ship a 0.2.1 manifest with an
unopted-in ELF or call newly built bytes an existing released artifact.

AlarmServiceV1.h is unchanged: 104-byte status, 16-byte token/sleep decision and
36-byte target table. schedules[0..1] remain the legacy Alarm/Countdown view.
Points token kinds are 3 + slot*2 + edge (3..18), with edge0=start and1=end;
revision/deadline/generation retain exact-token meaning. Generic consumers must
render status.label instead of reducing all kinds other than Countdown to ALARM.
A READY status proves a complete service reconciliation, but the unchanged
status does not expose a Points-specific confirmed revision. Consumers must not
claim stronger per-point arming confirmation than the API provides.

## Storage bounds and authority

Existing Runtime blob maximum is64 bytes, key maximum15 bytes, provider binding
maximum8 and selected app-policy capacity16. This design adds exactly two keys
to the existing five, with no generic capacity change:

- namespace5, points_cfg, service read-only and Points app writer
- namespace4, points_occ, service read/write; no app receives namespace4

Clock reads the same namespace5 config only at an explicit settled/startup
boundary, then renders from copied data. Namespace-scoped app grants are broader
than one key and are not per-key security isolation. Do not grant output or
provider-bound KV tables to an app.

Both records are explicit little-endian64-byte blobs with magic/version,
reserved-byte checks and FNV checksum. They are never native-struct writes.

Config: PTC1 magic4, catalog revision4, raw-RTC created4, eight packed6-byte
points, checksum4. Each point stores kind/enabled/mode, weekday mask (Sunday
bit0), hour, minute and duration minutes. Kind0 is canonical all-zero empty.
Populated kinds are Work Start, Work End, Lunch, Break, Bedtime; empty/disabled
slots are persisted without illustrative schedules. Duration0 disables the end;
1..720 minutes is accepted only for Lunch/Break. Mode0 follows Settings;
1=vibrate,2=sound,3=both. Missing alert_mode keeps the existing vibrate default;
malformed Settings mode blocks visibly. No text storage, heap allocation, event
history, erase or cross-key transaction is needed.

Ledger: PTO1 magic4, catalog revision4, occurrence generation4, active deadline4,
recovery-until4, slot/edge/state/resolved-mode4, sixteen civil-day highwaters32,
reserved4 and checksum4. Highwaters use the parent-start date, including an end
crossing midnight. Zero means never handled, day1 means2000-01-01; the last valid
day is36525. A PENDING entry is a replay exception to its already advanced
highwater. Revision and generation exhaustion fail closed. A reset replay
increments and verifies generation before exposing a new token.

## Save and recurrence semantics

Saving any point creates a new catalog revision with created=the current valid
raw RTC. This intentionally cancels all old-catalog alerts and derived duration
ends, including those from another unchanged point. The editor must explain this
small-format tradeoff. A start at or before created is excluded; its derived end
is also excluded even when that end would still be in the future. Retrying an
uncertain save must keep exactly the original bytes and creation cutoff. Put
success alone is not persistence proof: use exact readback before advancing UI.

The civil weekday comes from the existing selected display-time conversion.
All starts pass through PortableTime inverse and round-trip validation. On the
Watch deployment this is fixed UTC+08 raw RTC to America/Denver, selected by
PORTABLE_RTC_UTC8_DENVER. The same definition must be compiled into app, service
and face projection. A nonexistent or ambiguous start is skipped for that date;
neither possible fold offset is silently selected. Projection reports GAP/FOLD
flags, and the editor/face must explain the skip policy. End deadlines are elapsed
duration minutes after the unambiguous parent start, not independently converted
civil times. Date/RTC overflows never wrap.

The latest eligible past boundary per edge and the next bounded horizon are
computed directly. There is no per-missed-day replay loop. All expired edges
compact into one ledger write, then fresh due candidates compete with ordinary
Alarm/Countdown by deadline. Equal deadlines choose old kinds before Points,
then lower slot/start-before-end. One output invocation is active at a time;
simultaneous points can exceed the60-second recovery window and later ones may
expire. This is deterministic bounded delivery, not an unlimited notification
queue or an exactly-once audible-output guarantee.

## Failure, cancellation and sleep

The existing60-second raw-RTC recovery and20-second maximum invocation remain.
Before any sound/vibration the new ledger is persisted and exactly reread, the
RTC is sampled again, and the entire current config is reread/decoded/compared
in the same serialized phase as the first output operation. A committed catalog
edit observed there prevents old output. An already-playing alert is dismissed
with its exact token through the existing shell; editing cannot bypass it.
Cleanup independently stops haptics, silences and closes audio. Uncertain output
retains resources; stop_only never touches storage or acknowledges the ledger.
ACK becomes durable only after safe cleanup and verified ledger readback.

Detected awake RTC jumps, backward clocks, a clock earlier than catalog creation,
corrupted/reused/rolled-back observed records and contradictory active identities
block visibly. Explicit refresh retries without silently ACKing. Persistent
highwaters prevent old same-revision dates replaying after a backward time edit.
A fresh process cannot prove a missing key previously existed, and some underlying
NVS faults may look like missing; the existing documented storage limitation and
power-loss repeat/loss boundary remain.

prepare_sleep still reconciles fully and returns the earliest raw-RTC deadline
across legacy schedules and Points starts/ends. Clock consumes it immediately
with the already-owned Light/Deep timer path; the Runtime receives only duration.
Deep fresh-boot and Light return process due work before intro. No rendering,
frame-acquisition callback, provider poll or failed presentation may perform
normal service/storage I/O. All foreground failure paths preserve the existing
retention and cleanup contract.

## Verification

Run scripts/test_points_apps.py with exact System Apps and Runtime paths. It
builds raw and UTC+08/Denver policy fixtures both normally and with ASan/UBSan.
The production service source is used; coverage includes recurrence, end wake,
reboot/ACK/stale token, modes/Settings fallback, save cancellation before first
output, orphan-end cancellation, compact expired backlog, uncertain storage,
output failure, stop-only retention, invalid RTC, rollback and checksums.
Existing test_alarm_apps.py remains required and unchanged five-key tests pass.

scripts/build_points_service.py builds the real pinned GCC8.4 Xtensa ELF, checks
exports/imports, target ABI/ELF validity, source digests and distinct0.2.1 identity.
The target loader also checks this optional ELF when present. Exact Watch
cross-layer coverage, CI custody and real hardware wake/output/power-loss/current
draw qualification remain separate requirements. Building does not flash.

## 0.2.1 compiler-warning correction

GCC11 host CI reported a maybe-uninitialized candidate in the expired-edge path
under -O1 -Werror. Five service-local event candidates now have explicit zero
initializers; no warning is disabled. Shared Points headers, app helpers, ABI and
storage formats are unchanged. Pinned GCC8.4 produces changed service ELF bytes,
so this correction uses a new0.2.1 identity. Existing shared apps retain their
versions because their inputs and bytes are unaffected.

## Temporary factory schedule

Only a missing `points_cfg` uses the shared virtual revision-one defaults. A
present catalog, including an intentionally empty one, is never replaced. Invalid
or unavailable storage remains an error. Startup does not write catalog/metadata;
the service continues its ordinary durable occurrence bookkeeping. The first
user catalog edit is revision two and verifies missing default metadata before
saving the catalog; cancelling an edit writes nothing. Existing metadata wins.

Monday–Thursday only, all with sound and vibration (normal volume policy):

- 04:30 Wakeup, point event
- 05:30 Drive to Work, 15 minutes
- 06:00 Work, point event
- 09:00 Break, 15 minutes, warning at 09:12
- 12:00 Lunch, 30 minutes, warning at 12:27
- 14:15 Break, 15 minutes, warning at 14:27
- 16:30 Work End, point event

End notifications remain off. Duration ends remain visible in the schedule faces.
Drive to Work and Wakeup occupy the two default custom types. Metadata still has
64 bytes: existing PTM1 records retain their 12-character layout; PTM2 supports
13-character names, including the exact Drive to Work label. Old PTM1 metadata
remains readable. This does not change flash-storage overwrite behavior.
