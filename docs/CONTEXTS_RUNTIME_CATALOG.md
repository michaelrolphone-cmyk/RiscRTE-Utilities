# Standalone Contexts runtime catalog

Contexts 0.4.5 replaces its compiled catalog with the generic read-only
`app.catalog@1` publication. The standalone scene application retains the
repaired shared keyboard, resident modal policy, input continuity, backend
liveness, learning, rules, settings, faces, timer and message behavior from
Utilities `d068784cb5d581f1e81b5021f5d96d2917e03ec3`.

## Explicit source union

- Scene/text hosts, SDK, lifecycle clients and especially
  `PortableContextsClient.h`: System
  `f4b30f8ebcc6d8cb905c8e90c91b1691acbe8165`.
- Only `PortableAppCatalog.h`, `RiscAppCatalogV1.h`, and
  `PortableOptionalService.h`: catalog System
  `72f435fcee376ec62d17ece6180753385ea547d9`.
- Native catalog contract: Runtime
  `a6c7f273025a59b1383e9c2795fa0ff0a8e9efad` or its reviewed successor.

The build accepts a separate `--catalog-system` and records these three files'
exact paths and SHA-256 values in the package receipt. It does not replace the
repaired scene/text/client source with the older catalog System tree. The
`t5_app_manifest_t` layouts are identical between these two System versions;
the unrelated trailing frame-pacing callbacks in `t5_app_api_v1` are not used
by this client. If the Runtime contains a conflicting catalog contract, staging
fails instead of silently overwriting it.

Example (standalone app only, not a firmware image):

```sh
python scripts/build_contexts_scene.py --runtime <native-runtime> \
  --system-apps <repaired-system> --catalog-system <catalog-system> \
  --target watch --shared-text-input --face-header <ContextsFaces.h> \
  --output <fresh-directory>
```

Retain the selected product's original face header and shared-input flag. No
hardware/product branch is added to app catalog code. The source manifest lists
`app.catalog` as optional, so detection, editing and other actions still work
without it; the owner must explicitly grant API 1, instance 0 for app selection
and workflow launches. The product composer must read the new manifest and add
that bounded grant. `--catalog` is rejected, `catalog.c` is never generated or
linked, and the builder checks the complete ELF symbol table for both obsolete
catalog symbols. Existing face declarations are unrelated to the app inventory.

## Selection and safety

Each refresh copies one coherent publication. The native owner is the only
source of rows, availability, routes and generation. Empty inventories are
valid. Ordinary failures clear stale rows; ambiguous custody retains the
invocation, original grant and allocations and stops callbacks/cleanup.

Selection validates the displayed generation before modifying a workflow.
Workflow launch refreshes the inventory, resolves the exact saved key and
prepares a generation/index/route ticket. After scene teardown it validates the
ticket again, checks persistent Home, and only then requests the canonical
native route. Removed, unavailable or changed entries cannot launch from stale
UI state. Rejected launches receive the existing failed workflow report.
Catalog grants are closed before resident handoff. Picker resumption refreshes
and rebuilds the document before reopening it; focus is remapped by the copied canonical route or explicit Refresh action.
Missing entries clear focus, even when a new grant reuses a generation number.
Unchanged inventories preserve the predecessor navigation behavior.

The existing workflow schema is unchanged (`text[33]`). The shared bridge uses
the canonical route below 128 bytes and `id:<native ID>` for longer routes.
Only exact keys of at most 32 characters fit a workflow. Unsupported longer
keys remain visible with `APP KEY TOO LONG FOR THIS RULE`; selection does not
truncate them or invent an alias. Long canonical routes are launched intact
when the bridge's exact ID key fits the existing schema.

## Smooth list and explicit capacity boundary

The predecessor used one continuous scene-host scrolling list. That interaction
is retained, with no manual pages. The component ABI supports 96 nodes (the
base scene API's separate 24-node limit does not apply). One node is reserved
for Refresh, so this checkpoint supports **95 catalog rows** in the chooser.
Unavailable rows are visible and cannot be selected.

At 96 through 128 native rows the entire chooser visibly refuses with
`CATALOG EXCEEDS DISPLAY LIMIT` and `NO APPS OMITTED / UPDATE PRESENTER`.
The inventory remains available to safely resolve previously saved workflow
keys. No prefix of the list is silently shown, and there is no pagination
fallback. A separately reviewed generic virtual-list extension is still needed
for smooth selection from oversized inventories. The scene-host ABI is not
changed by this checkpoint.

## Verification

```sh
python scripts/test_contexts_runtime_catalog.py --runtime <native-runtime> \
  --system-apps <repaired-system> --catalog-system <catalog-system> --sanitize
python scripts/test_contexts_shared_input.py --runtime <native-runtime> \
  --system-apps <repaired-system> --catalog-system <catalog-system> \
  --baseline-utilities <d068784-checkout> --profiles watch \
  --output <evidence-directory> --sanitize
python -m unittest discover -s tests -p test_contexts_shared_build.py -v
```

The focused suite uses the production standalone controller and dispatcher,
with owner transports substituted. It covers one/95 rows, visible refusal at
96/128, empty/absent/changed/malformed/duplicate inventories, bounded and long
identities, stale selection, launch removal/generation changes during close,
Home during final validation, launch rejection, ordinary/retained failures,
checkpoint refresh including empty inventory/focus repair, faces and actions.
The shared-input suite exercises actual production scene/text hosts, all five
editors and repaired modal/custody cases, and compares non-keyboard presentation
and persisted-rule hashes against the predecessor. Sanitizer fixtures suppress
leak checking because intentional retained-custody cases cannot release memory.
Target checks cover the standalone Xtensa application only. No firmware, device
flash, hardware test or remote publication is part of this checkpoint.
