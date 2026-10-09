# Opt-in native UTC visual alarm service 0.4.4

This profile derives from PR45, with the tagged API-2 descriptor and checked
Light ticket reconciliation described in [the integration contract](ALARM_ABI_RECONCILIATION.md).
Build with `ALARM_SERVICE_TAGGED_V2`, `ALARM_NATIVE_UTC`, `ALARM_VISUAL_ONLY`,
`ALARM_DND_CONTROL`, and `POINTS_IN_TIME_SERVICE`. Matching API-2 consumers and
grants are mandatory. This is development source, with no product activation.

## Authority and time domain

The profile has exactly three dependencies: `platform.realtime@1`,
`platform.clock@1` and `storage.key-value.bound@1`. It requires Runtime 0.1.52
`30dcec5ce6ce33223f2b203a2399283e1f758567`. The readonly provider table includes
`sdk/driver/RiscPlatformRealtimeV1.h` and the canonical app SDK
`RiscRealtimeV1.h`; include both SDK directories. It has no seed/write method,
RTC provider, audio, haptic, volume, RTC-basis preference or general KV grant.

Only a valid native UTC snapshot is usable. Cold `UNSET`, invalid fractions,
reserved fields, reversed monotonic brackets, malformed snapshots, native I/O
failure and out-of-range epochs block with `ALARM_RTC`. The supported Unix
range is 946684800 through 2147483647 inclusive, the native 2038 limit.
Schedules additionally reserve the existing 60-second recovery tail and the
full elapsed duration. No signed or unsigned date wrap is accepted.

Absolute fields in all records, tokens, status and sleep decisions are **UTC
seconds since 2000-01-01**, obtained by checked subtraction of 946684800 from
Unix seconds. They are not Unix seconds and not raw RTC wall time. The existing
field name `rtc_seconds` and value-only ABI remain unchanged. A qualified client
must know this explicit profile and use the same UTC domain. Do not pair it with
an old raw-time writer or an unqualified shared foreground client.

## Separate durable records and full-install boundary

The example policy contains exactly nine bindings:

- Namespace 3 readonly: `alarm_utc_cfg`, `timer_utc_cfg`
- Namespace 4 read/write: `alarm_utc_occ`, `timer_utc_occ`, `points_utc_occ`
- Namespace 5 readonly: `points_utc_cfg`
- Namespace 1 readonly: `alert_mode`, `alert_dnd`, `time_zone`

All names fit the existing 15-byte key limit. No old key is read, copied,
rewritten, erased or interpreted as UTC. The ordinary 32-byte records and
64-byte Points config retain their bounded formats, but their new key names
are a mandatory domain boundary. The 64-byte native Points ledger has separate
`PTU1` magic; bytes 50–51 store the active occurrence's frozen catalog index,
byte 49 and bytes 52–59 must remain zero. Old PTO ledgers are rejected in this
profile. The checksum still covers bytes 0–59.

A future full install must select matching native-UTC service, writers, face
projection, retained foreground adapter and sleep client together. This change
provides helpers, not those product activations. An old raw-time schedule must
be explicitly recreated by a qualified writer after a valid UTC clock is
available. No automatic migration is included. Existing raw data stays intact
for its own profile; namespaces remain shared authority, not per-key app
security isolation. `points_utc_meta` is reserved by the future writer helper
and is not a service binding.

Missing Points config retains the existing virtual factory schedule under the
selected timezone, without persisting a config. Missing `time_zone` is a virtual
UTC rule and is never written. A present malformed, noncanonical, corrupt or
unavailable timezone preference blocks visibly; it is not silently repaired.
The readonly decoder accepts exactly the shared 44-byte TZ1 format.

## Frozen local-time projection

The profile links the unchanged pure C timezone core and 419-entry catalog from
System Apps `1d589d90bf27c7ffb76420de46564088ddb3714f`. Exact selected source and
license hashes are recorded in `sdk/native-utc-alarm-sources.json`; a newer
checkout is accepted only when those selected bytes match. These are frozen
representative recurring rules from the Reader catalog, not complete historical
IANA tzdb or a claim of current legislative accuracy. There is no network update,
process TZ, libc calendar, allocation or persistent timezone write.

`PointsUtcSchedule.h` exposes pure functions accepting an explicit resolved rule:
checked Unix/UTC2000 conversion, local day, event-for-day, next/previous
projection, latest eligible edge and due selection. A future Points writer and
face must use these hooks with `ALARM_NATIVE_UTC`, the same frozen core and the
new keys. DST gaps and folds are skipped explicitly and reported as flags; an
ambiguous offset is never selected. End and warning deadlines are elapsed
minutes from a unique UTC start, including across DST transitions.

A timezone edit changes future civil projections on the next reconciliation or
sleep preparation. A durable pending occurrence keeps its original absolute UTC
identity, original zone index, generation/replay and remaining recovery window.
Its exact-token dismissal still works after a timezone change or reboot.
Existing civil-day highwaters prevent a second same-revision delivery when a
zone change moves a local clock backward. They can consequently suppress an
additional local date after travel; they are deliberately not reset. A new
catalog revision has the existing explicit save/cancellation semantics.

## Custody, foreground and sleep

The existing Alarm/Countdown/Points ordering, 60-second recovery, 20-second
visual invocation, durable preactivation verification, exact cancellation
reread, DND marker, stale-token rejection and ACK verification remain. The
service has no poll callback. A Runtime yield/poll alone does no service I/O;
normal steps belong only at an authorized settled foreground safe point.
`stop_only` after an ordinary foreground failure is storage/time-free and does
not acknowledge the durable occurrence. Light return and fresh deep boot must
reconcile before the normal intro. `prepare_sleep` returns a fresh UTC2000
snapshot/deadline consumed immediately by the serialized UTC-aware caller.

`ALARM_RETAINED` is the additive integer constant -9. It is a macro so adding
it does not renumber GCC8.4 private symbols and change old target bytes. The
base field offsets and 104-byte status remain; the tagged API-2 descriptor is 56 bytes.
A native-time or bound-KV `CONTEXT` response during an authorized service phase
conservatively latches invocation custody. This is an access denial, not proof
of an electrical fault. Copied status is `BLOCKED`, error -9, with
`output_uncertain=1` meaning safe handoff has not been established.

On the first -9 or copied retained status, a qualified new-profile consumer must
call Runtime `retain_invocation` and stop further status/step/storage/render/
cleanup/release activity. Old default clients are not qualified consumers.
The provider itself prevents all later dependency I/O, makes `refresh`,
`acknowledge`, `prepare_sleep` and `stop_only` return -9, and makes `quiesce`
fail without dropping its tables. `status` remains memory-only for diagnostics,
including while active or retained. Only a fresh process/reset clears that
fence. A transient native `IO`/UNSET is instead retryable `ALARM_RTC`; ordinary
storage IO preserves existing exact-readback reconciliation.

## Validation

Current checks and receipts are in [the reconciliation contract](ALARM_ABI_RECONCILIATION.md).
Prior PR45 receipts are historical and are not claimed as tests of this branch.
