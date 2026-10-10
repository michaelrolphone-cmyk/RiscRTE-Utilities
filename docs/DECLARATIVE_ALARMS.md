# Declarative Alarms prototype

## Package boundary

Alarms **0.3.0** (`Apps/alarms_scene.c/.json`) is an opt-in replacement for the
legacy Alarms entry. The legacy build is preserved during migration. The new
application imports only Runtime, `ui.scene@1`, `alarm.control@1` and its bound
`storage.app-data@1` namespace. It has no display/input/font adapter, product
macro, coordinates, renderer, panel format, sleep driver or category branch.
Watch and X4 select the **same application ELF**.

The app declares routes, labels, fields, action IDs, validation/state and domain
operations. It does not decide whether its form occupies one page or several.
Home requests graceful suspension; Back from an editor applies the declared
business action (discard) instead of silently treating presentation pages as
application navigation.

Two separately versioned 0.1.0 `alarm.control@1` providers adapt the existing
scheduler without replacing it:

* `alarm-control-raw` uses the deployed API1 scheduler/raw-RTC time policy.
* `alarm-control-utc` uses the API2 native-UTC scheduler and saved timezone.

Both are presentation-independent. They preserve the current alarm configuration,
occurrence records, scheduling engine, countdown and Points-in-Time data. The
current application edits the existing one-shot alarm, not a new recurring-alarm
or Points-in-Time data model. Volume appears only when the service exposes it.

## Sustainable state and transactions

A 228-byte, versioned/checksummed `alarms.scene` checkpoint contains logical
route/focus, draft fields, observed revisions and an optional opaque pending
command. It contains no ELF pointers, callbacks, service grants, session tokens
or mount-local storage generations. Namespace selection belongs to boot policy;
the application asks for its unique authorized app-data service.

Graceful Home/suspend persists the draft before returning. The Runtime then
finalizes and unmaps the controller; a later invocation acquires fresh services
and reconstructs a fresh view. This is not a promise that unsaved typing survives
an arbitrary power failure. There is no write on every touch/repaint.

Before changing a saved alarm, the client persists the **exact** prepared command.
The control service freezes the time basis, previous record, next revision and
deadline. A retry reconciles that same before/after record; it never calculates a
new deadline from a later clock or creates a second schedule revision. Ambiguous
writes require exact readback. Stale data, expired pending deadlines, clock gaps
or folds, invalid zones and corrupt checkpoints are surfaced rather than silently
rewritten. Reset is a separate confirmed action and never cancels a saved alarm.

Display completion, input teardown and service retention remain separate from
model persistence. If teardown cannot prove safety, the invocation is retained.
After an explicit retained result or loss of its Runtime invocation, the client
performs no more service I/O, cleanup, diagnostic calls or release attempts.

Alarm scheduling does not belong to the view. The real Runtime integration test
arms an alarm, actually unloads Alarms, advances monotonic and wall time together,
then fires and dismisses it through the existing scheduler with **zero further UI
calls**. Product default/background ownership still chooses where reconciliation
runs. This increment does not introduce a new global idle/deep-sleep coordinator.

## Build and verification

Use sibling Runtime and System checkouts containing this prototype:

```
python scripts/test_alarms_scene.py --runtime ../Runtime --system-apps ../System
python scripts/test_alarms_scene.py --runtime ../Runtime --system-apps ../System --sanitize
python scripts/test_alarms_scene_runtime.py --runtime ../Runtime --system-apps ../System
python scripts/test_alarms_scene_runtime.py --runtime ../Runtime --system-apps ../System --sanitize
python scripts/build_alarms_scene.py --runtime ../Runtime --system-apps ../System --output build/scene-packages
```

The domain/controller suite has 58 executions, repeated under ASan/UBSan. The
production Runtime harness has six modes: compact-color and rotated monochrome,
each with demand, demand-retained and headless graphs. The physical buses are
explicit doubles; Runtime, provider graph, loader lifecycle, file storage,
controller, presenter, domain provider and scheduler are production code. Host
DSOs are actually `dlopen`/`dlclose`d; initialization/finalization/unmap witnesses
and `RTLD_NOLOAD` establish that restoration is not merely re-entry into resident
controller state. Host-only lifecycle hooks are never linked into target ELFs.

The target builder compiles one Xtensa Alarms ELF twice in distinct directories
and compares hashes. Both product selections consume that exact file. It checks
symbols and runs the existing strict native ELF validator on all target outputs.
These checks are not a physical Watch/X4 execution or power qualification.

## Product integration and remaining acceptance

`compose_alarms_scene.py` creates an explicit component stage from a current
product store. It changes only Alarms, its grants and the three selected providers.
It infers the exact existing scheduler storage bindings, preserves other package
bytes and selects a separate app-data namespace. It refuses namespace collisions,
clock-policy mismatches, version regressions and insufficient provider capacity.
It does not overwrite the source store or saved application data.

The existing Watch cohort selects 24 providers; this stage selects 27. Current
native firmware needs the generic capacity extension (28 providers/44 grants in
the prototype) with PSRAM metadata. Build-option text is not native proof: an
optional native ELF input is checked for the linked capacity witness. The old
`cohort.json` is deliberately removed; it cannot describe the changed packages.
**A component stage is not a flashable image.** The device repository must bind
it to its current native candidate/cohort and produce a new hardware-test image.
Do not flash a generic Runtime or substitute an older native snapshot for X4's
unpublished current native composition just to produce a binary.

The Watch and X4 repositories contain the corresponding opt-in profile and
staging instructions. Product-image binding and physical validation remain:
open/edit/arm, Home/relaunch draft restoration, alarm delivery with UI absent,
dismiss/cancel, existing countdown/Points preservation, touch/back behavior,
pending e-ink refresh teardown, sleep/wake, current draw and memory/performance.
No physical device was flashed by these scripts or tests.

## Future presenters

The scene declaration/event boundary permits an interactive terminal or remote
web presenter without changing Alarms. Those providers would own transport,
sessions, authentication and rendering. None is implemented or required here.
A headless graph may run alarm control and scheduling without selecting Alarms
GUI, `ui.scene`, a display, touch or navigation provider. Missing presentation
has no fallback that silently acquires hardware or starts networking.
