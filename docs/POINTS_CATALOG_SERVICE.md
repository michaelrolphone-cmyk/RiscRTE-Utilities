# Storage-scaled Points alarm service

These opt-in development profiles do not change the frozen Watch20 or X4
products. No release or product installation is part of this checkpoint.

- Watch reserves `alarm-service` 0.4.5, `alarm.service@1`, raw RTC with the
  existing UTC+08/Denver civil-time policy, short audio/haptic cues, DND and volume.
- X4 reserves 0.4.6, `alarm.service@2`, native UTC with the selected timezone,
  visual-only delivery and DND.
- `POINTS_CATALOG_SERVICE` selects the implementation. Both feature-unselected
  target ELFs remain byte-identical to the source baseline recorded in
  `sdk/points-catalog-service-sources.json`.

## Storage and migration

`points.catalog` is read-only and `points.ledger` is read-write through
`storage.app-data.bound@1`, each bound by exact filename to app-data namespace 5.
The policy examples contain the exact boot `storage` and `app_data` members.
Both profiles require ten explicit bound KV keys, including read-only legacy
Points configuration, metadata and occurrence keys. Runtime 0.1.83 now supplies
the qualified exact-file provider API and ten-key admission boundary. Public
commit 3fcbba6a04626f8484a523e7da59fb44c82e6e49 is tree-equivalent to the recorded
local target-build pin 34095f642d307f157ac0ed32ee5cb7f6532f9ac1. Both are accepted
by the build recipe; the actual selected checkout is recorded.

The service uses the filesystem catalog when present. Before the editor's first
file save, it reads legacy configuration and metadata, preserving custom names,
colors, schedules and original slot-derived IDs. A missing legacy configuration
uses the existing virtual factory schedule; it does not create or overwrite a
configuration. Legacy KV records are never written. A missing ledger migrates
matching legacy delivered bits and valid active occurrences. A previously
observed file disappearing, corrupt input or a revision rollback blocks delivery.

The PTL2 ledger has a 64-byte header, 16-byte entry per event, and a checksum.
Entries contain stable ID, scheduling revision, latest parent day and delivered
edge bits. Active identity includes event revision, parent day, edge, deadline,
recovery boundary, mode, mute marker and frozen timezone index. Per-event edits
reset only the edited event's cursor; insertion/deletion cannot transfer a cursor
to another event. Type-label changes preserve event scheduling revisions.

Committed state, reconciliation scans and desired writes own independent arrays.
Atomic replacement is checked by reading a complete document with its backend
revision. Ambiguous commits are resolved using read-only evidence; no blind
write retry occurs. A refused replacement or allocation leaves committed cursors
unchanged. CONTEXT/RETAINED permanently fences every dependency callback in the
current invocation, including clock, storage and output cleanup. Restart requires
a new invocation. Normal output failures retain the original cleanup behavior.

Capacity belongs to the filesystem backend and available allocation. No event
count limit or SD migration is introduced. Every event is scanned; missed edges
are compacted into a single ledger write. One-shot alarms and timers retain their
existing arbitration, acknowledgement, recovery and sleep-ticket behavior.

## Copied face projection

`PointsCatalogProjection.h` contains only POD types. `PointsServiceProjection.h`
adds separate size/tag/version-checked suffixes after unchanged Watch v1 sleep
and X4 v2 descriptor prefixes. `points_service_project` returns previous plus
four upcoming start/end rows, copied labels/colors/symbols, full 32-bit stable IDs and
revisions, source seconds, catalog revision, snapshot and exclusive `valid_until`.
The bounded projection is not a catalog capacity limit.

Labels are NUL-terminated printable ASCII, at most 31 characters. `snapshot` is
an invocation-local reconciliation counter; `catalog_revision` is durable.
`valid_until` is the fourth future deadline when full, otherwise seven days after
the snapshot, bounded by the time domain. Consumers must stop advancing cached
rows when `seconds >= valid_until`. A new retained schema is required; do not
reinterpret an old eight-slot retained record. X4 sparse timer wakes use only the
retained copy and must never call storage, service projection or SD.

The projection callback copies previously computed data without dependency I/O.
It returns the blocked error or terminal retained result rather than presenting
an invalid old snapshot as current. Faces must apply their own retained expiry
and malformed-data checks.

## Verification

`python3 scripts/test_points_catalog_service.py --runtime PATH --system-apps PATH`
runs the production service with normal and ASan/UBSan builds in both domains.
It covers 20 events/12 types, copied projection, stable IDs across deletion and
edits, legacy active migration, cold restart and light/deep-style wake recovery,
one-shot/timer arbitration, DND/volume, one-write missed compaction, allocation
refusal, uncertain committed/uncommitted writes, corrupt catalog/ledger, output
cleanup and no dependency I/O after stat/read/replace/KV retention. LeakSanitizer
is disabled because this executor cannot run its process inspection; ASan and
UBSan remain enabled.

`NATIVE_APP_CC=... python3 scripts/build_points_catalog_service.py --runtime PATH
--system-apps PATH` uses GCC8.4, validates both target ELFs and imports, proves
both unselected legacy binaries unchanged, and runs the real Xtensa loader at
eight alignments with independent relocated-section/redzone comparison. Exact
source and dependency hashes are emitted in `dist/points-catalog-service/`.
Hardware behavior and final product Runtime admission remain integration checks.

## Focused publication custody

The selected source was qualified at local 9bd572791a8304194ceb2b7542fc9cbd124e911b.
The public recipe replaces its historical Git-baseline lookup with a sealed
archive of exactly those alarm-service sources and headers. It also accepts
the verified public Runtime alias. All production C/header inputs are unchanged.
Run `scripts/check_points_catalog_sources.py` before the model/service tests.
The focused public reconstruction produces byte-identical selected service ELFs,
and its unselected Watch service also matches the preceding public Utilities
PR51 bytes. No broader X4 application or product history is included.
