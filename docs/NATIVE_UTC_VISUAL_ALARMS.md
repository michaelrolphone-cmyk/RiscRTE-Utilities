# Opt-in native UTC visual alarm service 0.4.3

This development profile builds the ordinary `alarm.service@1` singleton with
`ALARM_NATIVE_UTC`, `ALARM_VISUAL_ONLY`, `ALARM_DND_CONTROL` and
`POINTS_IN_TIME_SERVICE`. It does not activate a product, modify a Watch image,
write a release, or qualify physical e-paper/wake behavior. Select only
`native-utc-visual-manifest.json` with the matching ELF. The existing Watch
0.4.1 and raw-RTC visual 0.4.2 manifests, flags, keys and target bytes are
unchanged. Their exact base is Utilities
`23a4887f1eeb7b7ce867c0e243158b899e43f0f8`.

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
System Apps `d52a74bfb4acc01cc3f0a9dda2c95ba2ba679ee9`. Exact selected source and
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
existing 36-byte table, 40-byte visual descriptor and 104-byte status remain.
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

## Reproducible checks and limits

Run with existing dependencies and compiler, without installing tools:

```
ASAN_OPTIONS=detect_leaks=0 python scripts/test_native_utc_alarm.py \
  --system-apps /path/to/system --runtime /path/to/runtime-0.1.52
bash scripts/test_native_utc_alarm_runtime.sh /path/to/runtime-0.1.52 /path/to/system
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash scripts/test_native_utc_alarm_runtime.sh \
  /path/to/runtime-0.1.52 /path/to/system
NATIVE_APP_CC=/path/to/pinned-gcc8.4 python scripts/build_native_utc_alarm.py \
  --system-apps /path/to/system --runtime /path/to/runtime-0.1.52
```

Local leak detection is disabled only because the executor uses ptrace; address
and undefined-behavior checks remain enabled. CI uses sanitizer defaults.
Production-source fixtures cover cold UNSET, snapshot/2038 limits, DST gap/fold,
elapsed ends, timezone changes, replay/late delivery, token/ACK, storage faults,
DND, cancellation, clock discontinuity, prepare/resume and no I/O after custody.
A dynamically loaded native profile runs through actual Runtime/Graph/Module
with all nine bindings, no RTC or native-write grant, polling, sleep/ACK, cold
boot, and the real retain-invocation handshake. Only hardware/time/storage
boundaries are modeled. Existing Watch/raw visual, Alarm/Countdown, volume, DND
and real Runtime suites remain gates. The pinned compiler checks full ELF byte
identity for both flag-off profiles plus imports, exports and structure.

The production target loader checks all three ELFs at eight address alignments,
with independently compared relocated sections and allocation redzones. It does
not execute Xtensa instructions. Source/build records and logs are under
`docs/evidence/native-utc-alarm-0.4.3`. No physical board, battery, panel wake or
current-draw qualification is claimed. The consumed MIT license and Dave Allie
catalog notice are preserved in the build's license output and source pin.
