# Shared Viewer delivery

## Scope and preservation

Task `1dbd802d-3e68-4e90-985c-05c739d29c34`, original eight-point estimate. The custom renderer, OBJ/STL parsers, shading, preview atlas, touch transforms and controller implementation remain the existing `Apps/model_viewer.c` and `model_viewer_*.h`; native layout fixes from merged PR84 are reused, not rewritten as a scene UI. Hollow Trail retains its original adapter.

`lib/ViewerCanvas/client.c` adapts the existing engine to `ui.canvas@1`. It requests responsive logical dimensions, uses the existing extent limits, preserves queued-frame ownership, and converts the engine's one-is-black bitmap to the public one-is-white logical MONO1 encoding. Physical format, rotation, insets and touch mapping live in the System canvas provider and existing presentation profile. There are no device-class selections in the application.

## Capability and data behavior

The shared manifest requires canvas; missing/invalid presentation refuses startup with a Runtime diagnostic. Storage, selected-file sources and controllers are optional. Built-in cube remains available without storage. Saved OBJ/STL use the original private app-data names and namespace. Legacy X4 selected SD paths and Watch/Waveshare immutable file.open.source handoff both remain supported; short reads and BUSY are handled without manufacturing storage authority. Existing saved-file unavailable messaging remains in the renderer. Input gaps cancel gestures rather than create taps. Retained or uncertain provider custody retains the invocation and its allocations.

Product packaging transfers only the original Viewer display/touch/navigation grants behind its canvas provider, adds its canvas grant, and reuses each product's existing presentation profile. It preserves all other app policies, private namespaces, files, grants and image-source routes. No controller permission or external-volume authority is invented.

## Qualification and artifact boundaries

The owning workflow `.github/workflows/shared-viewer.yml` pins original product inputs, source repositories, boot-store readers and compiler. It exercises the actual renderer/client with the actual canvas provider, builds the complete Viewer twice, compares every ELF byte, validates the target structure, reconstructs original Watch/Waveshare/X4 stores and external SD packages, and runs the existing native Runtime graph/admission fixtures in normal and sanitized modes.

Provider metadata alone is compacted using System's proven loader-metadata-v3 method. Full provider sidecars and linker originals are retained; allocated bytes, loader sections, symbols, relocation targets and entrypoint are checked, alongside corruption rejection and deterministic compaction. The canonical Viewer ELF itself is complete, not reduced to an allocated-section hash.

Internal before/after inventory roots are separate from external SD before/after roots. SD byte identity and readback are packaging evidence, not claims that external target instructions executed. Native Runtime admission uses actual manifest/store authority and loader graph fixtures; no physical hardware execution is claimed.

A provider defect discovered by the optimized integration suite is corrected in System PR184: geometry equality compares values, not indeterminate C struct padding. The source includes poisoned-padding and every-field-change regression checks at O2.

## Handoff

Utilities PR118 and stacked System PR184 are review handoffs, not approval or merge requests. Final exact-head test counts, hashes, product readbacks and artifact links belong in the published qualification receipt and PR handoff after the owning run passes. Until then this is work in qualification, not a completed product claim.
