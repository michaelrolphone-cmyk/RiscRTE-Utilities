# One shared Stopwatch application

`Apps/stopwatch_scene.c` is the capability-driven scene application. Its single
`stopwatch.elf` is independent of board, display size, pixel format and input
backend. The scene provider owns native geometry, fonts, pagination, touch,
buttons and display custody. A resident shell, when available, owns shared
controls and policy/sleep transitions. The app exports the foreground descriptor
and uses the existing `RiscSceneResidentV1` bridge, not hardware lifecycle calls.

## Behavior and saved data

The existing 99:59:59.99 stopwatch model and SW01/SWU1 checksummed formats are
unchanged. Monotonic elapsed time, start/pause, two-action reset, approximate
recovery, clock-discontinuity freeze, explicit save-pause and exact retry are
preserved. There was no lap list or lap encoding in the recovered application;
this migration does not introduce or claim lap data.

The authorized KV namespace remains instance **2**. `stopwatch` holds raw-calendar
SW01 data and `stopwatch_utc` holds native UTC SWU1 data. Both timers remain
independently accessible through the semantic domain action; neither record is
reinterpreted, overwritten by opening the other, or silently migrated between
clock domains. Initial presentation selects the sole existing saved domain regardless of clock
availability. If both/neither exist, UTC is preferred when its reader is available,
otherwise raw calendar. Only NOT_FOUND means absent: malformed/unreadable records
remain visible for recovery rather than being hidden behind an empty timer. A user can inspect the other saved timer even when its
clock is unavailable. Paused values remain readable without a clock. Runtime
monotonic failure during restore cannot authorize replacing the saved value.

Writes occur only for explicit start, pause/save-pause, confirmed reset or an
exact retry. A write is acknowledged only after exact-byte readback. Failed or
ambiguous writes keep the pending record and block conflicting reset, domain
switch and departure. A frozen save-pause likewise blocks departure until saved.
No periodic elapsed update writes storage. UTC uses the existing 2000 epoch
encoding/range; raw-calendar timing retains its original documented accuracy and
clock-edit limitations. See the original `stopwatch.md` accuracy discussion.

## Optional capabilities and lifecycle

The manifest permits absent optional capabilities. With no scene service the app
returns without accessing state. Missing storage/clock services are disclosed in
the scene; unavailable storage cannot cause writes. Runtime must provide its
versioned monotonic and retention APIs. Successful grants and releases are
validated, and context loss/unknown KV status/uncertain scene closure retain the
invocation with no subsequent provider I/O.

Lifecycle tracking coalesces elapsed updates while a raster or input contact is
busy. Without that optional scene suffix, explicit refresh remains available and
the live-display limitation is disclosed. Rejected handoffs use the documented
configure boundary rather than leaving the presenter input fenced. Resident
controls and policy dispatch happen only after confirmed scene closure; reopening
restores semantic navigation/model state. Physical wake, display timing, touch,
clock accuracy, grant provisioning and deployment acceptance remain separate
from these software tests.

## Verification

`scripts/test_stopwatch_scene.py --include SDK --output OUT [--sanitize]` runs the
production-controller scenarios. `scripts/test_stopwatch_provider.py
--system-apps SYSTEM --include SDK --output OUT [--sanitize]` composes production
Stopwatch documents with the real selected System scene provider at 240x240,
466x466 and 480x800, checks native raster bounds, semantic actions and input-busy
tracking. It also writes PPM captures (MONO1 maximum denotes black).

One target build and byte-identity/loader/real Runtime evidence are required in
addition to these host fixtures. Host tests do not establish physical-device
acceptance or a deployment/release.

`scripts/test_stopwatch_runtime.py --runtime RUNTIME --system-apps SYSTEM
--output OUT [--sanitize]` runs the real Runtime loader and provider graph with
production scene/profile providers and this exact application. Only physical
hardware and storage backend are deterministic doubles. It checks namespace-2
broker grants, start/save, departure, actual module unload, a two-second offline
gap, running-record recovery and pause/save on a second invocation. Three native
geometries (800x480 MONO1, 240x240 and 410x502 RGB565) use one byte-identical host
ELF under demand and retained-provider modes. A third mode denies optional UI and
verifies clean unload without frames or writes. Compiler/header/transitive-source
hashes are retained in `dependencies.json`; `results.json` records the common ELF
hash and each case. These are host-loader tests, not Xtensa execution or physical
hardware evidence. The application exports no host witness hooks in target builds.
