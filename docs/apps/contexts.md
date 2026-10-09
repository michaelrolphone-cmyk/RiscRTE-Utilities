# Contexts 0.1.2 (development)

Contexts shows the current saved-signature room and event match from the awake
Audio/RF monitoring service. Unknown, ambiguous, paused, input-failed and
model-load-failed sources remain visibly distinct. The service imports saved
temporal event collections and validated neural checkpoints as well as legacy
signatures. Frequency-label catalogs remain outside this import contract.

Tap **Audio >** or **Radio >** to inspect model readiness. Missing saved events,
missing neural checkpoints, stale checkpoints and failed loads have separate
captions. Recognized events show their temporal or neural engine on the home
screen. An older service remains usable with a **Signatures only** fallback.
Neural inference refines eligible ambiguous temporal matches; it never trains
automatically or substitutes a model for unread source examples.

Monitoring is off when its preference is absent. Opening Contexts never writes
a default or creates a preset. The monitoring button explicitly enables or
disables it. **Load models** requests a fresh read from the original Audio
Spectrum and Waterfall owners, then returns to Clock for the bounded loading
handoff. It requires monitoring to be enabled. Saved samples stay owned by their
original apps, and no second persistent model copy is created.

**Presets** has eight independent slots. Choose a saved room, then select which
actions to apply: sleep mode, idle timer, deep timer, brightness, volume, Do Not
Disturb, notification output mode, and a deployment-provided watch face. Each
action can instead keep its current value. Enable the preset explicitly and use
**Save** to commit its one 64-byte record. A room's source, slot and exact saved
name identify it; selecting a new name or source is an explicit edit.

Fields remain a local draft until Save. Back or Discard asks before discarding
changed fields. A failed or unconfirmed save freezes the draft until retry or
discard. Discard reloads storage because an unconfirmed write may already have
committed. Unread or malformed saved records cannot be opened as empty slots or
silently overwritten. Watch-face choices come from the deployment's stable
catalog; generic builds with no catalog show the face action as unavailable.

Presets are applied by the shared client on a fresh confirmed room entry, not by
the editor. Manual changes, alarms, sleep and low-battery policy retain their
priority. Uncertain or partial writes are reported and never automatically
replayed. The service's [source and lifecycle contract](../CONTEXTS_SERVICE.md)
describes startup, source custody, freshness and model limits.

The source has a separate `contexts_apps` development inventory. Use
`scripts/test_contexts_app.py --system-apps /path/system-apps` for the actual
controller/shared NOVA raster tests and generated 240×240 screen evidence, and
`scripts/build_contexts_app.py --system-apps /path/system-apps` for a GCC 8.4
target ELF with structural/import/export checks. These are development
artifacts; Watch cohort integration and physical qualification are separate.

An RF-only provider reports Audio as `SOURCE UNAVAILABLE`; genuine model-load
failures keep the retry caption. Reloading all sources queues only the sources
that provider actually supports. No absent Audio model export is requested.

## Explicit X4 paper profile

[Contexts paper 0.1.3](../CONTEXTS_PAPER.md) adds capability-selected 480×800
rendering, continuous lists, shared Quick Controls and X4 automatic Light.
Select it with `build_contexts_app.py --profile x4-paper`; the default Watch
app and service stay 0.1.2. This local profile does not activate a product.
