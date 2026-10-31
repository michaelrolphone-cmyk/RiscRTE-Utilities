# Shared Countdown

`Apps/countdown_scene.c` is one provider-neutral foreground application. The
canonical `shared_scene_apps` recipe builds the same `countdown.elf` for all
selection cohorts. No product/display/backend macro changes the application.
Semantic integer/action/text components, navigation, focus, rendering, input,
suspend and shared resident controls are supplied by the scene/resident APIs.

## Preserved behavior and data

Duration is elapsed seconds, 1 through 359999 (99:59:59), with an initial five
minutes. Editing hours/minutes/seconds changes only the draft; Start prepares a
new revision. Cancel is an independent disabled revision. There is no invented
Pause, recurrence or local volume editor. A live running countdown is shown
from the provider's remaining seconds. Clock errors leave existing saved data
intact. Sound/haptic availability is disclosed independently of transient save
messages; physical output and occurrence/dismissal ownership stay in the
existing scheduler. Any current scheduler alert (including a one-shot Alarm)
remains visible and dismissible with its exact current token, as in the legacy
Countdown. Start, Cancel and duration editing remain disabled until that alert
is dismissed. Schedule edits themselves are restricted to Countdown records.

`countdown.control@1` owns the clock-policy adapter. Raw-calendar deployments
keep `timer_cfg`; native UTC deployments keep `timer_utc_cfg`. Both remain in
their existing namespace 3, and scheduler-owned occurrence keys remain in
namespace 4. Equal record sizes are **not** authority to reinterpret clock
domains or migrate records. UTC durations do not depend on a civil timezone.
Provider variants are clock-service adapters, not app variants.

The app uses a separate `storage.app-data@1` namespace for `countdown.scene`.
The selected product policy review reserves candidate namespace 66 after a
complete baseline package scan (separate from the proposed Alarms draft65).
Deployment must preserve any later-added authority and revalidate collisions;
this number is not a global hard-coded app assumption. The app acquires the
sole explicitly granted namespace using instance selection zero.

The app saves a copied, checksummed, pointer-free command before applying it.
An uncertain write retains the exact command, before/after records and deadline;
retry never recomputes a new deadline. Conflicts and invalid/expired pending
commands require explicit review/reset. Cancel cannot silently discard an
uncertain Start. Corrupt/unreadable drafts are not overwritten by normal exit.
Native clock-editor intents fence schedule mutations in the provider.

## Composition and qualification

The generic pure `project_schedule_scene.py` projector preserves every existing
scheduler row, unrelated application policy, image-source route, fallback and
manifest metadata. Only the reviewed app's grants and one new semantic control
provider are changed. The selected Watch path is internal; selected Wave and
X4 paths are SD `Apps/countdown/countdown.elf`, volumes12 and9 respectively.
A policy projection is not a built product image or a deployment.

Tests distinguish focused app/provider behavior, actual host Runtime execution,
Xtensa structural/build proof, complete product composition and physical
acceptance. A host DSO is not the target ELF. The two clock-policy providers are
reproducibility checked separately; the app is built without product defines
and its full bytes reused across the selections. No physical acceptance follows
from byte equality. Missing optional UI has no direct rendering fallback.

Existing source and historical tests remain retained. Current consumer routing
must select the shared registry entry rather than the legacy per-product
Countdown recipe; a canonical package alone does not establish that routing.
