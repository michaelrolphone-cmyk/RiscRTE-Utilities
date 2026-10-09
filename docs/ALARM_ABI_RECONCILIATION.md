# Alarm descriptor reconciliation (development 0.4.4)

This branch combines the alarm-specific changes from:

- Public paper: `bb81ab0cdc137e1b2fddcfbe191880c1c6e649ad`.
- PR45 visual/native UTC: `4be6383256d0742dd724538d720bd5b5d3802f89`.
- Read-only Runtime: `30dcec5ce6ce33223f2b203a2399283e1f758567`.
- Read-only System: `1d589d90bf27c7ffb76420de46564088ddb3714f`.

Remote refs and PR45 were checked on 2026-10-07. PR45 is open/draft and
reserves 0.4.3; no existing remote service manifest or tag reserved 0.4.4 at
that check. The new opt-in profiles reserve 0.4.4. No release/tag is created.
There is no Utilities AGENTS.md at either source ref.

## Why a new capability version

Both source lineages expose `alarm.service@1` and retain a 36-byte target base.
At offset 36, paper puts `resume_sleep`; visual puts `output_modes`. Both tables
are 40 bytes on Xtensa. Neither size nor alignment identifies the meaning.
A source-level compatibility hazard does not prove a delivered defect. The
current exact-artifact audit reports matched delivered families; this change
makes no contrary shipped-defect claim.

The old v1 header is unchanged. `AlarmServiceV2.h` defines `alarm.service@2`:

| Target offset | Field |
| --- | --- |
| 0–35 | Original field offsets, api_version=2, struct_size=56 |
| 36 | tag = 0x414c4432 |
| 40 | descriptor_version = 1 |
| 44 | output_modes: 0 visual, or physical mode bits 1/2 |
| 48 | features: bit 0 identifies checked sleep resume |
| 52 | resume_sleep callback |

Copied status (104), occurrence token (16), and sleep ticket (16) are unchanged.
`alarm_service_descriptor()` first checks API 2, then bounds, tag, descriptor
version, masks, callback/feature consistency and base callbacks. It **never**
reads a v1 suffix. Size checks bound access; they do not select a lineage.
Unknown descriptor versions/flags fail closed. No pointer/scalar heuristic is
used. `alarm_service_resume()` validates before dispatch.

| Consumer | Paper v1 | Visual v1 | Tagged v2 |
| --- | --- | --- | --- |
| Old prefix-only client | Prefix operations | Prefix operations | Reject |
| Old paper suffix owner | Matched family only | Unsafe/unqualified | Reject |
| Old visual suffix reader | Unsafe/unqualified | Matched family only | Reject |
| New descriptor consumer | Reject extension | Reject extension | Validate tag then use |

Old cross-lineage suffix behavior cannot be repaired in an already compiled
consumer. Never combine those old artifacts. The matrix deliberately does not
invoke an incompatible pointer. New clients must acquire API 2 explicitly;
there is no automatic downgrade or generic v1 output-mode inference.

## Integrator contract

Define `ALARM_SERVICE_TAGGED_V2` for the new descriptor. Physical builds use
`tagged-points-manifest.json` with the existing Points storage policy. Raw visual
uses `visual-points-manifest.json`; native UTC uses
`native-utc-visual-manifest.json`. Both visual modes require the tagged flag at
compile time. Grant/request `alarm.service` API 2 throughout the new cohort.
The System portable client currently requests API 1 and is intentionally not
edited here; integrator-owned consumers must adopt the new header/negotiation.
Do not co-advertise the new table as API 1.

Flag-off physical/Watch builds retain public paper 0.1.1/0.4.2 identities,
dependencies and behavior. The target gate compares the entire Watch ELF against
the exact public paper base, including symbols. It is not a comparison with a
product's delivered image. All build products are local development outputs.

Only the serialized sleep owner may call resume once after successful native
Light return with the unchanged successful prepare ticket. Refusal does not
call it. Wrong/reused/invalidated tickets return STALE without I/O. A successful
resume reads time once, rejects invalid/backward time or backward monotonic,
reanchors the clocks, and starts reconciliation; it does not start outputs,
write/ACK records, or imply the next sleep is safe. Intervening mutating service
calls invalidate tickets. External writers must be serialized and refresh the
service. Deep reset uses fresh-service durable recovery.

Native UTC uses the same ticket rules with platform.realtime. CONTEXT during
resume or another authorized phase fences custody: error -9, no later dependency
I/O/retry/start, failed quiescence, and retained invocation. A consumer must stop
normal work and retain resources. Ordinary output cleanup errors likewise do
not authorize unload, normal handoff, or implicit acknowledgment.

## Reproducible checks

Run with the pinned Runtime/System checkouts:

```
python3 scripts/test_alarm_abi.py --runtime ../runtime --system-apps ../system
python3 scripts/test_alarm_sleep_resume.py --runtime ../runtime --system-apps ../system
python3 scripts/test_native_utc_alarm.py --runtime ../runtime --system-apps ../system
python3 scripts/test_points_apps.py --runtime ../runtime --system-apps ../system
python3 scripts/test_alarm_apps.py --runtime ../runtime --system-apps ../system
python3 scripts/test_alarm_volume.py --runtime ../runtime --system-apps ../system
python3 scripts/test_dnd_service.py --runtime ../runtime --system-apps ../system
bash scripts/test_native_utc_alarm_runtime.sh ../runtime ../system
python3 scripts/build_native_utc_alarm.py --runtime ../runtime --system-apps ../system
```

The matrix separately compiles exact frozen old provider sources/headers and
System's real prefix client, plus the new physical/raw/native providers. New
production descriptor helpers test all pairings and malformed descriptors.
Behavior fixtures exercise normal and selected sanitizer builds. Their default
is ASan+UBSan; `ALARM_TEST_SANITIZERS=undefined` selects UBSan explicitly.
Linux CI runs the default and actual Runtime retention under sanitizers.
Local evidence/limitations are recorded in `docs/evidence/alarm-abi-0.4.4/`.
No System/product edits, hardware access, flashing, main merge or image changes
are part of this work.
